# PSP Pixel Studio V2

A lightweight pixel-art editor for PSP-1000. This is a new version of Pocket Pixel 64, intended to be compiled by the existing GitHub Actions workflow.

## V2 features
- New Project presets: 16x16, 32x32, 64x64, 128x128
- D-pad and analog stick both move the cursor; preserves V1 repeat timing and analog deadzone
- Automatically scrolls the viewport near the edge when zoomed
- SELECT toggles both floating side panels together
- START opens New / Save / Load / Options / Exit
- Frutiger Aero-inspired and Cyberpunk-inspired UI color themes (Options)
- TRIANGLE cycles grid overlays: off, halves, quarters, eighths. Major subdivisions are thicker than pixel guides. Grid is display-only.
- Nine built-in palettes; L/R select color; CIRCLE changes palette. Existing artwork retains its RGB colors when palettes change.
- X draw, SQUARE erase, hold while moving to draw continuously.
- Zoom adjustable from 1x to 16x via START > Options. Viewport pans automatically when cursor approaches edge.
- Saves one project **per canvas size** at `ms0:/PSP/PIXELSTUDIO/art_16.pps`, `art_32.pps`, etc. Saves include raw RGBA colors; grid lines are not stored.

## Important limitations
- **Not yet tested on PSP hardware.** GitHub Actions must compile the source and you should test the EBOOT.
- The save menu currently saves **one slot per canvas size**. Saving again overwrites that size's previous file. Back up saved files before replacing.
- V2 uses a new PPS2 save format. V1 `art.p64` is not automatically imported or overwritten.
- PNG export, layers, project naming, and undo are **not** implemented.
- Frutiger Aero and Cyberpunk are currently compact color themes, not the full decorative mockups.
- 128x128 can be navigated by automatic scrolling; large zoom settings display a small part of the canvas.

## GitHub Actions
The existing workflow using `pspdev/pspdev:latest` and `make` can build this project. Replace repository root `main.c`, `Makefile`, and `README.md` with these V2 versions. Leave `PALETTES.md` if desired.

Download the successful run's `PocketPixel64-PSP` artifact (or rename the workflow's artifact label to `PSP-Pixel-Studio-V2`). Put `EBOOT.PBP` in `PSP/GAME/PSPPIXELSTUDIO/` on the Memory Stick. Keep V1 installed in its separate folder as a fallback.
