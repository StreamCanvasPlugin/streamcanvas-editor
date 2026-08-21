# StreamCanvas Editor — UX Audit & Fix Plan

## Context

The StreamCanvas editor is a standalone Qt6/SARibbon desktop app that authors animated
broadcast-graphics scenes and hands them to the OBS plugin as `.ogt` bundles. It must feel
native beside OBS Studio. This pass audits UX and correctness and fixes the highest-value
issues. The audit is static (deep source read + direct verification of top findings); the
`engine/` submodule is not checked out and plan mode forbade building, so **live driving of
the app happens during implementation**, verifying each item before moving on.

**Approved direction (from clarifying questions):**
- **Theme:** stay dark-only (OBS default look). Fix hardcoded colors that break contrast/
  consistency *within* the dark theme; route them through `palette()` roles. **No OS light
  theme / `colorSchemeChanged` work this pass.**
- **Scope:** fix data-loss/desync bugs and low-cost UX wins first. Rotation/shear-correct
  canvas handles and multi-select are **deferred large items** (listed at the end for
  separate approval, not implemented in this pass).
- **Process:** per repo `CLAUDE.md`, this session plans/reviews and delegates implementation
  to the `implementer` subagent, one item at a time, `implementer → reviewer` loop, then
  `test-writer` where a test is meaningful. Small self-contained commits. Respect the
  licensing split (MIT engine / GPL plugin / proprietary editor) — no code moved across
  boundaries; all edits stay in the editor tree.

**Ignore:** `.claude/worktrees/*` (stale copies) and `ui/graphicproperties.{h,cpp}`
(dead "Scene/Graphic" code, not in `CMakeLists.txt`, references a nonexistent
`engine/scene.h`). Flag the dead file for deletion, don't fix it.

---

## Ranking method

Ranked by (user impact × inverse fix cost). P0 = correctness/data-loss, ship first.
P1 = cheap, high-visibility UX. P2 = medium. Deferred = large, needs separate sign-off.

---

## P0 — Correctness & data loss (do first)

### P0-1. Copy/paste and delete-undo silently drop data animations (and mask)
- **Repro:** Give an element a data-change animation (`SetElementAnimCmd` → DataAnimIn/Out).
  Copy→paste it, or delete it and Ctrl+Z. The data animation is gone.
- **Root cause:** `TitleDocument::elementToJson` (`model/TitleDocument.cpp:288-372`, verified)
  serializes `anim_in`/`anim_out` but **omits `data_anim_in`/`data_anim_out` and `mask`**.
  `insertElementFromJson` (`model/UndoCommands.cpp:150-268`) never parses them. Clipboard
  (`mainwindow.cpp:840`) and `RemoveElementCmd` snapshot (`UndoCommands.cpp:729`) both use
  this serializer. The engine's real `SerializeElement` writes these fields — the editor's
  snapshot path is the one that loses them.
- **Fix:** Add `data_anim_in`/`data_anim_out` (and `mask`) to `elementToJson` and the matching
  parse in `insertElementFromJson`, mirroring the engine field names/format so round-trips are
  lossless. **Standard:** clipboard/undo must be lossless (Figma/Canva/OBS duplicate preserves
  every property).

### P0-2. Selection silently retargets the wrong element after delete/reorder
- **Repro:** Select element B (index 2). Delete element A (index 1). Selection index 2 is
  still "valid" but now points at what was element C. Ribbon/canvas edit the wrong element.
- **Root cause:** selection is a raw vector **index** (`EditorTitle.h:11`); `validateSelection`
  (`EditorTitle.cpp:9-18`, verified) only range-checks. `RemoveElementCmd::redo` erases and
  shifts (`UndoCommands.cpp:754-760`); `MoveElementCmd::doMove` rotates (`:774-786`).
- **Fix:** After structural commands, reconcile selection by **element identity**, not index.
  Simplest robust approach: on delete, have the delete handler (`mainwindow.cpp:656-663`)
  select a sensible neighbor (or clear); on any structural change, `EditorTitle` re-resolves
  the selected index by matching the previously-selected element pointer/id and clears only if
  truly gone. **Standard:** in every layers-based editor, deleting a layer never silently
  transfers your selection to an unrelated layer.

### P0-3. Element ID rename has no undo and can corrupt prior undo history
- **Repro:** Edit element X's fill (undoable). Rename X via the ID field. The rename can't be
  undone; then undoing the earlier fill edit silently no-ops.
