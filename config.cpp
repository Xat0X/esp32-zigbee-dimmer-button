#include "config.h"
#include "utils.h"
#include "logger.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <math.h>

Config      config;
Preferences preferences;

void clampChannelConfig(ChannelConfig& ch_config) {
  if (ch_config.holdSPS < 1)  ch_config.holdSPS = 1;
  if (ch_config.holdSPS > 50) ch_config.holdSPS = 50;

  if (ch_config.dimStep < 1)   ch_config.dimStep = 1;
  if (ch_config.dimStep > 100) ch_config.dimStep = 100;

  if (ch_config.fadeStepDelayMs < 5)   ch_config.fadeStepDelayMs = 5;
  if (ch_config.fadeStepDelayMs > 200) ch_config.fadeStepDelayMs = 200;

  if (ch_config.minBrightness < 0)   ch_config.minBrightness = 0;
  if (ch_config.maxBrightness > 254) ch_config.maxBrightness = 254;
  if (ch_config.maxBrightness < ch_config.minBrightness) ch_config.maxBrightness = ch_config.minBrightness;

  if (ch_config.gamma < 1.2f) ch_config.gamma = 1.2f;
  if (ch_config.gamma > 3.5f) ch_config.gamma = 3.5f;

  if (ch_config.transitionMs < 0)    ch_config.transitionMs = 0;
  if (ch_config.transitionMs > 2000) ch_config.transitionMs = 2000;

  if (ch_config.minOnLevel < 0)      ch_config.minOnLevel = 0;
  if (ch_config.minOnLevel > 254)    ch_config.minOnLevel = 254;

  if (ch_config.longPressMs < 150)   ch_config.longPressMs = 150;
  if (ch_config.longPressMs > 2000)  ch_config.longPressMs = 2000;

  if (ch_config.minOnLevel < ch_config.minBrightness) ch_config.minOnLevel = ch_config.minBrightness;
}

void clampConfig() {
  // Global settings
  if (config.mqttKeepalive < 5)   config.mqttKeepalive = 5;
  if (config.mqttKeepalive > 120) config.mqttKeepalive = 120;

  if (config.backoffMinMs < 200) config.backoffMinMs = 200;
  if (config.backoffMaxMs < config.backoffMinMs) config.backoffMaxMs = config.backoffMinMs;

  if (config.telemetrySec < 5)    config.telemetrySec = 5;
  if (config.telemetrySec > 3600) config.telemetrySec = 3600;

  // Per-channel settings
  clampChannelConfig(config.ch[0]);
  clampChannelConfig(config.ch[1]);
}

