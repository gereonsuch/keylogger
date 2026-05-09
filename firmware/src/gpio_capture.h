// gpio configuration header for USB keylogger
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

#ifndef GPIO_CAPTURE_H
#define GPIO_CAPTURE_H


#include <stdint.h>
#include <stdbool.h>

#define CAPTURE_PIN 2
#define SYS_CLK_HZ 150000000u
#define SAMPLE_RATE_HZ 7500000u
#define DMA_BUFFER_WORDS 8192u
#define DMA_BUFFER_BYTES (DMA_BUFFER_WORDS * 4u)
#define FILL_TIME_US ((DMA_BUFFER_WORDS * 32u) / (SAMPLE_RATE_HZ / 1000000u))


// buffers for memory interaction
extern uint32_t capture_buf[2][DMA_BUFFER_WORDS];

/// initialization
void capture_begin(void);

/// \returns if the buffer with given index is ready
bool capture_buffer_ready(uint8_t *idx);

/// Signal that the current buffer was consumed
void capture_buffer_consumed(void);

// get number of overruns, mainly for debugging purpose...
uint32_t capture_overrun_count(void);

#endif // GPIO_CAPTURE_H
