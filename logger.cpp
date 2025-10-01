#include "logger.h"
#include "web.h"
#include <ArduinoJson.h>

// ======== Ring Buffer Implementation ========
LogBuffer::LogBuffer(size_t size) {
    _buffer = new char[size];
    _size = size;
    _head = 0;
    _tail = 0;
    _full = false;
}

LogBuffer::~LogBuffer() {
    delete[] _buffer;
}

void LogBuffer::put(char c) {
    if (_buffer == nullptr) return;
    _buffer[_head] = c;
    if (_full) {
        _tail = (_tail + 1) % _size;
    }
    _head = (_head + 1) % _size;
    _full = (_head == _tail);
}

String LogBuffer::read() {
    if (!_full && (_head == _tail)) {
        return ""; // Empty
    }

    String result = "";
    result.reserve(128);
    while (_tail != _head) {
        char c = _buffer[_tail];
        result += c;
        _tail = (_tail + 1) % _size;
    }
    _full = false;
    return result;
}

bool LogBuffer::available() {
    return (_full || (_head != _tail));
}

// ======== Logger Implementation ========
static LogBuffer logBuffer(2048); // 2KB buffer for logs

Logger::Logger() {}

size_t Logger::write(uint8_t c) {
    if (config.debugEnabled) {
        logBuffer.put((char)c);
    }
    return Serial.write(c);
}

size_t Logger::write(const uint8_t *buffer, size_t size) {
    if (config.debugEnabled) {
        for (size_t i = 0; i < size; i++) {
            logBuffer.put((char)buffer[i]);
        }
    }
    return Serial.write(buffer, size);
}

String Logger::readLog() {
    return logBuffer.read();
}

bool Logger::logAvailable() {
    return logBuffer.available();
}

// ======== Global Logger Instance ========
Logger Log;

// ======== WebSocket Handler ========
void handleLogToWebsocket() {
    static unsigned long lastLogSend = 0;
    unsigned long now = millis();

    if (ws.count() > 0 && Log.logAvailable() && (now - lastLogSend > 150)) {
        String logData = Log.readLog();
        if (logData.length() > 0) {
            DynamicJsonDocument doc(logData.length() + 64);
            doc["type"] = "log";
            doc["data"] = logData;
            
            String json;
            serializeJson(doc, json);
            ws.textAll(json);
            lastLogSend = now;
        }
    }
}