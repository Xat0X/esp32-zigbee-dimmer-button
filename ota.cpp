#include "ota.h"
#include "config.h"
#include "version.h"
#include <ArduinoOTA.h>

static bool otaReady = false;

void setupOTA() {
  if (!config.otaEnabled) {
    Serial.println("[OTA] Disabled in config"); otaReady = false; return;
  }

  ArduinoOTA.setHostname(config.mdns_hostname.c_str());
  if (config.ota_password.length()) {
    ArduinoOTA.setPassword(config.ota_password.c_str());
  }

  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("[OTA] Start updating " + type);
  });
  ArduinoOTA.onEnd([](){ Serial.println("\n[OTA] Update complete"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]\n", error);
  });

  ArduinoOTA.begin();
  otaReady = true;
  Serial.println("[OTA] Ready. Use Arduino IDE to upload via network.");
}

void otaHandle() { if (otaReady && config.otaEnabled) ArduinoOTA.handle(); }