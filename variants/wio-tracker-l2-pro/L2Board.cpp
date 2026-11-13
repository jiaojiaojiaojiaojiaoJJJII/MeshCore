#include "L2Board.h"
#include <esp_sleep.h>

bool L2Board::write16(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(l2::EXP_ADDR);
  Wire.write(reg); Wire.write(uint8_t(value)); Wire.write(uint8_t(value >> 8));
  return Wire.endTransmission() == 0;
}

bool L2Board::read16(uint8_t address, uint8_t reg, uint16_t& value, bool bigEndian) {
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, uint8_t(2)) != 2) return false;
  uint8_t a = Wire.read(), b = Wire.read();
  value = bigEndian ? (uint16_t(a) << 8) | b : (uint16_t(b) << 8) | a;
  return true;
}

bool L2Board::expWrite(l2::ExpPin pin, bool high) {
  if (!ready) return false;
  uint16_t next = high ? outputs | (1U << pin) : outputs & ~(1U << pin);
  if (!write16(2, next)) return false;
  outputs = next;
  uint16_t dir = directions & ~(1U << pin);
  if (!write16(6, dir)) return false;
  directions = dir;
  return true;
}

bool L2Board::expInput(l2::ExpPin pin) {
  uint16_t dir = directions | (1U << pin);
  if (!ready || !write16(6, dir)) return false;
  directions = dir; return true;
}

bool L2Board::expRead(l2::ExpPin pin, bool& high) {
  uint16_t bits;
  if (!ready || !read16(l2::EXP_ADDR, 0, bits)) return false;
  high = (bits & (1U << pin)) != 0; return true;
}

bool L2Board::registerWrite(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}

void L2Board::begin() {
  ESP32Board::begin();
  Wire.setClock(100000); Wire.setTimeOut(20);
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
  pinMode(l2::EXP_IRQ, INPUT_PULLUP);
  // Set safe latches BEFORE enabling outputs. Inputs P00..P03 stay inputs.
  // P04 is CODEC_EN on V1.0, not a GPIO-controlled SD chip select.
  outputs = 0; directions = 0x000F;
  ready = write16(2, outputs) && write16(6, directions);
  if (!ready) { Serial.println("L2: PCA9535 unavailable"); return; }
  expWrite(l2::BatterySense, true);
  // GPS, codec, speaker, SD, Grove and USB OTG remain off until requested.
}

bool L2Board::setGpsPower(bool on) {
  // Assert reset before changing GNSS power.
  if (on) {
    if (!expWrite(l2::GpsReset, true) || !expWrite(l2::GpsPower, true)) return false;
    delay(120); // allow GNSS rail to settle while RESET_N is asserted
    return expWrite(l2::GpsReset, false);
  }
  bool ok = expWrite(l2::GpsReset, true);
  return expWrite(l2::GpsPower, false) && ok;
}

bool L2Board::initBacklight(uint8_t brightness) {
  bool ok = registerWrite(l2::LIGHT_ADDR, 0x00, 1);
  ok = registerWrite(l2::LIGHT_ADDR, 0x01, 0) && ok; // 25.5mA/channel maximum
  ok = registerWrite(l2::LIGHT_ADDR, 0x02, 0) && ok;
  ok = registerWrite(l2::LIGHT_ADDR, 0x04, 0x4E) && ok;
  ok = registerWrite(l2::LIGHT_ADDR, 0x05, 0xF0) && ok;
  // Configure the backlight output current.
  for (int i = 0; i < 4; ++i) ok = registerWrite(l2::LIGHT_ADDR, 0x14+i, 153) && ok;
  ok = registerWrite(l2::LIGHT_ADDR, 0x02, 0x0F) && ok;
  ok = registerWrite(l2::LIGHT_ADDR, 0x0F, 0x55) && ok;
  delay(5);
  return setBacklight(brightness) && ok;
}

bool L2Board::setBacklight(uint8_t brightness) {
  bool ok = true;
  for (int i = 0; i < 4; ++i) ok = registerWrite(l2::LIGHT_ADDR, 0x18+i, brightness) && ok;
  return ok;
}

bool L2Board::cardPresent() {
  bool high;
  return expRead(l2::CardDetect, high) && !high;
}

void L2Board::loop() {
  if (!ready) return;
  uint32_t now = millis();
  if (adcWaiting) {
    if (now - adcStarted < 10) return;
    uint16_t raw;
    if (read16(l2::ADC_ADDR, 0, raw, true) && int16_t(raw) > 0) {
      // ADS1115 AIN0, PGA +/-4.096V: 0.125mV/LSB; R162/R163 = 1:1.
      batteryMv = (uint32_t(raw) + 2) / 4;
    } else batteryMv = 0;
    adcWaiting = false; nextBattery = now + 10000;
  } else if (int32_t(now - nextBattery) >= 0) {
    Wire.beginTransmission(l2::ADC_ADDR);
    Wire.write(1); Wire.write(0xC3); Wire.write(0x83); // AIN0 single-shot 128SPS
    adcWaiting = Wire.endTransmission() == 0;
    adcStarted = now;
    if (!adcWaiting) { batteryMv = 0; nextBattery = now + 10000; }
  }
}

void L2Board::powerOff() {
  setBacklight(0);
  setGpsPower(false);
  expWrite(l2::AmplifierPower, false);
  expWrite(l2::CodecPower, false);
  expWrite(l2::CardPower, false);
  expWrite(l2::BatterySense, false);
  // Keep LCD rail on: powering it down can clamp the shared I2C bus.
  // GPIO0 is an RTC pin; wake button P00 is NOT an ESP32 GPIO.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  Serial.flush();
  esp_deep_sleep_start();
}
