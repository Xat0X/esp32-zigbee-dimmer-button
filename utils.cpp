#include "utils.h"
#include <WiFi.h>

String chipIdHex() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t id  = (uint32_t)(mac & 0xFFFFFFULL);
  char buf[7];
  snprintf(buf, sizeof(buf), "%06X", id);
  return String(buf);
}