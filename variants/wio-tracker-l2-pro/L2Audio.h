#pragma once
#include <Arduino.h>
#include <FS.h>
#include <AudioOutput.h>
#include "L2Media.h"
class AudioGenerator;
class AudioFileSourceFS;

class L2Audio {
public:
  enum Cue { Startup, Shutdown, PrivateMessage, ChannelMessage, Ack, Sent };
  bool begin();
  size_t diagnostics(char* output,size_t capacity) const;
  size_t readRecording(unsigned index,uint32_t offset,uint8_t* dest,size_t length,uint32_t& size);
  void loop();
  void cue(Cue sound);
  bool record();
  bool play(int index,bool music);
  void stop();
  bool togglePause();
  bool step(int direction);
  bool seek(unsigned permille);
  bool paused() const { return pausedNow; }
  bool hasTrack() const { return trackPath[0]; }
  bool musicTrack() const { return trackMusic; }
  const char* trackName() const { return activeName; }
  uint32_t elapsedMs() const;
  uint32_t durationMs() const { return trackDuration; }
  bool durationApproximate() const { return approximateDuration; }
  unsigned progress() const;
  const int16_t* waveform() const { return waveBins; }
  bool nearSilent() const { return !trackMusic && rate>0 && playedFrames>unsigned(rate*3/2) && trackRawPeak<4; }
  unsigned outputLevel() const { return outputPeak*100U/32768; }
  unsigned inputLevel() const { return inputPeak*100U/32768; }
  uint32_t recordingMs() const { return recordedBytes/32; }
  bool scan(bool music);
  bool ensureStorage() { return mount(); }
  bool mounted() const { return sdReady; }
  bool recording() const { return recordingNow; }
  bool playing() const { return decoder!=nullptr || wavActive || tailUntil; }
  bool busy() const { return recordingNow || decoder || wavActive || noteCount || tailUntil; }
  int fileCount() const { return count; }
  const char* fileName(int index) const { return index>=0 && index<count?files[index].name:""; }
  bool isLatest(int index) const { return index>=0 && index<count && !strcmp(files[index].name,lastRecording); }
  const char* error() const { return failure; }
  int volume() const { return level; }
  void setVolume(int value,bool persist=true);
  bool setRate(int rate);
  bool writeSample(const int16_t* stereo);
private:
  bool sdReady=false,codecReady=false,i2sReady=false,recordingNow=false,listMusic=false;
  int rate=0,level=35,count=0;
  struct FileEntry { char name[96]={}; } files[128];
  char lastRecording[96]={},recordPath[128]={},failure[96]="SD unavailable";
  uint32_t tailUntil=0;
  fs::File recordFile;
  uint32_t recordedBytes=0,headerAt=0,cardCheckAt=0,pcmFrames=0;
  uint32_t recordGainQ8=256;
  uint16_t recordPeak=0,recordPeakL=0,recordPeakR=0;
  struct Note { uint16_t hz,ms; } notes[4];
  unsigned noteCount=0,noteIndex=0,noteSample=0;
  uint32_t phase=0;
  AudioGenerator* decoder=nullptr;
  AudioFileSourceFS* source=nullptr;
  class Output : public AudioOutput {
    L2Audio& owner;
  public:
    explicit Output(L2Audio& audio):owner(audio) { hertz=16000; bps=16; channels=2; SetGain(1); }
    bool SetRate(int hz) override { hertz=hz; return owner.setRate(hz); }
    bool begin() override { return owner.setRate(hertz); }
    bool stop() override { return true; }
    bool ConsumeSample(int16_t sample[2]) override { int16_t stereo[2]={sample[0],sample[1]}; MakeSampleStereo16(stereo); return owner.writeSample(stereo); }
  } output{*this};
  fs::File wavFile;
  l2::WaveInfo waveInfo{};
  bool wavActive=false,pausedNow=false,trackMusic=false,approximateDuration=false,inputMode=false;
  char activeName[96]={},trackPath[128]={};
  uint32_t trackDuration=0,trackStart=0,trackSize=0,wavSent=0,playedFrames=0,playedBefore=0;
  uint16_t outputPeak=0,inputPeak=0,trackRawPeak=0;
  uint32_t wavePlaybackGain=256;
  int16_t waveBins[32]={}; unsigned waveBin=0,waveEvery=0;
  int16_t pcmBuffer[512]={}; size_t pcmCount=0,pcmSent=0;
  bool loadTrack(const char* path,bool music,unsigned permille=0);
  bool findMp3(fs::File& file,uint32_t from,uint32_t& offset,l2::Mp3Info& info);
  void waveLoop();
  void measure(int16_t sample);
  void finishTrack();
  bool mount();
  bool configure(int hz,bool input);
  bool writeHeader();
  void recoverRecordings();
};
extern L2Audio audio;
