#pragma once
#include <Arduino.h>
#include "config.h"

// A simple ring buffer for logs
class LogBuffer {
public:
    LogBuffer(size_t size);
    ~LogBuffer();
    void put(char c);
    String read();
    bool available();
private:
    char*  _buffer;
    size_t _size;
    size_t _head;
    size_t _tail;
    bool   _full;
};

// A Print class that writes to Serial and the LogBuffer
class Logger : public Print {
public:
    Logger();
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);
    String readLog();
    bool   logAvailable();
};

extern Logger Log;

void handleLogToWebsocket();