#pragma once
#include <MicroNMEA.h>
#include <helpers/SensorManager.h>
#include "L2Board.h"

class L2GPS : public LocationProvider {
  L2Board& board;
  mesh::RTCClock& clock;
  HardwareSerial serial{1};
  char buffer[120];
  MicroNMEA nmea{buffer, sizeof(buffer)};
  bool enabled = false, seen = false, valid = false;
  uint32_t lastSample = 0, sequence = 0, lastUTC = 0;
  uint32_t received=0,sentences=0,checksumErrors=0,lastData=0,lastGood=0,baudChanged=0;
  unsigned baudIndex=0; uint8_t visible[5]={};
  char raw[128]={},lastRaw[128]={}; unsigned rawUsed=0;
  long lastLat = 0, lastLon = 0, lastAlt = 0;
  long courseMilli=-1,speedMilli=0; uint32_t courseAt=0;
public:
  L2GPS(L2Board& b, mesh::RTCClock& c) : board(b), clock(c) {}
  bool setEnabled(bool on);
  size_t diagnostics(char* output,size_t capacity) const;
  uint32_t bytesReceived() const { return received; }
  uint32_t sentenceCount() const { return sentences; }
  unsigned baudRate() const;
  long satellitesVisible() const;
  const char* lastSentence() const { return lastRaw; }
  void begin() override { setEnabled(true); }
  void stop() override { setEnabled(false); }
  void reset() override;
  void loop() override;
  bool isEnabled() override { return enabled; }
  bool isValid() override { return enabled && seen && valid && millis()-lastSample < 5000; }
  bool hasCourse() { return isValid() && millis()-courseAt<5000 && courseMilli>=0 && courseMilli<360000 && speedMilli>=1000; }
  float courseDegrees() const { return courseMilli/1000.0f; }
  float speedKmh() const { return speedMilli>0?speedMilli*0.001852f:0; }
  bool hasPosition() const { return seen; }
  uint32_t age() const { return seen ? millis()-lastSample : UINT32_MAX; }
  uint32_t sampleId() const { return sequence; }
  long getLatitude() override { return lastLat; }
  long getLongitude() override { return lastLon; }
  long getAltitude() override { return lastAlt; }
  long satellitesCount() override { return enabled ? nmea.getNumSatellites() : 0; }
  long getTimestamp() override { return lastUTC; }
  void sendSentence(const char* sentence) override { if (enabled) MicroNMEA::sendSentence(serial, sentence); }
};

class L2Sensors : public SensorManager {
  L2GPS gps;
  uint32_t interval = 300, lastPublish = 0, lastSequence = 0;
  char intervalText[12];
public:
  L2Sensors(L2Board& board, mesh::RTCClock& clock) : gps(board, clock) {}
  bool begin() override { return true; }
  void loop() override;
  L2GPS& location() { return gps; }
  LocationProvider* getLocationProvider() override { return &gps; }
  bool isGPSDetected() const override { return true; }
  int getNumSettings() const override { return 2; }
  const char* getSettingName(int i) const override { return i == 0 ? "gps" : "gps_interval"; }
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* key, const char* value) override;
  bool querySensors(uint8_t permissions, CayenneLPP& telemetry) override;
};
