#include "L2Display.h"
#include "target.h"
#include <esp_heap_caps.h>
#include <algorithm>

// LovyanGFX 1.2.28's NV3031B init table is odr-used without an
// out-of-class definition. Supply it once for this project's C++11 toolchain.
namespace lgfx { inline namespace v1 {
constexpr uint8_t Panel_NV3031B::init_cmds[];
} }

namespace {
bool readTouchRegister(uint16_t reg, uint8_t* data, uint8_t size) {
  Wire.beginTransmission(l2::TOUCH_ADDR);
  Wire.write(uint8_t(reg >> 8)); Wire.write(uint8_t(reg));
  if (Wire.endTransmission(false) || Wire.requestFrom(l2::TOUCH_ADDR, size) != size) return false;
  for (unsigned i = 0; i < size; ++i) data[i] = Wire.read();
  return true;
}
}

ColorVal UIColor::window_bkg = 0x0841, UIColor::title_bkg = 0x01C9;
ColorVal UIColor::title_txt = 0xFFFF, UIColor::primary_txt = 0xFFFF;
ColorVal UIColor::secondary_txt = 0xA534, UIColor::warning_txt = 0xFDE0;
ColorVal UIColor::popup_bkg = 0x1923, UIColor::popup_txt = 0xFFFF, UIColor::corp_blue = 0xB766;

L2Panel::L2Panel() {
  auto b = bus.config();
  b.spi_host = SPI3_HOST; b.spi_mode = 3;
  b.freq_write = 40000000; b.freq_read = 16000000;
  b.pin_sclk = l2::LCD_CLK; b.pin_io0 = l2::LCD_D0;
  b.pin_io1 = l2::LCD_D1; b.pin_io2 = l2::LCD_D2; b.pin_io3 = l2::LCD_D3;
  bus.config(b); panel.setBus(&bus);
  auto p = panel.config();
  p.pin_cs = l2::LCD_CS; p.pin_rst = -1; p.pin_busy = -1;
  p.panel_width = p.memory_width = 240;
  p.panel_height = p.memory_height = 320;
  p.offset_rotation = 1; p.invert = true; p.rgb_order = true;
  p.bus_shared = false; panel.config(p); setPanel(&panel);
}

bool L2Display::begin() {
  if (initialized) { turnOn(); return true; }
  if (!board.ioReady() || !board.expWrite(l2::LcdPower, true)) return false;
  delay(50);
  bool ok = board.expWrite(l2::LcdReset, true);
  delay(5); ok = board.expWrite(l2::LcdReset, false) && ok;
  delay(10); ok = board.expWrite(l2::LcdReset, true) && ok;
  // GT911 address selection: hold INT LOW while releasing reset -> 0x5D.
  ok = board.expWrite(l2::TouchReset, false) && ok;
  ok = board.expWrite(l2::TouchInterrupt, false) && ok;
  delay(10); ok = board.expWrite(l2::TouchReset, true) && ok;
  delay(60); ok = board.expInput(l2::TouchInterrupt) && ok;
  if (!ok || !panel.init()) return false;
  logTouchInfo();
  panel.setRotation(0); // panel offset_rotation=1 gives landscape 320x240
  frame.setPsram(true); frame.setColorDepth(16);
  if (!frame.createSprite(320, 240)) return false;
  presented = static_cast<uint8_t*>(heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!presented) Serial.println("L2 display: no PSRAM for differential refresh; full-frame fallback");
  presentedValid = false;
  frame.setFont(&fonts::efontCN_16);
  initialized = on = true;
  if (!board.initBacklight(brightness)) Serial.println("L2: LP5814 unavailable at 0x2C");
  return true;
}

void L2Display::startFrame(ColorVal bkg) {
  frame.fillScreen(bkg); frame.setTextColor(UIColor::primary_txt);
  frame.setTextSize(1); frame.setCursor(0, 0);
}

void L2Display::turnOn() {
  if (!initialized || on) return;
  panel.wakeup(); on = true; presentedValid = false; ++wakeCount;
  endFrame(); board.setBacklight(brightness);
  Serial.println("L2 display: wake");
}
void L2Display::turnOff() {
  if (!initialized || !on) return;
  board.setBacklight(0); panel.sleep(); on = false; ++sleepCount;
  Serial.println("L2 display: sleep");
}
bool L2Display::setBrightness(uint8_t value) {
  if (!board.setBacklight(on ? value : 0)) return false;
  brightness = value; return true;
}

