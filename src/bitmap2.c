// Commander X16 Emulator
// SDRAM-backed bitmap layer ("VERA_2") -- $9F60-$9F6F.  See bitmap2.h / vera_2.md.
//
// Copyright (c) 2026 Jean-Yves Vinet.  BSD-2-Clause (same terms as the emulator).

#include "bitmap2.h"
#include <string.h>

// 1 MB linear framebuffer (20-bit byte pointer).  8bpp displays the first
// 300 KB; the rest is save-under scratch (a full-screen save-under = 600 KB
// fits) + headroom for future use.
#define BMP2_FB_SIZE (1u << 20)
#define BMP2_FB_MASK (BMP2_FB_SIZE - 1u)

static uint8_t  bmp_fb[BMP2_FB_SIZE];

static bool     bmp_enable;
static uint8_t  bmp_mode;          // 1 = 640x480x8bpp, 2 = 640x480x4bpp
static bool     bmp_passthru;      // CTRL[3]: VERA sprites/opaque over the bitmap
static uint32_t bmp_ptr;           // 20-bit read/write pointer (shared; blit src)
static uint8_t  bmp_incr;          // ADDR_H[7:4]: DATA auto-increment stride select
static uint32_t bmp_blit_dst;      // blit destination byte address
static uint32_t bmp_blit_len;      // blit length in bytes
static uint8_t  bmp_pal_lo;        // latched {G,B} between PAL_LO and PAL_HI
static uint8_t  bmp_pal_cursor;    // palette write cursor
static uint16_t bmp_pal[256];      // RGB444 entries
static uint32_t bmp_pal_bgra[256]; // precomputed 0x00RRGGBB (framebuffer format)

// DATA auto-increment stride, selected by ADDR_H[7:4] -- the same bit position
// as VERA's ADDRx_H increment field, but this table is SIGNED so both
// directions fit in one field (VERA needs a separate DECR bit).  Index 0 = +1,
// so "set the pointer and stream" is the default.  Mirrors the `step` table in
// rtl/bitmap_regs.sv -- keep the two in sync.
static const int16_t bmp_step[16] = {
	   1,      // 0: linear streaming (reset default)
	   0,      // 1: hold -- 4bpp read-modify-write, re-read the same byte
	   2, 4, 8, 16, 32, 64, 128, 256,
	 320,      // A: 4bpp row stride, one pixel down
	 640,      // B: 8bpp row stride, one pixel down
	  -1,      // C: reverse streaming / overlapping copy
	  -2,
	-320,      // E: 4bpp row stride, one pixel up
	-640       // F: 8bpp row stride, one pixel up
};

static void
bmp_advance(void)
{
	bmp_ptr = (bmp_ptr + (uint32_t)bmp_step[bmp_incr]) & BMP2_FB_MASK;
}

static void
recalc_pal_entry(uint8_t i)
{
	uint16_t e = bmp_pal[i];
	uint32_t r = (e >> 8) & 0xf; r = (r << 4) | r;   // 4-bit -> 8-bit (like VERA)
	uint32_t g = (e >> 4) & 0xf; g = (g << 4) | g;
	uint32_t b =  e       & 0xf; b = (b << 4) | b;
	bmp_pal_bgra[i] = (r << 16) | (g << 8) | b;
}

void
bitmap2_reset(void)
{
	bmp_enable = false;
	bmp_mode = 0;
	bmp_passthru = false;
	bmp_ptr = 0;
	bmp_incr = 0;
	bmp_blit_dst = 0;
	bmp_blit_len = 0;
	bmp_pal_lo = 0;
	bmp_pal_cursor = 0;
	memset(bmp_fb, 0, sizeof bmp_fb);
	for (int i = 0; i < 256; i++) {
		bmp_pal[i] = 0;
		bmp_pal_bgra[i] = 0;
	}
}

uint8_t
bitmap2_read(uint8_t reg, bool debugOn)
{
	switch (reg & 0xf) {
		case 0x0: // CTRL: {passthru, mode, enable}
			return (uint8_t)((bmp_passthru ? 8 : 0) | (bmp_mode << 1) | (bmp_enable ? 1 : 0));
		case 0x1: return 0xB5;                                              // ID
		case 0x2: return (uint8_t)( bmp_ptr        & 0xff);                 // ADDR_L
		case 0x3: return (uint8_t)((bmp_ptr >>  8) & 0xff);                 // ADDR_M
		case 0x4: return (uint8_t)((bmp_incr << 4) | ((bmp_ptr >> 16) & 0xf)); // ADDR_H
		case 0x5: { // DATA read-back: byte at the pointer, then advance by the stride
			uint8_t v = bmp_fb[bmp_ptr & BMP2_FB_MASK];
			if (!debugOn) bmp_advance();
			return v;
		}
		default:  return 0x00;
	}
}

