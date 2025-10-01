#include "button2.h"
#include "config.h"
#include "mqtt.h"
#include "web.h"
#include "logger.h"

#include <WiFi.h>
#include <math.h>

// ------------------ Global dimmer status (Channel B) ------------------
int  brightness2           = 128;
int  dimTargetBrightness2  = 128;
bool lightOn2              = true;

// ------------------ Internal timing & click params ------------------
static const unsigned long CLICK_MIN_MS  = 30;
static const unsigned long CLICK_MAX_MS  = 800;
static const unsigned long MULTICLICK_GAP_MS = 350;

// 10x click for factory reset
static const uint8_t  RESET_PRESS_COUNT  = 10;
static const unsigned long RESET_WINDOW_MS = 12000;

// Debounce
static const unsigned long DEBOUNCE_MS = 10;

// ------------------ Internal states ------------------
static bool lastButtonState = HIGH;  // debounced, pulled-up
static bool lastReadState   = HIGH;  // raw
static unsigned long lastDebounceTime = 0;

static bool buttonHeld      = false;
static bool dimmingDown     = true;

static unsigned long buttonPressTime   = 0;
static unsigned long lastDimStepTime   = 0;
static unsigned long lastFadeStepTime  = 0;
static unsigned long lastOnTimestamp   = 0;
static uint8_t       resetPressCounter = 0;
static unsigned long resetWindowStart  = 0;

// multi-click bundling
static uint8_t       clickCount        = 0;
static unsigned long clickEvalDeadline = 0;

// Temporary fade override during power toggle transition
static unsigned long fadeOverrideUntilMs = 0;
static unsigned long fadeOverrideDelayMs = 0;

// Helpers
static inline int clampB2(int v, const ChannelConfig* ch_config) {
  if (v < ch_config->minBrightness) return ch_config->minBrightness;
  if (v > ch_config->maxBrightness) return ch_config->maxBrightness;
  return v;
}

static inline int maxOnFloor2(const ChannelConfig* ch_config) {
  return max(ch_config->minOnLevel, ch_config->minBrightness);
}

// --- Ease fade step ---
static int computeFadeStep2(int delta, const ChannelConfig* ch_config) {
  if (!ch_config->easeFade) return 1;
  const float alpha = 0.22f;
  const int   cap   = 24;
  int ad = abs(delta);
  int s  = (int)ceilf(ad * alpha);
  if (s < 1)  s = 1;
  if (s > cap) s = cap;
  return s;
}

// ---- Gamma helpers ----
static float toPerceptual2(int b, const ChannelConfig* ch_config) {
  int minB = ch_config->minBrightness, maxB = ch_config->maxBrightness;
  if (maxB <= minB) return 0.0f;
  float n = (float)(b - minB) / (float)(maxB - minB);
  n = fmaxf(0.0f, fminf(1.0f, n));
  if (!ch_config->gammaEnabled) return n;
  return powf(n, 1.0f / ch_config->gamma);
}
static int fromPerceptual2(float p, const ChannelConfig* ch_config) {
  int minB = ch_config->minBrightness, maxB = ch_config->maxBrightness;
  p = fmaxf(0.0f, fminf(1.0f, p));
  if (ch_config->gammaEnabled) p = powf(p, ch_config->gamma);
  int b = minB + (int)lroundf(p * (float)(maxB - minB));
  if (b < minB) b = minB; if (b > maxB) b = maxB;
  return b;
}

static void pushState2(bool forceMqtt) {
  if (forceMqtt) sendStateCh(1, true);
  wsPushState();
}

// Central helper to apply a new target brightness (exposed)
void applyTargetBrightness2(int newTarget, bool forceOn, bool forceSend) {
  newTarget = clampB2(newTarget, &config.ch[1]);
  if (forceOn && newTarget > 0) lightOn2 = true;
  dimTargetBrightness2 = newTarget;
  if (lightOn2) lastOnTimestamp = millis();
  if (forceSend) pushState2(true);
}

