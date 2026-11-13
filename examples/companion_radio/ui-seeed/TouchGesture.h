#pragma once
#include <stdint.h>
#include <stddef.h>

namespace seeed {
inline int clamp(int value,int low,int high) { return value<low?low:value>high?high:value; }
inline int nextMainPage(int index,bool swipeLeft) { return (index+(swipeLeft?1:5))%6; }
inline bool menuExpired(uint32_t now,uint32_t last,bool held) { return !held && uint32_t(now-last)>=5000; }
inline bool captureHold(int x,int y,uint32_t now,uint32_t since) { return x>=0 && x<40 && y>=0 && y<30 && uint32_t(now-since)>=1200; }
struct TouchGesture {
  enum Axis { Pending, Horizontal, Vertical } axis=Pending;
  int x0=0,y0=0,x=0,y=0;
  bool moved=false, sliderCancelled=false;
  void begin(int px,int py) { x0=x=px; y0=y=py; axis=Pending; moved=false; sliderCancelled=false; }
  int dx() const { return x-x0; }
  int dy() const { return y-y0; }
  static int magnitude(int n) { return n<0?-n:n; }
  void update(int px,int py) {
    x=px; y=py;
    int ax=magnitude(dx()), ay=magnitude(dy());
    if(ax>10 || ay>10) moved=true;
    if(startsSlider() && ay>22) sliderCancelled=true;
    if(axis==Pending && (ax>12 || ay>12)) {
      if(ax*4>ay*5) axis=Horizontal;
      else if(ay*4>ax*5) axis=Vertical;
    }
  }
  bool tap() const { return !moved; }
  bool pageSwipe() const { return axis==Horizontal && magnitude(dx())>=56; }
  bool opensMenu() const { return y0>=216 && axis==Vertical && dy()<=-28; }
  bool startsSlider() const { return x0>=16 && x0<62 && y0>=192 && y0<232; }
  bool completesUnlock() const { return startsSlider() && !sliderCancelled && dx()>=228 && magnitude(dy())<=22; }
};

template<class Predicate>
inline int latestUnread(unsigned size,Predicate unread,int* indices,int capacity=10) {
  int count=0,limit=capacity<10?capacity:10;
  for(int i=int(size)-1;i>=0 && count<limit;--i) if(unread(i)) indices[count++]=i;
  return count;
}

// Limit by Unicode code points, not UTF-8 byte count. Newlines become spaces.
inline void previewText(char* dest,size_t capacity,const char* text,unsigned characters=15) {
  if(!capacity) return;
  size_t used=0; unsigned count=0;
  while(*text && count<characters) {
    const char* end=text+1;
    while(*end && (static_cast<uint8_t>(*end)&0xC0)==0x80) ++end;
    size_t bytes=end-text;
    if(used+bytes>=capacity) break;
    if(*text=='\n' || *text=='\r') dest[used++]=' ';
    else { for(size_t i=0;i<bytes;++i) dest[used++]=text[i]; }
    text=end; ++count;
  }
  dest[used]=0;
}
}
