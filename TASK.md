# TASK: StreamCanvas Editor — UX Audit & Fixes
> Created: 2026-07-21 | Updated: 2026-07-21

## Goal
Audit + improve UX/correctness of the StreamCanvas Qt6 editor (plan approved, see PLAN.md).
Per user redirect: engine-related fixes go in the separate engine clone
`~/Projects/obs-graphics-engine` (master, dirty tree — touch only serialization/asset code),
then the editor's submodule is bumped to consume them. Editor branch: `ux-audit-fixes`.
Dark-only theme. Rotation & multi-select deferred.

## Plan
### Engine (~/Projects/obs-graphics-engine)
- [x] E1: Expose ParseElement/SerializeElement as public `ogt::` API (title.h/.cpp) — VERIFIED
- [x] E2: Title::Save missing-asset — throws (path listed), no broken .ogt — VERIFIED
- [x] E3: Title::Save basename collision — de-dupe to `@name`/`@name_1` — VERIFIED
- [x] E4: `.ogt` title.json `version:1` field (write + tolerant read/warn) — VERIFIED
### Editor (obs-graphics-editor)
- [x] P0-1: Refactor elementToJson/insertElementFromJson to call `ogt::` API — VERIFIED
      (mirror of E1 applied to submodule working tree; formal submodule bump pending)
- [x] P0-2: selection retargets wrong element after delete/reorder — VERIFIED (code deb5b2b + GUI smoke)
- [x] P0-3: ID rename not undoable + corrupts undo history — VERIFIED (impl+reviewer APPROVE + GUI)
- [x] P0-4: editor-side missing-asset validation (complements E2) — VERIFIED (inline GUI + review)
- [ ] P1-1..7: shortcuts, hit-slack, tree reset, undo merge, error surfacing, palette colors, DPR
- [ ] Delete dead ui/graphicproperties.{h,cpp}

## Log
### 2026-07-21
- Branch `ux-audit-fixes` created (exit 0); engine submodule populated (48 files); editor
  built clean via background cmake (exit 0).
- Verified: engine already serializes data_anim (title.cpp:650-655 master) — the loss is the
  editor's DUPLICATE serializer. `mask` is documented but unimplemented in engine (0 matches).
- Engine `ParseElement` (title.cpp:422) / `SerializeElement` (:599, takes AssetRegFn) are
  `static`; editor must reuse via new public wrappers passing identity asset callback.
- `~/Projects/obs-graphics-engine` = master 934cef4 (ahead of submodule 6db8ded), tree dirty
  with unrelated CI/CMake/LICENSE edits — commit only serialization/asset files.

## Log (verification)
### 2026-07-21 — E1 + P0-1
- Engine E1 built clean (implementer, `cmake --build build --target engine` exit 0).
- Editor P0-1 built clean (`cmake --build build` exit 0, ninja). Reviewer APPROVED round-trip.
- Deterministic proof: scratchpad/roundtrip.cpp links build/engine/libengine.a, exercises
  ogt::SerializeElement + ogt::ParseElement → `ALL PASS (failures=0)`, run exit 0.
  Confirmed survive: data_anim_in/out (type/easing/duration/delay), scale_mode "tile" +
  image_tile_scale, text_align_x "justify", raw image path. JSON dump inspected.
- Bonus: old editor kScaleModeStr[] had no "tile" entry (out-of-bounds for ScaleMode::Tile) —
  now fixed by delegating to engine ScaleModeToStr.
### 2026-07-21 — E2/E3/E4
- Engine built clean (implementer, exit 0). Diff reviewed (65+/5-, title.cpp only).
- Deterministic proof: scratchpad/save_test.cpp links engine build/libengine.a + minizip →
  `SAVE-LOGIC PASS (failures=0)`, run exit 0. Verified via `unzip`: title.json `version=1`;
  two same-basename assets archived as logo.png + logo_1.png (@logo.png / @logo_1.png);
  missing asset → Save throws with path, no .ogt written.
- Submodule jump 6db8ded→934cef4 = 5 Lua/HTTP commits (compiled out, LUA OFF) + dormant
  data-source polling; NO serialization change. Bumping editor to master+commit is safe.

### 2026-07-22 — P0-1/P0-2 GUI smoke (qt-auto-test, NOT use-computer)
- Editor launched via `qt-auto-test launch -- ./build/stream-canvas-editor` (pid 205225), renders
  clean dark theme (screenshot smoke-01/editor.png). NOTE: use `qt-auto-test`, never use-computer.
- Added 2 rectangles → contextual Element/Style ribbon tabs appear on selection (two-elems.png).
- P0-2 delete: selected element_2 deleted → selection CLEARS cleanly, element_1 remains with no
  handles, contextual tabs gone (after-delete.png). No silent retarget to a stranger. ✓
