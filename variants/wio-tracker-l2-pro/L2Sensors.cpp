#include "L2Sensors.h"
#include <RTClib.h>
#include <stdlib.h>
#include <algorithm>
namespace { const unsigned gpsBauds[]={9600,115200,38400,57600,19200,4800}; }

bool L2GPS::setEnabled(bool on) {
  if(enabled==on) return true;
  if(on) {
    baudIndex=0; serial.setRxBufferSize(2048);
    serial.begin(gpsBauds[0],SERIAL_8N1,l2::GPS_RX,l2::GPS_TX);
    if(!board.setGpsPower(true)) { serial.end(); return false; }
    nmea.clear(); valid=false; courseMilli=-1; speedMilli=0; received=sentences=checksumErrors=0; rawUsed=0; lastRaw[0]=0;
    memset(visible,0,sizeof(visible)); baudChanged=millis();
  } else { if(!board.setGpsPower(false)) return false; serial.end(); valid=false; }
  enabled=on; return true;
}
unsigned L2GPS::baudRate() const { return gpsBauds[baudIndex]; }
long L2GPS::satellitesVisible() const { if(visible[4]) return visible[4]; return visible[0]+visible[1]+visible[2]+visible[3]; }
size_t L2GPS::diagnostics(char* output,size_t capacity) const {
  if(!capacity) return 0;
  int n=snprintf(output,capacity,"on=%u baud=%u rx=%lu good=%lu bad=%lu view=%ld used=%ld fix=%u age=%lu RX=%d TX=%d",enabled,baudRate(),(unsigned long)received,(unsigned long)sentences,(unsigned long)checksumErrors,satellitesVisible(),enabled?long(nmea.getNumSatellites()):0,enabled&&seen&&valid&&millis()-lastSample<5000,(unsigned long)(seen?millis()-lastSample:UINT32_MAX),l2::GPS_RX,l2::GPS_TX);
  return n>0?std::min(capacity-1,size_t(n)):0;
}
void L2GPS::reset() {
  if (!enabled) return;
  board.expWrite(l2::GpsReset, true); delay(100); board.expWrite(l2::GpsReset, false);
  nmea.clear(); valid = false; courseMilli=-1; speedMilli=0; memset(visible,0,sizeof(visible));
}
void L2GPS::loop() {
  if (!enabled) return;
  if((!sentences || uint32_t(millis()-lastGood)>15000) && uint32_t(millis()-baudChanged)>=5000) {
    baudIndex=(baudIndex+1)%(sizeof(gpsBauds)/sizeof(gpsBauds[0])); serial.updateBaudRate(gpsBauds[baudIndex]); nmea.clear(); rawUsed=0; baudChanged=millis();
  }
  int budget = 512; // bounded work: GPS cannot starve LoRa or UI
  while (budget-- && serial.available()) {
    char ch=serial.read(); ++received; lastData=millis();
    if(ch=='$') rawUsed=0;
    if(rawUsed<sizeof(raw)-1 && ch!='\r' && ch!='\n') raw[rawUsed++]=ch;
    if(ch=='\n') {
      raw[rawUsed]=0;
      if(rawUsed && MicroNMEA::testChecksum(raw)) {
        ++sentences; lastGood=millis(); snprintf(lastRaw,sizeof(lastRaw),"%s",raw);
        if(rawUsed>9 && !strncmp(raw+3,"GSV",3)) {
          const char* p=raw; for(int f=0;f<3 && p;++f) p=strchr(p,',')?strchr(p,',')+1:nullptr;
          int count=p?atoi(p):0,index=raw[2]=='N'?4:raw[2]=='P'?0:raw[2]=='L'?2:raw[2]=='A'?3:1;
          if(count>=0 && count<=99) visible[index]=count;
        }
      } else if(rawUsed) ++checksumErrors;
      rawUsed=0;
    }
    if (!nmea.process(ch)) continue;
    if (strcmp(nmea.getMessageID(), "GGA") && strcmp(nmea.getMessageID(), "RMC")) continue;
    valid = nmea.isValid();
    if (!valid) continue;
    if(!strcmp(nmea.getMessageID(),"RMC")) { courseMilli=nmea.getCourse(); speedMilli=nmea.getSpeed(); courseAt=millis(); }
    lastSample = millis(); seen = true;
    lastLat=nmea.getLatitude(); lastLon=nmea.getLongitude(); nmea.getAltitude(lastAlt);
    uint32_t utc = 0;
    if (nmea.getYear() >= 2026 && nmea.getMonth() > 0 && nmea.getDay() > 0) {
      utc = DateTime(nmea.getYear(), nmea.getMonth(), nmea.getDay(), nmea.getHour(), nmea.getMinute(), nmea.getSecond()).unixtime();
    }
    if (utc != lastUTC || sequence == 0) {
      ++sequence; lastUTC = utc;
      if (utc && waitingTimeSync()) { clock.setCurrentTime(utc); _time_sync_needed = false; }
    }
  }
}
void L2Sensors::loop() {
  gps.loop();
  if (gps.isValid() && gps.sampleId() != lastSequence &&
      (lastSequence == 0 || millis()-lastPublish >= interval*1000UL)) {
    node_lat = gps.getLatitude()/1000000.0;
    node_lon = gps.getLongitude()/1000000.0;
    node_altitude = gps.getAltitude()/1000.0;
    lastPublish = millis(); lastSequence = gps.sampleId();
  }
}
const char* L2Sensors::getSettingValue(int i) const {
  if (i == 0) return const_cast<L2GPS&>(gps).isEnabled() ? "1" : "0";
  snprintf(const_cast<char*>(intervalText), sizeof(intervalText), "%lu", (unsigned long)interval);
  return intervalText;
}
bool L2Sensors::setSettingValue(const char* key, const char* value) {
  if (!strcmp(key, "gps")) return gps.setEnabled(!strcmp(value, "1"));
  if (strcmp(key, "gps_interval")) return false;
  char* end;
  unsigned long seconds = strtoul(value, &end, 10);
  if (*end || seconds < 1 || seconds > 86400) return false;
  interval = seconds; return true;
}
bool L2Sensors::querySensors(uint8_t permissions, CayenneLPP& telemetry) {
  if ((permissions & TELEM_PERM_LOCATION) && gps.isValid()) {
    telemetry.addGPS(TELEM_CHANNEL_SELF, node_lat, node_lon, node_altitude); return true;
  }
  return false;
}
