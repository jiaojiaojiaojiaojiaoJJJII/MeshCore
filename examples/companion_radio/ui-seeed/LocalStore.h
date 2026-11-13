#pragma once
#include <L2Storage.h>
#include <Preferences.h>
#include "RadioPresets.h"

namespace seeed {
uint32_t checksum(const void* bytes, size_t size);
struct Settings {
  uint32_t magic = 0x4C325301;
  uint8_t firmware[32] = {};
  int16_t preset = -1;
  uint8_t chinese = 0, gps = 0, confirmed = 0, brightness = 128;
  int16_t timezoneMinutes = 0;
  uint32_t gpsSeconds = 300;
  char name[32] = "Wio L2 Pro";
  uint32_t crc = 0;
};

enum class MessageState : uint8_t { Received, Queued, Unconfirmed, Delivered, Failed, Transmitted };
struct Message {
  uint32_t magic = 0x4C324D01, id = 0, timestamp = 0, ack = 0, deadline = 0;
  uint8_t target[32] = {}; // contact public key or SHA256(channel key), never a name
  uint8_t channel = 0, outgoing = 0, unread = 0;
  MessageState state = MessageState::Received;
  char sender[32] = {}, text[161] = {};
  uint32_t crc = 0;
};

class LocalStore {
  static constexpr unsigned capacity = 48;
  Message messages[capacity];
  unsigned count = 0;
  uint32_t nextId = 1;
  bool historyHealthy = true;
  struct Draft {
    uint8_t target[32] = {}, channel = 0, used = 0;
    char text[193] = {};
    uint32_t crc = 0;
  } drafts[12];
  bool append(Message& m);
  bool compact();
public:
  bool loadSettings(Settings& settings);
  bool saveSettings(Settings settings);
  void begin();
  unsigned size() const { return count; }
  Message& at(unsigned i) { return messages[i]; }
  const Message& at(unsigned i) const { return messages[i]; }
  bool healthy() const { return historyHealthy; }
  bool add(Message& message);
  bool update(Message& message);
  unsigned unread() const;
  bool saveDraft(const uint8_t* target,bool channel,const char* text);
  void loadDraft(const uint8_t* target,bool channel,char* text,size_t capacity) const;
};
}
