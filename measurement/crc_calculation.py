#!/usr/bin/env python3
# -*- coding: utf-8 -*-


# Test of crc caulculation for USB keylogger
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

def toBits(w, n = 16):
    s = ''
    for i in range(n):
        s += '0' if ((w >> i) & 1) == 0 else '1'
    return s[::-1]

def tolen(s, n = 8):
    if len(s) >= n: 
        return s
    return s + (' ' * ( n - len(s) ))


word = '000000011101001010000000000000000010000000000000000000000000000000000000000000001111111000111101010'
word = word[16:]
bits = word[:64]

crc = word[64:][:16]

print(bits)
print(crc)


poly = 0x8005
reg = 0xffff

#print(tolen('init'), toBits(reg))


for i, bit in enumerate(1 if b == '1' else 0 for b in bits):
    msb = ((reg >> 15) & 1)
    
    print(tolen(str(i)), toBits(reg), '/ msb =', msb, '/ next bit =', bit, end = '')
    
    reg = ((reg<<1) & 0xffff)
    
    
    applied = False
    if bit != msb:
        reg ^= poly
        applied = True
        print(' -> apply polynomial')
    else: print(' ')

print(tolen(str(len(bits))), toBits(reg))
    
    
reg ^= 0xffff

print(tolen('inverted'), toBits(reg))

