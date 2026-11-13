#pragma once
#include <stdint.h>

// Wio Tracker L2 Pro pin configuration.
namespace l2 {
constexpr int SDA = 47, SCL = 48, EXP_IRQ = 45;
constexpr int LCD_CS = 46, LCD_CLK = 42;
constexpr int LCD_D0 = 41, LCD_D1 = 40, LCD_D2 = 39, LCD_D3 = 38;
constexpr int GPS_RX = 18, GPS_TX = 17;
constexpr int SD_CLK = 2, SD_CMD = 3, SD_D0 = 1;
constexpr int AUDIO_MCLK = 10, AUDIO_BCLK = 11, AUDIO_WS = 12;
constexpr int AUDIO_OUT = 16, AUDIO_IN = 15;
constexpr uint8_t EXP_ADDR = 0x21, ADC_ADDR = 0x48;
constexpr uint8_t TOUCH_ADDR = 0x5D;
// TI LP5814 section 7.5.2 specifies the 7-bit address.
constexpr uint8_t LIGHT_ADDR = 0x2C;
enum ExpPin : uint8_t {
  Wake = 0, UsbInterrupt = 1, CardDetect = 2, TouchInterrupt = 3,
  CodecPower = 4, LcdPower = 5, LcdReset = 6, GrovePower = 7,
  TouchReset = 8, GpsReset = 9, UserLed = 10, OtgPower = 11,
  AmplifierPower = 12, GpsPower = 13, CardPower = 14, BatterySense = 15
};
}
