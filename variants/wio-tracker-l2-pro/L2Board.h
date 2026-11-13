#pragma once
#include <helpers/ESP32Board.h>
#include "L2Pins.h"

class L2Board : public ESP32Board {
  uint16_t outputs = 0, directions = 0xFFFF;
  bool ready = false, adcWaiting = false;
  uint16_t batteryMv = 0;
  uint32_t nextBattery = 0, adcStarted = 0;
  bool write16(uint8_t reg, uint16_t value);
  bool read16(uint8_t address, uint8_t reg, uint16_t& value, bool bigEndian = false);
public:
  void begin();
  void loop();
  bool ioReady() const { return ready; }
  bool expWrite(l2::ExpPin pin, bool high);
  bool expInput(l2::ExpPin pin);
  bool expRead(l2::ExpPin pin, bool& high);
  bool registerWrite(uint8_t address, uint8_t reg, uint8_t value);
  bool setGpsPower(bool on);
  bool initBacklight(uint8_t brightness);
  bool setBacklight(uint8_t brightness);
  bool cardPresent();
  uint16_t getBattMilliVolts() override { return batteryMv; }
  const char* getManufacturerName() const override { return "Seeed Wio Tracker L2 Pro"; }
  void powerOff() override;
};
