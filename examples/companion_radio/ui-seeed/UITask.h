#pragma once
#include "../AbstractUITask.h"
#include "LocalStore.h"
#include "TouchGesture.h"
#include <target.h>

class UITask : public AbstractUITask {
  enum Page { Home, Chats, Contacts, Location, Settings, Conversation, Composer,
    ContactDetail, Discover, Radio, Quick, Locked, About, SetupName, SetupRadio,
    SetupPrivacy, SetupFinish, Input, Emoji, Confirm, MessageDetail, Phrases, Diagnostics, TouchTest, Audio, PublicChannels, PrivateChannels, ChatContacts, BasicSettings, AdvancedSettings, Recordings, Music, NewChannel, AudioPlayer, Tracks, TrackDetail, Waypoints, WaypointDetail, Tiles, GPSStatus, Apps, Administration, Compass, ScreenshotSettings };
  enum Action { None, Go, Back, Scroll, ChoosePreset, OpenChannel, OpenContact,
    OpenDiscovered, AddDiscovered, Advert, Write, Send, Key, DeleteChar,
    Space, Shift, KeyboardMode, InputDone, Language, Gps, Brightness,
    Bluetooth, Lock, ApplyRadio, NextSetup, FinishSetup,
    Phrase, ShowEmoji, InsertEmoji, EditName, Timezone, Reboot, Shutdown,
    ConfirmYes, ConfirmNo, Help, OpenMessage, ChoosePhrase, ManualContact, ToggleFavorite, ExpandContact, AddChannel, RecordAudio, PlayAudio, StopAudio, AudioVolume, SoundToggle, PlayerPause, PlayerPrevious, PlayerNext, PlayerSeek, PlayerVolume, MapMenu, CloseMapMenu, MapPan, MapZoom, MapCenter, UnloadTrack, TrackRecord, OpenTrack, LoadTrack, DeleteTrack, AddWaypoint, OpenWaypoint, ShowWaypoint, DeleteWaypoint, ToggleTiles, RescanMap, ResetGps, ShowShortcuts, CloseShortcuts, ToggleShortcut, FactoryReset, ToggleScreenshots };
  struct Hit { int16_t x,y,w,h; Action action; int value; } hits[48];
  unsigned hitCount = 0;
  DisplayDriver* driver = nullptr;
  NodePrefs* prefs = nullptr;
  seeed::LocalStore local;
  seeed::Settings settings{}, draft{};
  Page page = Home, previous = Home, inputReturn = Home, confirmReturn = Home;
  Page navPage = Home, lockReturn = Home;
  Page detailReturn = Contacts, conversationReturn = Chats;
  int detailOffset = 0, conversationOffset = 0;
  int offsets[64] = {}, listOffset = 0, presetOffset = 0;
  bool dirty = true, onboarding = false, touchHeld = false, shift = false, symbols = false;
  bool displayWoke = false, wakeButtonHeld = false, buttonHeld = false;
  bool draftDirty = false, lastTouchDown = false, wakeRaw = false;
  enum InputKind { NodeName, ContactKey, ContactName, PublicChannelName, PrivateChannelName, ChannelKey } inputKind = NodeName;
  int messageLine = 0, messageLines = 0, touchX = -1, touchY = -1;
  struct Pending { uint8_t hash[MAX_HASH_SIZE] = {}; uint32_t id = 0; } pending[48];
  uint32_t lastDraw = 0, lastTouch = 0, lastActivity = 0;
  uint32_t nextPoll = 0, nextDraftSave = 0, wakeChangedAt = 0;
  bool channelTarget = true;
  int channelIndex = 0;
  uint8_t target[32] = {};
  ContactInfo selected{};
  char newChannelName[32]={};
  int expandedContact=-1;
  struct PeerState { uint8_t key[32]={}; uint32_t heard=0, batteryAt=0; float snr=0,rssi=0; uint16_t millivolts=0; bool used=false; } peers[350];
  PeerState* peerState(const uint8_t* key,bool create=false);
  void hearPeer(const ContactInfo& ci,float snr,float rssi);
  bool drawExtraPage();
  void drawContactList(bool chatOnly=false);
  int contactsHeight(bool chatOnly=false) const;
  int mainIndex() const;
  Page mainPage(int index) const;
  bool isPublicChannel(const ChannelDetails& ch) const;
  bool createChannel(const char* name,const uint8_t* key);
  void onPeerTelemetry(const ContactInfo& from,const uint8_t* bytes,uint8_t length) override;
  void notify(UIEventType t=UIEventType::none) override;
  ContactInfo discovered[16] = {};
  uint32_t discoveredAt[16] = {};
  unsigned discoveredCount = 0;
  int selectedDiscovery = -1;
  uint32_t selectedMessage = 0;
  Page phraseReturn = Composer;
  char conversationName[32] = "Public", text[193] = {}, input[65] = {};
  char notice[128] = {}, confirmText[96] = {};
  bool noticeError = false, noticeSticky=false;
  bool screenshotsEnabled=false,screenshotTaken=false;
  uint32_t screenshotHoldAt=0;
  void drawMakerClock(int y);
  void drawCompass();
  void updateScreenshot();
  uint32_t noticeUntil = 0;
  Action confirmedAction = None;
  bool menuOpen=false, sliderDragging=false, gestureMenu=false, gestureHitValid=false;
  Page parentPages[64]={};
  bool mapMenuOpen=false,shortcutPicker=false,gestureModal=false;
  int shortcutOffset=0,shortcutGestureOffset=0,selectedWaypoint=-1;
  char selectedTrack[64]={};
  struct HomeShortcuts { uint32_t magic=0x4c324801; uint8_t count=0,ids[8]={},reserved[3]={}; uint32_t crc=0; } shortcuts;
  bool rootPage() const;
  void drawBackButton();
  bool drawUtilityPage();
  void drawFloatingMenus();
  void loadHomeShortcuts();
  bool saveHomeShortcuts();
  const char* shortcutName(int id) const;
  Page shortcutPage(int id) const;
  int playerSlider=0,playerSeekPreview=0;
  int sliderOffset=0, lockedOffset=0, gestureScroll=0;
  int scrollPositions[64]={};
  int hitClipTop=0, hitClipBottom=240;
  Page gesturePage=Home;
  Hit gestureHit{};
  seeed::TouchGesture gesture;
  void selectFont(int size=1,bool bold=false);
  void beginContent(int top=34,int bottom=214);
  void endContent();
  bool drawMainPage();
  void drawLocked();
  void drawMap();
  void drawGestureHint();
  void touchBegin(int x,int y);
  void touchMove(int x,int y);
  void touchEnd();
  void lockScreen();
  bool navigationAllowed() const;
  int maxScroll() const;
  int unreadCards(int* indices) const;