// Begin a soft transition for power toggles using transitionMs
void dimmerBeginPowerTransition2() {
  if (config.ch[1].transitionMs <= 0) { fadeOverrideUntilMs = 0; return; }
  unsigned long now = millis();
  fadeOverrideUntilMs = now + (unsigned long)config.ch[1].transitionMs;
  unsigned long step = (unsigned long)max(5, config.ch[1].transitionMs / 40);
  fadeOverrideDelayMs = min((unsigned long)config.ch[1].fadeStepDelayMs, step);
}

// ------------------ Setup ------------------
void button2Setup(const ChannelConfig* ch_config) {
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  int lastB = preferences.getInt("last_brightness_ch2", ch_config->startBrightness);
  int startB = ch_config->restoreLast ? lastB : ch_config->startBrightness;
  startB = clampB2(startB, ch_config);

  brightness2 = startB;
  dimTargetBrightness2 = startB;
  lightOn2 = ch_config->startOn;
  dimmingDown = ch_config->reverseDirection;
  lastOnTimestamp = millis();
  lastReadState = lastButtonState = digitalRead(BUTTON2_PIN);

  Log.println("[BTN B] Setup done");
  Log.printf("[BTN B] start: on=%d, b=%d, min=%d, max=%d, step=%d, holdSPS=%d, fade=%dms, longPress=%dms\n",
    lightOn2, startB, ch_config->minBrightness, ch_config->maxBrightness,
    ch_config->dimStep, ch_config->holdSPS, ch_config->fadeStepDelayMs, ch_config->longPressMs);
}

// ------------------ Multi-click evaluation ------------------
static void evaluateClicks2(const ChannelConfig* ch_config) {
  if (clickCount == 0) return;

  Log.printf("[BTN B] Evaluate clicks: %u\n", clickCount);

  if (clickCount == 1) {
    bool prev = lightOn2;
    lightOn2 = !lightOn2;
    if (lightOn2) {
      if (dimTargetBrightness2 < maxOnFloor2(ch_config)) dimTargetBrightness2 = maxOnFloor2(ch_config);
      lastOnTimestamp = millis();
    }
    if (prev != lightOn2) dimmerBeginPowerTransition2();
    pushState2(true);

  } else if (clickCount == 2) {
    int b = clampB2(ch_config->preset1Brightness, ch_config);
    applyTargetBrightness2(b, (b > 0), true);

  } else if (clickCount == 3) {
    int b = clampB2(ch_config->preset2Brightness, ch_config);
    applyTargetBrightness2(b, (b > 0), true);
  }

  clickCount = 0;
  clickEvalDeadline = 0;
}

