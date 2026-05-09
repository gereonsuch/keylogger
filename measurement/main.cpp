// Measurement processing for USB keylogger
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


// this file processes a measurement file of an oscilloscope fromt he USB D- lane
// the signal is clock synchronized, then the bits are interpreted and
// decoded as HID packets. 

#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <optional>
#include <math.h>

void readUint8Values(std::vector<float>& samples, const std::string& filename, size_t skip_front = 2822) {
    std::ifstream fh;
    fh.open(filename, std::ios::in | std::ios::binary);

    while (1) {
        char buf[1024];
        fh.read(buf, 1024);
        size_t n = fh.gcount();
        if (n > 1024) break;

        for (size_t i = std::min(n, skip_front); i < n; i++)
            samples.push_back(static_cast<float>(static_cast<unsigned char>(buf[i])));
        skip_front -= std::min(n, skip_front);
        if (fh.eof()) break;
    }
}

void normalize(std::vector<float>& samples) {
    if (samples.empty()) return;

    auto min = *std::min_element(samples.begin(), samples.end());
    for (auto& v: samples)
        v -= min;

    auto max = *std::max_element(samples.begin(), samples.end());
    for (auto& v: samples)
        v = (v / max) * 2 - 1; // from +1 to -1
}

void synchronizeSymbolsFromZeroCrossing(const std::vector<float>& samples, double sps, std::vector<float>& symbols, std::vector<double>* positions = nullptr) {
    if (! std::isfinite(sps) || sps <= 0.) return;
    double pos = 0.;

    while (pos + 2 * sps < samples.size()) {

        size_t measure_beg = static_cast<size_t>(std::max(0., pos));
        size_t measure_end = std::min<size_t>(measure_beg + sps + 1, samples.size());

        std::optional<double> zerocrossing;

        for (size_t i = measure_beg + 1; i < measure_end; i++) {
            auto& prev = samples[i - 1];
            auto& next = samples[i];

            if (std::signbit(prev) != std::signbit(next)) {
                auto m = next - prev; // attenuation, (yb - ya) / (xb - xa) with xb=1 und xa=0
                auto rel_pos = static_cast<double>(-prev / m);
                zerocrossing = (i - 1) - measure_beg + rel_pos;
                break;
            }
        }

        if (zerocrossing.has_value())
            pos += zerocrossing.value() - sps/2;

        if (pos >= 0. && pos < samples.size()) {
            symbols.push_back(samples[static_cast<size_t>(pos)]); // interpolate by floor integer... interp linear?
            if (positions)
                positions->push_back(pos);
        }

        pos += sps;
    }
}

void decodeSymbolsToPackets(const std::vector<float>& symbols, std::vector<std::vector<unsigned char>>& packets, float upperthresh = 0.875, float lowerthresh = 0.6) {
    size_t num_bits_in_current_packet = 0; // packet ends if value drops below thresh
    size_t no_edge_count = 0; // one is no magnitude change
    std::optional<unsigned char> prev;

    packets.push_back({});

    for (auto& v: symbols) {

        if (v > lowerthresh && v < upperthresh) { // end of packet
            if (! packets.back().empty())
                packets.push_back({});
            no_edge_count = 0;
            num_bits_in_current_packet = 0;
            prev.reset();
            continue;
        }

        unsigned char curbit = v > 0 ? 1 : 0;

        if (prev.has_value()) {
            auto diff = curbit ^ prev.value();

            if (no_edge_count >= 6) { // drop current value
                no_edge_count = 0;
            } else {
                packets.back().push_back(diff ^ 1);

                if (diff)
                    no_edge_count = 0;
                else
                    no_edge_count++;
            }
        }
        prev = curbit;
    }

}


template <typename T>
void tofile(const std::string& filename, const std::vector<T>& values) {
    std::ofstream fh;
    fh.open(filename, std::ios::out | std::ios::binary);
    fh.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
}

bool checkCRC16(const unsigned char* bits, size_t bitlen, unsigned ref_crc) {
    unsigned poly = 0x8005;
    unsigned reg = 0xffff; // start with all one

    for (size_t i = 0; i < bitlen; i++) {
        unsigned char bit = bits[i];
        unsigned char msb = ((reg & 0x8000) != 0) ? 1 : 0;
        reg = ((reg << 1) & 0xffff);

        if (bit != msb)
            reg ^= poly;
    }
    reg ^= 0xffff; // invert at end
    return reg == ref_crc;
}