- Undo (Home→Undo button) restored element_2; both elements coexist, stack labels read
  "Undo Add element"/"Redo Remove element" (after-undo2.png). RemoveElementCmd undo via the
  P0-1 JSON-snapshot round-trip works live, no crash/loss. ✓
- Injected key-shortcuts (send-keys <Ctrl+Z> to a child widget) do NOT trigger window-level
  QAction shortcuts — use toolbar buttons for shortcut-bound actions in qt-auto-test.
- Minor observation (not P0): after undo, the Animation Timing panel did not re-add element_2's
  row (tree updated, timeline stayed 1 row) — panel-refresh staleness, adjacent to P1-3.

### 2026-07-22 — P0-3 undoable rename (impl → reviewer APPROVE → GUI smoke)
- Impl: new `SetElementIdCmd` (UndoCommands.h/.cpp); `onIdEditingFinished` now validates
  empty+duplicate (revert field + QToolTip) and pushes the command; `EditorTitle` caches
  `m_selectedElementId` and re-emits `selectionChanged` on id-change-with-same-index so the
  ribbon re-syncs on undo/redo. `cmake --build build -j` exit 0, clean.
- Reviewer APPROVED with full undo/redo-ordering + re-entrancy + null-safety trace.
- GUI (qt-auto-test pid 451964): renamed element_1→text_hero via ID field (w0573)+Return →
  ID field/tree/timeline all show "text_hero"; undo label became "Undo Rename element" (rename
  is now on the stack — the core fix). Undo → ID field/tree/timeline revert to "element_1"
  (EditorTitle re-emit re-syncs live); stack shows "Undo Add element"/"Redo Rename element".
- GUI dup-rejection: added element_2, renamed →"element_1" (exists) → field REVERTED to
  "element_2", tree unchanged, undo label stayed "Undo Add element" (NO rename cmd pushed). ✓
- No committed test infra in repo (no CTest/gtest/QTest); test-writer skipped, consistent with
  P0-1/P0-2 — verified deterministically via undo-label + field-text evidence instead.

### 2026-07-22 — P0-4 missing-asset validation + error surfacing (impl → reviewer APPROVE → GUI)
- Impl: `RibbonFormatSection::updateImagePathValidity()` (QFileInfo + QImageReader::canRead) →
  red border + warning tooltip on a missing/unreadable path, non-blocking (command still pushed);
  called from onImagePathChanged (tooltip popup on invalid) and the image refresh (state only).
  `TitleDocument::lastError()` captures engine `e.what()` (catch narrowed from `...` to
  `const std::exception&`); onOpen/onSave/onSaveAs append the real cause. Build exit 0.
- Reviewer APPROVED. Critically verified the catch-narrowing is safe: traced every throw on the
  engine Save/Load path — all std::exception-derived (runtime_error, nlohmann json, filesystem);
  the zipper lib never throws (bool status codes only). No non-std throw can now escape.
- GUI (qt-auto-test pid 472218): added image_1, Image tab → set path "/tmp/does-not-exist-xyz.png"
  + Return → Path field shows a RED BORDER, value still applied (field+model hold the path)
  (screenshot p04-badpath.png). ✓ Inline non-blocking validation confirmed live.
- Save-error dialog NOT script-verified: the "Save Title As" QFileDialog is a native/portal modal
  that blocks qt-auto-test's click call (had to dismiss via xdotool Escape). The save-error
  surfacing is a trivial reviewer-verified string append + the engine throw-before-write is
  deterministically proven by scratchpad/save_test.cpp (throws w/ path, no .ogt written).

## Unverified / Pending
- GUI smoke (launch editor, copy/paste/undo/save) NOT yet run — deferred to one session after
  submodule bump so multiple items verify together.
- Submodule handoff (commit E1..E4 in engine → push → bump editor submodule) NOT done; will
  surface to user before pushing to their MIT origin. Editor P0-1 changes therefore uncommitted.

## Errors & Fixes
| Error | Cause | Fix | Evidence |
|-------|-------|-----|----------|
| (none yet) | | | |

## Current State
Plan approved + reshaped after user redirect: engine (de)serializer is the single source of
truth; editor's lossy duplicate will be replaced by calls into a newly-exposed `ogt::` engine
API. Engine work happens in ~/Projects/obs-graphics-engine (master, dirty — surgical commits
only). Next: E1 (expose ParseElement/SerializeElement), build-verify engine, then bump the
editor submodule and refactor elementToJson/insertElementFromJson (P0-1). All prior state
(branch, build) verified in Log.