void loadConfig() {
  preferences.begin("config", false);

  // MQTT
  config.mqtt_server = preferences.getString("mqtt_server", config.mqtt_server);
  config.mqtt_port   = preferences.getInt   ("mqtt_port",   config.mqtt_port);
  config.mqtt_user   = preferences.getString("mqtt_user",   config.mqtt_user);
  config.mqtt_pass   = preferences.getString("mqtt_pass",   config.mqtt_pass);
  config.mqtt_topic  = preferences.getString("mqtt_topic",  config.mqtt_topic);
  config.mqtt_topic2 = preferences.getString("mqtt_topic2", config.mqtt_topic2);

  // MQTT tuning
  config.mqttKeepalive = preferences.getInt("mqttKeepalive", config.mqttKeepalive);
  config.backoffMinMs  = preferences.getInt("backoffMinMs",  config.backoffMinMs);
  config.backoffMaxMs  = preferences.getInt("backoffMaxMs",  config.backoffMaxMs);
  config.telemetrySec  = preferences.getInt("telemetrySec",  config.telemetrySec);

  // Network
  config.mdns_hostname = preferences.getString("mdns_hostname", config.mdns_hostname);

  // Web
  config.web_user = preferences.getString("web_user", config.web_user);
  config.web_pass = preferences.getString("web_pass", config.web_pass);

  // Per-channel settings
  for (int i = 0; i < 2; i++) {
    String p = "ch" + String(i+1) + "_";
    // Legacy migration for ch1 from old global keys
    if (i == 0 && !preferences.isKey((p + "holdSPS").c_str())) {
      int legacyHoldMs = preferences.getInt("dimHoldIntervalMs", -1);
      if (legacyHoldMs > 0) {
        int sps = (int)lroundf(1000.0f / (float)legacyHoldMs);
        if (sps < 1)  sps = 1;
        if (sps > 50) sps = 50;
        config.ch[i].holdSPS = sps;
        preferences.remove("dimHoldIntervalMs");
        preferences.putInt((p + "holdSPS").c_str(), config.ch[i].holdSPS);
      } else {
         config.ch[i].holdSPS = preferences.getInt("holdSPS", config.ch[i].holdSPS);
      }
      config.ch[i].dimStep         = preferences.getInt("dimStep", config.ch[i].dimStep);
      config.ch[i].minBrightness   = preferences.getInt("minBrightness",   config.ch[i].minBrightness);
      config.ch[i].maxBrightness   = preferences.getInt("maxBrightness",   config.ch[i].maxBrightness);
      config.ch[i].fadeStepDelayMs = preferences.getInt("fadeStepDelayMs", config.ch[i].fadeStepDelayMs);
      config.ch[i].longPressMs     = preferences.getInt("longPressMs",     config.ch[i].longPressMs);
      config.ch[i].transitionMs    = preferences.getInt("transitionMs", config.ch[i].transitionMs);
      config.ch[i].minOnLevel      = preferences.getInt("minOnLevel",   config.ch[i].minOnLevel);
      config.ch[i].startBrightness   = preferences.getInt ("startBrightness",   config.ch[i].startBrightness);
      config.ch[i].startOn           = preferences.getBool("startOn",           config.ch[i].startOn);
      config.ch[i].restoreLast       = preferences.getBool("restoreLast",       config.ch[i].restoreLast);
      config.ch[i].reverseDirection  = preferences.getBool("reverseDirection",  config.ch[i].reverseDirection);
      config.ch[i].autoOffMinutes    = preferences.getInt("autoOffMinutes",     config.ch[i].autoOffMinutes);
      config.ch[i].preset1Brightness = preferences.getInt("preset1Brightness",  config.ch[i].preset1Brightness);
      config.ch[i].preset2Brightness = preferences.getInt("preset2Brightness",  config.ch[i].preset2Brightness);
      config.ch[i].easeFade          = preferences.getBool ("easeFade",       config.ch[i].easeFade);
      config.ch[i].gammaEnabled      = preferences.getBool ("gammaEnabled",   config.ch[i].gammaEnabled);
      config.ch[i].gamma             = preferences.getFloat("gamma",          config.ch[i].gamma);
    }

    config.ch[i].holdSPS = preferences.getInt((p + "holdSPS").c_str(), config.ch[i].holdSPS);
    config.ch[i].dimStep = preferences.getInt((p + "dimStep").c_str(), config.ch[i].dimStep);
    config.ch[i].minBrightness = preferences.getInt((p + "minBright").c_str(), config.ch[i].minBrightness);
    config.ch[i].maxBrightness = preferences.getInt((p + "maxBright").c_str(), config.ch[i].maxBrightness);
    config.ch[i].fadeStepDelayMs = preferences.getInt((p + "fadeDelay").c_str(), config.ch[i].fadeStepDelayMs);
    config.ch[i].longPressMs = preferences.getInt((p + "longPress").c_str(), config.ch[i].longPressMs);
    config.ch[i].transitionMs = preferences.getInt((p + "transMs").c_str(), config.ch[i].transitionMs);
    config.ch[i].minOnLevel = preferences.getInt((p + "minOnLvl").c_str(), config.ch[i].minOnLevel);
    config.ch[i].startBrightness = preferences.getInt((p + "startB").c_str(), config.ch[i].startBrightness);
    config.ch[i].startOn = preferences.getBool((p + "startOn").c_str(), config.ch[i].startOn);
    config.ch[i].restoreLast = preferences.getBool((p + "restore").c_str(), config.ch[i].restoreLast);
    config.ch[i].reverseDirection = preferences.getBool((p + "reverse").c_str(), config.ch[i].reverseDirection);
    config.ch[i].autoOffMinutes = preferences.getInt((p + "autoOff").c_str(), config.ch[i].autoOffMinutes);
    config.ch[i].preset1Brightness = preferences.getInt((p + "preset1").c_str(), config.ch[i].preset1Brightness);
    config.ch[i].preset2Brightness = preferences.getInt((p + "preset2").c_str(), config.ch[i].preset2Brightness);
    config.ch[i].easeFade = preferences.getBool((p + "ease").c_str(), config.ch[i].easeFade);
    config.ch[i].gammaEnabled = preferences.getBool((p + "gammaEn").c_str(), config.ch[i].gammaEnabled);
    config.ch[i].gamma = preferences.getFloat((p + "gamma").c_str(), config.ch[i].gamma);
  }

  // Debug/OTA
  config.debugEnabled      = preferences.getBool ("debugEnabled",      config.debugEnabled);
  config.otaEnabled        = preferences.getBool ("otaEnabled",        config.otaEnabled);
  config.ota_password      = preferences.getString("ota_password",     config.ota_password);

  // HA
  config.haEnabled         = preferences.getBool  ("haEnabled", config.haEnabled);
  config.haPrefix          = preferences.getString("haPrefix",  config.haPrefix);
  config.haName            = preferences.getString("haName",    config.haName);
  config.haExtras          = preferences.getBool  ("haExtras",  config.haExtras);
  config.haOnBoot          = preferences.getBool  ("haOnBoot",  config.haOnBoot);

  if (config.mqtt_topic.endsWith("/set"))
    config.mqtt_topic.remove(config.mqtt_topic.length()-4);
  if (config.mqtt_topic2.endsWith("/set"))
    config.mqtt_topic2.remove(config.mqtt_topic2.length()-4);

  clampConfig();
}