// ------------------ Loop ------------------
void button2Loop(const ChannelConfig* ch_config) {
  unsigned long now = millis();

  // Debounce
  bool raw = digitalRead(BUTTON2_PIN);
  if (raw != lastReadState) {
    lastDebounceTime = now;
    lastReadState = raw;
  }
  bool s = lastButtonState;
  if (now - lastDebounceTime > DEBOUNCE_MS) {
    s = lastReadState;
  }

  if (clickCount > 0 && now > clickEvalDeadline) {
    evaluateClicks2(ch_config);
  }

  // Edge: press
  if (s == LOW && lastButtonState == HIGH) {
    buttonPressTime = now;
    buttonHeld = false;
    Log.println("[BTN B] Down");
  }

  // Long press -> start dimming
  if (s == LOW && !buttonHeld && (now - buttonPressTime > (unsigned long)ch_config->longPressMs)) {
    buttonHeld = true;
    Log.println("[DIM B] Hold start");

    clickCount = 0;
    clickEvalDeadline = 0;

    if (!lightOn2) {
      lightOn2 = true;
      dimTargetBrightness2 = maxOnFloor2(ch_config);
      lastOnTimestamp = now;
      dimmerBeginPowerTransition2();
      pushState2(true);
    }

    if (dimTargetBrightness2 <= ch_config->minBrightness + 1) {
      dimmingDown = false;
    } else if (dimTargetBrightness2 >= ch_config->maxBrightness - 1) {
      dimmingDown = true;
    } else {
      dimmingDown = !dimmingDown;
    }
    Log.printf("[DIM B] Direction: %s\n", dimmingDown ? "down" : "up");
  }

  if (buttonHeld && s == LOW) {
    unsigned long interval = 1000UL / (unsigned long)max(1, ch_config->holdSPS);
    if (now - lastDimStepTime >= interval) {
      const int range = max(1, ch_config->maxBrightness - ch_config->minBrightness);
      float p = toPerceptual2(dimTargetBrightness2, ch_config);
      int   step = max(1, ch_config->dimStep);
      float dn   = (float)step / (float)range;

      if (dimmingDown) p -= dn; else p += dn;

      dimTargetBrightness2 = fromPerceptual2(p, ch_config);
      lastOnTimestamp = now;

      sendStateCh(1, false);
      wsPushState();
      lastDimStepTime = now;
    }
  }

  // Edge: release
  if (s == HIGH && lastButtonState == LOW) {
    unsigned long pressDuration = now - buttonPressTime;

    if (!buttonHeld) {
      if (pressDuration >= CLICK_MIN_MS && pressDuration <= CLICK_MAX_MS) {
        clickCount = (clickCount == 0) ? 1 : (clickCount + 1);
        clickEvalDeadline = now + MULTICLICK_GAP_MS;

        if (resetWindowStart == 0 || (now - resetWindowStart) > RESET_WINDOW_MS) {
          resetWindowStart = now;
          resetPressCounter = 0;
        }
        resetPressCounter++;
        Log.printf("[RST] Clicks (B): %u/%u\n", resetPressCounter, RESET_PRESS_COUNT);
        if (resetPressCounter >= RESET_PRESS_COUNT && (now - resetWindowStart) <= RESET_WINDOW_MS) {
          Log.println("[RST] 10x click (B) -> Factory reset");
          performWiFiReset(true);
        }
      }
    } else {
      Log.println("[DIM B] Hold end");
      resetWindowStart = 0;
      resetPressCounter = 0;

      pushState2(true);
      clickCount = 0;
      clickEvalDeadline = 0;
    }
  }

  if (resetWindowStart != 0 && (now - resetWindowStart) > RESET_WINDOW_MS) {
    resetWindowStart = 0;
    resetPressCounter = 0;
  }

  lastButtonState = s;

  // --- Fade engine ---
  unsigned long effDelay = ch_config->fadeStepDelayMs;
  if (fadeOverrideUntilMs && now < fadeOverrideUntilMs) {
    effDelay = fadeOverrideDelayMs ? fadeOverrideDelayMs : effDelay;
  }

  if (brightness2 != dimTargetBrightness2 &&
      (now - lastFadeStepTime > effDelay)) {

    int delta = dimTargetBrightness2 - brightness2;
    int step = computeFadeStep2(delta, ch_config);
    if (delta > 0) {
      brightness2 += step;
      if (brightness2 > dimTargetBrightness2) brightness2 = dimTargetBrightness2;
    } else if (delta < 0) {
      brightness2 -= step;
      if (brightness2 < dimTargetBrightness2) brightness2 = dimTargetBrightness2;
    }
    lastFadeStepTime = now;
  }

  // --- Auto OFF ---
  if (ch_config->autoOffMinutes > 0 && lightOn2 &&
      (now - lastOnTimestamp) > (unsigned long)(ch_config->autoOffMinutes * 60000UL)) {
    lightOn2 = false;
    dimmerBeginPowerTransition2();
    pushState2(true);
    Log.println("[AUTO-OFF B] Light turned OFF by timer");
  }
}