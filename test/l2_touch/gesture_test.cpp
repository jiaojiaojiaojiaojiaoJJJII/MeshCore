#include "TouchGesture.h"
#include "L2Battery.h"
#include "L2AudioFormat.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#endif

int main() {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  _set_error_mode(_OUT_TO_STDERR);
#endif
  assert(seeed::nextMainPage(0,true)==1 && seeed::nextMainPage(1,false)==0);
  assert(seeed::nextMainPage(5,true)==0);
  assert(seeed::nextMainPage(0,false)==5);
  assert(seeed::nextMainPage(2,true)==3);
  assert(!seeed::menuExpired(5999,1000,false));
  assert(seeed::menuExpired(6000,1000,false));
  assert(!seeed::menuExpired(7000,1000,true));
  assert(seeed::menuExpired(4000,0xfffffc18U,false));
  seeed::TouchGesture g;
  g.begin(120,100); g.update(125,104); assert(g.tap()); assert(!g.pageSwipe());
  g.begin(120,100); g.update(120,142); assert(!g.tap()); assert(!g.pageSwipe());
  g.begin(250,90); g.update(180,94); assert(g.pageSwipe()); assert(!g.tap()); assert(seeed::nextMainPage(0,g.dx()<0)==1);
    g.begin(120,90); g.update(190,94); assert(g.pageSwipe()); assert(seeed::nextMainPage(1,g.dx()<0)==0);
  g.begin(30,230); g.update(33,160); assert(g.opensMenu()); assert(!g.pageSwipe());
  g.begin(30,180); g.update(30,100); assert(!g.opensMenu());
  g.begin(30,230); g.update(30,160); g.update(30,226); assert(!g.opensMenu()); assert(!g.tap());
  g.begin(35,210); g.update(200,212); assert(!g.completesUnlock());
  g.begin(35,210); g.update(270,212); assert(g.completesUnlock());
  g.begin(35,210); g.update(15,210); assert(!g.completesUnlock());
  g.begin(35,150); g.update(290,150); assert(!g.startsSlider()); assert(!g.completesUnlock());
  g.begin(160,210); g.update(319,210); assert(!g.completesUnlock());
  g.begin(35,210); g.update(260,170); g.update(280,210); assert(!g.completesUnlock());
  g.begin(35,210); g.update(270,210); g.update(80,210); assert(!g.completesUnlock());

  char output[64];
  seeed::previewText(output,sizeof(output),u8"一二三四五六七八九十一二三四五六");
  assert(!strcmp(output,u8"一二三四五六七八九十一二三四五"));
  assert(strlen(output)==45);
  seeed::previewText(output,sizeof(output),"1234567890123456"); assert(!strcmp(output,"123456789012345"));
  seeed::previewText(output,sizeof(output),u8"A🙂中B",3); assert(!strcmp(output,u8"A🙂中"));
  seeed::previewText(output,sizeof(output),"A\nB\rC",5); assert(!strcmp(output,"A B C"));
  char small[4]={'x','x','x','x'};
  seeed::previewText(small,3,u8"中"); assert(small[0]==0 && small[3]=='x');
  seeed::previewText(output,sizeof(output),"abc",0); assert(output[0]==0);
  char untouched='x'; seeed::previewText(&untouched,0,"abc"); assert(untouched=='x');

  struct Message { bool unread; bool outgoing; } messages[24]={};
  for(int i=0;i<24;++i) messages[i]={true,i%4==0};
  messages[23].unread=false;
  int indices[24]={};
  auto predicate=[&](int i) { return messages[i].unread && !messages[i].outgoing; };
  int count=seeed::latestUnread(24,predicate,indices,24); assert(count==10);
  assert(indices[0]==22 && indices[1]==21 && indices[2]==19);
  for(int i=0;i<count;++i) { assert(predicate(indices[i])); if(i) assert(indices[i]<indices[i-1]); }
  assert(seeed::latestUnread(0,predicate,indices)==0);
  assert(seeed::latestUnread(24,predicate,indices,2)==2);
  assert(seeed::latestUnread(24,predicate,indices,0)==0);
  assert(l2::batteryPercent(3300)==0 && l2::batteryPercent(4200)==100 && l2::batteryPercent(3750)==50);
  const l2::BatteryPoint curve[]={{3300,0},{3700,40},{4200,100}};
  assert(l2::interpolateBattery(0,curve,3)==-1);
  assert(l2::interpolateBattery(3000,curve,3)==0);
  assert(l2::interpolateBattery(3500,curve,3)==20);
  assert(l2::interpolateBattery(3950,curve,3)==70);
  assert(l2::interpolateBattery(4400,curve,3)==100);
  const l2::BatteryPoint invalid[]={{3300,0},{3700,40},{3700,60}};
  assert(l2::interpolateBattery(3400,invalid,3)==-1);
  const uint8_t telemetry[]={1,103,0,240,1,116,1,154,2,136,0,0,0,0,0,0,0,0,0};
  assert(l2::telemetryBatteryMv(telemetry,sizeof(telemetry))==4100);
  assert(l2::telemetryBatteryMv(telemetry,sizeof(telemetry)-1)==-1);
  const uint8_t wrongChannel[]={2,116,1,154}; assert(l2::telemetryBatteryMv(wrongChannel,4)==-1);
  const uint8_t unknownType[]={1,255,1,116,1,154}; assert(l2::telemetryBatteryMv(unknownType,6)==-1);
  uint32_t gain=256;
  int16_t silence[4]={0,0,1,-1}; for(int i=0;i<100;++i) l2::normalizeRecording(silence,4,gain); assert(gain==256 && silence[2]==1);
  for(int i=0;i<100;++i) { int16_t quiet[4]={100,-100,100,-100}; l2::normalizeRecording(quiet,4,gain); assert(quiet[0]<=16000 && quiet[0]>0 && quiet[1]<0); }
  assert(gain<=128*256 && gain>32*256);
  int16_t loud[4]={32767,-32768,32000,-32000}; l2::normalizeRecording(loud,4,gain); assert(gain==256 && loud[0]==32767 && loud[1]==-32768);
  uint8_t header[44]; l2::recordingHeader(header,32000);
  assert(!memcmp(header,"RIFF",4) && !memcmp(header+8,"WAVEfmt ",8));
  assert(header[24]==0x80 && header[25]==0x3e && header[22]==1 && header[34]==16);
  assert(l2::recoverableRecording(header)); header[22]=2; assert(!l2::recoverableRecording(header));
  puts("PASS: battery interpolation, telemetry bounds, recoverable mono WAV format");
  puts("PASS: gesture arbitration, slider boundaries, UTF-8 preview limits and latest-ten unread selection");
}
