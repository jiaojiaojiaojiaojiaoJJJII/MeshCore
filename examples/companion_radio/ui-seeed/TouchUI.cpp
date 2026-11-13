#include "UITask.h"
#include <algorithm>
#include <math.h>
#include <L2Audio.h>
#include <L2Map.h>

namespace {
constexpr uint16_t BG=0x0882, CARD=0x1125, GREEN=0xBF66, TEAL=0x01C9;
constexpr uint16_t WHITE=0xF7BE, MUTED=0xBDF7, AMBER=0xFE88;
constexpr int viewport=180, rowHeight=60;
}

void UITask::selectFont(int size,bool bold) {
  auto& c=display.canvas(); c.setTextSize(size>=3?2:1);
  if(size==0) c.setFont(bold?&fonts::efontCN_14_b:&fonts::efontCN_14);
  else if(size==2) c.setFont(&fonts::efontCN_24_b);
  else c.setFont(bold?&fonts::efontCN_16_b:&fonts::efontCN_16);
}
void UITask::beginContent(int top,int bottom) {
  hitClipTop=top; hitClipBottom=bottom;
  display.canvas().setClipRect(0,top,320,bottom-top);
}
void UITask::endContent() {
  hitClipTop=0; hitClipBottom=240; display.canvas().clearClipRect();
}
bool UITask::navigationAllowed() const {
  return !onboarding && page!=Locked && page!=Confirm;
}
void UITask::drawNavigation() {
  auto& c=display.canvas();
  // Last drawn, and the only active hit regions: the drawer cannot click through.
  endContent(); hitCount=0;
  c.fillRoundRect(10,182,300,58,10,TEAL);
  c.fillRoundRect(139,186,42,3,2,MUTED);
  const char* names[]={tr("Home","首页"),tr("Msg","消息"),tr("People","联系人"),tr("Map","地图"),tr("Audio","音频"),tr("Set","设置")};
  for(int i=0;i<6;++i) {
    int x=10+i*50;
    button(x+2,194,46,40,"",Go,int(mainPage(i)),mainIndex()==i);
    selectFont(0,true); int width=c.textWidth(names[i]);
    label(x+(50-width)/2,207,names[i],mainIndex()==i?BG:WHITE,0,48,true);
  }
}
void UITask::drawGestureHint() {
  if(!navigationAllowed() || page>Settings && page!=Audio) return;
  auto& c=display.canvas(); c.fillRect(0,215,320,25,TEAL);
  c.drawFastHLine(10,215,300,TEAL);
  label(12,220,tr("Swipe < next","左滑下一页"),MUTED,0,112);
  for(int i=0;i<6;++i) c.fillCircle(139+i*9,228,i==mainIndex()?3:2,i==mainIndex()?GREEN:MUTED);
  label(232,220,tr("Menu","上滑菜单"),GREEN,0,84,true);
  c.drawLine(216,228,222,222,GREEN); c.drawLine(222,222,228,228,GREEN);
}
int UITask::maxScroll() const {
  if(page==Home) return std::max(0,88+(3+int(shortcuts.count))*60+46-180);
  if(page==Location) return 0;
  if(page==Tracks) return std::max(0,int(l2map.trackCount())*60-132);
  if(page==Waypoints) return std::max(0,int(l2map.waypointCount())*60-132);
  if(page==Apps) return std::max(0,9*60-180);
  if(page==Contacts || page==ChatContacts) return std::max(0,contactsHeight(page==ChatContacts)-180);
  if(page==Chats || page==Settings || page==Discover || page==PublicChannels || page==PrivateChannels || page==BasicSettings || page==AdvancedSettings) return std::max(0,itemCount()*rowHeight-viewport);
  if(page==Recordings || page==Music) return std::max(0,audio.fileCount()*60-136);
  if(page==Radio || page==SetupRadio) return std::max(0,int(seeed::presetCount)*37-111);
  if(page==Conversation) return std::max(0,itemCount()*60-120);
  if(page==MessageDetail) return std::max(0,messageLines*20-118);
  return 0;
}
int UITask::unreadCards(int* indices) const {
  return seeed::latestUnread(local.size(),[&](int i) {
    const auto& m=local.at(i); return m.unread && !m.outgoing;
  },indices);
}
void UITask::drawLocked() {
  int indices[10],count=unreadCards(indices);
  lockedOffset=seeed::clamp(lockedOffset,0,std::max(0,count-2));
  char title[48]; snprintf(title,sizeof(title),"%u %s",local.unread(),tr("unread messages","条未读消息"));
  drawHeader(title,"H02",false);
  if(!count) {
    display.canvas().fillRoundRect(10,45,300,117,10,CARD);
    label(26,72,tr("No unread messages","暂无未读消息"),WHITE,1,270,true);
    label(26,110,tr("Your conversations are up to date.","滑动下方滑块进入设备"),MUTED,0,270);
  }
  for(int n=0;n<2 && lockedOffset+n<count;++n) {
    const auto& m=local.at(indices[lockedOffset+n]);
    int y=36+n*70;
    display.canvas().fillRoundRect(10,y,300,62,9,CARD);
    char sender[64],preview[64]; const char* body=m.text;
    snprintf(sender,sizeof(sender),"%s",m.sender);
    // Group text embeds its claimed sender before ': '; show that sender,
    // while preserving the original channel message in history.
    const char* colon=m.channel?strstr(body,": "):nullptr;
    if(colon && colon-body>0 && colon-body<=31) {
      memcpy(sender,body,colon-body); sender[colon-body]=0; body=colon+2;
    }
    seeed::previewText(preview,sizeof(preview),body,15);
    label(21,y+7,sender,GREEN,1,275,true);
    label(21,y+33,preview,WHITE,1,275);
  }
  label(12,172,tr("Swipe down for more","下滑查看更多未读"),MUTED,0,220);
  if(count) {
    snprintf(title,sizeof(title),"%d-%d / %d",lockedOffset+1,std::min(lockedOffset+2,count),count);
    label(234,172,title,MUTED,0,76);
  }
  auto& c=display.canvas(); c.fillRoundRect(12,192,296,40,20,TEAL);
  label(76,202,tr("Slide right to unlock","向右滑动解锁"),WHITE,1,222,true);
  c.fillRoundRect(16+sliderOffset,196,42,32,15,GREEN);
  c.drawLine(32+sliderOffset,204,40+sliderOffset,212,BG);
  c.drawLine(40+sliderOffset,212,32+sliderOffset,220,BG);
}
void UITask::drawMap() {
  auto& c=display.canvas(); auto& gps=sensors.location(); l2map.render(c);
  if(gps.hasPosition()) {
    l2::GeoPoint point; point.lat=gps.getLatitude(); point.lon=gps.getLongitude(); double x,y;
    if(l2map.project(point,x,y) && x>=0 && x<320 && y>=0 && y<180) {
      uint16_t ink=gps.isValid()?GREEN:AMBER; c.drawCircle(x,y+34,9,ink); if(gps.isValid()) c.fillCircle(x,y+34,5,ink); else c.drawCircle(x,y+34,5,ink);
    }
  }
  char value[96]; snprintf(value,sizeof(value),"Z%d · %s",l2map.zoom(),l2map.tilesDrawn()?tr("Offline tiles","离线瓦片"):tr("Grid","栅格"));
  c.fillRoundRect(7,112,150,24,5,BG); label(12,117,value,MUTED,0,140);
  const char* state=!gps.isEnabled()?tr("GPS off","GPS 关闭"):gps.isValid()?tr("GPS fixed","GPS 已定位"):!gps.bytesReceived()?tr("GPS: waiting UART","GPS：等待串口数据"):tr("GPS: acquiring","GPS：搜星中");
  snprintf(value,sizeof(value),"%s  %ld/%ld",state,gps.satellitesCount(),gps.satellitesVisible()); button(7,144,205,29,value,Go,GPSStatus);
  if(l2map.loaded()) button(7,38,130,30,tr("Exit track","退出重载"),UnloadTrack);
  if(l2map.trackRecording()) { c.fillCircle(150,51,4,0xF249); label(161,43,"REC",0xF249,0,42,true); }
  const char* controls[]={"<","^","v",">","O","+","-"};
  for(int i=0;i<7;++i) button(8+i*43,180,38,30,controls[i],i<4?MapPan:i==4?MapCenter:MapZoom,i<4?i:i==5?1:-1);
  button(275,38,35,30,"",MapMenu);
  for(int i=0;i<3;++i) c.drawFastHLine(284,47+i*5,17,GREEN);
}
bool UITask::drawMainPage() {
  if(page>Settings && page!=Discover) return false;
  const char* titles[]={prefs->node_name,tr("Messages","消息"),tr("Contacts","联系人"),tr("Map","地图"),tr("Settings","设置")};
  drawHeader(page==Discover?tr("Heard adverts","发现设备"):titles[page],"",false);
  scrollPositions[page]=seeed::clamp(scrollPositions[page],0,maxScroll());
  int offset=scrollPositions[page];
  beginContent();
  if(page==Home) {
    drawMakerClock(34-offset);
    int y=122-offset;
    char value[80]; snprintf(value,sizeof(value),"%u %s",local.unread(),tr("unread messages","条未读消息")); row(y,tr("Messages","消息"),value,Go,Chats); y+=60;
    snprintf(value,sizeof(value),"%d %s",the_mesh.getNumContacts(),tr("saved nodes","个已保存节点")); row(y,tr("Contacts","联系人"),value,Go,Contacts); y+=60;
    row(y,tr("Apps","应用"),tr("Audio, GPS, tracks and device tools","音频、定位、轨迹及设备工具"),Go,Apps); y+=60;
    for(unsigned i=0;i<shortcuts.count;++i) { row(y,shortcutName(shortcuts.ids[i]),tr("Custom shortcut","自定义快捷入口"),Go,shortcutPage(shortcuts.ids[i])); y+=60; }
    button(10,y,300,46,"+",ShowShortcuts,0,true);
  } else if(page==Location) drawMap();
  else {
    int logical=0,shown=0;
    auto add=[&](const char* title,const char* detail,Action action,int value) {
      int y=34+logical++*rowHeight-offset;
      if(y+54>34 && y<214) { row(y,title,detail,action,value); ++shown; }
    };
    if(page==Chats) {
      add(tr("Public channel","公开频道"),tr("Public and #named channels","Public 和 #命名频道"),Go,PublicChannels);
      add(tr("Private channel","私有频道"),tr("Channels using a shared secret","使用共享密钥的频道"),Go,PrivateChannels);
      add(tr("Contacts","联系人"),tr("Private conversations","一对一私聊"),Go,ChatContacts);
    } else if(page==Contacts) {
      drawContactList(); shown=the_mesh.getNumContacts();
    } else if(page==Discover) {
      for(unsigned i=0;i<discoveredCount;++i) {
        char detail[48]; snprintf(detail,sizeof(detail),"%lus %s",(unsigned long)((millis()-discoveredAt[i])/1000),tr("ago","前收到广播"));
        add(discovered[i].name,detail,OpenDiscovered,i);
      }
    } else if(page==Settings) {
      add(tr("Basic settings","基础设置"),tr("LoRa, display, sound and Bluetooth","LoRa、显示、声音与蓝牙"),Go,BasicSettings);
      add(tr("Advanced settings","高级设置"),tr("GPS, hardware and administration","GPS、硬件及管理操作"),Go,AdvancedSettings);
    }
    if(!shown) empty(tr("Nothing here yet","暂无内容"),tr("Swipe up from the bottom for navigation.","从底部上滑打开菜单"));
  }
  endContent();
  // Small contextual actions stay outside the scrolling list, not in a left rail.
  if(page==Chats || page==Contacts || page==Discover) {
    display.canvas().fillRect(0,29,320,4,BG);
    if(page==Contacts) { button(150,3,44,24,tr("Find","发现"),Go,Discover); button(198,3,44,24,tr("Add","添加"),ManualContact); }
    else if(page==Chats) button(198,3,44,24,tr("Add","新增"),Go,NewChannel);
    else button(150,3,92,24,tr("Advertise","广播信息"),Advert);
  }
  drawGestureHint(); return true;
}
