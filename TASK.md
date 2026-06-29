# TASK: UI Polish Pass
> Created: 2026-06-29 | Updated: 2026-06-29

## Goal
Five targeted UI fixes: neutral colour palette, split-view resize cursors, remove scrub bar,
add Data Change In/Out animation tabs (with icons on all tabs), always-visible element outlines.

## Plan
- [x] Remove blue-ish tint from dark palette (`main.cpp`)
- [x] Fix split-view resize cursor — stylesheet + `showEvent` QSplitter scan (`mainwindow.h/.cpp`)
- [x] Remove scrub bar (`GraphicTimingEditor.h/.cpp`)
- [x] Add Data Change In / Data Change Out animation tabs with icons on all 4 tabs
      (`UndoCommands.h/.cpp`, `GraphicTimingEditor.h/.cpp`, `mainwindow.cpp`)
- [x] Always draw dashed element boundary even when nothing is selected (`CanvasWidget.cpp`)

## Log
### 2026-06-29
- Build: `cmake --build build -j$(nproc)` → 59/59 targets, exit 0, no errors

## Unverified / Pending
- All five changes implemented, NOT manually tested (no runtime run yet)
- Splitter cursor fix may or may not work on Wayland — relies on `findChildren<QSplitter*>()` in `showEvent`; dock area separators may be a Qt-private type instead of `QSplitter`

## Current State
All five changes are implemented and the binary compiles clean. The palette is now neutral grey
(was blue-tinted). The timing editor has 4 tabs (In, Out, Data Change In, Data Change Out) each
with an icon; the scrub ruler row is gone. Element outlines always draw, not only when selected.
Split-view cursor fix is attempted via `showEvent` + QSplitter scan + separator stylesheet.
