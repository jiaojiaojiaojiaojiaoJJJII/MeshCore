#pragma once
#include "L2Geo.h"
#include "L2Sensors.h"
#include <FS.h>
#include <LovyanGFX.hpp>
class L2Map {
public:
  struct Waypoint { l2::GeoPoint point; char name[24]={}; };
  void begin();
  void loop(L2GPS& gps);
  void render(lgfx::LGFX_Sprite& canvas,int y=34);
  void pan(int dx,int dy);
  void zoomBy(int delta);
  bool centerGps(L2GPS& gps);
  bool project(const l2::GeoPoint& p,double& x,double& y) const;
  bool startTrack();
  void stopTrack();
  bool trackRecording() const { return recording; }
  unsigned trackPointsWritten() const { return written; }
  bool scanTracks();
  unsigned trackCount() const { return tracksCount; }
  const char* trackName(unsigned index) const { return index<tracksCount?tracks[index].name:""; }
  bool loadTrack(const char* name);
  bool deleteTrack(const char* name);
  void unload() { loadedCount=0; loadedName[0]=0; dirty=true; }
  bool loaded() const { return loadedCount>0; }
  const char* loadedTrack() const { return loadedName; }
  bool addWaypoint(L2GPS& gps);
  bool removeWaypoint(unsigned index);
  bool centerWaypoint(unsigned index);
  unsigned waypointCount() const { return waypointSize; }
  const Waypoint& waypoint(unsigned index) const { return waypoints[index]; }
  bool tilesEnabled() const { return tiles; }
  void toggleTiles();
  bool rescan();
  bool anchored() const { return anchor; }
  int zoom() const { return zoomLevel; }
  unsigned tilesDrawn() const { return drawnTiles; }
  void currentTile(char* output,size_t capacity) const;
  const char* error() const { return failure; }
  double metersPerPixel() const;
private:
  struct TrackEntry { char name[64]={}; } tracks[32];
  unsigned tracksCount=0,waypointSize=0,loadedCount=0;
  Waypoint waypoints[24]; l2::GeoPoint loadedPoints[512];
  char loadedName[64]={},activeTrack[64]={},failure[96]={};
  fs::File trackFile;
  bool recording=false,tiles=true,anchor=false,follow=true,dirty=true,cacheReady=false;
  double centerX=0,centerY=0; int zoomLevel=16;
  uint32_t lastSample=0,lastPointAt=0; unsigned written=0,drawnTiles=0;
  lgfx::LGFX_Sprite cache;
  bool loadWaypoints();
  bool saveWaypoints();
  bool storage();
  void center(double lat,double lon,bool keepFollow=false);
  bool path(char* output,size_t capacity,const char* name) const;
};
extern L2Map l2map;
