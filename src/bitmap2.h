// Commander X16 Emulator
// SDRAM-backed bitmap layer ("VERA_2") -- $9F60-$9F6F.
//
// Mirrors the X16-MiSTer core's optional 640x480 4bpp/8bpp linear framebuffer
// (see vera_2.md in the core repo).  A full-screen bitmap composited OVER
// VERA's output.  Not real X16 hardware; enable with the -bitmap2 flag.
//
// Copyright (c) 2026 Jean-Yves Vinet.  BSD-2-Clause (same terms as the emulator).

#ifndef _BITMAP2_H_
#define _BITMAP2_H_

#include <stdint.h>
#include <stdbool.h>

void     bitmap2_reset(void);
uint8_t  bitmap2_read(uint8_t reg, bool debugOn);
void     bitmap2_write(uint8_t reg, uint8_t value);
bool     bitmap2_active(void);                      // enabled & valid mode
bool     bitmap2_passthru(void);                    // CTRL[3]: VERA sprites over bitmap
uint32_t bitmap2_color_at(uint16_t x, uint16_t y);  // 0x00RRGGBB for the composite

#endif
