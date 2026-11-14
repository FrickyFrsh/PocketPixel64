# Pocket Pixel 64 — PSP-1000 homebrew source

A minimal 64x64 pixel editor for PSP, using 64 fixed RGB colors (4 levels each of red, green and blue). No network or extra assets required.

## Build

Requires the PSPDEV / PSPSDK toolchain (not included).

1. Open a shell with `psp-config` and `psp-gcc` available on PATH.
2. In this directory run `make`.
3. Copy the resulting `EBOOT.PBP` to `ms0:/PSP/GAME/POCKETPIXEL64/EBOOT.PBP` on the Memory Stick.
4. Start Pocket Pixel 64 from the PSP Game > Memory Stick menu.

**Note:** This ZIP contains source code, not a precompiled EBOOT.PBP. The PSP toolchain was not available in the creation environment, so the build has not been tested on hardware.

## Controls

- D-pad: move cursor (hold to repeat; wraps around canvas)
- X: draw current color (hold X and move to paint)
- Square: erase to white/transparent (hold and move to erase)
- L / R: cycle through 64 colors
- Triangle: toggle grid lines
- Start: save art
- Select: load art

## Save format

Save path: `ms0:/PSP/GAME/POCKETPIXEL64/art.p64`

Binary format: 4 ASCII bytes `PP64`, followed by 4096 row-major bytes: palette indices 0..63, or 255 for blank. Colors are computed by `r=((index>>4)&3)*85`, `g=((index>>2)&3)*85`, `b=(index&3)*85`.

Saves are not PNG images. A PC conversion utility can be added later.

## Limitations

- No text UI/status display yet; save/load feedback is not rendered.
- Only one save slot, overwritten by Start.
- No undo, zoom, PNG export, or palette customization.
- Canvas occupies 192x192 screen pixels; individual logical pixels are 3x3 screen pixels.

## Lospec palette update
Circle cycles 9 built-in palettes (RGB64 + eight Lospec palettes). See PALETTES.md for artist credits and links. Start now saves PP65 files with a palette ID, and Select can load older PP64 saves.

## Analog stick support
The analog nub and D-pad both move the cursor. A centered deadzone avoids drift; holding either repeats movement. D-pad takes priority if both are used. X draws and Square erases while moving with either control.
