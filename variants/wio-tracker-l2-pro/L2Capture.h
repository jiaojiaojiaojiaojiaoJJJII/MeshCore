#pragma once
#include <FS.h>
class L2Display;
class L2Capture {
  fs::File file;
  uint8_t* snapshot=nullptr;
  unsigned row=0;
  char destination[64]={},partial[72]={},failure[96]={};
  int finish(bool ok);
public:
  bool busy() const { return snapshot!=nullptr; }
  bool start(L2Display& display);
  int loop(); // 1 saved, -1 failed, 0 idle/in progress
  const char* error() const { return failure; }
  const char* path() const { return destination; }
};
extern L2Capture capture;
