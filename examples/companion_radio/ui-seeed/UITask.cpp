#include "UITask.h"
#include <esp_ota_ops.h>
#include <algorithm>
#include <L2Audio.h>
#include <L2Map.h>
#include <L2Battery.h>
#include <L2Capture.h>

namespace {
constexpr uint16_t rgb(unsigned c) { return ((c>>19)&31)<<11 | ((c>>10)&63)<<5 | ((c>>3)&31); }
constexpr uint16_t BG=rgb(0x0B1214), CARD=rgb(0x162429), GREEN=rgb(0xBEED35);
constexpr uint16_t TEAL=rgb(0x003A4A), MUTED=rgb(0xA4B4B7), WHITE=rgb(0xF4F7F6), AMBER=rgb(0xFFD166);
const char* phrases[] = {"OK, understood.","On my way.","Please wait.","I have arrived.","Please share your location."};
const char* emojis[] = {"\xF0\x9F\x99\x82","\xE2\x9C\x85","\xE2\x8F\xB3","\xF0\x9F\x93\x8D","\xE2\x9A\xA0\xEF\xB8\x8F"};
const char* emojiNames[] = {"Smile / 微笑","Confirmed / 确认","Wait / 等待","Location / 位置","Warning / 警告"};

void channelIdentity(uint8_t* target,const mesh::GroupChannel& channel) {
  mesh::Utils::sha256(target,32,channel.secret,sizeof(channel.secret));
}
}

const char* UITask::tr(const char* en,const char* zh) const { return (onboarding ? draft.chinese : settings.chinese) ? zh : en; }
void UITask::label(int x,int y,const char* value,uint16_t color,int size,int width,bool bold) {
  auto& c=display.canvas(); selectFont(size,bold); c.setTextColor(color);
  int limit=width>0?width:std::max(0,318-x);
  char fitted[256]; snprintf(fitted,sizeof(fitted),"%s",value);
  if(char* newline=strpbrk(fitted,"\r\n")) *newline=0;
  size_t n=strlen(fitted);
  while(n && c.textWidth(fitted)>limit) {
    do { --n; } while(n && (uint8_t(fitted[n])&0xC0)==0x80);
    fitted[n]=0;
  }
  c.setCursor(x,y); c.print(fitted); selectFont();
}
int UITask::wrapped(int x,int y,int width,const char* body,int skip,int rows,uint16_t color) {
  selectFont();
  char line[256]={}; size_t used=0; int count=0;
  auto flush=[&]() {
    if(count>=skip && count<skip+rows) label(x,y+(count-skip)*20,line,color,1,width);
    ++count; used=0; line[0]=0;
  };
  for(const char* p=body;*p;) {
    if(*p=='\r') { ++p; continue; }
    if(*p=='\n') { flush(); ++p; continue; }
    const char* end=p+1; while(*end && (uint8_t(*end)&0xC0)==0x80) ++end;
    size_t bytes=end-p;
    if(used+bytes>=sizeof(line)) flush();
    memcpy(line+used,p,bytes); line[used+bytes]=0;
    if(used && display.canvas().textWidth(line)>width) { line[used]=0; flush(); }
    memcpy(line+used,p,bytes); used+=bytes; line[used]=0; p=end;
  }
  if(used || !count) flush();
  return count;
}
const char* UITask::messageState(seeed::MessageState state) const {
  switch(state) {
    case seeed::MessageState::Received: return tr("Received","已接收");
    case seeed::MessageState::Queued: return tr("Queued","已排队");
    case seeed::MessageState::Unconfirmed: return tr("Not confirmed","未获确认");
    case seeed::MessageState::Delivered: return tr("Recipient ACK","对方已确认");
    case seeed::MessageState::Failed: return tr("Send failed","发送失败");
    case seeed::MessageState::Transmitted: return tr("Transmitted","已发射");
  }
  return "?";
}
int UITask::itemCount() const {
  if(page==Radio || page==SetupRadio) return seeed::presetCount;
  if(page==Settings) return 2;
  if(page==Chats) return 3;
  if(page==BasicSettings) return 8;
  if(page==AdvancedSettings) return 3;
  if(page==PublicChannels || page==PrivateChannels) { ChannelDetails ch; int count=0; for(int i=0;i<MAX_GROUP_CHANNELS;++i) if(the_mesh.getChannel(i,ch)&&ch.name[0]&&isPublicChannel(ch)==(page==PublicChannels)) ++count; return count; }
  if(page==Discover) return discoveredCount;
  if(page==Contacts) return the_mesh.getNumContacts();
  if(page==Conversation) {
    int count=0; for(unsigned i=0;i<local.size();++i) if(belongs(local.at(i))) ++count;
    return count;
  }
  return 0;
}
void UITask::button(int x,int y,int w,int h,const char* value,Action action,int arg,bool primary) {
  if(action==Back) return; // one consistent icon-only back control is drawn last
  auto& c=display.canvas(); c.fillRoundRect(x,y,w,h,7,primary?GREEN:CARD);
  int role=h<=30?0:1, fontHeight=role==0?14:16;
  label(x+8,y+(h-fontHeight)/2,value,primary?BG:WHITE,role,w-16,true);
  int top=std::max(y,hitClipTop),bottom=std::min(y+h,hitClipBottom);
  if(hitCount<48 && action!=None && bottom>top && x+w>0 && x<320)
    hits[hitCount++]={int16_t(x),int16_t(top),int16_t(w),int16_t(bottom-top),action,arg};
}
void UITask::row(int y,const char* title,const char* detail,Action action,int value) {
  button(10,y,300,54,"",action,value);
  label(20,y+7,title,WHITE,1,266,true); label(20,y+32,detail,MUTED,0,266);
  auto& c=display.canvas(); c.drawLine(294,y+22,299,y+27,GREEN); c.drawLine(299,y+27,294,y+32,GREEN);
}
void UITask::empty(const char* title,const char* detail) {
  label(12,80,title,GREEN,1,296,true); label(12,116,detail,MUTED,0,296);
}
void UITask::drawHeader(const char* title,const char* id,bool navigation) {
  (void)id; (void)navigation;
  bool actions=page==Chats || page==Contacts || page==Discover || page==Audio || page==PublicChannels || page==PrivateChannels || page==ChatContacts || page==BasicSettings || page==AdvancedSettings || page==AudioPlayer;
  display.canvas().fillRect(0,0,320,30,rgb(0x284B32));
  int start=rootPage()?10:42;
  label(start,6,title,page==Administration?rgb(0xF24949):GREEN,1,(actions?144:radio_driver.isTxAllowed()?232:186)-start,true);
  auto& c=display.canvas();
  {
    char value[16]; int percent=l2::batteryPercent(board.getBattMilliVolts());
    if(percent>=0) snprintf(value,sizeof(value),"%d%%",percent); else strcpy(value,"--%");
    label(247,8,value,MUTED,0,40);
    c.drawRoundRect(289,8,22,14,2,MUTED); c.fillRect(312,12,3,6,MUTED);
    if(percent>=0) c.fillRect(292,11,percent*16/100,8,percent<20?AMBER:GREEN);
    if(!radio_driver.isTxAllowed()) { if(actions) c.fillCircle(140,16,3,AMBER); else label(183,8,"TX LOCK",AMBER,0,60,true); }
  }
  c.drawFastHLine(10,29,300,TEAL);
}

