#pragma once
#include <stdint.h>
#include <stddef.h>
namespace l2 {
struct BatteryPoint { uint16_t mv; uint8_t percent; };
inline int interpolateBattery(uint16_t mv,const BatteryPoint* curve,size_t count) {
  if(!mv || !curve || count<2) return -1;
  for(size_t i=0;i<count;++i) if(curve[i].percent>100 || (i && (curve[i].mv<=curve[i-1].mv || curve[i].percent<curve[i-1].percent))) return -1;
  if(mv<=curve[0].mv) return curve[0].percent;
  for(size_t i=1;i<count;++i) if(mv<=curve[i].mv) return curve[i-1].percent+(uint32_t(mv-curve[i-1].mv)*(curve[i].percent-curve[i-1].percent)+(curve[i].mv-curve[i-1].mv)/2)/(curve[i].mv-curve[i-1].mv);
  return curve[count-1].percent;
}
inline int batteryPercent(uint16_t mv) {
  // User-approved provisional endpoints; replace with measured curve later.
  static const BatteryPoint curve[]={{3300,0},{4200,100}};
  return interpolateBattery(mv,curve,2);
}
inline int telemetryBatteryMv(const uint8_t* bytes,size_t length) {
  size_t at=0; int battery=-1;
  while(at+2<=length) {
    uint8_t channel=bytes[at++],type=bytes[at++]; size_t size=0;
    switch(type) {
      case 0:case 1:case 102:case 104:case 120:case 142:size=1;break;
      case 2:case 3:case 101:case 103:case 115:case 116:case 117:case 121:case 125:case 128:case 132:size=2;break;
      case 100:case 118:case 130:case 131:case 133:size=4;break;
      case 113:case 134:size=6;break; case 135:size=3;break; case 136:size=9;break;
      case 240: if(at>=length || bytes[at]<8) return -1; size=bytes[at]; break;
      default:return -1;
    }
    if(at+size>length) return -1;
    if(type==116 && channel==1) battery=int(bytes[at]*256+bytes[at+1])*10;
    at+=size;
  }
  return at==length?battery:-1;
}
}
