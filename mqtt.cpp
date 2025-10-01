#include "mqtt.h"
#include "config.h"
#include "button.h"
#include "button2.h"
#include "web.h"
#include "version.h"
#include "utils.h"
#include "logger.h"
#include <WiFi.h>
#include <ArduinoJson.h>

// ===== Global client =====
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ===== State Cache =====
static int  lastSentBrightness[2] = {-1, -1};
static bool lastSentOn[2] = {false, false};
static unsigned long lastNvsWrite[2] = {0, 0};
static const unsigned long NVS_WRITE_DEBOUNCE_MS = 10000;

// ===== MQTT Topics =====
static String availTopic;
static String clientId;
static String chip;

// --- Subscriptions ---
static String subTopicA, subTopicB;
static String swDebugCmdTopic, swOtaCmdTopic;
static String chCmdTopics[2][4]; // 0:AutoOff, 1:MinB, 2:MaxB, 3:TransMs
static String chRestoreCmdTopic[2];

// --- State Publishing ---
static String swDebugStateTopic, swOtaStateTopic;
static String sensRssiTopic, sensHeapTopic, sensUptimeTopic, sensIpTopic;
static String chStateTopics[2][4];
static String chRestoreStateTopic[2];

// --- HA Discovery Config ---
static String lightCfgTopic[2];
static String chCfgTopics[2][4];
static String chRestoreCfgTopic[2];
static String sensRssiCfgTopic, sensHeapCfgTopic, sensUpCfgTopic, sensIpCfgTopic;
static String swDbgCfgTopic, swOtaCfgTopic;


static void logConnectFail() {
  Log.print(F("[MQTT] Connect failed, state="));
  Log.println(mqttClient.state());
}

static inline String baseTopicNoSet(const String& t){
  String b = t;
  if (b.endsWith("/set")) b.remove(b.length()-4);
  return b;
}

static void publishConfigStates();

// Controller-mode inbound state parser (we receive state from the real device on <base>)
static void parseInboundJson(const String& msg, int ch) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Log.printf("[MQTT] JSON parse failed for Ch %d: %s\n", ch + 1, err.c_str());
    return;
  }

  bool on = (ch == 0 ? lightOn : lightOn2);
  int  b  = (ch == 0 ? dimTargetBrightness : dimTargetBrightness2);
  const ChannelConfig& ch_config = config.ch[ch];

  if (doc.containsKey("state")) {
    String st = doc["state"].as<String>(); st.toUpperCase();
    if (st == "ON")  on = true;
    if (st == "OFF") on = false;
  }
  if (doc.containsKey("brightness")) {
    int vb = doc["brightness"].as<int>();
    vb = constrain(vb, ch_config.minBrightness, ch_config.maxBrightness);
    b = vb;
  }
  if (doc.containsKey("cmd")) {
    String c = doc["cmd"].as<String>();
    if (c == "reboot") { Log.println(F("[MQTT] Reboot requested")); delay(300); ESP.restart(); }
    else if (c == "factory") { Log.println(F("[MQTT] Factory reset")); delay(300); performWiFiReset(true); }
  }

  if (ch == 0) {
    lightOn = on;
    brightness = b; // Snap brightness to reported state
    dimTargetBrightness = b;
  } else {
    lightOn2 = on;
    brightness2 = b; // Snap brightness to reported state
    dimTargetBrightness2 = b;
  }

  lastSentBrightness[ch] = b;
  lastSentOn[ch] = on;
  wsPushState();
}

void onMQTTMessage(char* ctopic, byte* payload, unsigned int length) {
  String t(ctopic);
  String msg; msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Log.print(F("[MQTT] Message from ")); Log.print(t); Log.print(F(": ")); Log.println(msg);

  auto isOn=[&](const String& s){ String a=s; a.toLowerCase(); return (a=="on"||a=="1"||a=="true"); };

  // --- Global Settings ---
  if (t == swDebugCmdTopic) {
    config.debugEnabled = isOn(msg);
    saveConfig();
    mqttClient.publish(swDebugStateTopic.c_str(), config.debugEnabled ? "ON" : "OFF", true);
    wsPushState();
    return;
  }
  if (t == swOtaCmdTopic) {
    config.otaEnabled = isOn(msg);
    saveConfig();
    mqttClient.publish(swOtaStateTopic.c_str(), config.otaEnabled ? "ON" : "OFF", true);
    wsPushState();
    return;
  }

  // --- Per-Channel Settings ---
  for (int i = 0; i < 2; i++) {
    if (t == chCmdTopics[i][0]) { config.ch[i].autoOffMinutes = msg.toInt(); }
    else if (t == chCmdTopics[i][1]) { config.ch[i].minBrightness = msg.toInt(); }
    else if (t == chCmdTopics[i][2]) { config.ch[i].maxBrightness = msg.toInt(); }
    else if (t == chCmdTopics[i][3]) { config.ch[i].transitionMs = msg.toInt(); }
    else if (t == chRestoreCmdTopic[i]) { config.ch[i].restoreLast = isOn(msg); }
    else { continue; }

    clampConfig();
    saveConfig();
    publishConfigStates(); // Republish all config states
    wsPushState();
    return;
  }

  // --- Main Light State Topics ---
  if (t == subTopicA) { parseInboundJson(msg, 0); return; }
  if (t == subTopicB) { parseInboundJson(msg, 1); return; }
}