bool saveConfig() {
  preferences.putString("mqtt_server", config.mqtt_server);
  preferences.putInt   ("mqtt_port",   config.mqtt_port);
  preferences.putString("mqtt_user",   config.mqtt_user);
  preferences.putString("mqtt_pass",   config.mqtt_pass);
  preferences.putString("mqtt_topic",  config.mqtt_topic);
  preferences.putString("mqtt_topic2", config.mqtt_topic2);

  preferences.putInt("mqttKeepalive", config.mqttKeepalive);
  preferences.putInt("backoffMinMs",  config.backoffMinMs);
  preferences.putInt("backoffMaxMs",  config.backoffMaxMs);
  preferences.putInt("telemetrySec",  config.telemetrySec);

  preferences.putString("mdns_hostname", config.mdns_hostname);
  preferences.putString("web_user",      config.web_user);
  preferences.putString("web_pass",      config.web_pass);

  // Per-channel settings
  for (int i = 0; i < 2; i++) {
    String p = "ch" + String(i+1) + "_";
    preferences.putInt((p + "holdSPS").c_str(), config.ch[i].holdSPS);
    preferences.putInt((p + "dimStep").c_str(), config.ch[i].dimStep);
    preferences.putInt((p + "minBright").c_str(), config.ch[i].minBrightness);
    preferences.putInt((p + "maxBright").c_str(), config.ch[i].maxBrightness);
    preferences.putInt((p + "fadeDelay").c_str(), config.ch[i].fadeStepDelayMs);
    preferences.putInt((p + "longPress").c_str(), config.ch[i].longPressMs);
    preferences.putInt((p + "transMs").c_str(), config.ch[i].transitionMs);
    preferences.putInt((p + "minOnLvl").c_str(), config.ch[i].minOnLevel);
    preferences.putInt((p + "startB").c_str(), config.ch[i].startBrightness);
    preferences.putBool((p + "startOn").c_str(), config.ch[i].startOn);
    preferences.putBool((p + "restore").c_str(), config.ch[i].restoreLast);
    preferences.putBool((p + "reverse").c_str(), config.ch[i].reverseDirection);
    preferences.putInt((p + "autoOff").c_str(), config.ch[i].autoOffMinutes);
    preferences.putInt((p + "preset1").c_str(), config.ch[i].preset1Brightness);
    preferences.putInt((p + "preset2").c_str(), config.ch[i].preset2Brightness);
    preferences.putBool((p + "ease").c_str(), config.ch[i].easeFade);
    preferences.putBool((p + "gammaEn").c_str(), config.ch[i].gammaEnabled);
    preferences.putFloat((p + "gamma").c_str(), config.ch[i].gamma);
  }

  preferences.putBool ("debugEnabled",      config.debugEnabled);
  preferences.putBool ("otaEnabled",        config.otaEnabled);
  preferences.putString("ota_password",     config.ota_password);

  preferences.putBool  ("haEnabled", config.haEnabled);
  preferences.putString("haPrefix",  config.haPrefix);
  preferences.putString("haName",    config.haName);
  preferences.putBool  ("haExtras",  config.haExtras);
  preferences.putBool  ("haOnBoot",  config.haOnBoot);

  // As Preferences::end() does not return a status, we assume success if we reach here.
  // In a more complex scenario with potential write failures, this might need more robust checking.
  return true;
}

void connectWiFi() {
  WiFiManager wm;
  wm.setConnectTimeout(30);
  wm.setConfigPortalTimeout(120);

  wm.setSaveConfigCallback([](){
    Log.println("[WiFi] Credentials saved. Rebooting…");
    delay(1200);
    ESP.restart();
  });

  String ap = "ESP32-DIMMER-" + chipIdHex();
  if (!wm.autoConnect(ap.c_str(), "dimmer123")) {
    Log.println("[WiFi] AutoConnect failed. Rebooting…");
    delay(1500);
    ESP.restart();
  }

  Log.print("[WiFi] Connected. IP="); Log.println(WiFi.localIP());

  if (config.mdns_hostname.length() == 0) {
    config.mdns_hostname = "esp32-dimmer-" + chipIdHex();
    saveConfig();
  }

#if defined(ARDUINO_ARCH_ESP32)
  WiFi.setHostname(config.mdns_hostname.c_str());
#endif

  if (MDNS.begin(config.mdns_hostname.c_str())) {
    Log.print("[mDNS] http://"); Log.print(config.mdns_hostname); Log.println(".local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Log.println("[mDNS] Failed to start mDNS responder");
  }
}

void performWiFiReset(bool factoryReset) {
  Log.println(factoryReset ? "[RST] Factory Reset (WiFi + Config)" : "[RST] WiFi Reset");
  WiFiManager wm;
  wm.resetSettings();
  if (factoryReset) preferences.clear();
  delay(400);
  ESP.restart();
}