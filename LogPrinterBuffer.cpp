#include "LogPrinterBuffer.h"


size_t LogBufferPrinter::write(uint8_t ch) {
        Serial.write(ch);  // still print to Serial
        buffer += (char)ch;
        if (buffer.length() > 10000) {
          int trim_index = buffer.indexOf('\n');
          if(trim_index >= 0 )
            buffer.remove(0, trim_index+1);  // keep it trimmed
        }
        return 1;
}
    
size_t LogBufferPrinter::write(const uint8_t *data, size_t len){
        for (size_t i = 0; i < len; i++) {
          write(data[i]);
        }
        return len;
}

    