  const char* tr(const char* en, const char* zh) const;
  void draw();
  void drawHeader(const char* title, const char* id, bool navigation = true);
  void drawNavigation();
  void label(int x,int y,const char* text,uint16_t color=0xFFFF,int size=1,int width=0,bool bold=false);
  int wrapped(int x,int y,int width,const char* text,int skip,int lines,uint16_t color=0xFFFF);
  const char* messageState(seeed::MessageState state) const;
  int itemCount() const;
  unsigned inputCapacity() const { return inputKind==ContactKey ? 64 : inputKind==ChannelKey ? 32 : 31; }
  void button(int x,int y,int w,int h,const char* text,Action action,int value=0,bool primary=false);
  void row(int y,const char* title,const char* detail,Action action,int value=0);
  void empty(const char* title,const char* detail);
  void handle(Action action,int value=0);
  void navigate(Page next);
  void back();
  void alert(const char* text,bool error=false);
  void ask(const char* text,Action action);
  bool commitSettings(const seeed::Settings& next);
  bool applyRadio(const seeed::Settings& next);
  void chooseChannel(int index);
  void chooseContact(const ContactInfo& contact);
  void send();
  unsigned textCapacity() const;
  bool belongs(const seeed::Message& message) const;
  void markRead();
  bool saveDraft();
  void loadDraft();
  void eraseLast(char* buffer);
  bool appendText(const char* value);
public:
  UITask(mesh::MainBoard* board,MultiSerialInterface* serial) : AbstractUITask(board,serial) {}
  void begin(DisplayDriver* display,SensorManager* sensors,NodePrefs* nodePrefs);
  void loop() override;
  void onMessageRecv(mesh::Packet* pkt,const ContactInfo& from,uint8_t txt_type,uint32_t timestamp,const char* text) override;
  void onChannelMessageRecv(mesh::Packet* pkt,ChannelDetails& channel,const char* text) override;
  void onQueueSizeChanged(int count) override { dirty=true; }
  void onDiscoveredContact(ContactInfo& contact,bool is_new,uint8_t path_len,const uint8_t* path) override;
  void onACKRecv(uint32_t ack) override;
  void onTextQueued(mesh::Packet* pkt,const ContactInfo* contact,const mesh::GroupChannel* channel,
                    uint32_t timestamp,const char* text,uint32_t ack,uint32_t timeout) override;
  void onTextTransmit(mesh::Packet* pkt,bool success) override;
};
