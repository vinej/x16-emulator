# VERA_2 bitmap-layer demos

Example programs for the SDRAM bitmap layer (`$9F60`–`$9F6F`) added by this fork.
See [`../vera_2.md`](../vera_2.md) for the full register spec. Each demo
feature-detects `$9F61`, so the same `.PRG` runs on this emulator **and** on the
X16-MiSTer FPGA core.

| Source | PRG | What it shows |
|---|---|---|
| `vera2fill.s` | `VERA2FILL.PRG` | Switch to 8bpp, fill the whole screen fast with the **blit** (doubling a 16-colour seed), wait for a key, return to BASIC. |
| `vera2demo.s` | `VERA2DEMO.PRG` | Auto-cycles between an 8bpp 256-colour diagonal gradient and 4bpp 16-colour bands. |
| `vera2sprites.s` | `VERA2SPRITES.PRG` | 8bpp gradient + random **VERA sprites** + **mouse** (passthru); a top status bar turns **green** when the `$9F65` **write / read-back** self-test passes. |
| `vera2blit.s` | `VERA2BLIT.PRG` | Everything above plus **save-under**: **left-click** the gradient drops a message box (band saved to scratch via the blit), **click the box** to restore it exactly. |

`vera2demo.cfg` is the cc65 linker config all four use (a minimal `$0801` PRG with
a BASIC `SYS` stub).

## Build

Needs [cc65](https://cc65.github.io/):

```
ca65 --cpu 65C02 vera2fill.s -o vera2fill.o
ld65 -C vera2demo.cfg vera2fill.o -o VERA2FILL.PRG
```

(repeat for `vera2demo`, `vera2sprites`, `vera2blit`)

## Run

**Emulator** — must be launched with the `-bitmap2` flag:

```
x16emu -bitmap2 -prg VERA2FILL.PRG -run
```

**Hardware** (X16-MiSTer) — turn on **Bitmap Layer** in the OSD, copy the `.PRG`
to the SD card, then `LOAD"VERA2FILL.PRG"` / `RUN`.
