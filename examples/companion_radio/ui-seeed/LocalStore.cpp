#include "LocalStore.h"
#include <stddef.h>

namespace seeed {
uint32_t checksum(const void* bytes, size_t size) {
  const uint8_t* p = static_cast<const uint8_t*>(bytes);
  uint32_t crc = 0xFFFFFFFF;
  while (size--) {
    crc ^= *p++;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U-(crc & 1U)));
  }
  return ~crc;
}
bool LocalStore::loadSettings(Settings& settings) {
  Preferences nvs;
  if (!nvs.begin("seeed-l2", true)) return false;
  Settings saved{};
  bool ok = nvs.getBytes("settings", &saved, sizeof(saved)) == sizeof(saved);
  nvs.end();
  ok = ok && saved.magic == settings.magic && saved.crc == checksum(&saved, offsetof(Settings, crc));
  ok = ok && saved.name[31] == 0 && saved.name[0] && saved.chinese <= 1 &&
       saved.gps <= 1 && saved.confirmed <= 1 && saved.preset >= -1 &&
       saved.preset < int(presetCount) && saved.timezoneMinutes >= -720 &&
       saved.timezoneMinutes <= 840 && saved.gpsSeconds >= 1 && saved.gpsSeconds <= 86400;
  if (ok) settings = saved;
  return ok;
}
bool LocalStore::saveSettings(Settings settings) {
  settings.crc = checksum(&settings, offsetof(Settings, crc));
  Preferences nvs;
  if (!nvs.begin("seeed-l2", false)) return false;
  bool ok = nvs.putBytes("settings", &settings, sizeof(settings)) == sizeof(settings);
  nvs.end();
  Settings check{};
  return ok && loadSettings(check) && !memcmp(&check, &settings, sizeof(check));
}
void LocalStore::begin() {
  Preferences nvs;
  if (nvs.begin("seeed-l2",true)) {
    nvs.getBytes("drafts",drafts,sizeof(drafts)); nvs.end();
    for (auto& draft : drafts) if(draft.text[192] || draft.used > 1 || draft.channel > 1 ||
        draft.crc != checksum(&draft,offsetof(Draft,crc))) draft = Draft{};
  }
  // Recover a power loss between moving the old log aside and installing a snapshot.
  if (!l2FS.exists("/l2-messages") && l2FS.exists("/l2-messages.bak")) {
    if (!l2FS.rename("/l2-messages.bak", "/l2-messages")) { historyHealthy=false; return; }
  }
  File file = l2FS.open("/l2-messages", "r");
  if (!file) return;
  Message m{};
  while (file.available()) {
    if (file.read(reinterpret_cast<uint8_t*>(&m), sizeof(m)) != sizeof(m) ||
        m.magic != 0x4C324D01 || uint8_t(m.state) > uint8_t(MessageState::Transmitted) ||
        m.channel > 1 || m.outgoing > 1 || m.unread > 1 || m.crc != checksum(&m, offsetof(Message, crc))) {
      historyHealthy = false; break;
    }
    m.sender[31] = 0; m.text[160] = 0;
    int found = -1;
    for (unsigned i = 0; i < count; ++i) if (messages[i].id == m.id) { found = i; break; }
    if (found >= 0) messages[found] = m;
    else {
      if (count == capacity) { memmove(messages, messages+1, sizeof(Message)*(capacity-1)); --count; }
      messages[count++] = m;
    }
    if (m.id >= nextId) nextId = m.id + 1;
  }
  file.close();
  if (historyHealthy) l2FS.remove("/l2-messages.bak");
  // A reboot loses in-flight send evidence. Preserve text; never claim delivery.
  for (unsigned i=0;i<count;++i) if (messages[i].state == MessageState::Queued ||
      (messages[i].state == MessageState::Transmitted && !messages[i].channel)) messages[i].state = MessageState::Unconfirmed;
}
bool LocalStore::compact() {
  File out = l2FS.open("/l2-messages.next", "w");
  if (!out) return false;
  bool ok=true;
  for (unsigned i=0; i<count && ok; ++i) {
    Message snapshot=messages[i];
    snapshot.crc=checksum(&snapshot,offsetof(Message,crc));
    ok=out.write(reinterpret_cast<const uint8_t*>(&snapshot),sizeof(snapshot))==sizeof(snapshot);
  }
  out.flush(); out.close();
  File verify=l2FS.open("/l2-messages.next","r");
  ok=ok && verify && verify.size()==count*sizeof(Message);
  for (unsigned i=0; i<count && ok; ++i) {
    Message check{};
    Message expected=messages[i]; expected.crc=checksum(&expected,offsetof(Message,crc));
    ok=verify.read(reinterpret_cast<uint8_t*>(&check),sizeof(check))==sizeof(check) &&
       !memcmp(&check,&expected,sizeof(check));
  }
  verify.close();
  if (!ok) { l2FS.remove("/l2-messages.next"); return false; }
  if (l2FS.exists("/l2-messages.bak") && !l2FS.remove("/l2-messages.bak")) return false;
  bool hadLog=l2FS.exists("/l2-messages");
  if (hadLog && !l2FS.rename("/l2-messages","/l2-messages.bak")) return false;
  if (!l2FS.rename("/l2-messages.next","/l2-messages")) {
    if (hadLog && !l2FS.rename("/l2-messages.bak","/l2-messages")) historyHealthy=false;
    return false;
  }
  l2FS.remove("/l2-messages.bak");
  return true;
}

