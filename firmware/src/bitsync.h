#ifndef BITSYNC_H
#define BITSYNC_H

// Synchronisation utility for USB keylogger
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


template <unsigned capacity = 64>
class BitSync {
    unsigned char buf[capacity];
    unsigned n {0};

    const unsigned sps {5};
    const unsigned spshalf {2};
    unsigned pos {0};

public:

    void reset() {
        n = 0;
        pos = 0;
    }

    template<typename T>
    void pushBitsFromPacked(T value) {
        const unsigned nbit = sizeof(T) * 8u;
        if(n + nbit > capacity) return;

        for(unsigned shift = 0; shift < nbit; shift++) {
            const unsigned char bit = static_cast<unsigned char>((value >> shift) & 1u);
            buf[n] = bit;
            n++;
        }
    }

    template<typename T>
    void pushBitsFromPackedWithFirstBitInMSB(T value) {
        const unsigned nbit = sizeof(T) * 8u;
        if(n + nbit > capacity) return;

        for(unsigned shift = 0; shift < nbit; shift++) {
            const unsigned char bit = static_cast<unsigned char>((value >> (nbit - shift - 1u)) & 1u);
            buf[n] = bit;
            n++;
        }
    }

    void consume() {
        unsigned num = (pos < n ? pos : n);
        if(num <= 0u) return;

        unsigned nleft = n - num;
        if(nleft > 0u) {
            for(unsigned i = 0; i < nleft; i++)
                buf[i] = buf[num + i]; // copy form back to front
        }
        n = nleft;
        pos -= num;
    }

    // synchronizes the buffered bits and writes output bitstream to target. 
    // differential_output false -> outputs bits[t] 
    // differential_output true -> outputs bits[t] ^ bits[t + sps]
    // inverting_output false -> outputs the bit directly
    // inverting_output true -> negates the output bit
    // usb is differential and inverting since a 0 is a magnitude change, which is differential 1
    template <bool differential_output = true, bool inverting_output = true>
    unsigned sync(unsigned char* target) {
        unsigned iout = 0;

        while(pos + sps < n) {
            unsigned char cur = buf[pos];
            unsigned char next = buf[pos + sps];

            if constexpr (! differential_output) { // non differential
                if constexpr(inverting_output)
                    target[iout] = cur;
                else
                    target[iout] = cur ^ 1;
            }

            if(cur == next) { // no flank, just iter by sps
                if constexpr (differential_output) { // differential with values equal
                    if constexpr(inverting_output)
                        target[iout] = 1;
                    else
                        target[iout] = 0;
                }

                pos += sps;
            } else {
                // else flank change, identify where it is

                if constexpr (differential_output) {  // differential with values not equal
                    if constexpr(inverting_output)
                        target[iout] = 0;
                    else
                        target[iout] = 1;
                }

                unsigned offset = findFirstNonEuqalInRange5(&(buf[pos]), cur);
                pos += offset + spshalf;
            }

            iout++;
        }

        return iout;
    }

    /// This method returns the index of the first value deviating from value
    /// with the premisse, that p[0] is value and p[5] != value ;) see sync method!
    unsigned findFirstNonEuqalInRange5(const unsigned char* p, unsigned char value) const {
        if(p[2] == value) {
            if(p[4] == value)
                return 5;
            if(p[3] == value)
                return 4;
            return 3;
        }
        if(p[1] == value)
            return 2;
        return 1;
    }
};

#endif //BITSYNC_H
