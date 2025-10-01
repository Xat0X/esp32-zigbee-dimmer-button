#pragma once
#include <Arduino.h>
#include "config.h"

// Globale dimmerstatus die ook door web/mqtt wordt gebruikt (Channel A)
extern int  brightness;           // current visual brightness (fading towards target)
extern int  dimTargetBrightness;  // target brightness to fade to
extern bool lightOn;              // ON/OFF state

// Init & loop (Channel A)
void buttonSetup(const ChannelConfig* ch_config);
void buttonLoop(const ChannelConfig* ch_config);

// Shared helpers (exposed for MQTT/web) for Channel A
void applyTargetBrightness(int newTarget, bool forceOn = false, bool forceSend = false);
void dimmerBeginPowerTransition();