- **Root cause:** `RibbonFormatSection::onIdEditingFinished` (`:1100-1108`) mutates directly via
  `applyMutation`/`SetId` with **no command pushed**, inside a `catch(...){}` that then
  unconditionally updates local state (`:1102-1106`). Commands target elements by string id
  (`m_ei`); a rename orphans every queued command's `getElement(m_ei)` (throws, swallowed).
  No duplicate-id validation either.
- **Fix:** Add a `SetElementIdCmd` (or `RenameElementCmd`) that (a) is undoable, (b) rejects
  duplicate/empty ids with user feedback, and (c) rewrites the `m_ei` target on any stacked
  commands referencing the old id — OR, cleaner, switch commands to a stable numeric handle.
  Minimum viable: undoable rename + duplicate rejection; note the stacked-command retarget as a
  follow-up if full retargeting is too large. **Standard:** every mutation is undoable (repo
  `CLAUDE.md`: "all mutations go through the undo stack").

### P0-4. Missing assets silently produce broken `.ogt` for the OBS plugin
- **Repro:** Reference an image, then the file becomes unreadable at save. The `.ogt` still lists
  the asset but the archive omits it; the plugin loads a broken path with no error.
- **Root cause (editor side):** `onImagePathChanged` (`RibbonFormatSection.cpp:1355`) does no
  existence/format validation and gives no feedback. (Engine `Title::Save` `continue`s past
  unreadable assets — MIT engine, out of editor scope to change, but the editor can prevent the
  bad state.)
