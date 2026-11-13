#define NOMINMAX
#include "L2Media.h"
#include "L2CaptureFormat.h"
#include "L2AudioFormat.h"
#include "L2Geo.h"
#include "L2Pins.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#endif
int main() {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX); _set_error_mode(_OUT_TO_STDERR);
#endif
  static_assert(l2::GPS_RX==18 && l2::GPS_TX==17,"GPS UART pin regression");
  uint8_t bmp[54]; l2::screenshotHeader(bmp);
  assert(bmp[0]=='B' && bmp[1]=='M' && l2::media32(bmp+2)==230454 && l2::media32(bmp+10)==54);
  assert(l2::media32(bmp+18)==320 && int32_t(l2::media32(bmp+22))==-240 && l2::media16(bmp+28)==24);
  uint8_t pixel[2]={0xf8,0},bgr[3]; l2::screenshotPixel(pixel,bgr); assert(bgr[0]==0 && bgr[1]==0 && bgr[2]==255);
  pixel[0]=0x07; pixel[1]=0xe0; l2::screenshotPixel(pixel,bgr); assert(bgr[0]==0 && bgr[1]==255 && bgr[2]==0);
  pixel[0]=0; pixel[1]=0x1f; l2::screenshotPixel(pixel,bgr); assert(bgr[0]==255 && bgr[1]==0 && bgr[2]==0);
  std::vector<uint8_t> file(60); l2::recordingHeader(file.data(),16); l2::WaveInfo info;
  auto reader=[&](uint32_t offset,uint8_t* dest,size_t size) { if(offset>file.size() || size>file.size()-offset) return false; memcpy(dest,file.data()+offset,size); return true; };
  assert(l2::readWave(file.size(),reader,info)); assert(info.rate==16000 && info.channels==1 && info.align==2 && info.offset==44 && info.bytes==16);
  l2::put16(file.data()+22,2); l2::put32(file.data()+28,64000); l2::put16(file.data()+32,4); assert(l2::readWave(file.size(),reader,info)); assert(info.channels==2 && info.align==4);
  l2::put16(file.data()+32,6); assert(!l2::readWave(file.size(),reader,info));
  l2::recordingHeader(file.data(),16); assert(!l2::readWave(file.size()-1,reader,info));
  std::vector<uint8_t> padded(70); memcpy(padded.data(),file.data(),12); memcpy(padded.data()+12,"JUNK",4); l2::put32(padded.data()+16,1); padded[20]=99; memcpy(padded.data()+22,file.data()+12,48); l2::put32(padded.data()+4,62); file=padded;
  assert(l2::readWave(file.size(),reader,info) && info.offset==54 && info.bytes==16);
  l2::put32(file.data()+16,0xffffffffU); assert(!l2::readWave(file.size(),reader,info));
  assert(l2::mixMedia(-32768,-32768)==-32768 && l2::mixMedia(32767,32767)==32767 && l2::mixMedia(32767,-32768)==0);
  assert(l2::mediaTrack(0,-1,3)==2 && l2::mediaTrack(2,1,3)==0 && l2::mediaTrack(0,1,0)==-1);
  l2::Mp3Info mp3; uint8_t frame[]={0xff,0xfb,0x90,0x64}; assert(l2::mp3Header(frame,mp3)); assert(mp3.rate==44100 && mp3.bitrate==128000 && mp3.frameBytes==417 && mp3.samples==1152);
  frame[2]=0; assert(!l2::mp3Header(frame,mp3)); frame[2]=0x9c; assert(!l2::mp3Header(frame,mp3));
  double x,y,lat,lon; l2::mapProject(0,0,3,x,y); assert(fabs(x-1024)<1e-6 && fabs(y-1024)<1e-6);
  l2::mapProject(22.5431,114.0579,16,x,y); l2::mapUnproject(x,y,16,lat,lon); assert(fabs(lat-22.5431)<1e-8 && fabs(lon-114.0579)<1e-8);
  l2::mapProject(90,180,18,x,y); assert(isfinite(x) && isfinite(y) && x==0 && y>=0); l2::mapUnproject(x,y,18,lat,lon); assert(lat>85 && lat<86);
  assert(fabs(l2::mapDelta(l2::mapWorld(3)-1,1,l2::mapWorld(3))+2)<1e-6);
  double x0=-100,y0=50,x1=500,y1=50; assert(l2::mapClip(x0,y0,x1,y1,319,179)); assert(fabs(x0)<1e-6 && fabs(x1-319)<1e-6);
  x0=x1=-10;y0=0;y1=100;assert(!l2::mapClip(x0,y0,x1,y1,319,179));
  l2::GeoPoint point; assert(l2::geoCsv("22.543100,114.057900,1700000000",point)); assert(point.lat==22543100 && point.lon==114057900 && point.utc==1700000000);
  assert(!l2::geoCsv("latitude,longitude,utc",point));assert(!l2::geoCsv("nan,12,0",point));assert(!l2::geoCsv("91,0,0",point));assert(!l2::geoCsv("0,181,0",point));assert(!l2::geoCsv("1x,2,0",point));
  uint8_t png[24]={137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,1,0,0,0,1,0};assert(l2::tilePng(png,24));png[19]=1;assert(!l2::tilePng(png,24));
  puts("PASS: native WAV bounds/chunks, MP3 metadata, track navigation, Mercator/CSV/tile validation and GPS pin regression");
}
