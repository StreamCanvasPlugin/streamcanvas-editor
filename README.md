# StreamCanvas Editor

Standalone Qt6 desktop application for authoring animated broadcast graphics scenes. Scenes are saved as JSON and loaded by the [StreamCanvas](https://github.com/OBS-Graphics/obs-graphics) OBS plugin at runtime.

## Dependencies

- Qt6 (Widgets)
- Cairo + Pango (via the engine submodule)
- Lua 5.x (via the engine submodule)
- CMake 3.16+

## Building

```bash
git submodule update --init   # populate engine/
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/stream-canvas-editor
```

QtAwesome is fetched automatically via CPM at configure time.

## Structure

```
src/              (repo root)
  main.cpp
  mainwindow.h/cpp
  icons.h/cpp
  model/
    SceneDocument.h/cpp   Scene ownership, undo stack, file I/O
    EditorScene.h/cpp     Selection tracking
    UndoCommands.h/cpp    Template-based undo commands
  ui/
    elementproperties.*   Element property panel
    fontproperties.*      Font settings
    painteditor.*         Fill/stroke/gradient editor
    styleproperties.*     Stroke, corner radius, opacity
    transformeditor.*     Bounds, rotation, opacity
    animationeditor.*     In/out animation picker
    sceneproperties.*     Scene name and resolution
    widgets/              Reusable custom widgets (canvas, color, gradient, tree)
engine/           Submodule: streamcanvas-engine
data/             Demo scenes and data sources
```

## Related repos

- [StreamCanvas Engine](https://github.com/OBS-Graphics/obs-graphics-engine) — rendering engine (submodule)
- [StreamCanvas](https://github.com/OBS-Graphics/obs-graphics) — OBS plugin that loads scenes produced by this editor

## AI disclosure

Parts of this project were built with the help of AI coding tools. All AI-assisted changes were reviewed before merging.
