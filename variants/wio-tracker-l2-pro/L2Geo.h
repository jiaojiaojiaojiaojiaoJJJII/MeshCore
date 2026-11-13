#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace l2 {
constexpr double mapPi=3.14159265358979323846;
inline double mapWorld(int zoom) { return double(uint32_t(1)<<zoom)*256.0; }
inline double mapWrap(double value,double size) { value=fmod(value,size); return value<0?value+size:value; }
inline void mapProject(double lat,double lon,int zoom,double& x,double& y) {
  lat=lat>85.05112878?85.05112878:lat<-85.05112878?-85.05112878:lat;
  double world=mapWorld(zoom),r=lat*mapPi/180.0;
  x=mapWrap((lon+180.0)/360.0*world,world); y=(1.0-log(tan(r)+1.0/cos(r))/mapPi)*0.5*world;
  if(y<0) y=0; if(y>world) y=world;
}
inline void mapUnproject(double x,double y,int zoom,double& lat,double& lon) {
  double world=mapWorld(zoom); lon=mapWrap(x,world)/world*360.0-180.0; lat=atan(sinh(mapPi*(1.0-2.0*y/world)))*180.0/mapPi;
}
inline double mapDelta(double x,double center,double world) { double d=x-center; if(d>world/2) d-=world; if(d<-world/2) d+=world; return d; }
struct GeoPoint { int32_t lat=0,lon=0; uint32_t utc=0; };
inline bool geoCsv(const char* text,GeoPoint& point) {
  char* end=nullptr; double lat=strtod(text,&end); if(end==text || *end!=',') return false;
  text=end+1; double lon=strtod(text,&end); if(end==text || (*end!=',' && *end!='\r' && *end!='\n' && *end)) return false;
  if(!isfinite(lat) || !isfinite(lon) || lat<-90 || lat>90 || lon<-180 || lon>180) return false;
  point.lat=lround(lat*1e6); point.lon=lround(lon*1e6); point.utc=0;
  if(*end==',') { const char* start=end+1; unsigned long stamp=strtoul(start,&end,10); if(end==start || stamp>0xffffffffUL || (*end && *end!=',' && *end!='\r' && *end!='\n')) return false; point.utc=stamp; }
  return true;
}
inline bool mapClip(double& x0,double& y0,double& x1,double& y1,double width,double height) {
  double dx=x1-x0,dy=y1-y0,t0=0,t1=1,p[]={-dx,dx,-dy,dy},q[]={x0,width-x0,y0,height-y0};
  for(int i=0;i<4;++i) {
    if(p[i]==0) { if(q[i]<0) return false; continue; }
    double t=q[i]/p[i]; if(p[i]<0) { if(t>t1) return false; if(t>t0) t0=t; } else { if(t<t0) return false; if(t<t1) t1=t; }
  }
  x1=x0+t1*dx; y1=y0+t1*dy; x0+=t0*dx; y0+=t0*dy; return true;
}
inline bool tilePng(const uint8_t* bytes,size_t count) {
  static const uint8_t magic[]={137,80,78,71,13,10,26,10};
  return count>=24 && !memcmp(bytes,magic,8) && !memcmp(bytes+12,"IHDR",4) && bytes[16]==0 && bytes[17]==0 && bytes[18]==1 && bytes[19]==0 && bytes[20]==0 && bytes[21]==0 && bytes[22]==1 && bytes[23]==0;
}
}