void setupMQTT() {
  chip = chipIdHex();
  clientId = "ESP32Dimmer-" + chip;
  String devId = "esp32-dimmer-" + chip;
  String haPrefix = config.haPrefix.length() ? config.haPrefix : "homeassistant";

  mqttClient.setServer(config.mqtt_server.c_str(), config.mqtt_port);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  mqttClient.setKeepAlive(config.mqttKeepalive);

  // --- Base Topics ---
  availTopic = devId + "/availability";
  subTopicA = baseTopicNoSet(config.mqtt_topic);
  subTopicB = baseTopicNoSet(config.mqtt_topic2);

  // --- Global HA Extras ---
  swDebugCmdTopic = devId + "/switch/debug/set";
  swDebugStateTopic = devId + "/switch/debug/state";
  swOtaCmdTopic   = devId + "/switch/ota/set";
  swOtaStateTopic = devId + "/switch/ota/state";
  sensRssiTopic   = devId + "/sensor/rssi";
  sensHeapTopic   = devId + "/sensor/heap";
  sensUptimeTopic = devId + "/sensor/uptime";
  sensIpTopic     = devId + "/sensor/ip";

  // --- Per-Channel HA Extras ---
  const char* entities[] = {"auto_off", "min_brightness", "max_brightness", "transition"};
  for (int i = 0; i < 2; i++) {
    String chPrefix = devId + "/ch" + String(i+1);
    for (int j = 0; j < 4; j++) {
      chCmdTopics[i][j] = chPrefix + "/number/" + entities[j] + "/set";
      chStateTopics[i][j] = chPrefix + "/number/" + entities[j] + "/state";
    }
    chRestoreCmdTopic[i] = chPrefix + "/switch/restore/set";
    chRestoreStateTopic[i] = chPrefix + "/switch/restore/state";
  }

  // --- HA Discovery Config Topics ---
  lightCfgTopic[0] = haPrefix + "/light/" + devId + "_ch1/config";
  lightCfgTopic[1] = haPrefix + "/light/" + devId + "_ch2/config";
  swDbgCfgTopic = haPrefix + "/switch/" + devId + "_debug/config";
  swOtaCfgTopic = haPrefix + "/switch/" + devId + "_ota/config";
  sensRssiCfgTopic = haPrefix + "/sensor/" + devId + "_rssi/config";
  sensHeapCfgTopic = haPrefix + "/sensor/" + devId + "_heap/config";
  sensUpCfgTopic   = haPrefix + "/sensor/" + devId + "_uptime/config";
  sensIpCfgTopic   = haPrefix + "/sensor/" + devId + "_ip/config";
  for (int i = 0; i < 2; i++) {
    String chPrefix = devId + "_ch" + String(i+1);
    for (int j = 0; j < 4; j++) {
      chCfgTopics[i][j] = haPrefix + "/number/" + chPrefix + "_" + entities[j] + "/config";
    }
    chRestoreCfgTopic[i] = haPrefix + "/switch/" + chPrefix + "_restore/config";
  }
}

static void publishConfigStates() {
    if (!config.haExtras || !mqttClient.connected()) return;
    char buf[16];
    // Global
    mqttClient.publish(swDebugStateTopic.c_str(), config.debugEnabled ? "ON" : "OFF", true);
    mqttClient.publish(swOtaStateTopic.c_str(), config.otaEnabled ? "ON" : "OFF", true);
    // Per-channel
    for (int i = 0; i < 2; i++) {
        snprintf(buf, sizeof(buf), "%d", config.ch[i].autoOffMinutes);
        mqttClient.publish(chStateTopics[i][0].c_str(), buf, true);
        snprintf(buf, sizeof(buf), "%d", config.ch[i].minBrightness);
        mqttClient.publish(chStateTopics[i][1].c_str(), buf, true);
        snprintf(buf, sizeof(buf), "%d", config.ch[i].maxBrightness);
        mqttClient.publish(chStateTopics[i][2].c_str(), buf, true);
        snprintf(buf, sizeof(buf), "%d", config.ch[i].transitionMs);
        mqttClient.publish(chStateTopics[i][3].c_str(), buf, true);
        mqttClient.publish(chRestoreStateTopic[i].c_str(), config.ch[i].restoreLast ? "ON" : "OFF", true);
    }
}

