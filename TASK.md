# TASK: Multi-fix: Extension, Title, QR Icon, Text Spacing
> Created: 2026-06-29 | Updated: 2026-06-29

## Goal
Four targeted fixes/features: auto-add `.ogt` extension on Save As, keep window title in sync
with title name/ID changes, fix QR icon in tree (was scanner/copier), and add line spacing +
character spacing to text elements across engine, editor, and (pending) plugin.

## Plan
- [x] Auto-add `.ogt` extension in `onSaveAs()` (`mainwindow.cpp`)
- [x] Update window title on every `documentChanged` (title name/id edits) (`mainwindow.cpp`)
- [x] Fix QR icon in tree — use `qrCodeIcon()` not `Hardware_Scanner` (`TitleTreeModel.cpp`)
- [x] Add `lineSpacing` / `charSpacing` to `TextElement::TextStyle` (`engine/element_text.h`)
- [x] Apply spacings via Pango in `BuildLayout` (`engine/element_text.cpp`)
- [x] Serialize/parse `line_spacing` / `char_spacing` in engine (`engine/title.cpp`)
- [x] Serialize in editor undo snapshots (`model/TitleDocument.cpp`)
- [x] Add `LineSpacing` / `CharSpacing` merge tags (`model/UndoCommands.h`)
- [x] Add "Spacing" ribbon group with Line/Char spinboxes (`ui/widgets/RibbonFormatSection.h/.cpp`)
- [ ] Plugin repo: parse `line_spacing` / `char_spacing` in `element_text` loader

## Log
### 2026-06-29
- Build: `cmake --build build -j$(nproc)` → 16/16 targets, exit 0, no errors

## Unverified / Pending
- All editor/engine changes implemented, NOT manually tested (no runtime run)
- Plugin needs equivalent changes to its `element_text` deserialization

## Current State
All four tasks are implemented and the binary compiles clean (16/16, exit 0).

**Auto-extension**: `onSaveAs()` appends `.ogt` if the chosen path lacks it; also calls
`updateWindowTitle()` explicitly after a successful Save As.

**Window title sync**: Added `connect(m_doc, &TitleDocument::documentChanged, this,
&MainWindow::updateWindowTitle)` so the `[*] Title Editor` caption updates whenever the title
name/ID changes via the ribbon name field or undo/redo.

**QR icon**: `TitleTreeModel` now returns `qrCodeIcon()` (the custom SVG) instead of
`Hardware_Scanner` for QR elements.

**Text spacing**: `TextElement::TextStyle` gained `lineSpacing` and `charSpacing` (float, pixels,
default 0). Engine renders them via `pango_layout_set_spacing` and `pango_attr_letter_spacing_new`.
Engine serializes/parses `line_spacing` / `char_spacing` JSON keys. Editor serializes them in
undo snapshots. A "Spacing" ribbon group with Line ↕ and Char ↔ spinboxes (± 999 px) appears
in the Text ribbon tab with merge-tag-based undo collapsing.

Plugin changes still needed — see pending item above.
