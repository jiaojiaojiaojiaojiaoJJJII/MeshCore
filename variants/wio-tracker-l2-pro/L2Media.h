#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace l2 {
inline uint16_t media16(const uint8_t* p) { return uint16_t(p[0])|uint16_t(p[1])<<8; }
inline uint32_t media32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
struct WaveInfo { uint32_t rate=0,offset=0,bytes=0; uint16_t channels=0,align=0; };
template<class Reader> bool readWave(uint32_t size,Reader read,WaveInfo& wave) {
  wave=WaveInfo{}; uint8_t h[16];
  if(size<44 || !read(0,h,12) || memcmp(h,"RIFF",4) || memcmp(h+8,"WAVE",4)) return false;
  uint32_t riff=media32(h+4); if(riff<36 || riff>size-8) return false;
  uint32_t end=riff+8,pos=12; bool fmt=false,data=false;
  for(unsigned chunks=0;chunks<256 && pos<=end-8;++chunks) {
    if(!read(pos,h,8)) return false;
    uint32_t length=media32(h+4),body=pos+8; if(length>end-body) return false;
    if(!memcmp(h,"fmt ",4)) {
      if(length<16 || !read(body,h,16) || media16(h)!=1 || media16(h+14)!=16) return false;
      wave.channels=media16(h+2); wave.rate=media32(h+4); wave.align=media16(h+12);
      if((wave.channels!=1 && wave.channels!=2) || !wave.rate || wave.rate>96000 || wave.align!=wave.channels*2 || media32(h+8)!=wave.rate*wave.align) return false;
      fmt=true;
    } else if(!memcmp(h,"data",4)) { if(data) return false; wave.offset=body; wave.bytes=length; data=true; }
    if(fmt && data) return wave.bytes>0 && wave.bytes%wave.align==0;
    if(length==end-body) break;
    uint32_t padded=length+(length&1); if(padded>end-body) return false;
    pos=body+padded;
  }
  return false;
}
inline int16_t mixMedia(int16_t left,int16_t right) { return int16_t((int32_t(left)+int32_t(right))/2); }
inline int mediaTrack(int index,int direction,int count) { return count>0?(index+direction%count+count)%count:-1; }
struct Mp3Info { uint32_t rate=0,bitrate=0,frameBytes=0,samples=0; bool mono=false,crc=false,mpeg1=false; };
inline bool mp3Header(const uint8_t* h,Mp3Info& info) {
  if(h[0]!=0xff || (h[1]&0xe0)!=0xe0) return false;
  unsigned version=(h[1]>>3)&3,layer=(h[1]>>1)&3,br=h[2]>>4,sr=(h[2]>>2)&3;
  if(version==1 || layer!=1 || !br || br==15 || sr==3) return false;
  static const uint16_t rates[]={44100,48000,32000};
  static const uint16_t high[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
  static const uint16_t low[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
  info.mpeg1=version==3; info.rate=rates[sr]/(version==3?1:version==2?2:4);
  info.bitrate=(info.mpeg1?high[br]:low[br])*1000; info.samples=info.mpeg1?1152:576;
  info.frameBytes=(info.mpeg1?144:72)*info.bitrate/info.rate+((h[2]>>1)&1);
  info.mono=(h[3]>>6)==3; info.crc=!(h[1]&1); return true;
}
}
