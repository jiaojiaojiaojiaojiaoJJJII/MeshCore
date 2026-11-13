#include "target.h"
#include <esp_partition.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <algorithm>

L2FileSystem l2FS;

namespace {
const char* storageReason = "Not checked";
constexpr const char* partitionName = "spiffs";

void reportStorage() {
  const auto* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partitionName);
  Serial.printf("L2 storage: %s; internal Flash, not SD\n", storageReason);
  if (part) Serial.printf("L2 storage: partition=%s offset=0x%08lx size=0x%08lx; heap=%u PSRAM=%u\n",
      part->label, (unsigned long)part->address, (unsigned long)part->size,
      ESP.getFreeHeap(), ESP.getPsramSize());
  Serial.println("L2 storage: commands: status, retry, ERASE INTERNAL STORAGE");
}

bool initializeStorage() {
  // Explicit recovery only: never erase an unknown existing filesystem on boot.
  const auto* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partitionName);
  uint8_t check;
  if (!part || esp_partition_read(part, 0, &check, 1) != ESP_OK) {
    storageReason = "Internal partition is not readable"; return false;
  }
  Serial.println("L2 storage: explicit initialization requested; SD is not touched");
  if (!l2FS.format()) { storageReason = "Format failed"; return false; }
  if (!l2FS.begin()) {
    storageReason = "Mount failed after format"; return false;
  }
  // The mesh identity/history were reset; restart the matching local setup, too.
  l2ResetUserSettings();
  storageReason = "Ready after explicit initialization";
  Serial.printf("L2 storage: ready; total=%u used=%u\n",
      unsigned(l2FS.totalBytes()), unsigned(l2FS.usedBytes()));
  return true;
}

void recoveryScreen(DisplayDriver* screen, bool confirm, bool busy = false) {
  if (!screen) return;
  screen->startFrame(0x0841);
  screen->setColor(0xB766);
  screen->drawTextCentered(160, 15, confirm ? "Initialize internal storage?" : "Internal storage recovery");
  screen->setColor(0xFFFF);
  if (busy) {
    screen->drawTextCentered(160, 85, "Initializing internal Flash...");
    screen->drawTextCentered(160, 115, "Keep power connected.");
  } else {
    screen->drawTextCentered(160, 48, confirm ? "Erases internal identity, contacts" : storageReason);
    screen->drawTextCentered(160, 70, confirm ? "and message history/settings." : "Existing data has been preserved.");
    screen->drawTextCentered(160, 100, "This is not the SD card.");
    screen->drawTextCentered(160, 122, "SD card contents are not changed.");
    screen->drawTextCentered(160, 153, confirm ? "Tap Erase or hold BOOT for 3 seconds." : "Tap Initialize or click BOOT to review.");
    screen->setColor(0x01C9); screen->fillRect(10, 184, 140, 43);
    screen->setColor(confirm ? 0xA940 : 0xB766); screen->fillRect(165, 184, 145, 43);
    screen->setColor(0xFFFF); screen->drawTextCentered(80, 198, confirm ? "Cancel" : "Retry");
    screen->setColor(confirm ? 0xFFFF : 0x0841);
    screen->drawTextCentered(237, 198, confirm ? "Erase & initialize" : "Initialize...");
  }
  screen->endFrame();
}
}

bool L2FileSystem::begin() {
  if (backend != None) return true;
  const auto* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partitionName);
  if (!part) return false;
  bool littleHint = false;
  uint8_t header[256];
  for (size_t offset : {size_t(0), size_t(4096)}) {
    if (part->size < offset + sizeof(header) || esp_partition_read(part, offset, header, sizeof(header)) != ESP_OK) continue;
    for (size_t i = 0; i + 8 <= sizeof(header); ++i) if (!memcmp(header + i, "littlefs", 8)) littleHint = true;
  }
  if (littleHint) {
    Serial.println("L2 storage: detected existing LittleFS; trying compatible mount without format");
    if (LittleFS.begin(false, "/l2fs", 10, partitionName)) { attach(LittleFS, Little); return true; }
  }
  if (SPIFFS.begin(false, "/l2fs", 10, partitionName)) { attach(SPIFFS, Spiffs); return true; }
  if (!littleHint && LittleFS.begin(false, "/l2fs", 10, partitionName)) { attach(LittleFS, Little); return true; }
  return false;
}

