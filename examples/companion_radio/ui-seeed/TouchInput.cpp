#include "UITask.h"
#include <algorithm>
#include <L2Audio.h>

void UITask::lockScreen() {
  saveDraft(); lockReturn=page; page=Locked;
  menuOpen=false; sliderDragging=false; sliderOffset=0; lockedOffset=0;
  gestureHitValid=false; dirty=true;
}
void UITask::touchBegin(int x,int y) {
  screenshotTaken=false; screenshotHoldAt=millis();
  gesture.begin(x,y); gesturePage=page; gestureMenu=menuOpen;
  gestureScroll=scrollPositions[page]; gestureHitValid=false; playerSlider=0;
  bool noticeActive=notice[0] && (noticeError || noticeSticky || int32_t(noticeUntil-millis())>0);
  gestureModal=(!menuOpen && (shortcutPicker || mapMenuOpen)) || noticeActive; shortcutGestureOffset=shortcutOffset;
  if(noticeActive) { gestureHit={0,0,320,240,Help,0}; gestureHitValid=true; return; }
  if(page==AudioPlayer && !menuOpen && !gestureModal) {
    if(x>=12 && x<=308 && y>=132 && y<154) { playerSlider=1; playerSeekPreview=seeed::clamp((x-12)*1000/296,0,1000); dirty=true; return; }
    if(x>=96 && x<268 && y>=198 && y<232) { playerSlider=2; audio.setVolume(seeed::clamp((x-100)*100/160,0,100),false); dirty=true; return; }
  }
  sliderDragging=page==Locked && gesture.startsSlider();
  if(page==Locked) { sliderOffset=0; dirty=true; return; }
  for(int i=int(hitCount)-1;i>=0;--i) {
    auto h=hits[i];
    if(x>=h.x && y>=h.y && x<h.x+h.w && y<h.y+h.h) { gestureHit=h; gestureHitValid=true; break; }
  }
}
void UITask::touchMove(int x,int y) {
  if(page!=gesturePage) { gestureHitValid=false; return; }
  gesture.update(x,y);
  if(gestureModal) { if(shortcutPicker && gesture.axis==seeed::TouchGesture::Vertical) { shortcutOffset=seeed::clamp(shortcutGestureOffset-gesture.dy()/34,0,7); dirty=true; } return; }
  if(playerSlider) {
    if(playerSlider==1) playerSeekPreview=seeed::clamp((x-12)*1000/296,0,1000);
    else audio.setVolume(seeed::clamp((x-100)*100/160,0,100),false);
    dirty=true; return;
  }
  if(page==Locked) {
    if(sliderDragging) {
      if(seeed::TouchGesture::magnitude(gesture.dy())>22) { sliderDragging=false; sliderOffset=0; }
      else sliderOffset=seeed::clamp(gesture.dx(),0,240);
      dirty=true;
    }
    return;
  }
  if(gestureMenu) return;
  if(navigationAllowed() && gesture.opensMenu()) { menuOpen=true; dirty=true; return; }
  if(menuOpen) return;
  if(gesture.axis==seeed::TouchGesture::Vertical) {
    scrollPositions[page]=seeed::clamp(gestureScroll-gesture.dy(),0,maxScroll()); dirty=true;
  }
}
void UITask::touchEnd() {
  if(screenshotTaken) { screenshotTaken=false; gestureHitValid=false; return; }
  if(page!=gesturePage) { sliderDragging=false; gestureHitValid=false; return; }
  if(gestureModal) {
    if(gesture.tap() && gestureHitValid) { auto h=gestureHit; if(gesture.x>=h.x && gesture.y>=h.y && gesture.x<h.x+h.w && gesture.y<h.y+h.h) handle(h.action,h.value); }
    gestureHitValid=false; gestureModal=false; return;
  }
  if(playerSlider) {
    int slider=playerSlider; playerSlider=0;
    if(slider==1) { if(!audio.seek(playerSeekPreview)) alert(audio.error(),true); }
    else audio.setVolume(audio.volume());
    gestureHitValid=false; dirty=true; return;
  }
  if(page==Locked) {
    if(sliderDragging && gesture.completesUnlock()) {
      page=lockReturn; lastActivity=millis();
    } else if(!sliderDragging && gesture.y0<184 && gesture.axis==seeed::TouchGesture::Vertical && seeed::TouchGesture::magnitude(gesture.dy())>=32) {
      int indices[10],count=unreadCards(indices);
      // Explicit lock-screen convention: swipe DOWN to inspect older unread cards.
      lockedOffset=seeed::clamp(lockedOffset+(gesture.dy()>0?2:-2),0,std::max(0,count-2));
    }
    sliderDragging=false; sliderOffset=0; gestureHitValid=false; dirty=true; return;
  }
  if(!gestureMenu && menuOpen) { menuOpen=gesture.opensMenu(); gestureHitValid=false; dirty=true; return; }
  if(gestureMenu) {
    if(gesture.axis==seeed::TouchGesture::Vertical && gesture.dy()>24) { menuOpen=false; dirty=true; return; }
    if(gesture.tap()) {
      menuOpen=false; dirty=true;
      if(gestureHitValid && gestureHit.action==Go) handle(gestureHit.action,gestureHit.value);
    }
    return;
  }
  if(navigationAllowed() && (page<=Settings || page==Audio) && gesture.pageSwipe()) {
    int next=seeed::nextMainPage(mainIndex(),gesture.dx()<0);
    navigate(mainPage(next));
    gestureHitValid=false; return;
  }
  if(gesture.tap() && gestureHitValid) {
    auto h=gestureHit;
    if(gesture.x>=h.x && gesture.y>=h.y && gesture.x<h.x+h.w && gesture.y<h.y+h.h) handle(h.action,h.value);
  }
  gestureHitValid=false;
}
