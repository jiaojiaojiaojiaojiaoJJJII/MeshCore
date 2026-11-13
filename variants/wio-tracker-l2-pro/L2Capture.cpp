#include "L2Capture.h"
#include "L2CaptureFormat.h"
#include "L2Display.h"
#include "L2Audio.h"
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
L2Capture capture;
int L2Capture::finish(bool ok) {
  if(file) { file.flush(); file.close(); }
  if(ok && !SD_MMC.rename(partial,destination)) { snprintf(failure,sizeof(failure),"Screenshot rename failed"); ok=false; }
  if(!ok && partial[0]) SD_MMC.remove(partial); // only this capture's new temporary file
  if(snapshot) heap_caps_free(snapshot); snapshot=nullptr;
  return ok?1:-1;
}
bool L2Capture::start(L2Display& display) {
  if(busy()) return false;
  failure[0]=0; destination[0]=partial[0]=0;
  if(!audio.ensureStorage()) { snprintf(failure,sizeof(failure),"%s",audio.error()); return false; }
  if(!SD_MMC.exists("/screenshots") && !SD_MMC.mkdir("/screenshots")) { snprintf(failure,sizeof(failure),"Cannot create /screenshots"); return false; }
  snapshot=static_cast<uint8_t*>(heap_caps_malloc(320*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!snapshot) { snprintf(failure,sizeof(failure),"Not enough PSRAM for screenshot"); return false; }
  if(display.readFrameBytes(0,snapshot,320*240*2)!=320*240*2) { snprintf(failure,sizeof(failure),"Display capture unavailable"); finish(false); return false; }
  bool available=false;
  for(int n=0;n<16;++n) {
    snprintf(destination,sizeof(destination),"/screenshots/L2-%08lx.bmp",(unsigned long)esp_random());
    snprintf(partial,sizeof(partial),"%s.part",destination);
    if(!SD_MMC.exists(destination) && !SD_MMC.exists(partial)) { available=true; break; }
  }
  if(!available) { partial[0]=0; snprintf(failure,sizeof(failure),"Screenshot name unavailable"); finish(false); return false; }
  file=SD_MMC.open(partial,FILE_WRITE); uint8_t header[54]; l2::screenshotHeader(header);
  if(!file || file.write(header,sizeof(header))!=sizeof(header)) { snprintf(failure,sizeof(failure),"Cannot write screenshot"); finish(false); return false; }
  row=0; return true;
}
int L2Capture::loop() {
  if(!busy()) return 0;
  if(!audio.mounted()) { snprintf(failure,sizeof(failure),"SD removed during screenshot"); return finish(false); }
  uint8_t pixels[960];
  for(unsigned n=0;n<4 && row<240;++n,++row) {
    for(unsigned x=0;x<320;++x) l2::screenshotPixel(snapshot+(row*320+x)*2,pixels+x*3);
    if(file.write(pixels,sizeof(pixels))!=sizeof(pixels)) { snprintf(failure,sizeof(failure),"Screenshot SD write failed"); return finish(false); }
  }
  return row==240?finish(true):0;
}
