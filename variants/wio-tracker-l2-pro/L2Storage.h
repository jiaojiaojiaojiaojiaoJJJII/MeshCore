#pragma once
#include <FS.h>
#include <helpers/ui/DisplayDriver.h>

// DataStore keeps a reference to this stable FS object before setup().
// Attach the mounted backend later, without replacing that reference.
class L2FileSystem : public fs::FS {
public:
  enum Backend { None, Spiffs, Little };
private:
  Backend backend = None;
  void attach(fs::FS& source, Backend kind) {
    fs::FS::operator=(source); backend = kind;
  }
public:
  L2FileSystem() : fs::FS(fs::FSImplPtr()) {}
  bool begin();
  bool format();
  size_t totalBytes();
  size_t usedBytes();
  const char* typeName() const { return backend==Little ? "LittleFS" : backend==Spiffs ? "SPIFFS" : "unmounted"; }
};
extern L2FileSystem l2FS;
bool l2BeginStorage();
void l2StorageRecovery(DisplayDriver* screen);