bool LocalStore::append(Message& m) {
  // A corrupt tail must be repaired explicitly; don't append unreachable records.
  if (!historyHealthy) return false;
  File existing=l2FS.open("/l2-messages","r");
  bool rollover=existing && existing.size()+sizeof(m)>64*1024;
  existing.close();
  if (rollover && !compact()) return false;
  File file = l2FS.open("/l2-messages", "a");
  if (!file) return false;
  m.crc = checksum(&m, offsetof(Message, crc));
  size_t offset = file.size();
  bool ok = file.write(reinterpret_cast<const uint8_t*>(&m), sizeof(m)) == sizeof(m);
  file.flush(); file.close();
  File verify = l2FS.open("/l2-messages", "r");
  Message check{};
  ok = ok && verify && verify.seek(offset) && verify.read(reinterpret_cast<uint8_t*>(&check), sizeof(check)) == sizeof(check);
  verify.close();
  ok = ok && !memcmp(&check, &m, sizeof(m));
  if (!ok) historyHealthy = false;
  return ok;
}
bool LocalStore::add(Message& m) {
  m.id = nextId++;
  bool ok = append(m);
  if (count == capacity) { memmove(messages, messages+1, sizeof(Message)*(capacity-1)); --count; }
  messages[count++] = m;
  return ok;
}
bool LocalStore::update(Message& m) { return append(m); }
unsigned LocalStore::unread() const {
  unsigned n = 0; for (unsigned i=0;i<count;++i) n += messages[i].unread != 0; return n;
}
bool LocalStore::saveDraft(const uint8_t* target,bool channel,const char* text) {
  if(strlen(text) >= sizeof(Draft::text)) return false;
  int slot=-1;
  for(int i=0;i<12;++i) if(drafts[i].used && drafts[i].channel==channel && !memcmp(drafts[i].target,target,32)) { slot=i; break; }
  if(slot<0) for(int i=0;i<12;++i) if(!drafts[i].used) { slot=i; break; }
  if(slot<0) return !text[0];
  if(drafts[slot].used==bool(text[0]) && drafts[slot].channel==channel &&
     !memcmp(drafts[slot].target,target,32) && !strcmp(drafts[slot].text,text)) return true;
  Draft old=drafts[slot], next{};
  memcpy(next.target,target,32); next.channel=channel; next.used=text[0]!=0;
  strcpy(next.text,text); next.crc=checksum(&next,offsetof(Draft,crc)); drafts[slot]=next;
  Preferences nvs;
  if(!nvs.begin("seeed-l2",false)) { drafts[slot]=old; return false; }
  bool ok=nvs.putBytes("drafts",drafts,sizeof(drafts))==sizeof(drafts); nvs.end();
  if(!ok) drafts[slot]=old;
  return ok;
}
void LocalStore::loadDraft(const uint8_t* target,bool channel,char* text,size_t capacity) const {
  if(!capacity) return;
  text[0]=0;
  for(auto& draft:drafts) if(draft.used && draft.channel==channel && !memcmp(draft.target,target,32)) {
    snprintf(text,capacity,"%s",draft.text); return;
  }
}
}