void UITask::begin(DisplayDriver* d,SensorManager*,NodePrefs* p) {
  driver=d; prefs=p;
  local.loadSettings(settings); local.begin();
  bool firmwareMatch=!memcmp(settings.firmware,esp_ota_get_app_description()->app_elf_sha256,32);
  onboarding=!settings.confirmed || !firmwareMatch || settings.preset<0 || settings.preset>=int(seeed::presetCount);
  draft=settings;
  if(!driver) { onboarding=true; radio_driver.allowTransmit(false); }
  if(onboarding) { radio_driver.allowTransmit(false); page=SetupName; draft.confirmed=0; }
  else if(!applyRadio(settings)) { onboarding=true; page=SetupRadio; radio_driver.allowTransmit(false); }
  // Never power GPS from stale app preferences while a new setup is pending.
  prefs->gps_enabled=!onboarding && settings.gps;
  prefs->advert_loc_policy=ADVERT_LOC_NONE;
  the_mesh.applyGpsPrefs();
  if(!onboarding) {
    prefs->gps_enabled=settings.gps; prefs->gps_interval=settings.gpsSeconds;
    prefs->manual_add_contacts=1; prefs->advert_loc_policy=ADVERT_LOC_NONE;
    the_mesh.applyGpsPrefs();
  }
  disableBluetooth();
  display.setBrightness(settings.brightness);
  // Ensure the built-in Public conversation without replacing a saved channel.
  { ChannelDetails ch; bool found=false; int free=-1;
    const uint8_t key[]={0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};
    for(int i=0;i<MAX_GROUP_CHANNELS;++i) if(the_mesh.getChannel(i,ch)) { if(ch.name[0] && !memcmp(ch.channel.secret,key,16)) found=true; if(!ch.name[0] && free<0) free=i; }
    if(!found && free>=0) { ch=ChannelDetails{}; strcpy(ch.name,"Public"); memcpy(ch.channel.secret,key,16); the_mesh.setChannel(free,ch); if(!the_mesh.persistUIChannels()) alert(tr("Public channel save failed","Public 频道保存失败"),true); }
  }
  loadHomeShortcuts();
  { Preferences p; if(p.begin("l2-ui",true)) { screenshotsEnabled=p.getBool("screenshots",false); p.end(); } }
  audio.begin();
  l2map.begin();
  if(!prefs->buzzer_quiet) audio.cue(L2Audio::Startup);
  user_btn.begin(); lastActivity=millis();
  display.logTouchInfo();
  Serial.printf("L2 UI ready: display=%s onboarding=%s SD=%s internal_fs=%u/%u PSRAM=%u\n",
      driver?"ready":"failed", onboarding?"yes":"no", board.cardPresent()?"inserted":"absent",
      unsigned(l2FS.usedBytes()), unsigned(l2FS.totalBytes()), ESP.getPsramSize());
  if(!local.healthy()) alert(tr("History damaged: readable messages retained.","历史文件损坏，保留可读记录。"),true);
  if(!driver) Serial.println("L2: display initialization failed; TX remains locked");
}

bool UITask::applyRadio(const seeed::Settings& next) {
  if(next.preset<0 || next.preset>=int(seeed::presetCount)) return false;
  if(the_mesh.hasPendingWork() || radio_driver.isTransmitting()) return false;
  auto& p=seeed::presets[next.preset];
  if(!seeed::inHardwareBand(p.frequency)) return false;
  radio_driver.allowTransmit(false);
  if(!radio_driver.configure(p.frequency,p.bandwidth,p.sf,p.cr,14)) return false;
  prefs->freq=p.frequency; prefs->bw=p.bandwidth; prefs->sf=p.sf; prefs->cr=p.cr;
  prefs->tx_power_dbm=14; prefs->path_hash_mode=p.pathBytes-1;
  snprintf(prefs->node_name,sizeof(prefs->node_name),"%s",next.name);
  radio_driver.allowTransmit(next.confirmed);
  return true;
}
bool UITask::commitSettings(const seeed::Settings& next) {
  if(!local.saveSettings(next)) { alert(tr("Could not save settings.","设置保存失败。"),true); return false; }
  settings=next; draft=next; the_mesh.savePrefs(); return true;
}
void UITask::alert(const char* value,bool error) {
  snprintf(notice,sizeof(notice),"%s",value); noticeError=error; noticeSticky=false;
  noticeUntil=millis()+2000; dirty=true;
}
void UITask::ask(const char* value,Action action) {
  snprintf(confirmText,sizeof(confirmText),"%s",value); confirmedAction=action; confirmReturn=page;
  page=Confirm; dirty=true;
}
void UITask::navigate(Page next) {
  if(!saveDraft()) return;
  if(page<=Settings) offsets[page]=listOffset;
  if(next!=page && next>Settings && next!=Audio) parentPages[next]=page;
  mapMenuOpen=false; shortcutPicker=false; menuOpen=false;
  previous=page; page=next;
  if(next<=Settings || next==Audio) { navPage=next; listOffset=offsets[next]; }
  dirty=true;
}
void UITask::back() {
  if(menuOpen) { menuOpen=false; dirty=true; return; }
  if(shortcutPicker) { shortcutPicker=false; dirty=true; return; }
  if(mapMenuOpen) { mapMenuOpen=false; dirty=true; return; }
  notice[0]=0;
  if(page==SetupName) return;
  if(page==SetupRadio) { if(onboarding) page=SetupName; else page=Settings; }
  else if(page==SetupPrivacy) page=SetupRadio;
  else if(page==SetupFinish) page=SetupPrivacy;
  else if(page==Composer) { if(saveDraft()) page=Conversation; }
  else if(page==Emoji || page==Phrases) page=Composer;
  else if(page==MessageDetail) page=Conversation;
  else if(page==Conversation) { page=conversationReturn; listOffset=conversationOffset; }
  else if(page==Input) page=inputReturn;
  else if(page==ContactDetail) { page=detailReturn; listOffset=detailOffset; }
  else if(page==Discover) { page=Contacts; listOffset=offsets[Contacts]; }
  else if(page==Radio || page==Diagnostics || page==Apps || page==Administration || page==Tracks || page==Waypoints || page==Tiles || page==GPSStatus || page==TrackDetail || page==WaypointDetail || page==Compass || page==ScreenshotSettings) page=parentPages[page];
  else if(page==About) page=BasicSettings;
  else if(page==BasicSettings || page==AdvancedSettings) page=Settings;
  else if(page==PublicChannels || page==PrivateChannels || page==ChatContacts || page==NewChannel) page=Chats;
  else if(page==AudioPlayer) { page=audio.musicTrack()?Music:Recordings; audio.scan(page==Music); }
  else if(page==Recordings || page==Music) { if(audio.recording()) audio.stop(); page=Audio; }
  else if(page==TouchTest) page=Diagnostics;
  else if(page==Confirm) page=confirmReturn;
  else if(page==Quick) page=previous;
  else if(page!=Locked) page=Home;
  if(page<=Settings || page==Audio) navPage=page;
  dirty=true;
}

