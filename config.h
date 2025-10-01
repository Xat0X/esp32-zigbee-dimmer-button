#pragma once
#include <Arduino.h>
#include <Preferences.h>

// ======= Hardware defaults (override via build flags if needed) =======
#ifndef BUTTON_PIN
#define BUTTON_PIN 9   // Example: XIAO ESP32-C3 BOOT pin (internal pull-up) -> G09
#endif

#ifndef BUTTON2_PIN
#define BUTTON2_PIN 10  // Second button pin (change to your chosen GPIO on XIAO C3)
#endif

// Per-channel specific configuration
struct ChannelConfig {
  // Dimming
  int  holdSPS         = 10;     // hold steps per second
  int  dimStep         = 25;     // base step per hold tick (perceptual space), 1..100
  int  minBrightness   = 40;
  int  maxBrightness   = 254;
  int  fadeStepDelayMs = 50;
  int  longPressMs     = 400;    // configurable long-press threshold for dim start

  // UX extras
  int  transitionMs    = 200;    // soft on/off transition (0 = instant)
  int  minOnLevel      = 20;     // minimum level when switching from OFF to ON

  // Startup behavior
  int  startBrightness   = 128;
  bool startOn           = true;
  bool restoreLast       = true; // Only restores brightness (not ON/OFF state) by design
  bool reverseDirection  = false;

  // Auto-off
  int  autoOffMinutes   = 0;

  // Presets
  int  preset1Brightness = 64;
  int  preset2Brightness = 192;

  // Fade easing & Gamma
  bool  easeFade = true;
  bool  gammaEnabled = true;
  float gamma        = 2.2f;
};

struct Config {
  // MQTT
  String mqtt_server = "192.168.0.92";
  int    mqtt_port   = 1883;
  String mqtt_user   = "mosquitto";
  String mqtt_pass   = "";
  String mqtt_topic  = "zigbee2mqtt/livingroom";     // base without /set (Channel A)
  String mqtt_topic2 = "zigbee2mqtt/livingroom2";    // base without /set (Channel B)

  // MQTT tuning
  int    mqttKeepalive = 20;     // seconds
  int    backoffMinMs  = 1000;   // ms (min backoff)
  int    backoffMaxMs  = 60000;  // ms (max backoff)
  int    telemetrySec  = 30;     // publish RSSI/heap/uptime every X seconds

  // Network
  String mdns_hostname = "";     // if empty -> auto "esp32-dimmer-<chip>"

  // Web UI auth
  String web_user = "admin";
  String web_pass = "dimmer2025";   // stored in NVS as plaintext (UI masks; backup omits)

  // Per-channel settings
  ChannelConfig ch[2];

  // Debug/OTA (Global)
  bool   debugEnabled = true;
  bool   otaEnabled   = true;
  String ota_password = "";      // ArduinoOTA password (optional)

  // Home Assistant Discovery (Global)
  bool   haEnabled = true;
  String haPrefix  = "homeassistant";
  String haName    = "";
  bool   haExtras  = true;   // sensors & switches
  bool   haOnBoot  = true;   // publish discovery on boot
};

extern Config      config;
extern Preferences preferences;

// API
void loadConfig();
bool saveConfig();
void clampConfig();  // now exposed

void connectWiFi();
void performWiFiReset(bool factoryReset);