// gpio configuration implementation for USB keylogger
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

#include "gpio_capture.h"
#include "gpio_capture.pio.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

#include <Arduino.h>

uint32_t capture_buf[2][DMA_BUFFER_WORDS];

static PIO pio_inst;
static uint pio_sm;
static uint pio_offset;

static int dma_chan[2];

// markers for runtime behaviour of PIO routine
static volatile uint8_t ready_idx = 0xFF;
static volatile bool ready_flag = false;
static volatile uint8_t isr_count = 0;
static volatile uint32_t overrun_cnt = 0;


// interrupt routine when one channel has finished, trigger transfer to next half-buffer
static void dma_irq_handler(void) {
    for (int ch = 0; ch < 2; ch++) {
        if (dma_channel_get_irq0_status(dma_chan[ch])) {
            dma_channel_acknowledge_irq0(dma_chan[ch]);

            //reset write address for next round
            dma_channel_set_write_addr(dma_chan[ch], capture_buf[ch], false);

            if (ready_flag) overrun_cnt++; // previous buffer not consumed yet. 
            ready_idx = (uint8_t)ch;
            ready_flag = true;
        }
    }
}

// configure PIO state machine
static void pio_setup(void) {
    pio_inst = pio0; // pio1, pio2 also available on RP235x
    pio_offset = pio_add_program(pio_inst, &gpio_capture_program);
    pio_sm = pio_claim_unused_sm(pio_inst, true);

    pio_sm_config c = gpio_capture_program_get_default_config(pio_offset);

    // setup capture pin
    sm_config_set_in_pins(&c, CAPTURE_PIN);
    pio_gpio_init(pio_inst, CAPTURE_PIN);
    pio_sm_set_consecutive_pindirs(pio_inst, pio_sm, CAPTURE_PIN, 1, false);

    // sm_config_set_in_shift(pio_sm_config *, bool shift_direction, bool autopush, uint push_threshold)
    // shift_direction false-> LSB first in register
    // autopush true -> if threshold is reached, push value to RX-FIFO
    // push threshold = 32 -> after 32 bit trigger autopush
    sm_config_set_in_shift(&c, false, true, 32);

    // merge RX and TX FIFO with each 4x32 bit
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // clock divider so bits are captured only at target sample rate
    float clkdiv = (float)SYS_CLK_HZ / (float)SAMPLE_RATE_HZ;
    sm_config_set_clkdiv(&c, clkdiv);

    pio_sm_init(pio_inst, pio_sm, pio_offset, &c);
}

// setup direct memory access and automatic buffer switching
static void dma_setup(void) {
    dma_chan[0] = dma_claim_unused_channel(true);
    dma_chan[1] = dma_claim_unused_channel(true);

    uint dreq = pio_get_dreq(pio_inst, pio_sm, false); // false = RX

    for (int i = 0; i < 2; i++) {
        dma_channel_config c = dma_channel_get_default_config(dma_chan[i]);

        channel_config_set_transfer_data_size(&c, DMA_SIZE_32); // 32 bit width
        channel_config_set_read_increment(&c, false); // no read increment... actually no reading.
        channel_config_set_write_increment(&c, true); // write each 32 bit element afer another
        channel_config_set_dreq(&c, dreq); // setup trigger for dma, only when pio buffer is full the transfer to memory is done
        channel_config_set_chain_to(&c, dma_chan[1 - i]); // switch buffer states, 0->1, 1->0

        dma_channel_configure(dma_chan[i], &c,
            capture_buf[i], // target = buffer i
            &pio_inst->rxf[pio_sm], // src = rx buffer
            DMA_BUFFER_WORDS, // bufferlen
            false // not startet yet
        );

        dma_channel_set_irq0_enabled(dma_chan[i], true); // activate interrupt
    }

    // register interrupt handler
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void capture_begin(void) {
    // align system clock
    set_sys_clock_khz(SYS_CLK_HZ / 1000, true);

    pio_setup();
    dma_setup();

    // start channel 0, channel_config_set_chain_to then triggers next one automatically 
    dma_channel_start(dma_chan[0]);

    // start pio state machine
    pio_sm_set_enabled(pio_inst, pio_sm, true);
}

bool capture_buffer_ready(uint8_t *idx) {
    if (!ready_flag) return false; // buffer not ready yet
    *idx = ready_idx; // ready idx written in interrupt, only captured here
    return true;
}

void capture_buffer_consumed(void) {
    ready_flag = false; // signal for the next pio transfer interrupt that the current buffer was processed
}

uint32_t capture_overrun_count(void) {
    return overrun_cnt;
}
