#pragma once

#include <ArduinoLog.h>

class LogBufferPrinter : public Print {
    public:
      String buffer;
    
      size_t write(uint8_t ch) override;
    
      size_t write(const uint8_t *data, size_t len) override;
};