- **Fix:** Validate image path on set (exists + loadable) and surface a clear inline error /
  status message; on save, warn if any referenced asset is missing before writing. Keep all
  changes editor-side (don't touch the MIT engine). **Standard:** vMix/CasparCG flag missing
  media rather than exporting a silently-broken bundle.

---

## P1 — Cheap, high-impact UX wins

### P1-1. Delete and Duplicate have no keyboard shortcuts
- **Root cause:** Delete action (`RibbonFormatSection.cpp:333`, `mainwindow.cpp:656`) has an icon
  but no `setShortcut`; there is **no Duplicate command at all**.
- **Fix:** Bind Delete → `Del`/`Backspace`; add a Duplicate command (undoable, reuses
  `elementToJson`/`insertElementFromJson`) → `Ctrl+D`, offset-pasted and selected. **Standard:**
  Del + Ctrl+D are universal (Figma/Canva/OBS).

### P1-2. Selection-handle hit area is smaller than the drawn handle
- **Repro:** Try to grab a resize handle near its edge — misses.
- **Root cause:** `SelectionHandles::hitTest` uses exact `handleRect(i).contains(pt)` on an 8×8
  rect (`SelectionHandles.cpp:48-55`) while the handle is drawn ~13px with its shadow stroke.
  Gradient editors already pad ±3px (`GradientHandlePainter.h:68-78`).
- **Fix:** Add hit-slack (grow the test rect by a few px, and resolve corner-vs-edge overlap so
  mid handles stay reachable on small elements). **Standard:** direct-manipulation handles have
  a hit target ≥ their visual size (Figma).

### P1-3. Tree model fully resets on every document change (flicker + lost state + O(n²))
- **Repro:** Drag an element on the canvas — the layers tree flickers, collapses/re-expands,
  loses scroll position, and can end up showing no highlighted row while the canvas shows a
  selection.
- **Root cause:** `TitleTreeModel` calls `beginResetModel/endResetModel` on every
  `documentChanged` (`TitleTreeModel.cpp:19-22`), and `TitleTreeView` re-`expandAll()`s each
  time; per-drag-frame this reruns O(n²) `childrenOf`/`rowOfElement` scans and drops selection.
- **Fix:** Two parts. (a) Coalesce: skip the tree reset during an in-progress canvas drag
  (only refresh spinboxes live; reset the tree on drag release). (b) After any reset, re-apply
  the current `EditorTitle` selection to the view. Lower-risk than a full incremental model.
  **Standard:** a layers panel keeps expansion/scroll/selection stable during canvas edits.

### P1-4. Per-keystroke undo for text vs. everything-merges for spinboxes
- **Root cause:** text edits push `SetElementFieldCmd<std::string>` with `mergeTag = -1` on every
  `textChanged` → one undo entry per keystroke (`RibbonFormatSection.cpp:786`,`:1342-1351`);
  spinbox fields merge unconditionally with no idle/focus break, so edits minutes apart collapse.
- **Fix:** Give text edits a merge tag so a continuous typing run is one undo step, and insert a
  merge barrier on focus-out / commit for spinbox fields (e.g. clear the merge id on
  `editingFinished`). **Standard:** one undo step per "edit gesture," not per keystroke and not
  per-lifetime (OBS text sources, Figma).

### P1-5. `catch(...){}` swallows errors app-wide with no feedback or logging
- **Root cause:** pervasive empty catches (`RibbonFormatSection.cpp:164,173,391,405,494,505,862,
  1103,1151,1161,1175,1352`; `TitleDocument::load/saveAs` discard exception text at
  `:171-175`,`:210-214`).
- **Fix:** Replace silent swallows with at least a `qWarning()` carrying the exception text, and
  surface the *actual* message in the file-error `QMessageBox` (which asset/why) instead of a
  generic "Could not save/open." Scope to the load/save path + rename path first. **Standard:**
  errors are logged and file errors name the cause.

### P1-6. Hardcoded colors that break contrast within the dark theme
- **Root cause:** literal RGB/stylesheet colors that don't derive from `palette()`:
  read-only id field `color:#888` (`RibbonFormatSection.cpp:914`), `makePaintSwatch` literals
  (`:123-153`), `PaintEditor` `font-size:10px;color:gray` (`painteditor.cpp:236`),
  `TransformEditor` `color:#aaa` (`transformeditor.cpp:8`), timeline accents
  (`AnimationTimingEditor.cpp:120-133`, `ScrubRuler.cpp:35-74`), swatch grid ring/border
  (`BrandColorSwatchGrid.cpp:42-48`). F1 shortcuts HTML fixed greys (`mainwindow.cpp:277-282`).
- **Fix (dark-only):** route text/disabled/border colors through `palette()` roles
  (`placeholderText`, `mid`, `dark`, `highlight`) so they stay consistent; keep intentional
  functional colors (checkerboards, playhead) but pull neutrals from the palette. No light-theme
  branch. **Standard:** custom widgets inherit palette roles, not literals (Qt guidelines).

### P1-7. ColorWheel is blurry on HiDPI (no devicePixelRatio)
- **Root cause:** `rebuildRing`/`rebuildTriangle` allocate `QImage(size(), …)` at logical res
  (`ColorWheel.cpp:179-208`) then `drawImage` 1:1 → upscaled on HiDPI.
- **Fix:** allocate backing images at `size() * devicePixelRatioF()`, set the image DPR, and draw
  in logical coords. **Standard:** custom-rendered widgets honor DPR (Qt HiDPI guide).

---

## P2 — Medium

- **P2-1. No busy indication on file/image I/O.** Save/load/image-load run on the UI thread with
  no `WaitCursor`/progress (`mainwindow.cpp:710-731`, `doPaste` `:923-959`). Fix: wrap in
  `QApplication::setOverrideCursor(Qt::WaitCursor)` at minimum (threading is a larger, separate
  effort). Standard: any op >100ms shows busy state.
- **P2-2. No tab order / focus management.** No `setTabOrder`/`setFocusPolicy` anywhere; non-modal
  `Qt::Tool` popups (gradient/content editors) don't restore focus on close. Fix: set explicit
  tab order on the ribbon/property spinbox rows and restore focus to the canvas/last widget when
  tool dialogs close.
- **P2-3. `fitToWindow()` doesn't fit.** `CanvasWidget.cpp:158` just resets zoom=1/pan=0. Fix:
  compute zoom so the title fits the viewport with margin (Ctrl+0 = true "zoom to fit").
  Standard: Ctrl+0/Shift+1 fits content (Figma/Canva).
- **P2-4. AnimationTimingEditor: handle zones overlap on short clips; discoverability.** 14px
  left/right zones overlap under 28px so right-resize is impossible (`:27,:150-160`); type/easing
  only via right-click. Fix: clamp/priority the zones by clip width; add a visible affordance for
  type/easing.
- **P2-5. Delete a lower element leaves graphic panel stale** until reselect
  (`graphicproperties.cpp:53` — but that's the dead file; confirm the live panel path). Fold into
  P0-2's selection reconciliation.

---

## Deferred — large items (separate approval, NOT this pass)

- **D-1. Rotation/shear-correct canvas manipulation.** Selection box, 8 handles, hit-test,
  cursor table, and resize/move math all assume axis-aligned `globalBounds`
  (`CanvasWidget.cpp:45-50,481`; `SelectionHandles.cpp`). Rotated/sheared elements show an
  unrotated box and fight the user. Requires transform-aware handle geometry and delta math.
- **D-2. Multi-select** (canvas marquee + tree `ExtendedSelection`), and the group operations it
  implies (move/delete/align multiple). Both are single-select today
  (`CanvasWidget` single `SelectionId`, `TitleTreeView.cpp:25`).
- **D-3. Tree feature parity with a real layers panel:** in-place rename, visibility/lock toggle,
  reorder (front/back/up/down) + duplicate in context menu, clipboard in tree.
- **D-4. `.ogt` schema versioning** for plugin-forward compatibility (engine-side, MIT — coordinate
  before touching; out of editor boundary).
- **D-5. Inline text markup** (bold/italic/underline/strike on a *range* of characters). Not
  supported today: `TextElement::Font` carries one `weight`/`isItalic`/`isUnderline`/
  `isStrikethrough` for the whole element (`engine/element_text.h:47-55`), the renderer calls
  `pango_layout_set_text` with no markup parsing (`engine/element_text.cpp:40`), the underline and
  strikethrough `PangoAttr`s are inserted with no index range (`engine/element_text.cpp:30-38`),
  and the JSON keys `font_italic`/`font_underline`/`font_strikethrough`/`font_weight` are scalars
  (`engine/title.cpp:511-523`). The editor's content dialog is a `QPlainTextEdit` and the ribbon
  B/I/U/S buttons push `SetElementFieldCmd<bool>` on the element font
  (`ui/widgets/RibbonFormatSection.cpp:909,1441-1457`). A user typing `<b>x</b>` gets those literal
  characters rendered. Cost: cross-repo and a save-format change —
  1. engine renders via `pango_layout_set_markup` (or an indexed `PangoAttrList`), with an escape
     policy for existing titles containing `<` or `&`;
  2. `text` becomes markup-bearing or gains a parallel span list in the JSON, gated on the `version`
     field added in E4;
  3. editor swaps `QPlainTextEdit` for `QTextEdit` + a rich-text→markup converter, and B/I/U/S act
     on the cursor selection instead of the element;
  4. the OBS plugin needs the same parsing change or old builds render the tags on screen.

---

## Execution order

1. P0-1 → P0-2 → P0-3 → P0-4 (correctness/data-loss first).
2. P1-1 → P1-2 → P1-3 → P1-4 → P1-5 → P1-6 → P1-7.
3. P2 items as time allows.
4. Delete dead `ui/graphicproperties.{h,cpp}` (confirm not referenced) as a standalone cleanup commit.

Each item: `implementer` (exact files above) → `reviewer` → `test-writer` where meaningful →
build → drive the app to verify → one focused commit.

---

## Verification (per item)

Prerequisite (once): `git submodule update --init` to populate `engine/`, then
`cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)`.

- **Build gate:** every item must compile clean (new `.cpp`/`.h` registered in
  `CMakeLists.txt` `PROJECT_SOURCES`).
- **Live drive** with the desktop-automation tools (`mcp__use-computer__*`): launch
  `./build/stream-canvas-editor`, reproduce the original bug, confirm it's fixed, and check no
  regression in adjacent flows. Screenshot before/after for the data-loss and selection items.
- **Item-specific checks:**
  - P0-1: add data-anim → copy/paste and delete/undo → data anim survives (inspect saved JSON).
  - P0-2: delete a lower-index element → selection stays on the same element or clears, never
    jumps to a stranger.
  - P0-3: rename → Ctrl+Z restores old id; duplicate id rejected with feedback; earlier undo of
    that element's edits still works.
  - P1-1: `Del` removes, `Ctrl+D` duplicates+offsets+selects.
  - P1-3: drag on canvas → tree keeps expansion/scroll/selection, no flicker.
  - P1-7: run on a HiDPI (2x) screen or forced `QT_SCALE_FACTOR=2` → wheel is crisp.
- **Regression:** save a scene, reload it, and (where possible) load the resulting `.ogt` through
  the engine's own preview path (`CanvasWidget` animation preview round-trips `.ogt`) to confirm
  the plugin handoff still parses.

## Deliverable note
The task asked for `PLAN.md` in the repo root. This plan file mirrors that content; on approval
I'll copy it to `PLAN.md` and commit it as the first change, then implement items in order.
