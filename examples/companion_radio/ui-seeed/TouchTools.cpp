#include "UITask.h"
#include <L2Capture.h>
#include <math.h>
#include <time.h>
namespace {
constexpr uint16_t BG=0x0882,CARD=0x1125,GREEN=0xBF66,WHITE=0xF7BE,MUTED=0xBDF7,TEAL=0x01C9;
void digit(lgfx::LGFX_Sprite& c,int x,int y,char ch) {
  const uint8_t masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
  uint8_t mask=ch>='0'&&ch<='9'?masks[ch-'0']:0x40;
  const int r[7][4]={{4,0,19,4},{23,4,4,17},{23,25,4,17},{4,42,19,4},{0,25,4,17},{0,4,4,17},{4,21,19,4}};
  for(int i=0;i<7;++i) c.fillRoundRect(x+r[i][0],y+r[i][1],r[i][2],r[i][3],2,mask&(1<<i)?GREEN:0x1946);
}
}
void UITask::drawMakerClock(int y) {
  auto& c=display.canvas(); c.fillRoundRect(10,y,300,78,9,CARD);
  bool synced=!sensors.location().waitingTimeSync(); struct tm tm{};
  if(synced) { time_t now=rtc_clock.getCurrentTime()+settings.timezoneMinutes*60; gmtime_r(&now,&tm); }
  char time[8]="--:--",seconds[8]="--";
  if(synced) { snprintf(time,sizeof(time),"%02d:%02d",tm.tm_hour,tm.tm_min); snprintf(seconds,sizeof(seconds),"%02d",tm.tm_sec); }
  label(22,y+4,"L2 // LOCAL TIME",MUTED,0,190,true);
  int x=25; for(int i=0;i<5;++i) { if(i==2) { bool blink=(millis()/500)%2==0; c.fillCircle(x+4,y+36,3,blink?GREEN:TEAL); c.fillCircle(x+4,y+51,3,blink?GREEN:TEAL); x+=16; } else { digit(c,x,y+23,time[i]); x+=34; } }
  label(192,y+24,seconds,WHITE,2,80,true);
  label(192,y+53,synced?"GPS SYNC":"WAIT GPS",MUTED,0,110);
  for(int i=0;i<12;++i) c.fillRect(22+i*13,y+72,9,2,i==int(millis()/200%12)?GREEN:TEAL);
  c.drawLine(282,y+10,298,y+10,TEAL); c.drawLine(298,y+10,298,y+26,TEAL);
  c.fillCircle(282,y+10,2,GREEN);
}
void UITask::drawCompass() {
  drawHeader(tr("Compass","指南针"),"",false); auto& gps=sensors.location();
  bool valid=gps.hasCourse(); auto& c=display.canvas(); int cx=105,cy=122;
  c.drawCircle(cx,cy,64,TEAL); c.drawCircle(cx,cy,60,MUTED);
  for(int i=0;i<24;++i) { float a=i*0.2617994f; int r=i%6?56:51; c.drawLine(cx+sinf(a)*r,cy-cosf(a)*r,cx+sinf(a)*60,cy-cosf(a)*60,i%6?TEAL:GREEN); }
  label(cx-5,cy-82,"N",GREEN,1,20,true); label(cx+69,cy-8,"E",MUTED,0,18); label(cx-5,cy+66,"S",MUTED,0,20); label(cx-80,cy-8,"W",MUTED,0,18);
  if(valid) {
    float a=gps.courseDegrees()*0.0174532925f,dx=sinf(a),dy=-cosf(a);
    c.fillTriangle(cx+dx*46,cy+dy*46,cx-dy*9,cy+dx*9,cx+dy*9,cy-dx*9,GREEN);
    c.fillTriangle(cx-dx*35,cy-dy*35,cx-dy*9,cy+dx*9,cx+dy*9,cy-dx*9,0xF249); c.fillCircle(cx,cy,4,WHITE);
    char text[24]; snprintf(text,sizeof(text),"%.0f deg",gps.courseDegrees()); label(187,91,text,GREEN,2,125,true);
    snprintf(text,sizeof(text),"%.1f km/h",gps.speedKmh()); label(192,129,text,MUTED,0,116);
  } else { label(195,97,"-- deg",MUTED,1,110); label(192,127,tr("Move to read","移动后读数"),WHITE,0,120); }
  label(12,204,tr("GPS travel compass · no magnetic sensor","GPS 行进罗盘 · 无磁力计"),MUTED,0,296);
  if(!gps.isEnabled()) button(190,156,120,34,tr("Enable GPS","开启 GPS"),Gps,0,true);
  else button(190,156,120,34,tr("GPS status","GPS 状态"),Go,GPSStatus);
}
void UITask::updateScreenshot() {
  int result=capture.loop(); if(!result) return;
  if(result<0) { alert(capture.error(),true); return; }
  char text[128]; snprintf(text,sizeof(text),"%s\n%s",tr("Screenshot saved","截屏成功"),capture.path());
  alert(text); noticeSticky=true;
}