static void publishTelemetry() {
  if (!mqttClient.connected()) return;
  char buf[24];
  // HA Extras
  if (config.haExtras) {
    snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
    mqttClient.publish(sensRssiTopic.c_str(), buf, true);
    snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
    mqttClient.publish(sensHeapTopic.c_str(), buf, true);
    unsigned long s = millis()/1000UL;
    snprintf(buf, sizeof(buf), "%lu", s);
    mqttClient.publish(sensUptimeTopic.c_str(), buf, true);
    mqttClient.publish(sensIpTopic.c_str(), WiFi.localIP().toString().c_str(), true);
  }
}

void handleMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  static unsigned long lastAttempt = 0;
  static uint32_t backoffMs = (uint32_t)config.backoffMinMs;
  const uint32_t BACKOFF_MAX = (uint32_t)config.backoffMaxMs;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastAttempt < backoffMs) return;
    lastAttempt = now;

    bool ok = mqttClient.connect(
      clientId.c_str(),
      config.mqtt_user.c_str(),
      config.mqtt_pass.c_str(),
      availTopic.c_str(), 1, true, "offline"
    );
    if (!ok) {
      logConnectFail();
      backoffMs = (backoffMs >= BACKOFF_MAX) ? BACKOFF_MAX : (backoffMs * 2);
      return;
    }

    backoffMs = (uint32_t)config.backoffMinMs;
    mqttClient.publish(availTopic.c_str(), "online", true);

    if (subTopicA.length()) mqttClient.subscribe(subTopicA.c_str(), 1);
    if (subTopicB.length()) mqttClient.subscribe(subTopicB.c_str(), 1);

    if (config.haExtras) {
      mqttClient.subscribe(swDebugCmdTopic.c_str(), 0);
      mqttClient.subscribe(swOtaCmdTopic.c_str(), 0);
      for(int i=0; i<2; i++) {
        for(int j=0; j<4; j++) mqttClient.subscribe(chCmdTopics[i][j].c_str(), 0);
        mqttClient.subscribe(chRestoreCmdTopic[i].c_str(), 0);
      }
    }

    Log.printf("[MQTT] Connected. Availability: %s\n", availTopic.c_str());

    publishTelemetry();
    publishConfigStates();
    wsPushState();
    if (config.haEnabled && config.haOnBoot) {
      haDiscoveryPublish(false);
    }
  }

  mqttClient.loop();

  static unsigned long lastTele = 0;
  unsigned long now = millis();
  if (now - lastTele > (unsigned long)config.telemetrySec * 1000UL && mqttClient.connected()) {
    publishTelemetry(); lastTele = now;
  }
}

void sendStateCh(int ch, bool force) {
  if (!mqttClient.connected()) return;

  unsigned long now = millis();
  static unsigned long lastSendMs[2] = {0,0};
  const unsigned long MIN_INTERVAL_MS = 200;

  bool on  = (ch==0 ? lightOn : lightOn2);
  int  tgt = (ch==0 ? dimTargetBrightness : dimTargetBrightness2);
  const ChannelConfig& ch_config = config.ch[ch];

  if (!force) {
    if (tgt == lastSentBrightness[ch] && on == lastSentOn[ch]) return;
    if (now - lastSendMs[ch] < MIN_INTERVAL_MS) return;
  }

  StaticJsonDocument<128> doc;
  if (on) {
    int b = constrain(tgt, ch_config.minBrightness, ch_config.maxBrightness);
    doc["state"] = "ON";
    doc["brightness"] = b;
  } else {
    doc["state"] = "OFF";
  }
  char payload[128];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  String base = baseTopicNoSet(ch==0 ? config.mqtt_topic : config.mqtt_topic2);
  if (!base.length()) return;

  String cmdTopic = base + "/set";
  bool ok = mqttClient.publish(cmdTopic.c_str(), payload, n);

  Log.printf("[MQTT] Sent CMD (%s): %s -> %s\n", (ch==0?"A":"B"), payload, (ok ? "OK" : "FAIL"));

  lastSendMs[ch] = now;
  lastSentBrightness[ch] = tgt;
  lastSentOn[ch] = on;

  const char* nvsKey = (ch==0) ? "last_brightness_ch1" : "last_brightness_ch2";
  if (force || (now - lastNvsWrite[ch]) > NVS_WRITE_DEBOUNCE_MS) {
    preferences.putInt(nvsKey, tgt);
    lastNvsWrite[ch] = now;
  }

  wsPushState();
}

