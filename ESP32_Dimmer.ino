#include <Arduino.h>
#include "version.h"
#include "config.h"
#include "web.h"
#include "mqtt.h"
#include "ota.h"
#include "button.h"
#include "button2.h"
#include "logger.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("[BOOT] ESP32 Dimmer %s\n", FW_VERSION);

  loadConfig();          // Prefs → config
  connectWiFi();         // WiFi + mDNS
  setupWebServer();      // HTTP + WebSocket
  setupMQTT();           // MQTT client, topics
  setupOTA();            // ArduinoOTA (indien enabled)
  buttonSetup(&config.ch[0]); // Button A + dimmer state init
  button2Setup(&config.ch[1]);// Button B + dimmer state init

  Log.println("[BOOT] Setup complete.");
}

void loop() {
  handleMQTT();          // MQTT loop + reconnect + telemetry
  otaHandle();           // OTA handler (indien enabled)
  buttonLoop(&config.ch[0]);  // Input & dim/fade engine (A)
  button2Loop(&config.ch[1]); // Input & dim/fade engine (B)
  handleLogToWebsocket();  // Send buffered logs to web UI
  delay(1);              // Yield to WiFi/RTOS
}