// Use Wire for all board I2C traffic. Mixing LGFX's ESP-IDF I2C driver and
// Arduino Wire on the same peripheral can leave BUSY set after a NACK.
bool L2Display::touch(int& x, int& y) {
  if (!initialized) return false;
  uint32_t now = millis();
  uint8_t status;
  if (!readTouchRegister(0x814E, &status, 1)) {
    ++touchErrors;
    // A short bus retry must not turn a continuous hold into a release.
    if (now - touchReadAt > 100) touchDown = false;
    x=savedX; y=savedY; return touchDown;
  }
  touchReadAt = now;
  if (!(status & 0x80)) {
    // No new report is NOT a release: GT911 can stay quiet under a stationary
    // finger. Keep the last state until a ready report explicitly says zero.
    if (touchDown) ++heldReports;
    x=savedX; y=savedY; return touchDown;
  }
  uint8_t p[8];
  bool valid = (status & 0x0F) > 0 && (status & 0x0F) <= 5 && readTouchRegister(0x8150, p, 8);
  Wire.beginTransmission(l2::TOUCH_ADDR);
  Wire.write(0x81); Wire.write(0x4E); Wire.write(0);
  bool acknowledged=Wire.endTransmission()==0;
  if ((status & 0x0F) == 0) {
    if (touchDown) ++releaseReports;
    touchDown=false; return false;
  }
  if (!acknowledged || !valid) {
    ++touchErrors; x=savedX; y=savedY; return touchDown;
  }
  // NV3031B panel offset_rotation=1 plus GT911 offset_rotation=2 gives
  // effective rotation 3: swap axes and invert landscape X. The opposite
  // rotation maps the on-screen "0" key onto "-" (a 180-degree mismatch).
  int rawX = p[0] | (uint16_t(p[1]) << 8);
  int rawY = p[2] | (uint16_t(p[3]) << 8);
  if (rawX >= touchWidth || rawY >= touchHeight) { ++touchErrors; x=savedX; y=savedY; return touchDown; }
  x = savedX = 319 - (uint32_t(rawY) * 319 + (touchHeight - 1)/2) / (touchHeight - 1);
  y = savedY = (uint32_t(rawX) * 239 + (touchWidth - 1)/2) / (touchWidth - 1);
  touchDown=true;
  return true;
}

void L2Display::logTouchInfo() {
  uint8_t product[5] = {}, config[4] = {};
  if (!readTouchRegister(0x8140, product, 4) || !readTouchRegister(0x8048, config, 4)) {
    Serial.println("L2 touch: GT911 register read failed at 0x5D"); return;
  }
  uint16_t width = config[0] | (uint16_t(config[1]) << 8);
  uint16_t height = config[2] | (uint16_t(config[3]) << 8);
  if (width > 1 && width <= 4096 && height > 1 && height <= 4096) {
    touchWidth = width; touchHeight = height;
  }
  Serial.printf("L2 touch: product=%s address=0x5D range=%ux%u; landscape X=319-scaledY Y=scaledX\n",
      reinterpret_cast<const char*>(product), touchWidth, touchHeight);
}

void L2Display::endFrame() {
  if (!initialized || !on) return;
  const auto* current = static_cast<const uint8_t*>(frame.getBuffer());
  if (!current) return;
  uint32_t started = micros();
  constexpr size_t stride = 320 * 2;
  if (!presented || !presentedValid) {
    frame.pushSprite(0, 0);
    rowCount += 240;
    if (presented) { memcpy(presented, current, stride * 240); presentedValid=true; }
  } else {
    // Full-width bands keep source rows contiguous for NV3031B QSPI RAMWR.
    // A key edit typically changes only the draft and byte counter, not the
    // keyboard/background. Identical frames generate no panel transfers.
    panel.startWrite();
    for (int row=0; row<240;) {
      if (!memcmp(current+row*stride,presented+row*stride,stride)) { ++row; continue; }
      int first=row, last=row;
      while (++row<240) {
        if (memcmp(current+row*stride,presented+row*stride,stride)) last=row;
        else if (row-last>2) break;
      }
      int height=last-first+1;
      panel.setClipRect(0,first,320,height);
      frame.pushSprite(0,0);
      memcpy(presented+first*stride,current+first*stride,height*stride);
      rowCount += height;
    }
    panel.clearClipRect(); panel.endWrite();
  }
  transferUs=micros()-started;
  maxTransferUs=std::max(maxTransferUs,transferUs); ++frameCount;
}

size_t L2Display::diagnostics(char* text, size_t capacity) const {
  if (!capacity) return 0;
  int n=snprintf(text,capacity,"on=%u frames=%lu rows=%lu us=%lu max=%lu sleeps=%lu wakes=%lu down=%u holds=%lu rel=%lu err=%lu",
      on, (unsigned long)frameCount, (unsigned long)rowCount, (unsigned long)transferUs,
      (unsigned long)maxTransferUs, (unsigned long)sleepCount, (unsigned long)wakeCount,
      touchDown, (unsigned long)heldReports, (unsigned long)releaseReports, (unsigned long)touchErrors);
  return n>0 ? std::min(capacity-1,size_t(n)) : 0;
}

size_t L2Display::readFrameBytes(uint32_t offset,uint8_t* bytes,size_t count) const {
  constexpr uint32_t size=320*240*2;
  if(!initialized || !frame.getBuffer() || offset>=size) return 0;
  size_t length=std::min(count,size_t(size-offset));
  memcpy(bytes,static_cast<const uint8_t*>(frame.getBuffer())+offset,length);
  return length;
}