void
bitmap2_write(uint8_t reg, uint8_t value)
{
	switch (reg & 0xf) {
		case 0x0: // CTRL
			bmp_enable   = value & 1;
			bmp_mode     = (value >> 1) & 3;
			bmp_passthru = (value >> 3) & 1;
			break;
		case 0x2: bmp_ptr = (bmp_ptr & 0xFFF00u) |  (uint32_t)value;                break; // ADDR_L
		case 0x3: bmp_ptr = (bmp_ptr & 0xF00FFu) | ((uint32_t)value << 8);          break; // ADDR_M
		case 0x4: // ADDR_H = {incr[3:0], ptr[19:16]} -- one store sets both
			bmp_ptr  = (bmp_ptr & 0x0FFFFu) | ((uint32_t)(value & 0xF) << 16);
			bmp_incr = (value >> 4) & 0xF;
			break;
		case 0x5: // DATA -> framebuffer, then advance by the stride
			bmp_fb[bmp_ptr & BMP2_FB_MASK] = value;
			bmp_advance();
			break;
		case 0x6: bmp_pal_cursor = value; break;   // PAL_IDX
		case 0x7: bmp_pal_lo = value;     break;   // PAL_LO {G,B}
		case 0x8: // PAL_HI {R} -> commit {R,G,B}, cursor++
			bmp_pal[bmp_pal_cursor] = ((uint16_t)(value & 0xf) << 8) | bmp_pal_lo;
			recalc_pal_entry(bmp_pal_cursor);
			bmp_pal_cursor++;
			break;
		case 0x9: bmp_blit_dst = (bmp_blit_dst & 0xFFF00u) |  (uint32_t)value;                break; // BDST_L
		case 0xA: bmp_blit_dst = (bmp_blit_dst & 0xF00FFu) | ((uint32_t)value << 8);          break; // BDST_M
		case 0xB: bmp_blit_dst = (bmp_blit_dst & 0x0FFFFu) | ((uint32_t)(value & 0xF) << 16); break; // BDST_H
		case 0xC: bmp_blit_len = (bmp_blit_len & 0xFFF00u) |  (uint32_t)value;                break; // BLEN_L
		case 0xD: bmp_blit_len = (bmp_blit_len & 0xF00FFu) | ((uint32_t)value << 8);          break; // BLEN_M
		case 0xE: bmp_blit_len = (bmp_blit_len & 0x0FFFFu) | ((uint32_t)(value & 0xF) << 16); break; // BLEN_H
		case 0xF: // BCTRL: start blit (SDRAM->SDRAM copy, matches the byte-wise HW blit)
			if (value & 1) {
				uint32_t s = bmp_ptr      & BMP2_FB_MASK;
				uint32_t d = bmp_blit_dst & BMP2_FB_MASK;
				uint32_t l = bmp_blit_len;
				if (s + l > BMP2_FB_SIZE) l = BMP2_FB_SIZE - s;
				if (d + l > BMP2_FB_SIZE) l = BMP2_FB_SIZE - d;
				if (l) memmove(bmp_fb + d, bmp_fb + s, l);
			}
			break;
		default: break;
	}
}

bool
bitmap2_active(void)
{
	return bmp_enable && (bmp_mode == 1 || bmp_mode == 2);
}

bool
bitmap2_passthru(void)
{
	return bmp_passthru;
}

uint32_t
bitmap2_color_at(uint16_t x, uint16_t y)
{
	uint8_t idx;
	if (bmp_mode == 1) {                          // 8bpp
		uint32_t off = (uint32_t)y * 640u + x;
		idx = bmp_fb[off & BMP2_FB_MASK];
	} else {                                      // 4bpp: hi nibble = left px
		uint32_t off = (uint32_t)y * 320u + (x >> 1);
		uint8_t byte = bmp_fb[off & BMP2_FB_MASK];
		idx = (x & 1) ? (byte & 0x0f) : (byte >> 4);
	}
	return bmp_pal_bgra[idx];
}