template <typename T>
T packBitsToValue(const unsigned char* p, size_t bitlen) {
    T result = 0;
    for (size_t i = 0; i < bitlen; i++)
        result = ((result << 1) ^ static_cast<T>(p[i] & 1));
    return result;
}

// minlen = 8 + 64 +16
void evalHUDPacket(const unsigned char* bits) {
    unsigned PID = packBitsToValue<unsigned>(bits, 4);
    unsigned PID_inv = packBitsToValue<unsigned>(bits + 4, 4);
    bool PID_ok = (PID ^ PID_inv) == 0xf;

    unsigned crc_recvd = packBitsToValue<unsigned>(bits + 8 + 64, 16);
    bool crc_ok = checkCRC16(bits + 8, 64, crc_recvd);

    std::cout << "PID: " << PID << ", PID ok: " << PID_ok << ", crc: " << crc_recvd << ", crc_ok: " << crc_ok << std::endl;

    if (crc_ok) {
        unsigned char words[8]; // 8 data words with each 8 bit, lsb first
        for (size_t w = 0; w < 8; w++) {
            unsigned char& word = words[w];
            word = 0;
            const unsigned char* beg = bits + 8 + w * 8ull;
            for (size_t b = 0; b < 8; b++)
                word = (word << 1) ^ beg[7-b];
            if (word != 0)
                std::cout << "   word " << w << " -> " << static_cast<unsigned>(word) << std::endl;
        }
    }
}

int main() {

    std::vector<float> samples;
    //std::string filename = "/home/gereon/proj/usb_dec/NewFile5.wfm";
    std::string filename = "/home/gereon/proj/usb_dec/NewFile1_ctrl_a.wfm";

    readUint8Values(samples, filename);
    normalize(samples);

    std::cout << "read " << samples.size() << " samples" << std::endl;

    //double sps = 84.43570375206853;
    double sps = 6.587625538492616;

    std::vector<float> symbols;
    synchronizeSymbolsFromZeroCrossing(samples, sps, symbols);


    std::cout << "got " << symbols.size() << " symbols" << std::endl;

    tofile(filename + ".symbols.f32", symbols);

    std::vector<std::vector<unsigned char>> packets;
    decodeSymbolsToPackets(symbols, packets);

    for (auto& packet: packets) {
        if (packet.size() < 6) continue;

        if (packet.size() < 40) continue;

        /*for (unsigned char b: packet)
            std::cout << (b ? '1' : '0');
        std::cout << std::endl;
        continue;*/

        // expect 0000 0001 at front, but first 0 may be lost. so sync at least 5 zeros, then one 1

        size_t i = 0;
        for (; i < packet.size(); i++)
            if (packet[i] != 0)
                break;
        if (i >= packet.size() || i < 6 || i > 7) continue;
        i++;

        size_t n_left = packet.size() - i;
        if (n_left < 8) continue;

        size_t hud_packet = 8 + 64 + 16;
        if (n_left == hud_packet + 2 || n_left == hud_packet + 3) { // 2 or 3 eop
            std::cout << std::endl << "HUD Packet: " << std::endl;

            for (size_t k = i; k < packet.size(); k++)
                std::cout << (packet[k] ? '1' : '0');
            std::cout << std::endl;
            evalHUDPacket(packet.data() + i);
            continue;
        }

        continue;

        std::cout << "sz: " << n_left << " -> ";

        // sync skipped

        for (size_t count = 0; count < 8 && i < packet.size(); i++, count++)
            std::cout << (packet[i] ? '1' : '0');
        std::cout << ' ';
        for (size_t count = 0; count < 7 && i < packet.size(); i++, count++)
            std::cout << (packet[i] ? '1' : '0');
        std::cout << ' ';
        for (size_t count = 0; count < 4 && i < packet.size(); i++, count++)
            std::cout << (packet[i] ? '1' : '0');
        std::cout << ' ';
        for (size_t count = 0; count < 5 && i < packet.size(); i++, count++)
            std::cout << (packet[i] ? '1' : '0');
        std::cout << ' ';
        for (size_t count = 0; count < 3 && i < packet.size(); i++, count++)
            std::cout << (packet[i] ? '1' : '0');

        std::cout << ' ';
        for (; i < packet.size(); i++)
            std::cout << (packet[i] ? '1' : '0');

        std::cout << std::endl;
    }

    return 0;
}