void UITask::draw() {
  if(!driver || !display.isOn()) return;
  endContent(); hitCount=0; display.startFrame(BG);
  char value[100];
  if(drawUtilityPage() || drawExtraPage() || drawMainPage()) {
  } else if(page==ContactDetail) {
    drawHeader(tr("Device details","设备详情"),"C03");
    label(12,39,selected.name,WHITE,2);
    char hex[65]; mesh::Utils::toHex(hex,selected.id.pub_key,32);
    for(int i=0;i<4;++i) { char line[17]; memcpy(line,hex+i*16,16); line[16]=0; label(12,72+i*19,line,MUTED,0); }
    bool saved=the_mesh.lookupContactByPubKey(selected.id.pub_key,32)!=nullptr;
    label(12,151,saved?tr("Saved identity","已保存的身份"):tr("Heard, not saved","听到但尚未保存"),GREEN);
    if(selected.type==ADV_TYPE_CHAT && saved) button(10,182,202,42,tr("Write a message","发消息"),Write);
    else if(!saved) button(10,182,202,42,tr("Add contact","添加联系人"),AddDiscovered);
    button(224,182,86,42,tr("Back","返回"),Back);
  } else if(page==Radio || page==SetupRadio) {
    bool setup=page==SetupRadio;
    drawHeader(tr("Region & preset","地区与预设"),"O03",false);
    label(10,33,tr("Hardware: 863-925 MHz","硬件频段：863–925 MHz"),MUTED);
    label(10,50,tr("Select where you will use it.","请选择实际使用地区。"),MUTED);
    int presetScroll=seeed::clamp(scrollPositions[page],0,maxScroll());
    presetOffset=presetScroll/37;
    beginContent(68,181);
    for(unsigned i=presetOffset;i<seeed::presetCount && i<unsigned(presetOffset+4);++i) {
      int y=70+(i-presetOffset)*37-presetScroll%37;
      button(10,y,300,32,seeed::presets[i].title,ChoosePreset,i,draft.preset==int(i));
    }
    endContent();
    if(draft.preset>=0) {
      auto& p=seeed::presets[draft.preset];
      snprintf(value,sizeof(value),"%.3f / BW%g / SF%u / 4/%u / %uB",p.frequency,p.bandwidth,p.sf,p.cr,p.pathBytes); label(10,184,value,GREEN,0,300);
    }
    button(10,205,45,30,"<",Scroll,-3); button(61,205,45,30,">",Scroll,3);
    button(112,205,65,30,tr("Back","返回"),Back);
    button(183,205,127,30,tr("Confirm band","确认频段"),draft.preset>=0?(setup?NextSetup:ApplyRadio):None,0,true);
  } else if(page==SetupName) {
    drawHeader("seeed studio","O02",false);
    label(12,44,tr("Make it yours.","从这里开始。"),WHITE,2);
    label(12,77,"Wio Tracker L2 Pro / MeshCore",MUTED);
    button(10,108,300,40,draft.name,EditName);
    button(10,158,145,37,"English",Language,0,!draft.chinese);
    button(165,158,145,37,"简体中文",Language,1,draft.chinese);
    button(10,207,300,30,tr("Next: region & preset","下一步：地区与预设"),NextSetup,0,true);
  } else if(page==SetupPrivacy) {
    drawHeader(tr("Location & privacy","定位与隐私"),"O04",false);
    label(12,46,tr("Your location, your choice.","由你决定是否定位。"),WHITE,2);
    button(10,88,300,43,draft.gps?tr("GPS: on","GPS：开启"):tr("GPS: off","GPS：关闭"),Gps);
    label(12,148,tr("Automatic sharing stays off.","自动分享位置默认关闭。"),MUTED);
    label(12,170,tr("Chatting does not require a fix.","无需定位即可聊天。"),MUTED);
    button(10,205,100,32,tr("Back","返回"),Back);
    button(120,205,190,32,tr("Continue","继续"),NextSetup,0,true);
  } else if(page==SetupFinish) {
    drawHeader(tr("Ready to begin","准备完成"),"O05",false);
    label(12,45,draft.name,WHITE,2);
    if(draft.preset>=0) label(12,86,seeed::presets[draft.preset].title,GREEN);
    label(12,112,tr("863-925 MHz hardware / 14 dBm","863–925 MHz 硬件 / 14 dBm"),MUTED);
    label(12,140,tr("No automatic location sharing.","不自动广播位置。"),MUTED);
    label(12,163,tr("Confirm the preset for your area.","请确认预设适用于使用地区。"),MUTED);
    button(10,205,100,32,tr("Back","返回"),Back);
    button(120,205,190,32,tr("Save & start","保存并开始"),FinishSetup,0,true);
  } else if(page==Conversation) {
    drawHeader(conversationName,"M02",false);
    button(10,32,64,26,tr("Back","返回"),Back);
    label(84,38,channelTarget?tr("Channel","频道"):tr("Direct message","私聊"),MUTED,0);
    int indices[48],count=0;
    for(unsigned i=0;i<local.size();++i) if(belongs(local.at(i))) indices[count++]=i;
    scrollPositions[Conversation]=seeed::clamp(scrollPositions[Conversation],0,maxScroll());
    beginContent(64,184);
    for(int i=0;i<count;++i) {
      int y=64+i*60-scrollPositions[Conversation]; if(y+54<=64 || y>=184) continue;
      auto& m=local.at(indices[i]); int x=m.outgoing?34:10, width=m.outgoing?276:300;
      button(x,y,width,54,"",OpenMessage,int(m.id));
      snprintf(value,sizeof(value),"%s / %s",m.sender,messageState(m.state)); label(x+9,y+6,value,MUTED,0,width-18);
      label(x+9,y+29,m.text,WHITE,1,width-18);
    }
    if(!count) label(12,90,tr("Send the first message.","发送第一条消息。"),MUTED);
    endContent();
    button(10,194,145,40,tr("Quick reply","快捷回复"),Phrase);
    button(165,194,145,40,tr("Write","写消息"),Go,Composer,true);
  } else if(page==MessageDetail) {
    drawHeader(tr("Message details","消息详情"),"M04",false);
    seeed::Message* msg=nullptr;
    for(unsigned i=0;i<local.size();++i) if(local.at(i).id==selectedMessage) { msg=&local.at(i); break; }
    if(msg) {
      label(12,38,msg->sender,GREEN);
      label(12,58,messageState(msg->state),MUTED);
      beginContent(80,198);
      messageLines=wrapped(12,81-scrollPositions[MessageDetail],294,msg->text,0,200);
      endContent();
    } else label(12,85,tr("Message no longer in history.","消息已不在历史中。"),MUTED);
    button(10,205,90,32,"<",Scroll,-7);
    button(106,205,90,32,">",Scroll,7);
    button(202,205,108,32,tr("Back","返回"),Back);
  } else if(page==Composer || page==Input) {
    bool editor=page==Composer;
    drawHeader(editor?conversationName:inputKind==ContactKey?tr("Contact public key","联系人公钥"):inputKind==ContactName?tr("Contact name","联系人名称"):inputKind==ChannelKey?tr("32-digit channel key","32位频道密钥"):inputKind==PublicChannelName||inputKind==PrivateChannelName?tr("Channel name","频道名称"):tr("Device name","设备名称"),editor?"M03":"G01",false);
    const char* current=editor?text:input;
    display.canvas().drawRoundRect(8,31,304,40,5,TEAL);
    // Keep the entire UTF-8 draft in storage; the viewport shows its tail.
    const char* tail=current; while(*tail && display.canvas().textWidth(tail)>290) { ++tail; while((uint8_t(*tail)&0xC0)==0x80) ++tail; }
    label(14,39,tail);
    snprintf(value,sizeof(value),"%u / %u bytes",unsigned(strlen(current)),editor?textCapacity():inputCapacity());
    label(12,76,value,strlen(current)>(editor?textCapacity():inputCapacity())?AMBER:MUTED,0);
    const char* keys=symbols?"1234567890-/#;()$&@\".,?!'[]_+=" : shift?"QWERTYUIOPASDFGHJKLZXCVBNM":"qwertyuiopasdfghjklzxcvbnm";
    unsigned len=strlen(keys);
    for(unsigned i=0;i<len && i<30;++i) { char ch[2]={keys[i],0}; button(5+(i%10)*31,95+(i/10)*29,28,26,ch,Key,uint8_t(keys[i])); }
    button(5,183,44,26,"Aa",Shift); button(53,183,44,26,"123",KeyboardMode);
    button(101,183,99,26,tr("Space","空格"),Space); button(204,183,48,26,"<-",DeleteChar);
    button(256,183,59,26,editor?"Emoji":"Done",editor?ShowEmoji:InputDone);
    button(5,213,83,25,tr("Back","返回"),Back);
    if(editor) {
      button(92,213,98,25,tr("Phrase","快捷短语"),Phrase,1);
      bool valid=strlen(text)>0 && strlen(text)<=textCapacity() && radio_driver.isTxAllowed();
      button(194,213,121,25,tr("Send","发送"),valid?Send:None,0,valid);
    } else button(92,213,223,25,tr("Continue","继续"),strlen(input)>0 && strlen(input)<=inputCapacity()?InputDone:None,0,true);
  } else if(page==Phrases) {
    drawHeader(tr("Quick replies","快捷短语"),"S16",false);
    for(int i=0;i<5;++i) button(10,34+i*33,300,29,phrases[i],ChoosePhrase,i);
    button(10,205,300,32,tr("Back","返回"),Back);
  } else if(page==Emoji) {
    drawHeader(tr("Choose emoji","选择表情"),"M11",false);
    for(int i=0;i<5;++i) button(10,34+i*33,300,29,emojiNames[i],InsertEmoji,i);
    button(10,205,300,32,tr("Back","返回"),Back);
  } else if(page==Quick) {
    drawHeader(tr("Quick panel","快捷操作"),"H03",false);
    button(10,37,145,42,tr("Brightness +","增加亮度"),Brightness,1);
    button(165,37,145,42,tr("Brightness -","降低亮度"),Brightness,-1);
    button(10,88,145,42,tr("Language","切换语言"),Language,settings.chinese?0:1);
    button(165,88,145,42,tr("Lock","锁定"),Lock);
    snprintf(value,sizeof(value),"UTC %c%02d:%02d",settings.timezoneMinutes<0?'-':'+',abs(settings.timezoneMinutes)/60,abs(settings.timezoneMinutes)%60);
    button(10,139,145,42,value,Timezone);
    button(165,139,145,42,tr("Sleep","休眠"),Shutdown);
    button(10,196,300,39,tr("Close","关闭"),Back);
  } else if(page==Locked) {
    drawLocked();
  } else if(page==About) {
    drawHeader(tr("About","关于"),"S11");
    label(12,43,"Seeed Wio Tracker L2 Pro",GREEN);
    label(12,68,FIRMWARE_VERSION); label(12,91,"Seeed UI / development 0.5.1",MUTED);
    label(12,116,"ESP32-S3 / SX1262 / NV3031B",MUTED);
    label(12,140,"16 MB flash / 8 MB PSRAM",MUTED);
    snprintf(value,sizeof(value),"BLE PIN: %06lu",(unsigned long)the_mesh.getBLEPin());
    if(isBluetoothEnabled()) label(12,165,value);
    button(10,195,300,39,tr("Back","返回"),Back);
  } else if(page==Diagnostics) {
    drawHeader(tr("Hardware status","硬件状态"),"S12",false);
    label(12,39,board.ioReady()?tr("I/O expander: ready","I/O 扩展器：就绪"):tr("I/O expander: unavailable","I/O 扩展器：不可用"),GREEN);
    snprintf(value,sizeof(value),"PSRAM: %u / %u KB free",ESP.getFreePsram()/1024,ESP.getPsramSize()/1024); label(12,65,value);
    snprintf(value,sizeof(value),"Heap: %u KB free",ESP.getFreeHeap()/1024); label(12,89,value);
    snprintf(value,sizeof(value),"Flash FS: %u / %u KB",unsigned(l2FS.usedBytes()/1024),unsigned(l2FS.totalBytes()/1024)); label(12,113,value);
    label(12,137,local.healthy()?tr("History: verified","消息日志：校验通过"):tr("History: storage error","消息日志：存储异常"),local.healthy()?GREEN:AMBER);
    label(12,161,audio.mounted()?tr("SD: mounted","SD：已挂载"):tr("SD: no card detected","SD：未检测到卡"),MUTED);
    button(10,199,145,36,tr("Touch test","触控测试"),Go,TouchTest);
    button(165,199,145,36,tr("Back","返回"),Back);
  } else if(page==TouchTest) {
    drawHeader(tr("Touch test","触控测试"),"S13",false);
    label(14,34,tr("Touch the four corners.","请依次触摸四个角。"),MUTED);
    for(int i=0;i<4;++i) display.canvas().drawRect(i%2?283:5,i/2?160:55,32,32,GREEN);
    if(touchX>=0) {
      display.canvas().drawCircle(touchX,touchY,7,GREEN);
      snprintf(value,sizeof(value),"X %d / 319    Y %d / 239",touchX,touchY); label(40,109,value);
    }
    button(10,205,300,32,tr("Back","返回"),Back);
  } else if(page==Confirm) {
    drawHeader(tr("Please confirm","请确认"),"",false);
    wrapped(12,49,296,confirmText,0,4); label(12,136,tr("This changes your device.","此操作会改变设备状态。"),MUTED,0);
    button(10,173,145,52,tr("Cancel","取消"),ConfirmNo,0,true);
    button(165,173,145,52,tr("Confirm","确认"),ConfirmYes);
  }
  drawBackButton();
  drawFloatingMenus();
  if(menuOpen && navigationAllowed()) drawNavigation();
  if(notice[0] && (noticeError || noticeSticky || int32_t(noticeUntil-millis())>0)) {
    display.canvas().fillRoundRect(4,4,312,108,5,noticeError?rgb(0x6D3920):TEAL);
    wrapped(12,10,296,notice,0,4,noticeError?AMBER:WHITE);
    label(12,92,tr("Tap to dismiss","点击关闭"),MUTED,0);
    // Error notices capture the whole screen, preventing fall-through actions.
    hitCount=1; hits[0]={0,0,320,240,Help,0};
  }
  display.endFrame(); dirty=false; lastDraw=millis();
}

