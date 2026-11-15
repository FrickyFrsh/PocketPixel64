# PSP Pixel Studio V2.2 — PSP-1000 homebrew

Build with PSPDEV: `make`. GitHub Actions can use the existing `pspdev/pspdev:latest` workflow. Copy `EBOOT.PBP` to `PSP/GAME/PIXELSTUDIO/EBOOT.PBP`.

## Controls
- D-pad / analog: move cursor (same acceleration as V2).
- X: draw; Square: erase. Hold while moving to paint.
- L / R: cycle colors in current palette.
- Circle: cycle zoom, wrapping to Fit Canvas.
- Triangle: toggle grid visibility. Set subdivisions in START > Options.
- SELECT: show/hide floating tool and palette panels. While panels are visible, L selects tools, R selects palette list; D-pad up/down chooses, X activates.
- START: New, Save, Load, Export PNG, Controls, Options, Exit.

## Features
- 16, 32, 64, 128 canvas presets; fit-and-center initial view, cursor-follow scrolling at high zoom.
- 10 numbered project slots (PPS3 native format), with unsaved-change warning on new/load.
- PNG RGBA exports at native resolution to `ms0:/PSP/PIXELSTUDIO/EXPORTS/ART_###.PNG`. Transparency preserved; grid/UI excluded.
- 64-action undo/redo, eyedropper, grid hierarchy, clean preview.
- Circle outline tool: SELECT opens panels, L selects tools, choose CIRCLE and X. D-pad Left/Right resizes; analog stick repositions the live preview; X places; Square cancels. One circle = one undo action.
- Aero/Cyber color schemes and compact overlay panels.

## Notes
- Saves are not compatible with V2 PPS2 format. Keep V2 backups.
- Undo/redo stores actions; continuous drawing may still use several actions.
- PNG files use valid uncompressed DEFLATE and may be larger than typical optimized PNGs.
- Source is provided for compilation/testing; no PSP hardware or PSPDEV compiler validation was available in this environment.
