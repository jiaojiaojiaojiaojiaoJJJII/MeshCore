#include "target.h"
#include <esp_system.h>
#include <esp_partition.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <algorithm>
L2Board board;
static SPIClass loraSpi(FSPI); // SPI2; LCD owns SPI3
CustomSX1262 radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, loraSpi);
L2RadioWrapper radio_driver(radio, board);
ESP32RTCClock rtc_clock;
L2Sensors sensors(board, rtc_clock);
L2Display display;
MomentaryButton user_btn(PIN_USER_BTN, 800, true, true);
bool radio_init() {
  rtc_clock.begin();
  return radio.std_init(&loraSpi);
}
mesh::LocalIdentity radio_new_identity() {
  class HardwareRNG : public mesh::RNG {
    void random(uint8_t* dest, size_t size) override { esp_fill_random(dest, size); }
  } rng;
  return mesh::LocalIdentity(&rng);
}

void l2ResetUserSettings() {
  radio_driver.allowTransmit(false);
  Preferences nvs;
  if(nvs.begin("seeed-l2",false)) { nvs.clear(); nvs.end(); }
}
