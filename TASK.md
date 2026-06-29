# TASK: Engine Scene→Title Refactor
> Created: 2026-06-28 | Updated: 2026-06-28 23:55

## Goal
Update the editor to match the engine's new architecture. Engine removed `Scene` and `Graphic`; replaced with `Title` (a self-contained ZIP-based `.ogt` file). The class hierarchy for elements changed from a flat `Element` struct to `IElement → Spatial → VisualElement → {RectangleElement, TextElement, ImageElement, QrElement}`. The editor now edits a single `Title`. No Add Graphic button. Assets auto-embed into `.ogt` on save.

## Plan

### Phase 1 — Model layer
- [x] `model/SceneDocument.h` — Title-based API
- [x] `model/SceneDocument.cpp` — Title load/save, mask resolution
- [x] `model/UndoCommands.h` — New commands (Bounds, Rotation, Shear); removed Graphic commands
- [x] `model/UndoCommands.cpp` — insertElementFromJson, all new command implementations
- [x] `model/EditorScene.h/.cpp` — Simplified SelectionId (no graphicIndex)

### Phase 2 — UI layer
- [x] `ui/widgets/SceneTreeModel.h/.cpp` — 2-level tree (scene → elements)
- [x] `ui/widgets/CanvasWidget.h/.cpp` — Title-based rendering, no MoveGraphic drag mode
- [x] `ui/widgets/GraphicTimingEditor.h/.cpp` — `load()` / `clear()` (no gi param)
- [x] `ui/widgets/RibbonFormatSection.h` — Element-only, removed Graphic tab
- [x] `ui/widgets/RibbonFormatSection.cpp` — Full rewrite with new element API
- [x] `mainwindow.h` — Cleaned up (no graphicproperties reference)
- [x] `mainwindow.cpp` — Full rewrite: no Graphic panel, .ogt file dialogs, element-only clipboard
- [x] `ui/widgets/SceneTreeView.cpp` — No Graphic level, element-only context menu
- [x] `ui/sceneproperties.cpp` — `m_doc->title()` instead of `m_doc->scene()`

### Phase 3 — CMakeLists + build
- [x] `CMakeLists.txt` — Removed `graphicproperties.h/cpp` (Graphic concept gone)
- [x] Build succeeds — `cmake --build build -j$(nproc)` → exit 0, 52/52 targets, no errors

## Log
### 2026-06-28
- Completed all model layer rewrites (SceneDocument, UndoCommands, EditorScene)
- Completed all widget rewrites (SceneTreeModel, CanvasWidget, GraphicTimingEditor, RibbonFormatSection)
- Wrote mainwindow.cpp and SceneTreeView.cpp rewrites
- Fixed sceneproperties.cpp (m_doc->title() instead of m_doc->scene())
- Removed graphicproperties.h/cpp from CMakeLists.txt (Graphic concept removed)
- Build: `cmake --build build -j$(nproc)` → 52/52 targets built, exit 0, no warnings from project code
- Renamed all "Scene" references to "Title": 10 files (SceneDocument→TitleDocument, EditorScene→EditorTitle, SceneTreeModel→TitleTreeModel, SceneTreeView→TitleTreeView, sceneproperties→titleproperties); classes, enums (SelectionId::Level::Scene→Title), undo commands (SetScene*Cmd→SetTitle*Cmd), API methods (sceneName→titleName), canvas helpers (sceneW/H, widgetToScene, renderStaticScene, etc.), MIME type (x-obs-scene-node→x-obs-title-node), UI strings ("Scene" dock/"Scene" ribbon→"Title"), undo text ("Set scene dimensions/name"→"Set title…")
- Build: `cmake --build build -j$(nproc)` → 85/85 targets built, exit 0

## Current State
**Full build passes (85/85).** All "Scene" references renamed to "Title" throughout the codebase — files, classes, enum values, undo commands, API methods, canvas coordinate helpers, MIME types, and UI strings. The binary is at `build/obs-graphics-editor`. Next step is manual testing to verify runtime behavior: file open/save (.ogt), element add/delete/undo, canvas drag, animation preview.
