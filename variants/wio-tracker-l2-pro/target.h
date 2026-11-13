#pragma once
#include <RadioLib.h>
#include "L2Board.h"
#include "L2Display.h"
#include "L2Radio.h"
#include "L2Sensors.h"
#include "L2Storage.h"
#include <helpers/ui/MomentaryButton.h>
extern L2Board board;
extern L2RadioWrapper radio_driver;
extern ESP32RTCClock rtc_clock;
extern L2Sensors sensors;
extern L2Display display;
extern MomentaryButton user_btn;
bool radio_init();
mesh::LocalIdentity radio_new_identity();
void l2ResetUserSettings();
