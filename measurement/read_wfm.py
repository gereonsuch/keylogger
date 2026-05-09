#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Test of signal evaluation of an oscilloscope recording for USB keylogger
# See https://github.com/gereonsuch/keylogger for more information
# Copyright (C) 2026 Gereon Such

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


import numpy
import matplotlib.pyplot as plt

filename = 'NewFile1_ctrl_a.wfm' # 10 Msps
inp_samp_rate = 10e6
d = numpy.fromfile(filename, dtype=numpy.uint8)[2822:].astype(numpy.float32)

d = d[:1024*1024]

def resample(d, inp_samp_rate, out_samp_rate):
    inp_t = numpy.arange(d.size)
    step = inp_samp_rate / out_samp_rate
    nout = int(inp_t.size / step)
    out_t = numpy.arange(nout) * step
    return numpy.interp(out_t, inp_t, d)

#d = d[::2]


#d = resample(d, inp_samp_rate, 5e6)
#d = resample(d, 3e6, 6e6) 
#samp_rate = 5e6

samp_rate = inp_samp_rate

#normalize for interpolation
d -= numpy.min(d)
d /= numpy.max(d)
d = d*2 - 1

o = int(1.7974*1e6) - 100
nlen = int(1.8e6 - o) - 1000
#d = d[o:o+nlen]


def measure_baudrate(d, begoffset_portion = 50, samp_rate = 1.):
    begoffset = int(d.size / begoffset_portion)
    a = numpy.abs(numpy.abs(d))
    a = numpy.fft.fft(a)
    a = a[begoffset:a.size//2]
    a = a.real * a.real + a.imag * a.imag
    #a = 10. * numpy.log10(a)
    plt.figure(2)
    plt.clf()
    plt.title('mag fft')
    x = (numpy.arange(a.size) + begoffset) / (begoffset + a.size) / 2 * samp_rate
    plt.plot(x, a)
    
    i = numpy.argmax(a)
    baudrate = x[i] / samp_rate
    sps = 1. / baudrate
    print('using rel baudrate =', baudrate, '->', sps, 'samples per symbol')
    return sps

sps = measure_baudrate(d, samp_rate=samp_rate)
sps = samp_rate / 1.5e6







def intp_gardner(d, sps, alpha = 1.):
    xd = numpy.arange(d.size)
    pos = 0.
    symbols = []
    symbpos = []
    while pos + sps < d.size:
        cur = numpy.interp(pos, xd, d)
        half = numpy.interp(pos + sps/2, xd, d)
        nxt = numpy.interp(pos + sps, xd, d)
        
        error = (nxt - cur) * half
        deviation = error * alpha
        
        #limit deviation
        deviation = min(deviation, sps/2)
        deviation = max(deviation, -sps/2)
        
        pos += sps - deviation
        
        symbpos.append(pos)
        symbols.append(float(numpy.interp(pos, xd, d)))
               

    return symbpos, symbols

def intp_zerocrossing(d, sps, minval = 0.25):
    pos = 0.
    symbols = []
    symbpos = []
    #uncorpositions = []
    xd = numpy.arange(d.size)
    
    while pos + sps < d.size:
        chunk = d[ int(pos) : int(pos + sps + 1) ]
        sgn = numpy.signbit(chunk)
        
        zerocrossing = 0.
        for i in range(chunk.size - 1):
            if(sgn[i] != sgn[i + 1]):
                m = ( chunk[i+1] - chunk[i] ) # / 1
                zerocrossing = -chunk[i] / m
                zerocrossing += i
                break
        
        if zerocrossing > 0.:
            pos = int(pos) + zerocrossing - sps/2.
        
        symbpos.append(pos)
        symbols.append(float(numpy.interp(pos, xd, d)))
        
        pos += sps
    
    return symbpos, symbols #, uncorpositions








#symbpos, symbols = intp_gardner(d, sps)
symbpos, symbols = intp_zerocrossing(d, sps)




'''


print(d.size / 1e6, 'M samples')
plt.figure(1)
plt.clf()
plt.title('samples')

plt.plot(d)
#plt.plot(symbpos, symbols, 'x')

#for x in uncorpositions:
#    plt.plot([x,x], [1, -1], '-b')


plt.show()

'''



#'''

#everything to bit file
bits = [0 if v < 0. else 1 for v in symbols]
while len(bits) % 8 != 0:
    del bits[-1]
bits = numpy.packbits(numpy.array(bits, dtype=numpy.uint8))
bits.tofile(filename + '.bits')


def divide_into_packets(symbols, upperthresh = 0.875, lowerthresh = 0.6):
    packets = []
    curpacket = []
    for v in symbols:
        if v > lowerthresh and v < upperthresh: # break paket
            if len(curpacket) > 0:
                packets.append(curpacket)
                curpacket = []
            continue
        # else normal symbol
        curpacket.append(1 if v < 0 else 0)
    #final push
    if len(curpacket) > 0:
        packets.append(curpacket)
    return packets

def decode_bits(bits):
    # usb implements non return to zero -> differential and on 6 ones, one zero is added
    out = []
    onecount = 0
    prev = bits[0]
    for b in bits[1:]:
        c = b ^ prev ^ 1
        prev = b
        
        drop = True if onecount >= 6 else False
        if c:
            onecount += 1
        else:
            onecount = 0
            
        if not drop:
            out.append(c)
    return out
        
            
def packet_to_hex(packet, diff = True):
    s = ''
    count = 0
    buf = 0
    prev = 0
    for bit in packet:
        if diff:
            buf = (buf << 1) ^ (bit ^ prev ^ 1)
        else:
            buf = (buf << 1) ^ bit
        count += 1
        if count >= 4:
            s += hex(buf)[2:]
            buf = 0
            count = 0
        prev = bit
    if count > 0: # adjust left
        while count < 4:
            buf = (buf << 1)
            count += 1
        s += hex(buf)[2:]
    return s
            

packets = divide_into_packets(symbols)
for p in packets:
    print(packet_to_hex(p))


#'''

