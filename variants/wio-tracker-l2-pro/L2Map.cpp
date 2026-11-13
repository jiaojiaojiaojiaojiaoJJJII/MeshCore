#include "L2Map.h"
#include "L2Audio.h"
#include <SD_MMC.h>
#include <Preferences.h>
#include <algorithm>
L2Map l2map;
namespace {
constexpr uint16_t BG=0x0882,GRID=0x1946,PATH=0xFE88;
bool readLine(fs::File& file,char* line,size_t capacity) {
  if(!file.available()) return false;
  size_t n=0; bool overflow=false;
  while(file.available()) { int c=file.read(); if(c<0 || c=='\n') break; if(c=='\r') continue; if(n+1<capacity) line[n++]=char(c); else overflow=true; }
  line[n]=0; if(overflow) line[0]=0; return true;
}
}
bool L2Map::storage() { if(audio.ensureStorage()) return true; snprintf(failure,sizeof(failure),"%s",audio.error()); return false; }
bool L2Map::path(char* output,size_t capacity,const char* name) const {
  if(!name || !name[0] || strlen(name)>63 || strchr(name,'/') || strchr(name,'\\') || strstr(name,"..")) return false;
  snprintf(output,capacity,"/tracks/%s",name); return true;
}
void L2Map::begin() {
  Preferences prefs; if(prefs.begin("l2-map",true)) { zoomLevel=std::min(18,std::max(3,int(prefs.getUChar("zoom",16)))); tiles=prefs.getBool("tiles",true); prefs.end(); }
  cache.setPsram(true); cache.setColorDepth(16); cacheReady=cache.createSprite(320,180)!=nullptr;
  centerX=centerY=l2::mapWorld(zoomLevel)/2;
  if(audio.mounted()) { loadWaypoints(); scanTracks(); }
}
void L2Map::center(double lat,double lon,bool keepFollow) { l2::mapProject(lat,lon,zoomLevel,centerX,centerY); anchor=true; follow=keepFollow; dirty=true; }
bool L2Map::centerGps(L2GPS& gps) { if(!gps.hasPosition()) { snprintf(failure,sizeof(failure),"GPS has no position yet"); return false; } center(gps.getLatitude()/1e6,gps.getLongitude()/1e6,true); failure[0]=0; return true; }
void L2Map::pan(int dx,int dy) { double world=l2::mapWorld(zoomLevel); centerX=l2::mapWrap(centerX+dx,world); centerY=std::max(0.0,std::min(world,centerY+dy)); follow=false; dirty=true; }
void L2Map::zoomBy(int delta) {
  int next=std::max(3,std::min(18,zoomLevel+delta)); if(next==zoomLevel) return;
  double factor=pow(2.0,next-zoomLevel); centerX*=factor; centerY*=factor; zoomLevel=next; dirty=true;
  Preferences p; if(p.begin("l2-map",false)) { p.putUChar("zoom",zoomLevel); p.end(); }
}
bool L2Map::project(const l2::GeoPoint& point,double& x,double& y) const {
  if(!anchor) return false; double wx,wy; l2::mapProject(point.lat/1e6,point.lon/1e6,zoomLevel,wx,wy);
  x=160+l2::mapDelta(wx,centerX,l2::mapWorld(zoomLevel)); y=90+wy-centerY; return true;
}
double L2Map::metersPerPixel() const { double lat,lon; l2::mapUnproject(centerX,centerY,zoomLevel,lat,lon); return 40075016.686*cos(lat*l2::mapPi/180)/l2::mapWorld(zoomLevel); }
void L2Map::currentTile(char* output,size_t capacity) const {
  if(!anchor) { snprintf(output,capacity,"Waiting for a map position"); return; }
  snprintf(output,capacity,"/map/%d/%lu/%lu.png",zoomLevel,(unsigned long)(centerX/256),(unsigned long)(centerY/256));
}
void L2Map::render(lgfx::LGFX_Sprite& canvas,int y) {
  if(!cacheReady) { canvas.fillRect(0,y,320,180,BG); return; }
  if(dirty) {
    cache.fillSprite(BG); drawnTiles=0;
    int gx=int(fmod(centerX,32.0)),gy=int(fmod(centerY,32.0));
    for(int x=-gx;x<320;x+=32) cache.drawFastVLine(x,0,180,GRID);
    for(int py=-gy;py<180;py+=32) cache.drawFastHLine(0,py,320,GRID);
    if(anchor && tiles && audio.mounted()) {
      int firstX=int(floor((centerX-160)/256)),firstY=int(floor((centerY-90)/256)),num=1<<zoomLevel;
      for(int ty=firstY;ty<=int(floor((centerY+90)/256));++ty) for(int tx=firstX;tx<=int(floor((centerX+160)/256));++tx) {
        if(ty<0 || ty>=num) continue; char fileName[96]; int wrapped=(tx%num+num)%num;
        snprintf(fileName,sizeof(fileName),"/map/%d/%d/%d.png",zoomLevel,wrapped,ty);
        fs::File file=SD_MMC.open(fileName,FILE_READ); if(!file) continue;
        uint8_t header[24]; bool valid=file.size()<=512*1024 && file.read(header,24)==24 && l2::tilePng(header,24); file.close();
        if(valid && cache.drawPngFile(static_cast<fs::FS&>(SD_MMC),fileName,int(tx*256-centerX+160),int(ty*256-centerY+90))) ++drawnTiles;
      }
    }
    for(unsigned i=1;i<loadedCount;++i) {
      double x0,y0,x1,y1; project(loadedPoints[i-1],x0,y0); project(loadedPoints[i],x1,y1);
      if(l2::mapClip(x0,y0,x1,y1,319,179)) { cache.drawLine(x0,y0,x1,y1,0x0000); cache.drawLine(x0,y0+1,x1,y1+1,PATH); }
    }
    for(unsigned i=0;i<loadedCount;++i) {
      if(i!=0 && i+1!=loadedCount) continue;
      double x,py; if(project(loadedPoints[i],x,py) && x>=0 && x<320 && py>=0 && py<180) { cache.fillCircle(x,py,4,i==0?0x07E0:PATH); cache.drawCircle(x,py,5,0xFFFF); }
    }
    for(unsigned i=0;i<waypointSize;++i) {
      double x,py; if(!project(waypoints[i].point,x,py) || x<-8 || x>328 || py<-8 || py>188) continue;
      cache.fillTriangle(x,py-6,x-5,py,x+5,py,0xF81F); cache.fillTriangle(x-5,py,x+5,py,x,py+6,0xF81F);
    }
    dirty=false;
  }
  cache.pushSprite(&canvas,0,y);
}
bool L2Map::scanTracks() {
  tracksCount=0; if(!storage()) return false;
  fs::File dir=SD_MMC.open("/tracks"); if(!dir || !dir.isDirectory()) { failure[0]=0; return true; }
  while(true) {
    fs::File f=dir.openNextFile(); if(!f) break; bool directory=f.isDirectory(); String name=f.name(); f.close();
    int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
    char validated[96]; if(!directory && name.endsWith(".csv") && path(validated,sizeof(validated),name.c_str()) && tracksCount<32) snprintf(tracks[tracksCount++].name,64,"%s",name.c_str());
  }
  std::sort(tracks,tracks+tracksCount,[](const TrackEntry& a,const TrackEntry& b){ return strcmp(a.name,b.name)>0; }); failure[0]=0; return true;
}
bool L2Map::startTrack() {
  if(recording) return true; if(!storage()) return false;
  if(!SD_MMC.exists("/tracks") && !SD_MMC.mkdir("/tracks")) { snprintf(failure,sizeof(failure),"Cannot create /tracks"); return false; }
  Preferences prefs; if(!prefs.begin("l2-map",false)) return false; uint32_t id=prefs.getUInt("nextTrack",0); char filename[96];
  for(unsigned tries=0;tries<64;++tries) { ++id; snprintf(activeTrack,sizeof(activeTrack),"track-%08lu.csv",(unsigned long)id); path(filename,sizeof(filename),activeTrack); if(!SD_MMC.exists(filename)) break; }
  prefs.putUInt("nextTrack",id); prefs.end(); if(SD_MMC.exists(filename)) return false;
  trackFile=SD_MMC.open(filename,FILE_WRITE); const char* header="latitude,longitude,utc\n";
  if(!trackFile || trackFile.print(header)!=strlen(header)) { if(trackFile) trackFile.close(); snprintf(failure,sizeof(failure),"Track file could not be created"); return false; }
  trackFile.flush(); recording=true; written=0; lastSample=0; lastPointAt=0; scanTracks(); failure[0]=0; return true;
}
void L2Map::stopTrack() { if(trackFile) { trackFile.flush(); trackFile.close(); } recording=false; }
void L2Map::loop(L2GPS& gps) {
  if(!audio.mounted() && recording) { stopTrack(); snprintf(failure,sizeof(failure),"Track stopped: SD unavailable"); }
  if(gps.isValid()) {
    if(follow || !anchor) { double x,y; l2::mapProject(gps.getLatitude()/1e6,gps.getLongitude()/1e6,zoomLevel,x,y); if(!anchor || fabs(l2::mapDelta(x,centerX,l2::mapWorld(zoomLevel)))>1 || fabs(y-centerY)>1) center(gps.getLatitude()/1e6,gps.getLongitude()/1e6,true); }
    if(recording && gps.sampleId()!=lastSample && (!written || uint32_t(millis()-lastPointAt)>=5000)) {
      char line[96]; snprintf(line,sizeof(line),"%.6f,%.6f,%lu\n",gps.getLatitude()/1e6,gps.getLongitude()/1e6,(unsigned long)gps.getTimestamp());
      if(trackFile.print(line)!=strlen(line)) { stopTrack(); snprintf(failure,sizeof(failure),"Track write failed"); }
      else { trackFile.flush(); lastSample=gps.sampleId(); lastPointAt=millis(); ++written; }
    }
  }
}
bool L2Map::loadTrack(const char* name) {
  char filename[96]; if(!path(filename,sizeof(filename),name) || !storage()) return false;
  if(recording && !strcmp(name,activeTrack)) trackFile.flush();
  fs::File file=SD_MMC.open(filename,FILE_READ); if(!file || file.size()>4*1024*1024) { snprintf(failure,sizeof(failure),"Track unavailable or exceeds 4 MB"); return false; }
  unsigned count=0,stride=1,seen=0; char line[128]; l2::GeoPoint p,last; bool any=false;
  while(readLine(file,line,sizeof(line))) {
    if(!l2::geoCsv(line,p)) continue; last=p; any=true;
    if(seen++%stride) continue;
    if(count==512) { for(unsigned i=0;i<256;++i) loadedPoints[i]=loadedPoints[i*2]; count=256; stride*=2; if((seen-1)%stride) continue; }
    loadedPoints[count++]=p;
  }
  if(!any || !count) { snprintf(failure,sizeof(failure),"Track has no valid GPS points yet"); return false; }
  if(count<512 && (loadedPoints[count-1].lat!=last.lat || loadedPoints[count-1].lon!=last.lon)) loadedPoints[count++]=last;
  loadedCount=count; snprintf(loadedName,sizeof(loadedName),"%s",name);
  for(zoomLevel=18;zoomLevel>=3;--zoomLevel) {
    double firstX,firstY,minX,maxX,minY,maxY; l2::mapProject(loadedPoints[0].lat/1e6,loadedPoints[0].lon/1e6,zoomLevel,firstX,firstY); minX=maxX=firstX; minY=maxY=firstY;
    for(unsigned i=1;i<loadedCount;++i) { double x,y; l2::mapProject(loadedPoints[i].lat/1e6,loadedPoints[i].lon/1e6,zoomLevel,x,y); x=firstX+l2::mapDelta(x,firstX,l2::mapWorld(zoomLevel)); minX=std::min(minX,x); maxX=std::max(maxX,x); minY=std::min(minY,y); maxY=std::max(maxY,y); }
    centerX=l2::mapWrap((minX+maxX)/2,l2::mapWorld(zoomLevel)); centerY=(minY+maxY)/2;
    if((maxX-minX<280 && maxY-minY<110) || zoomLevel==3) break;
  }
  anchor=true; follow=false; dirty=true; failure[0]=0; return true;
}
bool L2Map::deleteTrack(const char* name) {
  if(recording && !strcmp(name,activeTrack)) { snprintf(failure,sizeof(failure),"Stop recording this track first"); return false; }
  char filename[96]; if(!path(filename,sizeof(filename),name) || !storage() || !SD_MMC.remove(filename)) { snprintf(failure,sizeof(failure),"Track delete failed"); return false; }
  if(!strcmp(name,loadedName)) unload(); return scanTracks();
}
bool L2Map::loadWaypoints() {
  waypointSize=0;
  if(!SD_MMC.exists("/waypoints.csv") && SD_MMC.exists("/waypoints.bak")) SD_MMC.rename("/waypoints.bak","/waypoints.csv");
  fs::File file=SD_MMC.open("/waypoints.csv",FILE_READ); if(!file) return true;
  char line[128];
  while(waypointSize<24 && readLine(file,line,sizeof(line))) {
    l2::GeoPoint point; if(!l2::geoCsv(line,point)) continue;
    const char* name=line; for(int i=0;i<3 && name;++i) { const char* comma=strchr(name,','); name=comma?comma+1:nullptr; }
    auto& wp=waypoints[waypointSize++]; wp.point=point; snprintf(wp.name,sizeof(wp.name),"%s",name&&*name?name:"Waypoint");
  }
  dirty=true; return true;
}
bool L2Map::saveWaypoints() {
  if(!storage()) return false;
  fs::File file=SD_MMC.open("/waypoints.tmp",FILE_WRITE); if(!file) return false; bool ok=true;
  for(unsigned i=0;i<waypointSize;++i) { char line[128]; auto& wp=waypoints[i]; snprintf(line,sizeof(line),"%.6f,%.6f,0,%s\n",wp.point.lat/1e6,wp.point.lon/1e6,wp.name); if(file.print(line)!=strlen(line)) { ok=false; break; } }
  file.flush(); file.close(); if(!ok) return false;
  if(SD_MMC.exists("/waypoints.bak")) SD_MMC.remove("/waypoints.bak");
  bool old=SD_MMC.exists("/waypoints.csv"); if(old && !SD_MMC.rename("/waypoints.csv","/waypoints.bak")) return false;
  if(!SD_MMC.rename("/waypoints.tmp","/waypoints.csv")) { if(old) SD_MMC.rename("/waypoints.bak","/waypoints.csv"); return false; }
  SD_MMC.remove("/waypoints.bak"); dirty=true; return true;
}
bool L2Map::addWaypoint(L2GPS& gps) {
  if(!gps.isValid()) { snprintf(failure,sizeof(failure),"A valid GPS fix is required"); return false; }
  if(waypointSize>=24) { snprintf(failure,sizeof(failure),"Waypoint limit: 24"); return false; }
  auto& wp=waypoints[waypointSize]; wp.point.lat=gps.getLatitude(); wp.point.lon=gps.getLongitude(); wp.point.utc=gps.getTimestamp(); snprintf(wp.name,sizeof(wp.name),"Waypoint %u",waypointSize+1); ++waypointSize;
  if(!saveWaypoints()) { --waypointSize; snprintf(failure,sizeof(failure),"Waypoint save failed"); return false; } failure[0]=0; return true;
}
bool L2Map::removeWaypoint(unsigned index) {
  if(index>=waypointSize) return false; Waypoint old=waypoints[index];
  for(unsigned i=index+1;i<waypointSize;++i) waypoints[i-1]=waypoints[i]; --waypointSize;
  if(!saveWaypoints()) { for(unsigned i=waypointSize;i>index;--i) waypoints[i]=waypoints[i-1]; waypoints[index]=old; ++waypointSize; snprintf(failure,sizeof(failure),"Waypoint delete failed"); return false; }
  return true;
}
bool L2Map::centerWaypoint(unsigned index) { if(index>=waypointSize) return false; center(waypoints[index].point.lat/1e6,waypoints[index].point.lon/1e6); return true; }
void L2Map::toggleTiles() { tiles=!tiles; dirty=true; Preferences p; if(p.begin("l2-map",false)) { p.putBool("tiles",tiles); p.end(); } }
bool L2Map::rescan() { if(!storage()) return false; dirty=true; return loadWaypoints() && scanTracks(); }