bool L2FileSystem::format() {
  if (backend == Little) return LittleFS.format();
  // begin() set SPIFFS's partition label even if mounting failed.
  return SPIFFS.format();
}
size_t L2FileSystem::totalBytes() {
  return backend == Little ? LittleFS.totalBytes() : backend == Spiffs ? SPIFFS.totalBytes() : 0;
}
size_t L2FileSystem::usedBytes() {
  return backend == Little ? LittleFS.usedBytes() : backend == Spiffs ? SPIFFS.usedBytes() : 0;
}

bool l2BeginStorage() {
  if (l2FS.begin()) {
    storageReason = "Ready";
    Serial.printf("L2 storage: mounted %s; total=%u used=%u\n", l2FS.typeName(),
        unsigned(l2FS.totalBytes()), unsigned(l2FS.usedBytes()));
    return true;
  }
  const auto* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partitionName);
  if (!part) { storageReason = "Internal partition is missing"; reportStorage(); return false; }
  uint8_t bytes[512];
  for (size_t offset = 0; offset < part->size; offset += sizeof(bytes)) {
    size_t size = std::min(sizeof(bytes), size_t(part->size - offset));
    esp_err_t error = esp_partition_read(part, offset, bytes, size);
    if (error != ESP_OK) {
      storageReason = "Internal Flash read failed";
      Serial.printf("L2 storage: read error=%d at 0x%08lx\n", int(error), (unsigned long)(part->address + offset));
      reportStorage(); return false;
    }
    for (size_t i = 0; i < size; ++i) if (bytes[i] != 0xFF) {
      storageReason = "Existing filesystem cannot mount";
      Serial.printf("L2 storage: non-erased data at 0x%08lx; automatic format blocked\n",
          (unsigned long)(part->address + offset + i));
      reportStorage(); return false;
    }
    delay(1);
  }
  Serial.println("L2 storage: erased factory partition; creating SPIFFS");
  // Auto-initialize only a verified erased factory partition, preserving NVS.
  if (!l2FS.format()) { storageReason = "Factory format failed"; reportStorage(); return false; }
  if (!l2FS.begin()) {
    storageReason = "Factory mount failed"; reportStorage(); return false;
  }
  storageReason = "Ready";
  Serial.println("L2 storage: factory filesystem ready");
  return true;
}

void l2StorageRecovery(DisplayDriver* screen) {
  radio_driver.allowTransmit(false);
  user_btn.begin();
  bool confirm = false, touchHeld = false;
  uint32_t confirmAt = 0, heldAt = 0, reportAt = millis();
  char command[64] = {}; size_t length = 0;
  recoveryScreen(screen, false);
  reportStorage();
  for (;;) {
    uint32_t now = millis();
    int event = user_btn.check();
    bool redraw = false, retry = false, initialize = false;
    if (!confirm && event == BUTTON_EVENT_CLICK) { confirm = true; confirmAt = now; redraw = true; }
    if (confirm && user_btn.isPressed()) {
      if (!heldAt) heldAt = now;
      if (now - heldAt >= 3000) initialize = true;
    } else heldAt = 0;
    int x = 0, y = 0;
    bool down = screen && display.touch(x, y);
    if (down && !touchHeld && y >= 184 && y < 227) {
      if (x >= 10 && x < 150) {
        if (confirm) { confirm = false; heldAt = 0; redraw = true; }
        else retry = true;
      } else if (x >= 165 && x < 310) {
        if (confirm) initialize = true;
        else { confirm = true; confirmAt = now; redraw = true; }
      }
    }
    touchHeld = down;
    if (confirm && now - confirmAt > 20000) { confirm = false; heldAt = 0; redraw = true; }
    int budget = 128;
    while (budget-- && Serial.available()) {
      int c = Serial.read();
      if (c == '\r' || c == '\n') {
        command[length] = 0;
        if (!strcmp(command, "status")) reportStorage();
        else if (!strcmp(command, "retry")) retry = true;
        else if (!strcmp(command, "ERASE INTERNAL STORAGE")) initialize = true;
        length = 0;
      } else if (c >= 32 && c < 127 && length < sizeof(command) - 1) command[length++] = char(c);
      else length = 0;
    }
    if (retry) {
      if (l2BeginStorage()) return;
      confirm = false; redraw = true;
    }
    if (initialize) {
      recoveryScreen(screen, true, true);
      if (initializeStorage()) return;
      reportStorage(); confirm = false; heldAt = 0; redraw = true;
    }
    if (redraw) recoveryScreen(screen, confirm);
    if (now - reportAt >= 5000) { reportAt = now; reportStorage(); }
    board.loop();
    delay(20);
  }
}
