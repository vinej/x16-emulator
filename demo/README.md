# VERA_2 bitmap-layer demos

Example programs for the SDRAM bitmap layer (`$9F60`–`$9F6F`) added by this fork.
See [`../vera_2.md`](../vera_2.md) for the full register spec. Each demo
feature-detects `$9F61`, so the same `.PRG` runs on this emulator **and** on the
X16-MiSTer FPGA core.

| Source | PRG | What it shows |
|---|---|---|
| `vera2fill.s` | `VERA2FILL.PRG` | Switch to 8bpp, fill the whole screen fast with the **blit** (doubling a 16-colour seed), wait for a key, return to BASIC. |
| `vera2incr.s` | `VERA2INCR.PRG` | The **auto-increment stride** (`$9F64[7:4]`): vertical lines drawn with stride **+640**, and a rectangle outline drawn by walking the perimeter with `+1`, `+640`, `-1`, `-640` from a single pointer load. Self-tests the stride first and says so if your build predates it. |
| `vera2demo.s` | `VERA2DEMO.PRG` | Auto-cycles between an 8bpp 256-colour diagonal gradient and 4bpp 16-colour bands. |
| `vera2sprites.s` | `VERA2SPRITES.PRG` | 8bpp gradient + random **VERA sprites** + **mouse** (passthru), full-screen. |
| `vera2blit.s` | `VERA2BLIT.PRG` | Everything above plus **save-under**: **left-click** the gradient drops a message box (band saved to scratch via the blit), **click the box** to restore it exactly. |

`vera2demo.cfg` is the cc65 linker config all five use (a minimal `$0801` PRG with
a BASIC `SYS` stub).

> ⚠️ **These `.PRG`s need a build with the auto-increment stride** (`$9F64` =
> `{incr[3:0], ptr[19:16]}`). On an older `x16emu` or FPGA bitstream
> `VERA2INCR` prints a warning instead of drawing; the others still work, since
> they use the default `+1` stride. See the breaking-change note in
> [`../vera_2.md`](../vera_2.md).

## Build

Needs [cc65](https://cc65.github.io/):

```
ca65 --cpu 65C02 vera2fill.s -o vera2fill.o
ld65 -C vera2demo.cfg vera2fill.o -o VERA2FILL.PRG
```

(repeat for `vera2incr`, `vera2demo`, `vera2sprites`, `vera2blit`)

## Run

**Emulator** — must be launched with the `-bitmap2` flag:

```
x16emu -bitmap2 -prg VERA2FILL.PRG -run
```

**Hardware** (X16-MiSTer) — turn on **Bitmap Layer** in the OSD, copy the `.PRG`
to the SD card, then `LOAD"VERA2FILL.PRG"` / `RUN`.
