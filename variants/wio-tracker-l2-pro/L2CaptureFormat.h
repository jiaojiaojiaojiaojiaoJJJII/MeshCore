#pragma once
#include <stdint.h>
#include <stddef.h>
namespace l2 {
inline void bmpWord(uint8_t* p,uint32_t v,int bytes=4) { for(int i=0;i<bytes;++i) p[i]=uint8_t(v>>(8*i)); }
inline void screenshotHeader(uint8_t* p) {
  for(int i=0;i<54;++i) p[i]=0;
  p[0]='B'; p[1]='M'; bmpWord(p+2,54+320*240*3); bmpWord(p+10,54); bmpWord(p+14,40);
  bmpWord(p+18,320); bmpWord(p+22,uint32_t(-240)); bmpWord(p+26,1,2); bmpWord(p+28,24,2); bmpWord(p+34,320*240*3);
}
inline void screenshotPixel(const uint8_t* rgb565,uint8_t* bgr) {
  uint16_t p=(uint16_t(rgb565[0])<<8)|rgb565[1];
  bgr[0]=(p&31)*255/31; bgr[1]=((p>>5)&63)*255/63; bgr[2]=(p>>11)*255/31;
}
}
