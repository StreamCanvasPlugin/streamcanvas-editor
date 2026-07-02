# CLAUDE.md

Standalone Qt6 desktop app for authoring animated broadcast graphics scenes. Saves scenes as JSON consumed by the StreamCanvas OBS plugin.

## Build

```bash
git submodule update --init   # populate engine/
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/stream-canvas-editor
```

Qt Creator uses `build/Desktop-Debug` kit preset. QtAwesome is fetched by CPM at configure time.

## Adding a new source file

Register both `.h` and `.cpp` in the `PROJECT_SOURCES` list in `CMakeLists.txt`. Qt's AUTOMOC handles moc generation automatically.

## Engine submodule

Engine is at `engine/` (submodule: obs-graphics-engine). Include as:

```cpp
#include "engine/title.h"
#include "engine/types.hpp"
// etc.
```

See `engine/CLAUDE.md` for full engine type reference.

## Architecture

### Model layer

- **`TitleDocument`** (`model/TitleDocument.h/.cpp`) — Owns `Title` (engine) + `QUndoStack` + file I/O. **All mutations must go through `applyMutation(fn)`**, which updates the title and emits `documentChanged()`. `Title::elements` is a flat `std::vector<std::unique_ptr<IElement>>` — index `0` is an auto-created root, real elements start at index `1`; parent/child relationships are tracked directly on each element (`IElement::GetParent()`/`SetParent()`), not via a separate string-ID cross-reference table. `elementToJson(el)` serializes a single element to JSON for clipboard/undo snapshots. `load()` clears the undo stack so Ctrl+Z cannot cross file sessions.
- **`EditorTitle`** (`model/EditorTitle.h/.cpp`) — Wraps `TitleDocument`, tracks current selection (`SelectionId{Level::{None, Title, Element}, elementIndex}` — a single flat element index, no separate graphic-level tier). Emits `selectionChanged`. `validateSelection()` clears out-of-bounds selections on every `documentChanged`.
- **`UndoCommands`** (`model/UndoCommands.h/.cpp`) — Template-based commands:
  - `SetElementFieldCmd<T>` — generic field setter; pass `mergeTag` for rapid slider edits.
  - `SetElementPaintCmd` — full `Paint` struct.
  - `SetElementShadowCmd`, `SetCornerRadiusCmd` — whole-struct field setters (snapshot before/after as one undo unit).
  - `SetTitleDimensionsCmd`, `SetTitleNameCmd`, `SetBrandColorsCmd` — title-level fields.
  - `AddElementCmd` / `RemoveElementCmd` — serialize to JSON (via `insertElementFromJson`/`TitleDocument::elementToJson`) for safe undo storage.
  - `MoveElementCmd` — reorder within vector.

### UI layer (`ui/`)

Property panel widgets (pure code, no `.ui` files):
- `sceneproperties` — scene name, resolution preset combo + width/height spinboxes.
- `transformeditor` — bounds, rotation, opacity.
- `animationeditor` — in/out animation picker.
- `painteditor` — fill/stroke editor; integrates gradient widgets.
- `adaptivestack` — responsive layout helper.
- **`UiUtils`** — shared widget factories and helpers: `UpdateGuard` (RAII bool-flag guard replacing manual `m_updating` pairs); `makeSpinBox`, `makeTinyLabel`, `makeIconToolButton`, `makePopupButton` (returns `{QToolButton*, QMenu*}`); `GridBuilder` (fluent `QGridLayout` builder).

### Widgets (`ui/widgets/`)

