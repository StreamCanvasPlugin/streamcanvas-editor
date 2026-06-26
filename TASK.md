# TASK: Tile Scale Mode UI
> Created: 2026-06-26 | Updated: 2026-06-26

## Goal
Update engine submodule and implement editor UI for new Tile scale mode in both Image elements and Paint image type. ScaleMode enum moved to types.hpp and Tile added; new imageTileScale (element) and tileScale/imageScaleMode (paint) fields.

## Plan
- [x] Update engine submodule (86b2be6 — "feat: add Tile scale mode and image paint scale modes")
- [x] Add `ElemMergeTag::TileScale` to UndoCommands.h
- [x] RibbonFormatSection: add "Tile" option + tile scale spinbox (image element tab)
- [x] RibbonFormatSection: sort scale mode combo (None first, then alphabetical)
- [x] RibbonFormatSection: add `onImageTileScaleChanged` slot wired to undo command
- [x] PaintEditor: add scale mode combo + tile scale spinner to image paint page
- [x] PaintEditor: sort scale mode combo identically; map indices via helpers
- [x] Build successfully

## Log
### 2026-06-26
- Built with `cmake --build build -j$(nproc)` → exit 0, 31/31 targets

## Current State
Done. Engine submodule updated. Image element ribbon tab now shows a sorted Scale Mode combo (None, Contain, Cover, Fit Height, Fit Width, Stretch, Tile) plus a Tile Scale ×-spinner that appears only when Tile is selected. PaintEditor image page has the same combo + spinner. Both use index↔enum mapping helpers so display order is independent of enum values.
