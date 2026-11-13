#include "L2Audio.h"
#include "L2Board.h"
#include "L2Pins.h"
#include "L2AudioFormat.h"
#include <SD_MMC.h>
#include <driver/i2s.h>
#include <AudioGeneratorMP3.h>

#include <AudioFileSourceFS.h>
#include <Codecs/es8311/ES8311.h>
#include <Codecs/es7243e/ES7243E.h>
#include <algorithm>
#include <new>
#include <Preferences.h>

extern L2Board board;
L2Audio audio;
namespace {
ES8311 dac;
ES7243E adc;
codec_config_t config(int hz) {
  codec_config_t c{}; c.input_device=ADC_INPUT_NONE; c.output_device=DAC_OUTPUT_ALL;
  c.i2s.mode=MODE_SLAVE; c.i2s.fmt=I2S_NORMAL; c.i2s.bits=BIT_LENGTH_16BITS;
  switch(hz) { case 8000:c.i2s.rate=RATE_8K;break; case 11025:c.i2s.rate=RATE_11K;break; case 16000:c.i2s.rate=RATE_16K;break; case 22050:c.i2s.rate=RATE_22K;break; case 24000:c.i2s.rate=RATE_24K;break; case 32000:c.i2s.rate=RATE_32K;break; case 48000:c.i2s.rate=RATE_48K;break; default:c.i2s.rate=RATE_44K; }
  return c;
}

}
bool L2Audio::mount() {
  if(sdReady) return true;
  if(!board.cardPresent()) { snprintf(failure,sizeof(failure),"SD card not inserted"); return false; }
  if(!board.expWrite(l2::CardPower,true)) { snprintf(failure,sizeof(failure),"SD power unavailable"); return false; }
  delay(30);
  SD_MMC.setPins(l2::SD_CLK,l2::SD_CMD,l2::SD_D0);
  // Never format a user's card. L2 uses SDMMC one-bit, not SPI SD.
  sdReady=SD_MMC.begin("/sdcard",true,false,SDMMC_FREQ_DEFAULT,5);
  if(!sdReady) snprintf(failure,sizeof(failure),"SD mount failed (not formatted)");
  else { failure[0]=0; recoverRecordings(); }
  return sdReady;
}
bool L2Audio::configure(int hz,bool input) {
  if(hz!=8000 && hz!=11025 && hz!=16000 && hz!=22050 && hz!=24000 && hz!=32000 && hz!=44100 && hz!=48000) { snprintf(failure,sizeof(failure),"Unsupported audio sample rate"); return false; }
  board.expWrite(l2::AmplifierPower,false);
  if(!input && inputMode) adc.ctrlStateActive(CODEC_MODE_ENCODE,false);
  if(i2sReady) { i2s_driver_uninstall(I2S_NUM_0); i2sReady=false; }
  i2s_config_t cfg{};
  cfg.mode=i2s_mode_t(I2S_MODE_MASTER | (input?I2S_MODE_RX:I2S_MODE_TX)); cfg.sample_rate=hz;
  cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format=input?I2S_CHANNEL_FMT_ONLY_RIGHT:I2S_CHANNEL_FMT_RIGHT_LEFT; cfg.communication_format=I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1; cfg.dma_buf_count=8; cfg.dma_buf_len=256;
  cfg.use_apll=true; cfg.tx_desc_auto_clear=true; cfg.fixed_mclk=hz*256; cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
  if(i2s_driver_install(I2S_NUM_0,&cfg,0,nullptr)!=ESP_OK) { snprintf(failure,sizeof(failure),"I2S driver unavailable"); return false; }
  i2sReady=true;
  i2s_pin_config_t pins{}; pins.mck_io_num=l2::AUDIO_MCLK; pins.bck_io_num=l2::AUDIO_BCLK; pins.ws_io_num=l2::AUDIO_WS; pins.data_out_num=input?I2S_PIN_NO_CHANGE:l2::AUDIO_OUT; pins.data_in_num=input?l2::AUDIO_IN:I2S_PIN_NO_CHANGE;
  if(i2s_set_pin(I2S_NUM_0,&pins)!=ESP_OK) { snprintf(failure,sizeof(failure),"I2S pin setup failed"); return false; }
  auto c=config(hz);
  if(input) {
    Wire.beginTransmission(0x14); if(Wire.endTransmission()!=0) { snprintf(failure,sizeof(failure),"ES7243E not detected"); return false; }
    // Keep the microphone reference powered during recording.
    auto reference=config(hz); reference.input_device=ADC_INPUT_LINE1;
    if(dac.init(&reference)!=RESULT_OK || dac.configI2S(CODEC_MODE_BOTH,&reference.i2s)!=RESULT_OK || dac.ctrlStateActive(CODEC_MODE_BOTH,true)!=RESULT_OK) { snprintf(failure,sizeof(failure),"Microphone reference power failed"); return false; }
    dac.setVoiceMute(true);
    c.input_device=ADC_INPUT_ALL; c.output_device=DAC_OUTPUT_NONE;
    if(adc.init(&c)!=RESULT_OK || adc.ctrlStateActive(CODEC_MODE_ENCODE,true)!=RESULT_OK) { snprintf(failure,sizeof(failure),"ES7243E microphone unavailable"); return false; }
    // ctrlStateActive() overwrites the init PGA gain with 0 dB. Restore the
    // L2's 16-bit right-slot input and 33 dB PGA setting after START.
    if(adc.writeReg(0x0B,0x0C)!=RESULT_OK || adc.writeReg(0x20,0x1B)!=RESULT_OK || adc.writeReg(0x21,0x1B)!=RESULT_OK) { snprintf(failure,sizeof(failure),"Microphone gain setup failed"); return false; }
    delay(200); // microphone reference/bias stabilization
    int16_t discard[256]; size_t got=0; for(int i=0;i<8;++i) if(i2s_read(I2S_NUM_0,discard,sizeof(discard),&got,0)!=ESP_OK || !got) break;
  } else {
    if(dac.init(&c)!=RESULT_OK || dac.configI2S(CODEC_MODE_DECODE,&c.i2s)!=RESULT_OK || dac.ctrlStateActive(CODEC_MODE_DECODE,true)!=RESULT_OK) { snprintf(failure,sizeof(failure),"ES8311 speaker unavailable"); return false; }
    dac.setVoiceVolume(75); dac.setVoiceMute(false); i2s_zero_dma_buffer(I2S_NUM_0);
    if(!board.expWrite(l2::AmplifierPower,true)) return false;
    delay(250); // Board amplifier rail settling; only on mode/rate changes.
  }
  rate=hz; inputMode=input; return true;
}
bool L2Audio::begin() {
  bool powered=board.expWrite(l2::CodecPower,true); delay(15);
  dac.setWire(&Wire); dac.setAddress(0x18); adc.setWire(&Wire); adc.setAddress(0x14);
  Wire.beginTransmission(0x18); codecReady=powered && Wire.endTransmission()==0;
  if(codecReady) codecReady=configure(16000,false);
  if(!codecReady) snprintf(failure,sizeof(failure),"Audio codec unavailable");
  bool sd=mount();
  Serial.printf("L2 audio: speaker=%s SD=%s volume=%d\n",codecReady?"ready":"failed",sd?"mounted":"unavailable",level);
  Preferences prefs; if(prefs.begin("l2-audio",true)) { level=std::min(100,int(prefs.getUChar("volume",35))); prefs.getString("latest",lastRecording,sizeof(lastRecording)); prefs.end(); }
  return codecReady;
}
bool L2Audio::setRate(int hz) {
  if(!codecReady) return false;
  if(hz!=rate && rate>0 && playing()) { playedBefore+=uint64_t(playedFrames)*1000/rate; playedFrames=0; }
  return (i2sReady && !inputMode && hz==rate) || configure(hz,false);
}
void L2Audio::setVolume(int value,bool persist) { level=std::max(0,std::min(100,value)); if(!persist) return; Preferences prefs; if(prefs.begin("l2-audio",false)) { prefs.putUChar("volume",level); prefs.end(); } }
bool L2Audio::writeSample(const int16_t* stereo) {
  if(!i2sReady) return false;
  int32_t mono=l2::mixMedia(stereo[0],stereo[1]);
  int16_t scaled[2]={int16_t(mono*level/100),int16_t(mono*level/100)};
  size_t written=0; i2s_write(I2S_NUM_0,scaled,sizeof(scaled),&written,0); if(written==sizeof(scaled)) { ++pcmFrames; if(decoder && !pausedNow) ++playedFrames; measure(scaled[0]); } return written==sizeof(scaled);
}
void L2Audio::cue(Cue sound) {
  // Exact pitches and durations from MeshCore's RTTTL startup/shutdown and
  // ui-orig message/ACK definitions. Active local playback/recording wins.
  if(!codecReady || recordingNow || decoder || wavActive || noteCount || tailUntil) return;
  if(!setRate(16000)) return;
  if(sound==Startup) { notes[0]={1047,94}; notes[1]={1319,94}; notes[2]={1568,188}; noteCount=3; }
  else if(sound==Shutdown) { notes[0]={784,300}; notes[1]={659,150}; notes[2]={523,150}; noteCount=3; }
  else if(sound==PrivateMessage) { notes[0]={1319,38}; notes[1]={1568,38}; notes[2]={1976,38}; notes[3]={2093,75}; noteCount=4; }
  else if(sound==ChannelMessage) { notes[0]={1661,63}; notes[1]={1109,63}; noteCount=2; }
  else { notes[0]={4186,63}; noteCount=1; }
  noteIndex=noteSample=phase=0;
}
bool L2Audio::scan(bool music) {
  count=0; listMusic=music; if(!mount()) return false;
  fs::File dir=SD_MMC.open(music?"/music":"/record");
  if(!dir || !dir.isDirectory()) { if(dir) dir.close(); snprintf(failure,sizeof(failure),music?"No /music directory":"No recordings yet"); return true; }
  while(true) {
    fs::File file=dir.openNextFile(); if(!file) break;
    String name=file.name(); bool isDir=file.isDirectory(); file.close();
    int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
    String lower=name; lower.toLowerCase();
    if(!isDir && lower.endsWith(music?".mp3":".wav") && name.length()<sizeof(files[0].name) && count<128) snprintf(files[count++].name,sizeof(files[0].name),"%s",name.c_str());
    yield();
  }
  dir.close(); std::sort(files,files+count,[&](const FileEntry& a,const FileEntry& b) { bool latestA=!music && !strcmp(a.name,lastRecording),latestB=!music && !strcmp(b.name,lastRecording); return latestA!=latestB?latestA:strcmp(a.name,b.name)<0; });
  failure[0]=0; return true;
}
bool L2Audio::findMp3(fs::File& file,uint32_t from,uint32_t& offset,l2::Mp3Info& info) {
  if(from>=file.size()) return false;
  uint8_t bytes[516]; uint32_t limit=from+std::min(size_t(8192),file.size()-from);
  for(uint32_t at=from;at+4<=limit;at+=512) {
    if(!file.seek(at)) return false;
    int count=file.read(bytes,std::min(size_t(516),size_t(limit-at)));
    for(int i=0;i+4<=count && i<512;++i) if(l2::mp3Header(bytes+i,info)) { offset=at+i; return true; }
  }
  return false;
}
bool L2Audio::play(int index,bool music) {
  if(index<0 || index>=count || music!=listMusic || recordingNow) return false;
  char path[128]; snprintf(path,sizeof(path),"/%s/%s",music?"music":"record",files[index].name);
  return loadTrack(path,music);
}
bool L2Audio::loadTrack(const char* path,bool music,unsigned permille) {
  char savedPath[128]; snprintf(savedPath,sizeof(savedPath),"%s",path);
  stop(); if(!mount() || !codecReady) return false;
  fs::File file=SD_MMC.open(savedPath,FILE_READ);
  if(!file || file.isDirectory()) { snprintf(failure,sizeof(failure),"Audio file could not be opened"); return false; }
  trackSize=file.size(); trackDuration=trackStart=0; playedFrames=playedBefore=0; trackRawPeak=0; wavePlaybackGain=256;
  trackMusic=music; approximateDuration=false; pcmCount=pcmSent=0;
  snprintf(trackPath,sizeof(trackPath),"%s",savedPath);
  const char* name=strrchr(savedPath,'/'); snprintf(activeName,sizeof(activeName),"%s",name?name+1:savedPath);
  uint32_t targetRate=16000,position=0;
  if(music) {
    uint8_t id3[10]; uint32_t from=0;
    if(file.read(id3,10)==10 && !memcmp(id3,"ID3",3)) {
      for(int i=6;i<10;++i) { if(id3[i]&0x80) { snprintf(failure,sizeof(failure),"Invalid MP3 tag"); return false; } from=(from<<7)|id3[i]; }
      from+=10+(id3[5]&0x10?10:0);
    }
    l2::Mp3Info info; uint32_t first=0;
    if(!findMp3(file,from,first,info)) { snprintf(failure,sizeof(failure),"No supported MP3 frame"); return false; }
    targetRate=info.rate; trackStart=first;
    trackDuration=uint64_t(trackSize-first)*8000/info.bitrate; approximateDuration=true;
    uint32_t xingOffset=first+4+(info.crc?2:0)+(info.mpeg1?(info.mono?17:32):(info.mono?9:17));
    uint8_t xing[12];
    if(file.seek(xingOffset) && file.read(xing,sizeof(xing))==sizeof(xing) && (!memcmp(xing,"Xing",4)||!memcmp(xing,"Info",4)) && (xing[7]&1)) {
      uint32_t frames=uint32_t(xing[8])<<24|uint32_t(xing[9])<<16|uint32_t(xing[10])<<8|xing[11];
      if(frames) { trackDuration=uint64_t(frames)*info.samples*1000/info.rate; approximateDuration=false; }
    }
    position=first;
    if(permille) { uint32_t candidate=first+uint64_t(trackSize-first)*std::min(permille,999U)/1000; if(!findMp3(file,candidate,position,info)) { snprintf(failure,sizeof(failure),"Cannot seek this MP3"); return false; } }
  } else {
    auto reader=[&](uint32_t offset,uint8_t* data,size_t length) { return file.seek(offset) && file.read(data,length)==int(length); };
    if(!l2::readWave(file.size(),reader,waveInfo)) { snprintf(failure,sizeof(failure),"WAV must contain valid 16-bit PCM"); return false; }
    targetRate=waveInfo.rate; trackStart=waveInfo.offset;
    trackDuration=uint64_t(waveInfo.bytes)*1000/(waveInfo.rate*waveInfo.align);
    uint32_t frame=uint64_t(waveInfo.bytes/waveInfo.align)*std::min(permille,999U)/1000;
    wavSent=frame*waveInfo.align; position=waveInfo.offset+wavSent;
  }
  file.close();
  if(!setRate(targetRate) || !output.SetRate(targetRate)) return false;
  i2s_zero_dma_buffer(I2S_NUM_0); i2s_start(I2S_NUM_0);
  playedBefore=uint64_t(trackDuration)*std::min(permille,999U)/1000; playedFrames=0;
  if(music) {
    source=new(std::nothrow) AudioFileSourceFS(SD_MMC,savedPath);
    if(!source || !source->isOpen() || !source->seek(position,SEEK_SET)) { stop(); snprintf(failure,sizeof(failure),"MP3 source could not be opened"); return false; }
    decoder=new(std::nothrow) AudioGeneratorMP3();
    if(!decoder || !decoder->begin(source,&output)) { stop(); snprintf(failure,sizeof(failure),"MP3 decoder could not start"); return false; }
  } else {
    wavFile=SD_MMC.open(savedPath,FILE_READ);
    if(!wavFile || !wavFile.seek(position)) { stop(); snprintf(failure,sizeof(failure),"WAV source could not be opened"); return false; }
    wavActive=true;
  }
  failure[0]=0; return true;
}
bool L2Audio::togglePause() {
  if(!decoder && !wavActive) return hasTrack()?loadTrack(trackPath,trackMusic):false;
  pausedNow=!pausedNow;
  if(pausedNow) i2s_stop(I2S_NUM_0); else i2s_start(I2S_NUM_0);
  return true;
}
bool L2Audio::step(int direction) {
  if(!hasTrack()) return false;
  char name[96]; snprintf(name,sizeof(name),"%s",activeName); bool music=trackMusic;
  if(!scan(music) || !count) return false;
  int current=0; for(int i=0;i<count;++i) if(!strcmp(name,files[i].name)) { current=i; break; }
  return play(l2::mediaTrack(current,direction,count),music);
}
bool L2Audio::seek(unsigned permille) {
  if(!hasTrack()) return false;
  bool wasPaused=pausedNow;
  if(!loadTrack(trackPath,trackMusic,std::min(permille,999U))) return false;
  if(wasPaused) { pausedNow=true; i2s_stop(I2S_NUM_0); }
  return true;
}
uint32_t L2Audio::elapsedMs() const { return playedBefore+(rate>0?uint64_t(playedFrames)*1000/rate:0); }
unsigned L2Audio::progress() const {
  if(!trackDuration) return 0;
  return std::min(1000U,unsigned(uint64_t(elapsedMs())*1000/trackDuration));
}
void L2Audio::measure(int16_t sample) {
  outputPeak=std::max(outputPeak,uint16_t(abs(int(sample))));
  if(++waveEvery>=unsigned(std::max(1,rate/640))) { waveEvery=0; waveBins[waveBin++%32]=sample; }
}
void L2Audio::finishTrack() {
  if(decoder) { decoder->stop(); delete decoder; decoder=nullptr; }
  if(source) { source->close(); delete source; source=nullptr; }
  if(wavFile) wavFile.close(); wavActive=false; pausedNow=false; tailUntil=millis()+140;
}
void L2Audio::waveLoop() {
  if(pcmSent>=pcmCount) {
    if(wavSent>=waveInfo.bytes) { finishTrack(); return; }
    uint8_t bytes[1024]; int16_t mono[256];
    size_t frames=std::min(size_t(256),size_t((waveInfo.bytes-wavSent)/waveInfo.align));
    size_t required=frames*waveInfo.align; int got=wavFile.read(bytes,required);
    if(got!=int(required)) { finishTrack(); snprintf(failure,sizeof(failure),"WAV read failed"); return; }
    for(size_t i=0;i<frames;++i) {
      int16_t left=int16_t(l2::media16(bytes+i*waveInfo.align));
      mono[i]=waveInfo.channels==2?l2::mixMedia(left,int16_t(l2::media16(bytes+i*waveInfo.align+2))):left;
      if(playedFrames>unsigned(rate/4)) trackRawPeak=std::max(trackRawPeak,uint16_t(abs(int(mono[i]))));
    }
    // Local voice recordings can be much quieter than mastered MP3 tracks.
    // Apply the tested bounded level control during WAV playback as well,
    // without modifying the file or the MP3 path. Digital silence stays silent.
    l2::normalizeRecording(mono,frames,wavePlaybackGain);
    for(size_t i=0;i<frames;++i) pcmBuffer[i*2]=pcmBuffer[i*2+1]=int16_t(int32_t(mono[i])*level/100);
    pcmCount=frames*2; pcmSent=0;
  }
  size_t written=0; i2s_write(I2S_NUM_0,pcmBuffer+pcmSent,(pcmCount-pcmSent)*2,&written,0);
  size_t samples=written/2,frames=samples/2;
  for(size_t i=0;i<frames;++i) measure(pcmBuffer[pcmSent+i*2]);
  pcmSent+=frames*2; wavSent+=frames*waveInfo.align; playedFrames+=frames; pcmFrames+=frames;
}
bool L2Audio::writeHeader() {
  if(!recordFile) return false;
  uint8_t header[44]; l2::recordingHeader(header,recordedBytes);
  bool ok=recordFile.seek(0) && recordFile.write(header,sizeof(header))==sizeof(header) && recordFile.seek(44+recordedBytes);
  recordFile.flush(); return ok;
}
bool L2Audio::record() {
  if(busy()) return false;
  if(!mount()) return false;
  if(!codecReady) { snprintf(failure,sizeof(failure),"Audio codec unavailable"); return false; }
  if(!SD_MMC.exists("/record") && !SD_MMC.mkdir("/record")) { snprintf(failure,sizeof(failure),"Cannot create /record"); return false; }
  if(SD_MMC.totalBytes()-SD_MMC.usedBytes()<1024*1024) { snprintf(failure,sizeof(failure),"SD has less than 1 MB free"); return false; }
  if(!configure(16000,true)) return false;
  for(int attempt=0;attempt<8;++attempt) { snprintf(recordPath,sizeof(recordPath),"/record/rec-%08lx.wav.part",(unsigned long)esp_random()); if(!SD_MMC.exists(recordPath)) break; }
  if(SD_MMC.exists(recordPath)) return false;
  recordFile=SD_MMC.open(recordPath,FILE_WRITE); recordedBytes=0; recordGainQ8=256; recordPeak=recordPeakL=recordPeakR=0;
  if(!recordFile || !writeHeader()) { if(recordFile) recordFile.close(); snprintf(failure,sizeof(failure),"Cannot write recording header"); return false; }
  recordingNow=true; headerAt=millis(); failure[0]=0; return true;
}
void L2Audio::stop() {
  noteCount=0; tailUntil=0; pausedNow=false;
  if(i2sReady) i2s_start(I2S_NUM_0);
  if(wavFile) wavFile.close(); wavActive=false; pcmCount=pcmSent=0;
  if(decoder) { decoder->stop(); delete decoder; decoder=nullptr; }
  if(source) { source->close(); delete source; source=nullptr; }
  if(recordingNow) {
    recordingNow=false; bool ok=writeHeader(); recordFile.close();
    char finalPath[128]; snprintf(finalPath,sizeof(finalPath),"%s",recordPath); finalPath[strlen(finalPath)-5]=0;
    if(!ok || !SD_MMC.rename(recordPath,finalPath)) snprintf(failure,sizeof(failure),"Recording save failed; .part retained");
    else {
      const char* name=strrchr(finalPath,'/'); snprintf(lastRecording,sizeof(lastRecording),"%s",name?name+1:finalPath);
      Preferences prefs; if(prefs.begin("l2-audio",false)) { prefs.putString("latest",lastRecording); prefs.end(); }
    }
    adc.ctrlStateActive(CODEC_MODE_ENCODE,false); rate=0;
  }
  if(i2sReady) i2s_zero_dma_buffer(I2S_NUM_0);
}
void L2Audio::recoverRecordings() {
  fs::File dir=SD_MMC.open("/record"); if(!dir || !dir.isDirectory()) return;
  while(true) {
    fs::File f=dir.openNextFile(); if(!f) break;
    String path=f.path(); uint32_t size=f.size(); uint8_t header[44]; bool valid=!f.isDirectory() && path.endsWith(".wav.part") && size>=44 && f.read(header,44)==44;
    f.close();
    if(!valid || !l2::recoverableRecording(header)) continue;
    String final=path.substring(0,path.length()-5); if(SD_MMC.exists(final)) continue;
    f=SD_MMC.open(path,"r+"); if(!f) continue;
    l2::recordingHeader(header,(size-44)&~1U); bool ok=f.write(header,44)==44; f.flush(); f.close();
    if(ok) SD_MMC.rename(path,final);
  }
  dir.close();
}
void L2Audio::loop() {
  uint32_t now=millis();
  if(!playing() || pausedNow) outputPeak=outputPeak>100?outputPeak-100:0;
  if(tailUntil && int32_t(now-tailUntil)>=0) tailUntil=0;
  if(sdReady && uint32_t(now-cardCheckAt)>=500) {
    cardCheckAt=now;
    if(!board.cardPresent()) { stop(); SD_MMC.end(); sdReady=false; count=0; snprintf(failure,sizeof(failure),"SD card removed"); }
  }
  if(recordingNow) {
    int16_t mono[256]; size_t got=0;
    if(i2s_read(I2S_NUM_0,mono,sizeof(mono),&got,0)!=ESP_OK) return;
    size_t frames=got/2;
    // Hardware selects the physically connected RIGHT microphone slot.
    for(size_t i=0;i<frames;++i) recordPeakR=std::max(recordPeakR,uint16_t(abs(int(mono[i]))));
    inputPeak=0; for(size_t i=0;i<frames;++i) inputPeak=std::max(inputPeak,uint16_t(abs(int(mono[i]))));
    l2::normalizeRecording(mono,frames,recordGainQ8);
    inputPeak=0; for(size_t i=0;i<frames;++i) { recordPeak=std::max(recordPeak,uint16_t(abs(int(mono[i])))); inputPeak=std::max(inputPeak,uint16_t(abs(int(mono[i])))); }
    if(frames) {
      size_t size=frames*2,written=recordFile.write(reinterpret_cast<uint8_t*>(mono),size); recordedBytes+=written;
      if(written!=size) { stop(); snprintf(failure,sizeof(failure),"SD recording write failed"); return; }
    }
    if(uint32_t(now-headerAt)>=1000) { headerAt=now; if(!writeHeader()) { stop(); snprintf(failure,sizeof(failure),"Recording checkpoint failed"); } }
    if(recordedBytes>=32000U*60*30) stop(); // Bound one WAV to 30 minutes.
  } else if(wavActive && !pausedNow) {
    waveLoop();
  } else if(decoder && !pausedNow) {
    if(!decoder->loop()) finishTrack();
  } else if(noteCount) {
    for(int n=0;n<384;++n) {
      auto note=notes[noteIndex]; uint32_t total=uint32_t(note.ms)*16;
      int16_t sample[2]; int16_t value=noteSample*10<total*9?(phase<8000?6500:-6500):0; sample[0]=sample[1]=value;
      if(!writeSample(sample)) break;
      phase=(phase+note.hz)%16000;
      if(++noteSample>=total) { noteSample=phase=0; if(++noteIndex>=noteCount) { noteCount=0; tailUntil=millis()+140; break; } }
    }
  }
}

size_t L2Audio::diagnostics(char* output,size_t capacity) const {
  if(!capacity) return 0;
  int n=snprintf(output,capacity,"codec=%u sd=%u i2s=%u hz=%d rec=%u play=%u files=%d volume=%d pcm=%lu rec_bytes=%lu peak=%u L=%u R=%u gain=%lu err=%s",codecReady,sdReady,i2sReady,rate,recording(),playing(),count,level,(unsigned long)pcmFrames,(unsigned long)recordedBytes,recordPeak,recordPeakL,recordPeakR,(unsigned long)(recordGainQ8/256),failure);
  return n>0?std::min(capacity-1,size_t(n)):0;
}

size_t L2Audio::readRecording(unsigned index,uint32_t offset,uint8_t* dest,size_t length,uint32_t& size) {
  size=0; if(!sdReady || recordingNow || listMusic || index>=unsigned(count)) return 0;
  char path[128]; snprintf(path,sizeof(path),"/record/%s",files[index].name);
  File file=SD_MMC.open(path,FILE_READ); if(!file) return 0;
  size=file.size(); if(offset>=size || !file.seek(offset)) return 0;
  int got=file.read(dest,std::min(length,size_t(size-offset))); return got>0?size_t(got):0;
}
