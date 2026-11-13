#include "UITask.h"
#include <L2Audio.h>
#include <L2Map.h>
#include <stddef.h>
namespace { constexpr uint16_t BG=0x0882,CARD=0x1125,GREEN=0xBF66,WHITE=0xF7BE,MUTED=0xBDF7,RED=0xF249,TEAL=0x01C9; }
bool UITask::rootPage() const { return page<=Settings || page==Audio || page==Locked || (onboarding && page==SetupName); }
void UITask::drawBackButton() {
  if(rootPage()) return;
  auto& c=display.canvas(); c.fillRoundRect(4,2,29,26,6,0xFD00);
  c.drawLine(12,15,25,15,BG); c.drawLine(12,15,18,9,BG); c.drawLine(12,15,18,21,BG);
  c.drawLine(12,16,25,16,BG);
  if(hitCount<48) hits[hitCount++]={4,2,29,26,Back,0};
}
const char* UITask::shortcutName(int id) const {
  const char* names[]={tr("Map","地图"),tr("Audio","音频"),tr("Tracks","轨迹"),tr("Waypoints","航点"),tr("GPS status","GPS 状态"),tr("LoRa configuration","LoRa 配置"),tr("Quick panel","快捷操作"),tr("About","关于"),tr("Music","音乐"),tr("Add channel","新增频道"),tr("Hardware status","硬件状态")};
  return id>=0 && id<11?names[id]:"";
}
UITask::Page UITask::shortcutPage(int id) const { const Page pages[]={Location,Audio,Tracks,Waypoints,GPSStatus,Radio,Quick,About,Music,NewChannel,Diagnostics}; return id>=0 && id<11?pages[id]:Home; }
void UITask::loadHomeShortcuts() {
  Preferences p; if(!p.begin("l2-home",true)) return;
  HomeShortcuts saved{}; bool ok=p.getBytes("links",&saved,sizeof(saved))==sizeof(saved); p.end();
  ok=ok && saved.magic==shortcuts.magic && saved.count<=8 && saved.crc==seeed::checksum(&saved,offsetof(HomeShortcuts,crc));
  for(unsigned i=0;ok && i<saved.count;++i) { if(saved.ids[i]>=11) ok=false; for(unsigned j=0;j<i;++j) if(saved.ids[j]==saved.ids[i]) ok=false; }
  if(ok) shortcuts=saved;
}
bool UITask::saveHomeShortcuts() {
  shortcuts.crc=seeed::checksum(&shortcuts,offsetof(HomeShortcuts,crc)); Preferences p; if(!p.begin("l2-home",false)) return false;
  bool ok=p.putBytes("links",&shortcuts,sizeof(shortcuts))==sizeof(shortcuts); HomeShortcuts check{};
  ok=ok && p.getBytes("links",&check,sizeof(check))==sizeof(check) && !memcmp(&check,&shortcuts,sizeof(check)); p.end(); return ok;
}
void UITask::drawFloatingMenus() {
  auto& c=display.canvas(); endContent();
  if(shortcutPicker && page==Home) {
    hitCount=1; hits[0]={0,0,320,240,CloseShortcuts,0}; c.fillRoundRect(8,35,304,197,12,TEAL);
    label(19,45,tr("Home shortcuts","首页快捷入口"),GREEN,1,240,true); button(270,40,32,27,"x",CloseShortcuts);
    shortcutOffset=seeed::clamp(shortcutOffset,0,7);
    for(int row=0;row<4;++row) {
      int id=shortcutOffset+row,y=75+row*34; button(16,y,288,30,"",ToggleShortcut,id);
      label(24,y+6,shortcutName(id),WHITE,1,242);
      bool added=false; for(unsigned i=0;i<shortcuts.count;++i) if(shortcuts.ids[i]==id) added=true;
      if(added) { c.fillCircle(284,y+15,7,GREEN); c.drawLine(280,y+15,283,y+18,BG); c.drawLine(283,y+18,288,y+11,BG); }
    }
    label(19,215,tr("Swipe to browse · tap added to remove","上下浏览 · 点击已添加项可移除"),MUTED,0,285);
  } else if(mapMenuOpen && page==Location) {
    hitCount=1; hits[0]={0,0,320,240,CloseMapMenu,0}; c.fillRoundRect(174,73,139,124,9,TEAL);
    const char* names[]={"Track","Waypoint","Tile"}; const Page pages[]={Tracks,Waypoints,Tiles};
    for(int i=0;i<3;++i) {
      int y=80+i*36; button(180,y,126,32,"",Go,pages[i]); label(211,y+7,names[i],WHITE,1,90,true);
      if(i==0) { c.drawLine(187,y+22,192,y+11,GREEN); c.drawLine(192,y+11,198,y+20,GREEN); c.fillCircle(187,y+22,2,GREEN); c.fillCircle(198,y+20,2,GREEN); }
      else if(i==1) { c.drawCircle(193,y+12,5,GREEN); c.drawLine(188,y+16,193,y+25,GREEN); c.drawLine(198,y+16,193,y+25,GREEN); }
      else { c.drawRect(185,y+9,16,16,GREEN); c.drawFastHLine(185,y+17,16,GREEN); c.drawFastVLine(193,y+9,16,GREEN); }
    }
  }
}
bool UITask::drawUtilityPage() {
  char text[128]; auto& gps=sensors.location();
  if(page==Compass) { drawCompass(); return true; }
  if(page==ScreenshotSettings) {
    drawHeader(tr("Screenshots","截屏"),"",false);
    row(38,screenshotsEnabled?tr("Screenshots: on","截屏：开启"):tr("Screenshots: off","截屏：关闭"),tr("Tap to toggle","点击切换"),ToggleScreenshots);
    label(12,106,tr("Hold upper-left screen for 1.2s","长按屏幕左上角 1.2 秒"),WHITE,1,296,true);
    label(12,138,"/screenshots/*.bmp",GREEN,1,296);
    label(12,169,tr("SD card required · tap notice to dismiss","需要 SD 卡 · 点击关闭成功提示"),MUTED,0,296);
    return true;
  }
  if(page==Apps) {
    drawHeader(tr("Apps","应用"),"",false); beginContent();
    const char* names[]={tr("Audio","音频"),tr("GPS status","GPS 状态"),tr("Tracks","轨迹"),tr("Waypoints","航点"),tr("Tiles","离线瓦片"),tr("Quick panel","快捷操作"),tr("About","关于设备"),tr("Compass","指南针"),tr("Screenshots","截屏")};
    const char* details[]={tr("Recorder and music player","录音机与音乐播放器"),tr("Satellite and UART status","卫星与串口状态"),tr("Record, load or delete a route","新增、加载或删除轨迹"),tr("Save and view places","保存及查看地点"),tr("SD offline map tiles","SD 卡离线地图瓦片"),tr("Display and language","显示与语言"),tr("Version and device information","版本与设备信息"),tr("GPS direction of travel","GPS 行进方向罗盘"),tr("Enable upper-left long-press capture","开启左上角长按截屏")};
    const Page pages[]={Audio,GPSStatus,Tracks,Waypoints,Tiles,Quick,About,Compass,ScreenshotSettings};
    for(int i=0;i<9;++i) { int y=34+i*60-scrollPositions[page]; if(y+54>34 && y<214) row(y,names[i],details[i],Go,pages[i]); }
    endContent(); return true;
  }
  if(page==GPSStatus) {
    drawHeader(tr("GPS status","GPS 状态"),"",false);
    button(10,36,145,34,gps.isEnabled()?tr("Turn off","关闭 GPS"):tr("Turn on","开启 GPS"),Gps);
    button(165,36,145,34,tr("Restart GPS","重启 GPS"),gps.isEnabled()?ResetGps:None);
    const char* state=!gps.isEnabled()?tr("GPS is off","GPS 已关闭"):gps.isValid()?tr("Position fixed","定位成功"):!gps.bytesReceived()?tr("Waiting for UART data","等待串口数据"):!gps.sentenceCount()?tr("Detecting NMEA baud","识别 NMEA 波特率"):tr("Searching satellites","正在搜索卫星");
    label(12,81,state,gps.isValid()?GREEN:WHITE,1,296,true);
    snprintf(text,sizeof(text),"%s %ld  %s %ld",tr("Visible","可见"),gps.satellitesVisible(),tr("Used","使用"),gps.satellitesCount()); label(12,107,text,MUTED,0,296);
    snprintf(text,sizeof(text),"UART %u · %lu bytes",gps.baudRate(),(unsigned long)gps.bytesReceived()); label(12,129,text,MUTED,0,296);
    snprintf(text,sizeof(text),"NMEA %lu · RX18 / TX17",(unsigned long)gps.sentenceCount()); label(12,150,text,MUTED,0,296);
    if(gps.hasPosition()) snprintf(text,sizeof(text),"%.6f, %.6f",gps.getLatitude()/1e6,gps.getLongitude()/1e6); else snprintf(text,sizeof(text),"%s",tr("No position yet","尚未取得定位"));
    label(12,174,text,WHITE,1,296); button(10,201,300,33,tr("Open map","打开地图"),Go,Location); return true;
  }
  if(page==Tracks || page==Waypoints) {
    bool track=page==Tracks; drawHeader(track?tr("Tracks","轨迹"):tr("Waypoints","航点"),"",false);
    if(track) button(10,36,300,35,l2map.trackRecording()?tr("Stop track recording","停止记录轨迹"):tr("+ New track","+ 新增轨迹"),TrackRecord,0,true);
    else button(10,36,300,35,gps.isValid()?tr("+ Add current position","+ 保存当前位置"):tr("GPS fix needed - open GPS","需要定位 · 打开 GPS"),gps.isValid()?AddWaypoint:Go,gps.isValid()?0:GPSStatus,true);
    beginContent(80,214); int count=track?l2map.trackCount():l2map.waypointCount();
    for(int i=0;i<count;++i) {
      int y=80+i*60-scrollPositions[page]; if(y+54<=80 || y>=214) continue;
      if(track) row(y,l2map.trackName(i),tr("Tap to load or delete","点击加载到地图或删除"),OpenTrack,i);
      else { auto& point=l2map.waypoint(i); snprintf(text,sizeof(text),"%.6f, %.6f",point.point.lat/1e6,point.point.lon/1e6); row(y,point.name,text,OpenWaypoint,i); }
    }
    if(!count) empty(track?tr("No tracks yet","暂无轨迹"):tr("No waypoints yet","暂无航点"),track?tr("Only real GPS fixes are recorded.","仅记录真实 GPS 定位点"):tr("Save a position after GPS fixes.","GPS 定位后可保存当前位置"));
    endContent(); return true;
  }
  if(page==TrackDetail) {
    drawHeader(tr("Track","轨迹"),"",false); label(12,39,selectedTrack,GREEN,1,296,true);
    row(76,tr("Load on map","重载到地图"),tr("Show the route without changing its file","在地图上显示，保留原文件"),LoadTrack);
    row(140,tr("Delete track","删除轨迹"),tr("Requires confirmation","需要确认"),DeleteTrack); label(20,147,tr("Delete track","删除轨迹"),RED,1,266,true); return true;
  }
  if(page==WaypointDetail) {
    drawHeader(tr("Waypoint","航点"),"",false);
    if(selectedWaypoint>=0 && selectedWaypoint<int(l2map.waypointCount())) label(12,39,l2map.waypoint(selectedWaypoint).name,GREEN,1,296,true);
    row(76,tr("View on map","在地图查看"),tr("Center on this saved place","居中到此保存地点"),ShowWaypoint);
    row(140,tr("Delete waypoint","删除航点"),tr("Requires confirmation","需要确认"),DeleteWaypoint); label(20,147,tr("Delete waypoint","删除航点"),RED,1,266,true); return true;
  }
  if(page==Tiles) {
    drawHeader(tr("Tiles","离线瓦片"),"",false);
    row(36,l2map.tilesEnabled()?tr("Offline tiles: on","离线瓦片：开启"):tr("Offline tiles: off","离线瓦片：关闭"),tr("XYZ PNG 256 x 256","XYZ PNG 256 × 256"),ToggleTiles);
    label(12,103,"/map/z/x/y.png",WHITE,1,296); l2map.currentTile(text,sizeof(text)); label(12,129,text,MUTED,0,296);
    button(10,154,145,34,tr("Zoom -","缩小"),MapZoom,-1); button(165,154,145,34,tr("Zoom +","放大"),MapZoom,1);
    button(10,199,300,35,tr("Rescan SD","重新读取 SD"),RescanMap); return true;
  }
  if(page==Administration) {
    drawHeader(tr("Administration","管理操作"),"",false);
    const char* names[]={tr("Factory reset","恢复出厂设置"),tr("Reboot","重启"),tr("Shut down","关机")};
    const char* info[]={tr("Erase internal data; keep SD files","清空内部数据，保留 SD 文件"),tr("Restart this device","重新启动设备"),tr("BOOT button to wake","按 BOOT 可唤醒")};
    const Action actions[]={FactoryReset,Reboot,Shutdown};
    for(int i=0;i<3;++i) { int y=38+i*60; row(y,names[i],info[i],actions[i]); label(20,y+7,names[i],RED,1,266,true); }
    return true;
  }
  return false;
}