void UITask::handle(Action a,int v) {
  dirty=true; lastActivity=millis();
  if(a==Go) {
    if(v==Radio) { draft=settings; presetOffset=0; }
    if(v==Composer) loadDraft();
    if(v==Tracks) l2map.scanTracks();
    if(v==Waypoints || v==Tiles) l2map.rescan();
    if(v==Recordings || v==Music) { if(audio.recording()) audio.stop(); audio.scan(v==Music); }
    navigate(Page(v)); return;
  }
  if(a==Back) { back(); return; }
  if(a==ToggleScreenshots) {
    Preferences p; bool next=!screenshotsEnabled;
    if(!p.begin("l2-ui",false)) { alert(tr("Could not save setting","设置保存失败"),true); return; }
    bool ok=p.putBool("screenshots",next)==1 && p.getBool("screenshots",!next)==next; p.end();
    if(ok) screenshotsEnabled=next; else alert(tr("Could not save setting","设置保存失败"),true);
    return;
  }
  if(a==Help) { notice[0]=0; noticeSticky=false; return; }
  if(a==Scroll) {
    int step=(page==Radio || page==SetupRadio)?111:page==MessageDetail?100:120;
    scrollPositions[page]=seeed::clamp(scrollPositions[page]+(v>0?step:-step),0,maxScroll()); return;
  }
  if(a==ChoosePreset) { draft.preset=v; return; }
  if(a==NextSetup) { if(page==SetupName) page=SetupRadio; else if(page==SetupRadio&&draft.preset>=0) page=SetupPrivacy; else if(page==SetupPrivacy) page=SetupFinish; return; }
  if(a==FinishSetup || a==ApplyRadio) {
    if(the_mesh.hasPendingWork() || radio_driver.isTransmitting()) { alert(tr("Waiting for radio work to finish.","请等待无线电任务结束。"),true); return; }
    auto old=settings;
    draft.confirmed=0; memcpy(draft.firmware,esp_ota_get_app_description()->app_elf_sha256,32);
    radio_driver.allowTransmit(false);
    if(!local.saveSettings(draft) || !applyRadio(draft)) {
      local.saveSettings(old); applyRadio(old); radio_driver.allowTransmit(false);
      alert(tr("Could not apply radio settings.","无线电配置应用失败。"),true); return;
    }
    draft.confirmed=1;
    if(!local.saveSettings(draft)) { radio_driver.allowTransmit(false); alert(tr("Settings verification failed.","配置校验失败。"),true); return; }
    radio_driver.allowTransmit(true);
    settings=draft; onboarding=false; page=Home; navPage=Home;
    prefs->gps_enabled=settings.gps; prefs->gps_interval=settings.gpsSeconds; prefs->manual_add_contacts=1;
    prefs->advert_loc_policy=ADVERT_LOC_NONE; the_mesh.applyGpsPrefs(); the_mesh.savePrefs();
    alert(tr("Settings saved","配置已保存")); return;
  }
  if(a==Language) { if(onboarding) draft.chinese=v; else { auto next=settings; next.chinese=v; commitSettings(next); } return; }
  if(a==EditName) { inputReturn=page; inputKind=NodeName; snprintf(input,sizeof(input),"%s",onboarding?draft.name:settings.name); page=Input; return; }
  if(a==MapMenu) { mapMenuOpen=!mapMenuOpen; return; }
  if(a==CloseMapMenu) { mapMenuOpen=false; return; }
  if(a==MapPan) { const int dx[]={-64,0,0,64},dy[]={0,-64,64,0}; if(v>=0 && v<4) l2map.pan(dx[v],dy[v]); return; }
  if(a==MapZoom) { l2map.zoomBy(v); return; }
  if(a==MapCenter) { if(!l2map.centerGps(sensors.location())) alert(l2map.error(),true); return; }
  if(a==UnloadTrack) { l2map.unload(); return; }
  if(a==TrackRecord) {
    if(l2map.trackRecording()) l2map.stopTrack();
    else {
      if(!sensors.location().isEnabled()) { handle(Gps); if(!sensors.location().isEnabled()) return; }
      if(!l2map.startTrack()) alert(l2map.error(),true);
    }
    return;
  }
  if(a==OpenTrack) { if(v<0 || v>=int(l2map.trackCount())) return; snprintf(selectedTrack,sizeof(selectedTrack),"%s",l2map.trackName(v)); parentPages[TrackDetail]=Tracks; page=TrackDetail; return; }
  if(a==LoadTrack) { if(l2map.loadTrack(selectedTrack)) navigate(Location); else alert(l2map.error(),true); return; }
  if(a==DeleteTrack) { ask(tr("Delete this track file from SD?","从 SD 卡删除此轨迹文件？"),DeleteTrack); return; }
  if(a==AddWaypoint) { if(!l2map.addWaypoint(sensors.location())) alert(l2map.error(),true); return; }
  if(a==OpenWaypoint) { if(v<0 || v>=int(l2map.waypointCount())) return; selectedWaypoint=v; parentPages[WaypointDetail]=Waypoints; page=WaypointDetail; return; }
  if(a==ShowWaypoint) { if(selectedWaypoint>=0 && l2map.centerWaypoint(selectedWaypoint)) navigate(Location); return; }
  if(a==DeleteWaypoint) { ask(tr("Delete this saved waypoint?","删除此保存航点？"),DeleteWaypoint); return; }
  if(a==ToggleTiles) { l2map.toggleTiles(); return; }
  if(a==RescanMap) { if(!l2map.rescan()) alert(l2map.error(),true); else alert(tr("SD map files reloaded","已重新读取 SD 地图文件")); return; }
  if(a==ResetGps) { sensors.location().reset(); return; }
  if(a==ShowShortcuts) { shortcutPicker=true; shortcutOffset=0; return; }
  if(a==CloseShortcuts) { shortcutPicker=false; return; }
  if(a==ToggleShortcut) {
    if(v<0 || v>=11) return; auto old=shortcuts; unsigned index=0;
    while(index<shortcuts.count && shortcuts.ids[index]!=v) ++index;
    if(index<shortcuts.count) { for(unsigned i=index+1;i<shortcuts.count;++i) shortcuts.ids[i-1]=shortcuts.ids[i]; shortcuts.ids[--shortcuts.count]=0; }
    else if(shortcuts.count<8) shortcuts.ids[shortcuts.count++]=v;
    else { alert(tr("Maximum 8 custom shortcuts","最多可添加 8 个快捷入口"),true); return; }
    if(!saveHomeShortcuts()) { shortcuts=old; alert(tr("Shortcut save failed","快捷入口保存失败"),true); return; }
    shortcutPicker=false; scrollPositions[Home]=maxScroll(); return;
  }
  if(a==FactoryReset) { ask(tr("Erase identity, contacts and settings? SD files stay.","清空内部身份、联系人和设置？SD卡保留。"),FactoryReset); return; }
  if(a==AddChannel) { inputReturn=NewChannel; inputKind=v?PrivateChannelName:PublicChannelName; snprintf(input,sizeof(input),"%s",v?"":"#"); page=Input; return; }
  if(a==ExpandContact) { expandedContact=expandedContact==v?-1:v; if(expandedContact>=0 && the_mesh.getContactByIdx(v,selected) && radio_driver.isTxAllowed()) the_mesh.requestUITelemetry(selected); return; }
  if(a==ToggleFavorite) { ContactInfo ci; if(the_mesh.getContactByIdx(v,ci)) { auto* saved=the_mesh.lookupContactByPubKey(ci.id.pub_key,32); if(saved) { saved->flags^=1; the_mesh.persistUIContacts(); scrollPositions[page]=0; } } return; }
  if(a==RecordAudio) {
    if(audio.recording()) { audio.stop(); audio.scan(false); scrollPositions[Recordings]=0; }
    else if(!audio.record()) alert(audio.error(),true);
    return;
  }
  if(a==StopAudio) { audio.stop(); if(page!=AudioPlayer) { audio.scan(page==Music); scrollPositions[page]=0; } return; }
  if(a==PlayAudio) { if(!audio.play(v,page==Music)) alert(audio.error(),true); else page=AudioPlayer; return; }
  if(a==PlayerPause) { if(!audio.togglePause()) alert(audio.error(),true); return; }
  if(a==PlayerPrevious || a==PlayerNext) { if(!audio.step(a==PlayerNext?1:-1)) alert(audio.error(),true); return; }
  if(a==PlayerSeek) { if(!audio.seek(seeed::clamp((gesture.x-12)*1000/296,0,1000))) alert(audio.error(),true); return; }
  if(a==PlayerVolume) { audio.setVolume(seeed::clamp((gesture.x-100)*100/160,0,100)); return; }
  if(a==AudioVolume) { audio.setVolume(seeed::clamp(audio.volume()+v*10,0,100)); return; }
  if(a==SoundToggle) { prefs->buzzer_quiet=!prefs->buzzer_quiet; the_mesh.savePrefs(); if(!prefs->buzzer_quiet) audio.cue(L2Audio::Ack); return; }
  if(a==ManualContact) {
    inputReturn=Contacts; inputKind=ContactKey; input[0]=0; page=Input; return;
  }
  if(a==InputDone) {
    if(!input[0]||strlen(input)>inputCapacity()) return;
    if(inputKind==PublicChannelName || inputKind==PrivateChannelName) {
      if(inputKind==PublicChannelName) {
        if(input[0]!='#' || !input[1]) { alert(tr("Use a #channel name","请输入 #开头的频道名称"),true); return; }
        uint8_t key[32]; mesh::Utils::sha256(key,32,(const uint8_t*)input,strlen(input)); createChannel(input,key); return;
      }
      snprintf(newChannelName,sizeof(newChannelName),"%s",input); inputKind=ChannelKey; input[0]=0; return;
    }
    if(inputKind==ChannelKey) {
      uint8_t key[16]={};
      if(strlen(input)!=32) { alert(tr("Enter a 32-digit hexadecimal key","请输入 32 位十六进制密钥"),true); return; }
      for(int i=0;i<32;++i) { char c=input[i]; int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; if(digit<0) { alert(tr("Key must be hexadecimal","密钥只能包含十六进制字符"),true); return; } key[i/2]|=digit<<(i%2?0:4); }
      createChannel(newChannelName,key); memset(input,0,sizeof(input)); return;
    }
    if(inputKind==ContactKey) {
      if(strlen(input)!=64) { alert(tr("Enter all 64 hex characters.","请输入完整的 64 位十六进制公钥。"),true); return; }
      selected=ContactInfo{};
      for(int i=0;i<64;++i) {
        char c=input[i]; int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(digit<0) { alert(tr("Public key must be hexadecimal.","公钥只能包含十六进制字符。"),true); return; }
        selected.id.pub_key[i/2]|=uint8_t(digit)<<(i%2?0:4);
      }
      if(the_mesh.lookupContactByPubKey(selected.id.pub_key,32)) { alert(tr("Contact already saved.","联系人已存在。"),true); return; }
      selected.type=ADV_TYPE_CHAT; selected.out_path_len=OUT_PATH_UNKNOWN;
      char suffix[9]; memcpy(suffix,input+56,8); suffix[8]=0;
      inputKind=ContactName; snprintf(input,sizeof(input),"Peer %s",suffix); return;
    }
    if(inputKind==ContactName) {
      snprintf(selected.name,sizeof(selected.name),"%s",input);
      if(!the_mesh.addContact(selected)) { alert(tr("Contact storage full","联系人容量已满"),true); return; }
      the_mesh.persistUIContacts(); page=Contacts; listOffset=0;
      alert(tr("Contact saved","联系人已保存")); return;
    }
    if(onboarding) snprintf(draft.name,sizeof(draft.name),"%s",input);
    else { auto next=settings; snprintf(next.name,sizeof(next.name),"%s",input); if(!commitSettings(next)) return; snprintf(prefs->node_name,sizeof(prefs->node_name),"%s",input); the_mesh.savePrefs(); }
    page=inputReturn; return;
  }
  if(a==Key || a==Space) {
    char one[2]={char(a==Space?' ':v),0};
    if(page==Input) { if(strlen(input)<inputCapacity()) strcat(input,one); }
    else appendText(one); return;
  }
  if(a==DeleteChar) { eraseLast(page==Input?input:text); if(page!=Input) draftDirty=true; return; }
  if(a==Shift) { shift=!shift; return; }
  if(a==KeyboardMode) { symbols=!symbols; return; }
  if(a==ShowEmoji) { page=Emoji; return; }
  if(a==InsertEmoji) { appendText(emojis[v]); page=Composer; return; }
  if(a==Phrase) { if(page==Conversation) loadDraft(); page=Phrases; return; }
  if(a==ChoosePhrase) { appendText(phrases[v]); page=Composer; return; }
  if(a==OpenMessage) { selectedMessage=v; messageLine=0; scrollPositions[MessageDetail]=0; page=MessageDetail; return; }
  if(a==OpenChannel) { chooseChannel(v); return; }
  if(a==OpenContact) {
    if(the_mesh.getContactByIdx(v,selected)) { if(page==Contacts || page==ChatContacts) { chooseContact(selected); return; } selectedDiscovery=-1; detailReturn=page; detailOffset=listOffset; page=ContactDetail; }
    return;
  }
  if(a==OpenDiscovered && v>=0 && v<int(discoveredCount)) { selected=discovered[v]; selectedDiscovery=v; detailReturn=page; detailOffset=listOffset; page=ContactDetail; return; }
  if(a==Write) { chooseContact(selected); return; }
  if(a==AddDiscovered) {
    if(the_mesh.lookupContactByPubKey(selected.id.pub_key,32)) { alert(tr("Already saved","已保存此联系人")); return; }
    if(!the_mesh.addContact(selected)) { alert(tr("Contact storage full","联系人容量已满"),true); return; }
    // Public persistence API is supplied by the companion layer.
    the_mesh.persistUIContacts(); alert(tr("Contact saved","联系人已保存")); return;
  }
  if(a==Advert) { if(!radio_driver.isTxAllowed()) alert(tr("Confirm the radio preset first.","请先确认无线电预设。"),true); else alert(the_mesh.advert()?tr("Advert queued","设备信息已排队"):tr("Advert failed","广播失败")); return; }
  if(a==Send) { send(); return; }
  if(a==Gps) {
    if(onboarding) { draft.gps=!draft.gps; return; }
    bool on=!sensors.location().isEnabled();
    if(!sensors.location().setEnabled(on)) { alert(tr("GPS power write failed","GPS 电源写入失败"),true); return; }
    auto next=settings; next.gps=on;
    if(!commitSettings(next)) { sensors.location().setEnabled(!on); return; }
    prefs->gps_enabled=on; the_mesh.savePrefs(); return;
  }
  if(a==Brightness) {
    const uint8_t values[]={26,64,128,191,255}; int i=0; while(i<4 && settings.brightness>values[i]) ++i;
    i=std::min(4,std::max(0,i+v)); uint8_t old=settings.brightness;
    if(!display.setBrightness(values[i])) { alert(tr("Backlight write failed","背光写入失败"),true); return; }
    auto next=settings; next.brightness=values[i]; if(!commitSettings(next)) display.setBrightness(old); return;
  }
  if(a==Timezone) { auto next=settings; next.timezoneMinutes=next.timezoneMinutes>=840?-720:next.timezoneMinutes+60; commitSettings(next); return; }
  if(a==Bluetooth) {
  #ifdef BLE_PIN_CODE
    if(isBluetoothEnabled()) ask(tr("Disconnect and turn Bluetooth off?","断开连接并关闭蓝牙？"),Bluetooth);
    else { enableBluetooth(); alert(tr("Bluetooth enabled. PIN in About.","蓝牙已开启，配对码见关于。")); }
  #else
    alert(tr("Install the BLE build to pair a phone.","连接手机需要 BLE 版固件。"),true);
  #endif
    return;
  }
  if(a==Lock) { lockScreen(); return; }
  if(a==Reboot || a==Shutdown) { ask(a==Reboot?tr("Restart the device?","重启设备？"):tr("Sleep? Hold BOOT to wake.","进入休眠？按 BOOT 唤醒。"),a); return; }
  if(a==ConfirmNo) { page=confirmReturn; return; }
  if(a==ConfirmYes) {
    page=confirmReturn;
    if(confirmedAction==DeleteTrack) { if(l2map.deleteTrack(selectedTrack)) { page=Tracks; scrollPositions[Tracks]=0; } else alert(l2map.error(),true); }
    else if(confirmedAction==DeleteWaypoint) { if(selectedWaypoint>=0 && l2map.removeWaypoint(selectedWaypoint)) { page=Waypoints; scrollPositions[Waypoints]=0; } else alert(l2map.error(),true); }
    else if(confirmedAction==FactoryReset) { audio.stop(); l2map.stopTrack(); if(the_mesh.factoryResetUI()) board.reboot(); else alert(tr("Factory reset failed. Restart device.","恢复出厂失败，请重启设备。"),true); }
    else if(confirmedAction==Bluetooth) disableBluetooth();
    else if(confirmedAction==Reboot) { if(saveDraft()) { l2map.stopTrack(); audio.stop(); if(!prefs->buzzer_quiet) { audio.cue(L2Audio::Shutdown); uint32_t start=millis(); while(audio.busy() && millis()-start<2500) { audio.loop(); delay(1); } } board.reboot(); } }
    else if(confirmedAction==Shutdown) { if(!saveDraft()) return; l2map.stopTrack(); audio.stop(); if(!prefs->buzzer_quiet) { audio.cue(L2Audio::Shutdown); uint32_t start=millis(); while(audio.busy() && millis()-start<2500) { audio.loop(); delay(1); } } display.turnOff(); radio_driver.powerOff(); board.powerOff(); }
  }
}

