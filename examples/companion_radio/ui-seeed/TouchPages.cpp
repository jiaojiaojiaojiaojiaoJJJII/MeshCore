#include "UITask.h"
#include <L2Audio.h>
#include <L2Battery.h>
#include <algorithm>
#include <math.h>

namespace {
constexpr uint16_t BG=0x0882,CARD=0x1125,GREEN=0xBF66,WHITE=0xF7BE,MUTED=0xBDF7,RED=0xF249;
void star(lgfx::LGFX_Sprite& c,int x,int y,uint16_t color) {
  int px[10],py[10];
  for(int i=0;i<10;++i) { float a=-1.5707963f+i*0.6283185f; int r=i%2?4:10; px[i]=x+cosf(a)*r; py[i]=y+sinf(a)*r; }
  for(int i=0;i<10;++i) c.fillTriangle(x,y,px[i],py[i],px[(i+1)%10],py[(i+1)%10],color);
}
}
int UITask::mainIndex() const {
  // A direct Back can return to a root without passing through navigate().
  // Swipes must use the visible root, rather than a stale navigation selection.
  Page current=(page<=Settings || page==Audio)?page:navPage;
  return current==Audio?4:current==Settings?5:int(current);
}
UITask::Page UITask::mainPage(int i) const { const Page pages[]={Home,Chats,Contacts,Location,Audio,Settings}; return pages[(i%6+6)%6]; }
bool UITask::isPublicChannel(const ChannelDetails& ch) const {
  const uint8_t publicKey[]={0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};
  uint8_t hash[32]={},upper=0; for(int i=16;i<32;++i) upper|=ch.channel.secret[i]; if(upper) return false;
  if(ch.name[0]=='#') { mesh::Utils::sha256(hash,32,(const uint8_t*)ch.name,strlen(ch.name)); if(!memcmp(ch.channel.secret,hash,16)) return true; }
  return !memcmp(ch.channel.secret,publicKey,16);
}
UITask::PeerState* UITask::peerState(const uint8_t* key,bool create) {
  PeerState* free=nullptr;
  for(auto& p:peers) { if(p.used && !memcmp(p.key,key,32)) return &p; if(!p.used && !free) free=&p; }
  if(!create) return nullptr;
  if(!free) { free=&peers[0]; for(auto& p:peers) if(uint32_t(millis()-p.heard)>uint32_t(millis()-free->heard)) free=&p; }
  *free=PeerState{}; memcpy(free->key,key,32); free->used=true; return free;
}
void UITask::hearPeer(const ContactInfo& ci,float snr,float rssi) { auto* p=peerState(ci.id.pub_key,true); p->heard=millis(); p->snr=snr; p->rssi=rssi; dirty=true; }
void UITask::onPeerTelemetry(const ContactInfo& from,const uint8_t* bytes,uint8_t length) {
  hearPeer(from,radio_driver.getLastSNR(),radio_driver.getLastRSSI());
  int mv=l2::telemetryBatteryMv(bytes,length);
  if(mv>0 && mv<=65535) { auto* p=peerState(from.id.pub_key,true); p->millivolts=mv; p->batteryAt=millis(); dirty=true; }
}
int UITask::contactsHeight(bool chatOnly) const {
  int height=0;
  for(int i=MAX_ANON_CONTACTS;i<the_mesh.getTotalContactSlots();++i) { ContactInfo ci; if(the_mesh.getContactByIdx(i,ci) && (!chatOnly||ci.type==ADV_TYPE_CHAT)) height+=60+(expandedContact==i?78:0); }
  return height;
}
void UITask::drawContactList(bool chatOnly) {
  int y=34-scrollPositions[page]; auto& c=display.canvas();
  // Stable partition by MeshCore's persistent favourite flag.
  for(int favorite=1;favorite>=0;--favorite) for(int i=MAX_ANON_CONTACTS;i<the_mesh.getTotalContactSlots();++i) {
    ContactInfo ci; if(!the_mesh.getContactByIdx(i,ci)||int(ci.isFav())!=favorite||(chatOnly&&ci.type!=ADV_TYPE_CHAT)) continue;
    bool expanded=expandedContact==i; int height=expanded?132:54;
    if(y+height>34 && y<214) {
      auto* state=peerState(ci.id.pub_key); bool online=state && uint32_t(millis()-state->heard)<900000;
      int x=expanded?4:10,w=expanded?312:300;
      button(x,y,w,height,"",ExpandContact,i);
      label(20,y+7,ci.name,online?GREEN:RED,1,231,true);
      if((millis()/1000)%2==0) {
        label(20,y+32,tr("Signal","信号"),MUTED,0,52);
        if(state) { int bars=state->snr>8?4:state->snr>0?3:state->snr>-8?2:1; for(int b=0;b<4;++b) c.fillRect(78+b*9,y+46-b*3,6,5+b*3,b<bars?GREEN:MUTED); label(125,y+32,tr("Last hop","末跳接收"),MUTED,0,100); }
        else label(78,y+32,"--",MUTED,0,80);
      } else {
        int percent=state && uint32_t(millis()-state->batteryAt)<3600000?l2::batteryPercent(state->millivolts):-1;
        char text[48]; if(percent>=0) snprintf(text,sizeof(text),"%s %d%%",tr("Battery","电量"),percent); else snprintf(text,sizeof(text),"%s --%%",tr("Battery","电量"));
        label(20,y+32,text,MUTED,0,230);
      }
      button(267,y+3,37,45,"",ToggleFavorite,i); star(c,285,y+24,ci.isFav()?GREEN:0x7BEF);
      if(expanded) {
        char text[96];
        if(ci.gps_lat || ci.gps_lon) snprintf(text,sizeof(text),"GPS %.6f, %.6f",ci.gps_lat/1e6,ci.gps_lon/1e6); else snprintf(text,sizeof(text),"%s",tr("GPS: not reported","GPS：尚未上报"));
        label(20,y+59,text,WHITE,0,282);
        if(state) snprintf(text,sizeof(text),"SNR %.1f dB  RSSI %.0f dBm",state->snr,state->rssi); else snprintf(text,sizeof(text),"SNR --  RSSI --");
        label(20,y+80,text,MUTED,0,282);
        if(ci.type==ADV_TYPE_CHAT) button(18,y+103,95,25,tr("Message","发消息"),OpenContact,i);
        label(122,y+109,online?tr("Heard <15m","15分钟内听到"):tr("Not heard <15m","15分钟未听到"),online?GREEN:RED,0,181);
      }
    }
    y+=height+6;
  }
}
bool UITask::createChannel(const char* name,const uint8_t* key) {
  ChannelDetails existing{}; int slot=-1;
  for(int i=0;i<MAX_GROUP_CHANNELS;++i) if(the_mesh.getChannel(i,existing)) {
    if(existing.name[0] && (!strcmp(name,existing.name)||!memcmp(key,existing.channel.secret,16))) { alert(tr("Channel already exists","频道名称或密钥已存在"),true); return false; }
    if(!existing.name[0] && slot<0) slot=i;
  }
  if(slot<0) { alert(tr("Channel storage full","频道容量已满"),true); return false; }
  ChannelDetails ch{}; snprintf(ch.name,sizeof(ch.name),"%s",name); memcpy(ch.channel.secret,key,16);
  if(!the_mesh.setChannel(slot,ch)) return false;
  if(!the_mesh.persistUIChannels()) { ChannelDetails empty{}; the_mesh.setChannel(slot,empty); alert(tr("Channel save verification failed","频道保存校验失败"),true); return false; }
  page=isPublicChannel(ch)?PublicChannels:PrivateChannels; scrollPositions[page]=0;
  alert(tr("Channel saved","频道已保存")); return true;
}
bool UITask::drawExtraPage() {
  auto list=[&](const char* title,const char* const* names,const char* const* details,const Action* actions,const int* args,int count) {
    drawHeader(title,"",false); beginContent(); int offset=scrollPositions[page]=seeed::clamp(scrollPositions[page],0,maxScroll());
    for(int i=0;i<count;++i) { int y=34+i*60-offset; if(y+54>34 && y<214) row(y,names[i],details[i],actions[i],args[i]); }
    endContent(); button(198,3,44,24,tr("Back","返回"),Back);
  };
  if(page==AudioPlayer) {
    drawHeader(tr("Now playing","正在播放"),"",false);
    button(198,3,44,24,tr("Back","返回"),Back);
    label(12,37,audio.trackName(),GREEN,1,296,true);
    const char* state=audio.error()[0]?audio.error():audio.nearSilent()?tr("Recording is almost silent","录音文件接近静音"):audio.paused()?tr("Paused","已暂停"):audio.playing()?tr("Playing","正在播放"):tr("Stopped - tap Play","已停止，点击播放");
    label(12,59,state,audio.error()[0]||audio.nearSilent()?0xFE88:MUTED,0,296);
    auto& c=display.canvas(); c.fillRoundRect(10,78,300,38,8,CARD);
    const int16_t* bins=audio.waveform();
    for(int i=1;i<32;++i) c.drawLine(16+(i-1)*9,97-bins[i-1]/2048,16+i*9,97-bins[i]/2048,GREEN);
    char left[24],right[24]; uint32_t elapsed=audio.elapsedMs()/1000,total=audio.durationMs()/1000;
    snprintf(left,sizeof(left),"%lu:%02lu",(unsigned long)(elapsed/60),(unsigned long)(elapsed%60));
    if(total) snprintf(right,sizeof(right),"%s%lu:%02lu",audio.durationApproximate()?"~":"",(unsigned long)(total/60),(unsigned long)(total%60)); else strcpy(right,"--:--");
    label(12,120,left,MUTED,0,110); label(238,120,right,MUTED,0,70);
    unsigned position=playerSlider==1?playerSeekPreview:audio.progress();
    c.fillRoundRect(12,141,296,5,2,0x01C9); c.fillRoundRect(12,141,296*position/1000,5,2,GREEN); c.fillCircle(12+296*position/1000,143,5,WHITE);
    if(hitCount<48) hits[hitCount++]={12,132,296,22,PlayerSeek,0};
    button(10,158,55,34,"|<",PlayerPrevious);
    button(70,158,96,34,audio.playing()&&!audio.paused()?tr("Pause","暂停"):tr("Play","播放"),PlayerPause,0,true);
    button(171,158,55,34,">|",PlayerNext); button(231,158,79,34,tr("Stop","停止"),StopAudio);
    button(10,200,35,31,"-",AudioVolume,-1); button(275,200,35,31,"+",AudioVolume,1);
    snprintf(left,sizeof(left),"%d%%",audio.volume()); label(50,208,left,WHITE,0,44,true);
    c.fillRoundRect(100,213,160,5,2,0x01C9); c.fillRoundRect(100,213,160*audio.volume()/100,5,2,GREEN); c.fillCircle(100+160*audio.volume()/100,215,6,GREEN);
    if(hitCount<48) hits[hitCount++]={96,198,172,34,PlayerVolume,0};
    return true;
  }
  if(page==Audio) {
    const char* names[]={tr("Recording","录音"),tr("Music","音乐"),tr("Now playing","正在播放")};
    const char* details[]={tr("Record and play /record/*.wav","录制与播放 /record/*.wav"),tr("Play /music/*.mp3","播放 /music/*.mp3"),audio.trackName()};
    const Action actions[]={Go,Go,Go}; const int args[]={Recordings,Music,AudioPlayer};
    list(tr("Audio","音频"),names,details,actions,args,audio.hasTrack()?3:2); drawGestureHint(); return true;
  }
  if(page==BasicSettings || page==AdvancedSettings) {
    const char* names[]={tr("LoRa configuration","LoRa 配置"),tr("Display & language","显示与语言"),tr("Bluetooth","蓝牙"),tr("Device name","设备名称"),tr("Sound","提示音"),tr("Volume +","增加音量"),tr("Volume -","降低音量"),tr("About","关于设备")};
    const char* details[]={tr("Region, preset and radio","地区、预设与无线电"),tr("Brightness, language and timezone","亮度、语言与时区"),isBluetoothEnabled()?tr("On","已开启"):tr("Off","已关闭"),settings.name,prefs->buzzer_quiet?tr("Muted","静音"):tr("Enabled","已开启"),tr("Speaker volume","扬声器音量"),tr("Speaker volume","扬声器音量"),tr("Version and pairing PIN","版本与配对码")};
    const Action actions[]={Go,Go,Bluetooth,EditName,SoundToggle,AudioVolume,AudioVolume,Go}; const int args[]={Radio,Quick,0,0,0,1,-1,About};
    const char* advanced[]={tr("GPS","定位"),tr("Hardware status","硬件状态"),tr("Administration","管理操作")};
    const char* info[]={tr("Power, NMEA and positioning","电源、串口与定位"),tr("Storage, memory and touch","存储、内存与触控"),tr("Factory reset, reboot, shutdown","恢复出厂、重启、关机")};
    const Action acts[]={Go,Go,Go}; const int vals[]={GPSStatus,Diagnostics,Administration};
    if(page==BasicSettings) list(tr("Basic settings","基础设置"),names,details,actions,args,8);
    else { list(tr("Advanced settings","高级设置"),advanced,info,acts,vals,3); int y=34+120-scrollPositions[page]; beginContent(); label(20,y+7,advanced[2],0xF249,1,266,true); endContent(); }
    return true;
  }
  if(page==PublicChannels || page==PrivateChannels || page==ChatContacts) {
    drawHeader(page==PublicChannels?tr("Public channel","公开频道"):page==PrivateChannels?tr("Private channel","私有频道"):tr("Contacts","联系人"),"",false);
    beginContent(); scrollPositions[page]=seeed::clamp(scrollPositions[page],0,maxScroll());
    if(page==ChatContacts) drawContactList(true);
    else {
      ChannelDetails ch; int count=0;
      for(int i=0;i<MAX_GROUP_CHANNELS;++i) if(the_mesh.getChannel(i,ch)&&ch.name[0]&&isPublicChannel(ch)==(page==PublicChannels)) {
        int y=34+count++*60-scrollPositions[page]; if(y+54>34&&y<214) row(y,ch.name,page==PublicChannels?tr("Public conversation","公开频道会话"):tr("Shared-key conversation","共享密钥会话"),OpenChannel,i);
      }
      if(!count) empty(tr("No channels yet","暂无频道"),tr("Tap + to add a channel","点击 + 新增频道"));
    }
    endContent(); button(150,3,44,24,"+",Go,NewChannel); button(198,3,44,24,tr("Back","返回"),Back); return true;
  }
  if(page==NewChannel) {
    drawHeader(tr("Add channel","新增频道"),"",false);
    row(38,tr("Public #channel","公开 #频道"),tr("Same name joins the same channel","相同 #名称加入同一频道"),AddChannel,0);
    row(102,tr("Private channel","私有频道"),tr("Name + 32 hexadecimal key","名称 + 32 位十六进制密钥"),AddChannel,1);
    button(10,188,300,40,tr("Back","返回"),Back); return true;
  }
  if(page==Recordings || page==Music) {
    drawHeader(page==Recordings?tr("Recording list","录音列表"):tr("Music","音乐"),"",false);
    if(audio.error()[0]) label(12,36,audio.error(),0xFE88,0,296);
    else label(12,36,audio.recording()?tr("Recording... tap Stop to save","正在录音，点击停止保存"):audio.playing()?tr("Playing...","正在播放…"):page==Music?"/music/*.mp3":"/record/*.wav",MUTED,0,296);
    if(audio.recording()) {
      char time[48]; uint32_t seconds=audio.recordingMs()/1000;
      snprintf(time,sizeof(time),"%lu:%02lu  %s",(unsigned long)(seconds/60),(unsigned long)(seconds%60),tr("Recording","录音中"));
      display.canvas().fillRect(10,34,300,23,BG); label(12,36,time,GREEN,0,290,true);
      display.canvas().fillRect(12,53,296,3,CARD); display.canvas().fillRect(12,53,296*audio.inputLevel()/100,3,GREEN);
    }
    beginContent(58,191);
    for(int i=0;i<audio.fileCount();++i) { int y=58+i*60-scrollPositions[page]; if(y+54>58&&y<191) row(y,audio.fileName(i),page==Recordings && audio.isLatest(i)?tr("LATEST - tap to play","最新录音 · 点击播放"):tr("Tap to play","点击播放"),audio.recording()?None:PlayAudio,i); }
    endContent();

    if(page==Recordings) {
      button(10,198,300,37,audio.recording()?"":tr("Record","录音"),audio.recording()||!audio.busy()?RecordAudio:None,0,true);
      if(audio.recording()) { label(56,208,tr("Press again to stop","再按一次停止"),BG,1,244,true); auto& c=display.canvas(); for(int i=0;i<9;++i) c.drawFastVLine(20+i*3,210-(i%3)*2,6+(i%3)*4,BG); }
    } else button(210,199,100,35,tr("Stop","停止"),StopAudio);
    return true;
  }
  return false;
}
void UITask::notify(UIEventType type) {
  if(!prefs || !prefs->buzzer_quiet) {
    if(type==UIEventType::contactMessage) audio.cue(L2Audio::PrivateMessage);
    else if(type==UIEventType::channelMessage) audio.cue(L2Audio::ChannelMessage);
    else if(type==UIEventType::ack) audio.cue(L2Audio::Ack);
  }
  dirty=true;
}
