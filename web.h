#pragma once

#include <ESPAsyncWebServer.h>

void setupWebServer();
void wsPushState();

// Declare 'ws' so other files (like logger.cpp) know about it.
extern AsyncWebSocket ws;