bool UITask::belongs(const seeed::Message& m) const { return bool(m.channel)==channelTarget && !memcmp(m.target,target,32); }
void UITask::markRead() { for(unsigned i=0;i<local.size();++i) if(belongs(local.at(i))&&local.at(i).unread) { local.at(i).unread=0; local.update(local.at(i)); } }
void UITask::chooseChannel(int index) {
  ChannelDetails ch; if(!the_mesh.getChannel(index,ch)||!ch.name[0]) return;
  if(!saveDraft()) return;
  conversationReturn=page; conversationOffset=listOffset;
  channelTarget=true; channelIndex=index; channelIdentity(target,ch.channel);
  snprintf(conversationName,sizeof(conversationName),"%s",ch.name); listOffset=0; page=Conversation; scrollPositions[Conversation]=maxScroll(); markRead(); dirty=true;
}
void UITask::chooseContact(const ContactInfo& ci) {
  if(!saveDraft()) return;
  conversationReturn=page; conversationOffset=listOffset;
  channelTarget=false; memcpy(target,ci.id.pub_key,32);
  snprintf(conversationName,sizeof(conversationName),"%s",ci.name); listOffset=0; page=Conversation; scrollPositions[Conversation]=maxScroll(); markRead(); dirty=true;
}
unsigned UITask::textCapacity() const { return channelTarget?MAX_TEXT_LEN-std::min(unsigned(MAX_TEXT_LEN),unsigned(strlen(prefs->node_name)+2)):MAX_TEXT_LEN; }
void UITask::eraseLast(char* b) {
  size_t n=strlen(b); if(!n) return;
  do { --n; } while(n && (uint8_t(b[n])&0xC0)==0x80);
  // The supplied Warning emoji includes VS16; erase the base with its selector.
  if(uint8_t(b[n])==0xEF && uint8_t(b[n+1])==0xB8 && uint8_t(b[n+2])==0x8F && n) { do { --n; } while(n && (uint8_t(b[n])&0xC0)==0x80); }
  b[n]=0;
}
bool UITask::appendText(const char* s) {
  if(strlen(text)+strlen(s)>=sizeof(text)) { alert(tr("Draft buffer full","草稿容量已满"),true); return false; }
  strcat(text,s); draftDirty=true; return true;
}
bool UITask::saveDraft() {
  if(!draftDirty) return true;
  if(!local.saveDraft(target,channelTarget,text)) {
    alert(tr("Draft could not be saved","草稿保存失败"),true); return false;
  }
  draftDirty=false; return true;
}
void UITask::loadDraft() {
  if(!saveDraft()) return;
  local.loadDraft(target,channelTarget,text,sizeof(text)); draftDirty=false;
}
void UITask::send() {
  if(!text[0]||strlen(text)>textCapacity()||!radio_driver.isTxAllowed()) return;
  uint32_t timestamp=rtc_clock.getCurrentTimeUnique(),ack=0,timeout=0; bool ok=false;
  if(channelTarget) {
    ChannelDetails ch;
    uint8_t id[32];
    if(the_mesh.getChannel(channelIndex,ch)) {
      channelIdentity(id,ch.channel);
      if(!memcmp(id,target,32)) ok=the_mesh.sendGroupMessage(timestamp,ch.channel,prefs->node_name,text,strlen(text));
    }
  } else {
    auto* contact=the_mesh.lookupContactByPubKey(target,32);
    if(contact && contact->type==ADV_TYPE_CHAT) ok=the_mesh.sendUIMessage(*contact,timestamp,text,ack,timeout)!=MSG_SEND_FAILED;
  }
  if(!ok) { alert(tr("Send failed. Draft retained.","提交失败，草稿已保留。"),true); return; }
  text[0]=0; draftDirty=true; saveDraft(); page=Conversation; listOffset=0; scrollPositions[Conversation]=maxScroll();
}
void UITask::onMessageRecv(mesh::Packet* pkt,const ContactInfo& from,uint8_t,uint32_t stamp,const char* body) {
  notify(UIEventType::contactMessage);
  hearPeer(from,pkt->getSNR(),radio_driver.getLastRSSI());
  seeed::Message m{}; memcpy(m.target,from.id.pub_key,32);
  m.timestamp=stamp; m.unread=!(page==Conversation&&!channelTarget&&!memcmp(target,m.target,32));
  snprintf(m.sender,sizeof(m.sender),"%s",from.name); snprintf(m.text,sizeof(m.text),"%s",body);
  bool follow=page==Conversation && belongs(m) && scrollPositions[Conversation]>=maxScroll()-8;
  if(!local.add(m)) alert(tr("History could not be saved","历史保存失败"),true);
  if(follow) scrollPositions[Conversation]=maxScroll();
  if(page==Locked && lockedOffset>0 && m.unread) ++lockedOffset;
  dirty=true;
}
void UITask::onChannelMessageRecv(mesh::Packet*,ChannelDetails& ch,const char* body) {
  notify(UIEventType::channelMessage);
  seeed::Message m{}; m.channel=1; channelIdentity(m.target,ch.channel);
  m.timestamp=rtc_clock.getCurrentTime(); m.unread=!(page==Conversation&&channelTarget&&!memcmp(target,m.target,32));
  snprintf(m.sender,sizeof(m.sender),"%s",ch.name); snprintf(m.text,sizeof(m.text),"%s",body);
  bool follow=page==Conversation && belongs(m) && scrollPositions[Conversation]>=maxScroll()-8;
  if(!local.add(m)) alert(tr("History could not be saved","历史保存失败"),true);
  if(follow) scrollPositions[Conversation]=maxScroll();
  if(page==Locked && lockedOffset>0 && m.unread) ++lockedOffset;
  dirty=true;
}
void UITask::onDiscoveredContact(ContactInfo& ci,bool,uint8_t,const uint8_t*) {
  unsigned i=0; for(;i<discoveredCount;++i) if(!memcmp(discovered[i].id.pub_key,ci.id.pub_key,32)) break;
  if(i==16) { memmove(discovered,discovered+1,15*sizeof(ContactInfo)); memmove(discoveredAt,discoveredAt+1,15*sizeof(uint32_t)); i=15; }
  else if(i==discoveredCount) ++discoveredCount;
  discovered[i]=ci; discoveredAt[i]=millis(); hearPeer(ci,radio_driver.getLastSNR(),radio_driver.getLastRSSI()); dirty=true;
}
void UITask::onTextQueued(mesh::Packet* pkt,const ContactInfo* contact,const mesh::GroupChannel* channel,
                          uint32_t stamp,const char* body,uint32_t ack,uint32_t timeout) {
  if(!contact && !channel) return;
  seeed::Message m{}; m.outgoing=1; m.channel=channel!=nullptr;
  if(channel) channelIdentity(m.target,*channel); else memcpy(m.target,contact->id.pub_key,32);
  m.timestamp=stamp; m.ack=ack; m.deadline=millis()+(timeout?timeout:30000);
  m.state=seeed::MessageState::Queued;
  snprintf(m.sender,sizeof(m.sender),"%s",prefs->node_name); snprintf(m.text,sizeof(m.text),"%s",body);
  // Phone retries of the same message update one history entry.
  bool found=false, saved=false;
  for(unsigned i=0;i<local.size();++i) {
    auto& old=local.at(i);
    if(old.outgoing && old.timestamp==stamp && old.channel==m.channel &&
       !memcmp(old.target,m.target,32) && !strcmp(old.text,m.text)) {
      m.id=old.id; if(old.state==seeed::MessageState::Delivered) m.state=old.state;
      old=m; saved=local.update(old); found=true; break;
    }
  }
  if(!found) saved=local.add(m);
  Pending* slot=nullptr;
  for(auto& p:pending) if(p.id==m.id) { slot=&p; break; }
  if(!slot) for(auto& p:pending) if(!p.id) { slot=&p; break; }
  if(!slot) slot=&pending[0];
  pkt->calculatePacketHash(slot->hash); slot->id=m.id;
  if(!saved) alert(tr("Queued; history could not be saved.","已排队，但历史保存失败。"),true);
  dirty=true;
}
void UITask::onTextTransmit(mesh::Packet* pkt,bool success) {
  uint8_t hash[MAX_HASH_SIZE]; pkt->calculatePacketHash(hash);
  for(auto& p:pending) if(p.id && !memcmp(hash,p.hash,sizeof(hash))) {
    if(success && prefs && !prefs->buzzer_quiet) audio.cue(L2Audio::Sent);
    for(unsigned i=0;i<local.size();++i) {
      auto& m=local.at(i);
      if(m.id==p.id && m.state!=seeed::MessageState::Delivered) {
        m.state=success?seeed::MessageState::Transmitted:seeed::MessageState::Failed;
        if(!local.update(m)) alert(tr("History could not be saved","历史保存失败"),true);
        dirty=true; break;
      }
    }
    p.id=0;
  }
}
void UITask::onACKRecv(uint32_t ack) {
  if(ack) notify(UIEventType::ack);
  for(unsigned i=0;i<local.size();++i) {
    auto& m=local.at(i);
    if(m.outgoing&&!m.channel&&m.ack==ack&&ack&&m.state!=seeed::MessageState::Delivered) { m.state=seeed::MessageState::Delivered; local.update(m); dirty=true; }
  }
}
void UITask::loop() {
  audio.loop();
  updateScreenshot();
  l2map.loop(sensors.location());
  uint32_t now=millis();
  if((page==AudioPlayer || audio.recording()) && now-lastDraw>=100) dirty=true;
  if((page==Compass || page==GPSStatus || (page==Home && scrollPositions[Home]<88)) && now-lastDraw>=200) dirty=true;
  if(menuOpen && seeed::menuExpired(now,lastActivity,touchHeld)) { menuOpen=false; dirty=true; }
  if(!driver) { radio_driver.allowTransmit(false); return; }
  // External app radio changes require a fresh on-device confirmation.
  if(settings.confirmed && !onboarding && settings.preset>=0) {
    auto& p=seeed::presets[settings.preset];
    if(!radio_driver.isTxAllowed()||prefs->freq!=p.frequency||prefs->bw!=p.bandwidth||prefs->sf!=p.sf||prefs->cr!=p.cr||prefs->tx_power_dbm!=14||prefs->path_hash_mode!=p.pathBytes-1) {
      radio_driver.allowTransmit(false); settings.confirmed=0; draft=settings; page=Radio;
      alert(tr("Radio changed. Confirm the preset.","无线电配置已改变，请重新确认。"),true);
    }
  }
  for(unsigned i=0;i<local.size();++i) {
    auto& m=local.at(i);
    if((m.state==seeed::MessageState::Queued || (m.state==seeed::MessageState::Transmitted && !m.channel)) && int32_t(now-m.deadline)>=0) {
      m.state=seeed::MessageState::Unconfirmed; local.update(m); dirty=true;
    }
  }
  int event=user_btn.check();
  bool pressed=user_btn.isPressed(), pressEdge=pressed&&!buttonHeld;
  buttonHeld=pressed;
  bool pollTouch=int32_t(now-nextPoll)>=0;
  int x=touchX, y=touchY;
  bool down=lastTouchDown, wake=false;
  if(pollTouch) {
    nextPoll=now+25;
    down=lastTouchDown=display.touch(x,y);
    if(down) lastTouch=now;
    bool high=true;
    if(board.expRead(l2::Wake,high)) {
      bool raw=!high;
      if(raw!=wakeRaw) { wakeRaw=raw; wakeChangedAt=now; }
      if(raw!=wakeButtonHeld && now-wakeChangedAt>=40) {
        wakeButtonHeld=raw;
        // A touch must never become a screen-off side-button event.
        wake=raw && !down && now-lastTouch>180;
      }
    }
  }
  if(wake || pressEdge) {
    lastActivity=now;
    if(!display.isOn()) {
      display.turnOn(); dirty=true; displayWoke=true;
      user_btn.cancelClick();
    } else if(wake) {
      saveDraft();
      if(!onboarding && page!=Locked) lockScreen();
      display.turnOff(); displayWoke=true; dirty=true;
    }
  }
  if(event && !displayWoke && !onboarding && page!=Locked) {
    lastActivity=millis();
    if(event==BUTTON_EVENT_LONG_PRESS) back();
    else if(event==BUTTON_EVENT_CLICK) { menuOpen=!menuOpen; dirty=true; }
  }
  if(pollTouch) {
    if(down) {
      lastActivity=millis(); lastTouch=now; touchX=x; touchY=y;
      if(page==TouchTest) dirty=true;
      if(!display.isOn()) { display.turnOn(); displayWoke=true; dirty=true; }
      if(!touchHeld && !displayWoke) touchBegin(x,y);
      else if(touchHeld && !displayWoke) touchMove(x,y);
      touchHeld=true;
    } else {
      if(touchHeld && !displayWoke) touchEnd();
      touchHeld=false;
      if(!pressed && !wakeButtonHeld) displayWoke=false;
    }
  }
  if(touchHeld && !displayWoke && screenshotsEnabled && !screenshotTaken && !gesture.moved && seeed::captureHold(gesture.x0,gesture.y0,millis(),screenshotHoldAt)) {
    screenshotTaken=true; gestureHitValid=false;
    if(!capture.busy() && !capture.start(display)) alert(capture.error(),true);
  }
  // Actions above can set lastActivity with millis() AFTER
  // this loop's initial snapshot. Comparing that old snapshot to a newer
  // activity timestamp underflows uint32_t and immediately locks/sleeps.
  uint32_t idleNow=millis(), idleFor=idleNow-lastActivity;
  if(draftDirty && idleFor>1500 && int32_t(idleNow-nextDraftSave)>=0) {
    nextDraftSave=idleNow+5000; saveDraft();
  }
  if(!onboarding && page!=Locked && idleFor>60000) {
    lockScreen();
  }
  if(display.isOn()&&idleFor>30000) { saveDraft(); display.turnOff(); }
  // Keep progress smooth without flooding QSPI with redundant full-speed draws.
  if(now-lastDraw>=33 && (dirty||now-lastDraw>1000)) draw();
}
