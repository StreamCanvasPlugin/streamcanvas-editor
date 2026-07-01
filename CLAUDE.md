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
#include "engine/scene.h"
#include "engine/types.hpp"
// etc.
```

See `engine/CLAUDE.md` for full engine type reference.

## Architecture

### Model layer

- **`SceneDocument`** — Owns `Scene` + `QUndoStack` + file I/O. **All mutations must go through `applyMutation(fn)`**, which updates the scene and emits `documentChanged()`. Raw `Element*` pointers are rebuilt by `resolveElementPointers()` after every structural change (mask/parent relationships stored separately as string IDs in `m_elementRefs`). `setElementRef(gi, ei, maskId, parentId)` updates those IDs and resolves pointers — **not undoable**, does not adjust `bounds`; callers that change the parent must convert coordinates themselves. `load()` clears the undo stack so Ctrl+Z cannot cross file sessions.
- **`EditorScene`** — Wraps `SceneDocument`, tracks current selection (`SelectionId`: scene/graphic/element level + indices). Emits `selectionChanged`. `validateSelection()` clears out-of-bounds selections on every `documentChanged`.
- **`UndoCommands`** — Template-based commands:
  - `SetElementFieldCmd<T>` — generic field setter; pass `mergeTag` for rapid slider edits.
  - `SetGraphicFieldCmd<T>` — same for Graphic fields.
  - `SetElementPaintCmd` — full `Paint` struct.
  - `SetSceneDimensionsCmd`, `SetSceneNameCmd` — scene-level fields.
  - `AddGraphicCmd` / `RemoveGraphicCmd` / `AddElementCmd` / `RemoveElementCmd` — serialize to JSON for safe undo storage.
  - `MoveGraphicCmd` / `MoveElementCmd` — reorder within vector.

### UI layer (`ui/`)

Property panel widgets (pure code, no `.ui` files):
- `sceneproperties` — scene name, resolution preset combo + width/height spinboxes.
- `transformeditor` — bounds, rotation, opacity.
- `animationeditor` — in/out animation picker.
- `painteditor` — fill/stroke editor; integrates gradient widgets.
- `adaptivestack` — responsive layout helper.
- **`UiUtils`** — shared widget factories and helpers: `UpdateGuard` (RAII bool-flag guard replacing manual `m_updating` pairs); `makeSpinBox`, `makeTinyLabel`, `makeIconToolButton`, `makePopupButton` (returns `{QToolButton*, QMenu*}`); `GridBuilder` (fluent `QGridLayout` builder).

### Widgets (`ui/widgets/`)

- **`CanvasWidget`** — Cairo-backed canvas. Scene dimensions from `m_doc->scene().width/height`. Surface recreated in `onDocumentChanged()` on dimension change. Shift = aspect-ratio resize; Ctrl = center-scale resize on corner handles.
- **`SceneTreeModel`** — Hierarchical Scene → Graphics → Elements. Sorted descending by `zOrder` (highest = top). Drag-and-drop updates `zOrder` via undo commands, not vector moves. Icons: `Misc_FilmRoll` (scene), `Navigation_Layers1` (graphic), `Shape_Square` / `File_Font` / `File_Picture` / `Hardware_Scanner` (element by type).
- **`RibbonFormatSection`** — contextual ribbon tabs: "Graphic", "Element", "Style", "Text" (text elements only), "Image" (image elements only), "QR Code" (QR elements only). Tabs shown/hidden in `MainWindow::onSelectionChanged` based on element type. Shared `m_contentDialog` / `m_contentEdit` used for both Text ("Edit Text…") and QrCode ("Edit Content…") — title set on open, cursor position saved in `m_contentSavedCursor` and restored after every `setPlainText` to prevent caret reset. Emits `deleteGraphicRequested`/`deleteElementRequested` signals.
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

Add-element ribbon actions require a graphic or element selection; delete is handled by `RibbonFormatSection` via `deleteGraphicRequested`/`deleteElementRequested` signals. `updateToolBarState()` runs on every `selectionChanged`. New IDs generated by scanning for highest numeric suffix (`graphic_N`, `element_N`, `text_N`, `image_N`, `qr_N`).

## Key conventions

- **Normalized coordinates** — gradient endpoints and radii in `Paint` are 0–1, scaled by element bounds at Cairo render time. Gradient widgets store/emit in 0–1 space.
- **Local vs. global bounds** — `Element::bounds.x/y` are local when `element.parent != nullptr`. Always use `GetGlobalPosition()` for screen position.
- **Z-Order vs. vector order** — render sorts elements ascending by `zOrder` (lower = behind). Tree displays descending. Vector order is irrelevant to rendering.
- **Two-pass handle rendering** — selected handle drawn last via `pass == 0 / pass == 1` loop. Use this pattern in any widget with selectable handles.
- **`paint` vs `Paint`** — engine `Paint` holds doubles for Cairo; Qt-side uses `QColor`/`GradientStop`. Conversion at the paint editor boundary.
- **qlementine-icons API** — always use `themedIcon(Icons16::Category_Name)` (not `QIcon(iconPath(...))` which gives raw dark icons). Call `initIcons()` once at startup (done in `main.cpp`) before creating `MainWindow`. Icons header: `#include "icons.h"` (re-exports `oclero::qlementine::icons` namespace).
- **`textStyle{}`** — text layout fields (`alignX`, `alignY`, `autoScale`, `ellipsize`, `wrapMode`, `transform`) live in `el.textStyle.*`, not flat on `Element`. Access as `el.textStyle.alignX` etc.
- **`QVector<GradientStop>`** — `GradientStop` = `{qreal position, QColor color}`.
