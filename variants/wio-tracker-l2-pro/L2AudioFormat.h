#pragma once
#include <stdint.h>
#include <string.h>
namespace l2 {
inline void normalizeRecording(int16_t* samples,size_t count,uint32_t& gainQ8) {
  if(!count) return;
  int peak=0;
  for(size_t i=0;i<count;++i) { int value=samples[i]; if(value<0) value=-value; if(value>peak) peak=value; }
  uint32_t target=peak>3?uint32_t(16000*256)/peak:256;
  if(target<256) target=256; if(target>128*256) target=128*256;
  if(target<gainQ8) gainQ8=target; else gainQ8+=(target-gainQ8)/8;
  for(size_t i=0;i<count;++i) { int32_t value=int32_t(samples[i])*int32_t(gainQ8)/256; samples[i]=value>32767?32767:value<-32768?-32768:int16_t(value); }
}

inline void put16(uint8_t* p,uint16_t n) { p[0]=n; p[1]=n>>8; }
inline void put32(uint8_t* p,uint32_t n) { for(int i=0;i<4;++i) p[i]=n>>(8*i); }
inline void recordingHeader(uint8_t* h,uint32_t size) {
  memset(h,0,44); memcpy(h,"RIFF",4); put32(h+4,size+36); memcpy(h+8,"WAVEfmt ",8); put32(h+16,16); put16(h+20,1); put16(h+22,1); put32(h+24,16000); put32(h+28,32000); put16(h+32,2); put16(h+34,16); memcpy(h+36,"data",4); put32(h+40,size);
}
inline bool recoverableRecording(const uint8_t* header) {
  uint8_t expected[44]; recordingHeader(expected,0);
  return !memcmp(header,expected,4) && !memcmp(header+8,expected+8,32);
}
}