bool mqttIsConnected() { return mqttClient.connected(); }

bool mqttPublishTest() {
  if (!mqttClient.connected()) return false;
  String t = baseTopicNoSet(config.mqtt_topic) + "/test";
  char msg[40];
  snprintf(msg, sizeof(msg), "ping:%lu", (unsigned long)millis());
  return mqttClient.publish(t.c_str(), msg, false);
}

// ================= HA Discovery Helpers =================
static void publishHaEntity(const String& cfgTopic, JsonDocument& doc, const JsonObject& dev, bool remove) {
    if (remove) {
        mqttClient.publish(cfgTopic.c_str(), "", true);
        return;
    }
    doc["dev"] = dev;
    String json;
    serializeJson(doc, json);
    mqttClient.publish(cfgTopic.c_str(), json.c_str(), true);
}

void haDiscoveryPublish(bool remove) {
  if (!mqttClient.connected() || !config.haEnabled) return;

  String deviceName = (config.haName.length()? config.haName : ("ESP32 Dimmer " + chip));
  String devId = "esp32-dimmer-" + chip;

  // --- Master Device Object ---
  StaticJsonDocument<384> dev;
  dev["name"] = deviceName;
  dev["identifiers"][0] = devId;
  dev["manufacturer"] = "Xat0X";
  dev["model"] = "ESP32 Button Dimmer";
  dev["sw_version"]  = FW_VERSION;
  if (config.mdns_hostname.length() > 0) {
      dev["configuration_url"] = "http://" + config.mdns_hostname + ".local/";
  } else {
      dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
  }
  JsonObject devObj = dev.as<JsonObject>();

  // --- Unpublish all and return if requested ---
  if (remove) {
      if (baseTopicNoSet(config.mqtt_topic).length() > 0) mqttClient.publish(lightCfgTopic[0].c_str(), "", true);
      if (baseTopicNoSet(config.mqtt_topic2).length() > 0) mqttClient.publish(lightCfgTopic[1].c_str(), "", true);
      if (config.haExtras) {
          mqttClient.publish(sensRssiCfgTopic.c_str(), "", true);
          mqttClient.publish(sensHeapCfgTopic.c_str(), "", true);
          mqttClient.publish(sensUpCfgTopic.c_str(), "", true);
          mqttClient.publish(sensIpCfgTopic.c_str(), "", true);
          mqttClient.publish(swDbgCfgTopic.c_str(), "", true);
          mqttClient.publish(swOtaCfgTopic.c_str(), "", true);
          for(int i=0; i<2; i++) {
              for(int j=0; j<4; j++) mqttClient.publish(chCfgTopics[i][j].c_str(), "", true);
              mqttClient.publish(chRestoreCfgTopic[i].c_str(), "", true);
          }
      }
      return;
  }

  // --- Lights (per channel) ---
  String topics[2] = { config.mqtt_topic, config.mqtt_topic2 };
  for (int i = 0; i < 2; i++) {
    String base = baseTopicNoSet(topics[i]);
    if (base.length() == 0) continue;

    StaticJsonDocument<512> d;
    d["name"] = deviceName + " Ch " + String(i+1);
    d["uniq_id"] = devId + "_ch" + String(i+1);
    d["schema"] = "json";
    d["cmd_t"] = base + "/set";
    d["stat_t"]= base;
    d["avty_t"]= availTopic;
    d["brightness"] = true;
    d["brightness_scale"] = 254;
    publishHaEntity(lightCfgTopic[i], d, devObj, remove);
  }

  // --- HA Extras ---
  if (config.haExtras) {
    // --- Global Sensors & Switches ---
    { // IP Sensor
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " IP Address";
      d["uniq_id"] = devId + "_ip";
      d["stat_t"] = sensIpTopic;
      d["entity_category"] = "diagnostic";
      d["avty_t"] = availTopic;
      publishHaEntity(sensIpCfgTopic, d, devObj, remove);
    }
    { // RSSI Sensor
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " RSSI";
      d["uniq_id"] = devId + "_rssi";
      d["stat_t"] = sensRssiTopic;
      d["unit_of_meas"] = "dBm";
      d["dev_cla"] = "signal_strength";
      d["entity_category"] = "diagnostic";
      d["avty_t"] = availTopic;
      publishHaEntity(sensRssiCfgTopic, d, devObj, remove);
    }
    { // Heap Sensor
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " Free Heap";
      d["uniq_id"] = devId + "_heap";
      d["stat_t"] = sensHeapTopic;
      d["unit_of_meas"] = "B";
      d["entity_category"] = "diagnostic";
      d["avty_t"] = availTopic;
      publishHaEntity(sensHeapCfgTopic, d, devObj, remove);
    }
    { // Uptime Sensor
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " Uptime";
      d["uniq_id"] = devId + "_uptime";
      d["stat_t"] = sensUptimeTopic;
      d["unit_of_meas"] = "s";
      d["dev_cla"] = "duration";
      d["entity_category"] = "diagnostic";
      d["avty_t"] = availTopic;
      publishHaEntity(sensUpCfgTopic, d, devObj, remove);
    }
    { // Debug Switch
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " Debug Logs";
      d["uniq_id"] = devId + "_debug";
      d["cmd_t"] = swDebugCmdTopic;
      d["stat_t"]= swDebugStateTopic;
      d["entity_category"] = "config";
      d["avty_t"]= availTopic;
      publishHaEntity(swDbgCfgTopic, d, devObj, remove);
    }
    { // OTA Switch
      StaticJsonDocument<256> d;
      d["name"] = deviceName + " OTA";
      d["uniq_id"] = devId + "_ota";
      d["cmd_t"] = swOtaCmdTopic;
      d["stat_t"]= swOtaStateTopic;
      d["entity_category"] = "config";
      d["avty_t"]= availTopic;
      publishHaEntity(swOtaCfgTopic, d, devObj, remove);
    }

    // --- Per-Channel Config Entities ---
    for (int i = 0; i < 2; i++) {
        String chName = deviceName + " Ch " + String(i+1);
        { // Auto-Off
            StaticJsonDocument<384> d;
            d["name"] = chName + " Auto-Off";
            d["uniq_id"] = devId + "_ch" + String(i+1) + "_auto_off";
            d["cmd_t"] = chCmdTopics[i][0];
            d["stat_t"] = chStateTopics[i][0];
            d["min"] = 0; d["max"] = 1440; d["step"] = 1;
            d["unit_of_meas"] = "min";
            d["entity_category"] = "config";
            d["avty_t"] = availTopic;
            publishHaEntity(chCfgTopics[i][0], d, devObj, remove);
        }
        { // Min Brightness
            StaticJsonDocument<384> d;
            d["name"] = chName + " Min Brightness";
            d["uniq_id"] = devId + "_ch" + String(i+1) + "_min_brightness";
            d["cmd_t"] = chCmdTopics[i][1];
            d["stat_t"] = chStateTopics[i][1];
            d["min"] = 0; d["max"] = 254; d["step"] = 1;
            d["entity_category"] = "config";
            d["avty_t"] = availTopic;
            publishHaEntity(chCfgTopics[i][1], d, devObj, remove);
        }
        { // Max Brightness
            StaticJsonDocument<384> d;
            d["name"] = chName + " Max Brightness";
            d["uniq_id"] = devId + "_ch" + String(i+1) + "_max_brightness";
            d["cmd_t"] = chCmdTopics[i][2];
            d["stat_t"] = chStateTopics[i][2];
            d["min"] = 0; d["max"] = 254; d["step"] = 1;
            d["entity_category"] = "config";
            d["avty_t"] = availTopic;
            publishHaEntity(chCfgTopics[i][2], d, devObj, remove);
        }
        { // Transition
            StaticJsonDocument<384> d;
            d["name"] = chName + " Transition";
            d["uniq_id"] = devId + "_ch" + String(i+1) + "_transition";
            d["cmd_t"] = chCmdTopics[i][3];
            d["stat_t"] = chStateTopics[i][3];
            d["min"] = 0; d["max"] = 2000; d["step"] = 50;
            d["unit_of_meas"] = "ms";
            d["entity_category"] = "config";
            d["avty_t"] = availTopic;
            publishHaEntity(chCfgTopics[i][3], d, devObj, remove);
        }
        { // Restore Last
            StaticJsonDocument<384> d;
            d["name"] = chName + " Restore Last Brightness";
            d["uniq_id"] = devId + "_ch" + String(i+1) + "_restore";
            d["cmd_t"] = chRestoreCmdTopic[i];
            d["stat_t"] = chRestoreStateTopic[i];
            d["entity_category"] = "config";
            d["avty_t"] = availTopic;
            publishHaEntity(chRestoreCfgTopic[i], d, devObj, remove);
        }
    }
  }
}