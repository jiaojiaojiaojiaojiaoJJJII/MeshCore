#pragma once
#define LGFX_USE_V1
#include <FS.h>
#include <LovyanGFX.hpp>
#include <helpers/ui/DisplayDriver.h>
#include "L2Board.h"

class L2Panel : public lgfx::LGFX_Device {
  lgfx::Bus_SPI bus;
  lgfx::Panel_NV3031B panel;
public:
  L2Panel();
};

class L2Display : public DisplayDriver {
  L2Panel panel;
  lgfx::LGFX_Sprite frame;
  bool on = false, initialized = false;
  uint16_t color = 0xFFFF;
  uint8_t brightness = 128;
  uint16_t touchWidth = 240, touchHeight = 320;
  uint8_t* presented = nullptr;
  bool presentedValid = false, touchDown = false;
  int savedX = 0, savedY = 0;
  uint32_t touchReadAt = 0;
  uint32_t frameCount = 0, rowCount = 0, transferUs = 0, maxTransferUs = 0;
  uint32_t sleepCount = 0, wakeCount = 0, heldReports = 0, releaseReports = 0, touchErrors = 0;
public:
  L2Display() : DisplayDriver(320, 240), frame(&panel) {}
  bool begin();
  lgfx::LGFX_Sprite& canvas() { return frame; }
  bool touch(int& x, int& y);
  void logTouchInfo();
  size_t diagnostics(char* text, size_t capacity) const;
  size_t readFrameBytes(uint32_t offset,uint8_t* bytes,size_t count) const;
  bool isOn() override { return on; }
  void turnOn() override;
  void turnOff() override;
  bool setBrightness(uint8_t value);
  uint8_t getBrightness() const { return brightness; }
  void clear() override { frame.fillScreen(UIColor::window_bkg); }
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int size) override { frame.setTextSize(size); }
  void setColor(ColorVal c) override { color = c; frame.setTextColor(c); }
  void setCursor(int x, int y) override { frame.setCursor(x, y); }
  void print(const char* str) override { frame.print(str); }
  void fillRect(int x, int y, int w, int h) override { frame.fillRect(x,y,w,h,color); }
  void drawRect(int x, int y, int w, int h) override { frame.drawRect(x,y,w,h,color); }
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override { frame.drawBitmap(x,y,bits,w,h,color); }
  uint16_t getTextWidth(const char* str) override { return frame.textWidth(str); }
  void endFrame() override;
  void forceFullRefresh() override { presentedValid = false; }
};
