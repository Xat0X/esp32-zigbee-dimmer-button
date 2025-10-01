#pragma once
#include <Arduino.h>
#include "config.h"

// Globale dimmerstatus die ook door web/mqtt wordt gebruikt (Channel B)
extern int  brightness2;           // current visual brightness (fading towards target)
extern int  dimTargetBrightness2;  // target brightness to fade to
extern bool lightOn2;              // ON/OFF state

// Init & loop (Channel B)
void button2Setup(const ChannelConfig* ch_config);
void button2Loop(const ChannelConfig* ch_config);

// Shared helpers (exposed for MQTT/web) for Channel B
void applyTargetBrightness2(int newTarget, bool forceOn = false, bool forceSend = false);
void dimmerBeginPowerTransition2();