- **`CanvasWidget`** — Cairo-backed canvas. Title dimensions from `m_doc->title().width/height`. Surface recreated in `onDocumentChanged()` on dimension change. Shift = aspect-ratio resize; Ctrl = center-scale resize on corner handles.
- **`TitleTreeView`** — Hierarchical Title → Elements (flat, no separate graphic tier). Sorted descending by `zOrder` (highest = top). Drag-and-drop updates `zOrder` via undo commands, not vector moves.
- **`RibbonFormatSection`** — contextual ribbon tabs: "Element", "Style", "Text" (text elements only), "Image" (image elements only), "QR Code" (QR elements only). Tabs shown/hidden in `MainWindow::onSelectionChanged` based on element type. Shared `m_contentDialog` / `m_contentEdit` used for both Text ("Edit Text…") and QrCode ("Edit Content…") — title set on open, cursor position saved in `m_contentSavedCursor` and restored after every `setPlainText` to prevent caret reset. Emits `deleteElementRequested` signal.
- **`PaintPickerWidget`** — compact fill/stroke picker for ribbon dropdown menus. Shows "No Styling" button, brand colors grid, inline ColorPicker, and "More Options…" button opening the full `PaintEditor` dialog.
- **`BrandColorSwatchGrid`** — grid of brand color swatches. Two modes: `Full` (editable, in `PaintEditor`) and `Compact` (read-only, in `PaintPickerWidget`).
- **`CornerRadiusButton`** — compact button for editing corner radii inline in the ribbon.
- **`PaddingButton`** — compact button for editing padding values inline in the ribbon.
- **`GradientEditor`** — Horizontal stop bar; calls `sortStops()` on mouse release, not during drag.
- **`LinearGradientEditor`** — endpoints `m_p1`/`m_p2` in normalized 0–1 coords; `toPixel()`/`toNorm()` convert for rendering. Emits `p1Changed`/`p2Changed` on mouse release.
- **`RadialGradientEditor`** — `m_center`/`m_radiusEnd` in normalized 0–1 coords. Emits `geometryChanged(center, radius)` on drag release.
- **`ColorWheel`**, **`HueSlider`**, **`GradientSlider`**, **`ColorPreview`**, **`ColorLineEdit`**, **`ColorUtils`**.
- **`SelectionHandles`** — corner/edge drag handles on canvas.
- **`AnimationTimingEditor`** / **`GraphicTimingEditor`** / **`ScrubRuler`** — timeline widgets.

### MainWindow

Add-element ribbon actions require an element selection; delete is handled by `RibbonFormatSection` via the `deleteElementRequested` signal. `updateToolBarState()` runs on every `selectionChanged`. New IDs generated by scanning for highest numeric suffix (`element_N`, `text_N`, `image_N`, `qr_N`).

## Key conventions

- **Normalized coordinates** — gradient endpoints and radii in `Paint` are 0–1, scaled by element bounds at Cairo render time. Gradient widgets store/emit in 0–1 space.
- **Local vs. global bounds** — `GetBounds().x/y` are local when `element.GetParent() != nullptr`. Always use `GetGlobalPosition()` for screen position.
- **Z-Order vs. vector order** — render sorts elements ascending by `zOrder` (lower = behind). Tree displays descending. Vector order is irrelevant to rendering.
- **Two-pass handle rendering** — selected handle drawn last via `pass == 0 / pass == 1` loop. Use this pattern in any widget with selectable handles.
- **`paint` vs `Paint`** — engine `Paint` holds doubles for Cairo; Qt-side uses `QColor`/`GradientStop`. Conversion at the paint editor boundary.
- **qlementine-icons API** — always use `themedIcon(Icons16::Category_Name)` (not `QIcon(iconPath(...))` which gives raw dark icons). Call `initIcons()` once at startup (done in `main.cpp`) before creating `MainWindow`. Icons header: `#include "icons.h"` (re-exports `oclero::qlementine::icons` namespace).
- **`textStyle{}`** — text layout fields (`alignX`, `alignY`, `autoScale`, `ellipsize`, `wrapMode`, `transform`) live in `el.textStyle.*`, not flat on `Element`. Access as `el.textStyle.alignX` etc.
- **`QVector<GradientStop>`** — `GradientStop` = `{qreal position, QColor color}`.
