#include <pico/stdlib.h>
#include "hires_color_patterns.h"
#include "hires_dot_patterns.h"
#include "vgabuf.h"
#include "render.h"
#include "vgaout.h"


static void render_hires_line(uint line);


static uint hires_line_to_mem_offset(uint line) {
    return ((line & 0x07) << 10) | ((line & 0x38) << 4) | (((line & 0xc0) >> 6) * 40);
}


void __time_critical_func(render_hires)() {
    vga_prepare_frame();

    render_border();

    for(uint line=0; line < 192; line++) {
        render_hires_line(line);
    }

    render_border();
}


void __time_critical_func(render_mixed_hires)() {
    vga_prepare_frame();

    render_border();

    for(uint line=0; line < 160; line++) {
        render_hires_line(line);
    }

    if(terminal_switches & TERMINAL_80COL) {
        for(uint line=20; line < 24; line++) {
            render_terminal_line(line);
        }
    } else {
        for(uint line=20; line < 24; line++) {
            render_text_line(line);
        }
    }

    render_border();
}


static void __time_critical_func(render_hires_line)(uint line) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;

    const uint8_t *page = (const uint8_t *)((soft_switches & SOFTSW_PAGE_2) ? hgr_p2 : hgr_p1);
    const uint8_t *line_mem = page + hires_line_to_mem_offset(line);

    // Pad 40 pixels on the left to center horizontally
    while(sl_pos < 40/8) {
        sl->data[sl_pos] = (text_border|THEN_EXTEND_3) | ((text_border|THEN_EXTEND_3) << 16); // 8 pixels per word
        sl_pos++;
    }

    // Each hires byte contains 7 pixels which may be shifted right 1/2 a pixel. That is
    // represented here by 14 'dots' to precisely describe the half-pixel positioning.
    //
    // For each pixel, inspect a window of 8 dots around the pixel to determine the
    // precise dot locations and colors.
    //
    // Dots would be scanned out to the CRT from MSB to LSB (left to right here):
    //
    //            previous   |        next
    //              dots     |        dots
    //        +-------------------+--------------------------------------------------+
    // dots:  | 31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | ... | 14 | 13 | 12 | ...
    //        |              |         |              |
    //        \______________|_________|______________/
    //                       |         |
    //                       \_________/
    //                         current
    //                          pixel
    uint32_t dots = 0;
    uint oddness = 0;
    uint i;

    // Load in the first 14 dots
    dots |= (uint32_t)hires_dot_patterns[line_mem[0]] << 15;

    for(i=1; i < 41; i++) {
        // Load in the next 14 dots
        uint b = (i < 40) ? line_mem[i] : 0;
        if(b & 0x80) {
            // Extend the last bit from the previous byte
            dots |= (dots & (1u << 15)) >> 1;
        }
        dots |= (uint32_t)hires_dot_patterns[b] << 1;

        // Consume 14 dots
        for(uint j=0; j < 7; j++) {
            uint dot_pattern = oddness | ((dots >> 24) & 0xff);
            sl->data[sl_pos] = hires_color_patterns[dot_pattern];
            sl_pos++;
            dots <<= 2;
            oddness ^= 0x100;
        }
    }

    for(i=0; i < 40/8; i++) {
        sl->data[sl_pos] = (text_border|THEN_EXTEND_3) | ((text_border|THEN_EXTEND_3) << 16); // 8 pixels per word
        sl_pos++;
    }

    sl->length = sl_pos;
    sl->repeat_count = 1;
    vga_submit_scanline(sl);
}
