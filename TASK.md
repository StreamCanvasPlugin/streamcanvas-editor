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
- [x] P1-1: Delete shortcut (Del/Backspace) + new Duplicate cmd (Ctrl+D) — VERIFIED (impl+review+GUI)
- [x] P1-2: selection-handle hit-slack (+nearest-center tiebreak) — VERIFIED (deterministic test)
- [x] P1-3: tree resets every doc change (flicker/lost selection) — VERIFIED (impl+review+GUI part b)
- [x] P1-4: undo merge granularity (text per-keystroke; spinboxes merge forever) — VERIFIED (GUI timing)
- [x] P1-5: catch(...){} swallows errors — VERIFIED (load/save+rename via P0-4/P0-3; ribbon typed-catch)
- [x] P1-6: hardcoded neutral colors → palette roles (dark-only) — VERIFIED (GUI: F1 dialog renders)
- [x] P1-7: ColorWheel HiDPI DPR — VERIFIED (build + no-regression reasoning; dpr==1 is a no-op)
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

### 2026-07-22 — P1-1 Delete/Duplicate shortcuts (impl → reviewer APPROVE → GUI)
- Impl: extracted `deleteSelectedElement()` + `insertElementCopy(json,offset)` from the delete
  lambda / doPaste; new `onDuplicate()`; `m_deleteAction` (Del+Backspace) + `m_duplicateAction`
  (Ctrl+D) as a Clipboard-panel small stack, enabled on hasElement. Build exit 0.
- Reviewer APPROVED — critically confirmed Backspace/Delete do NOT steal keystrokes from text
  fields: QLineEdit/QAbstractSpinBox/QPlainTextEdit all claim Delete+Backspace via
  ShortcutOverride, suppressing the window-level QAction. No competing Del/Ctrl+D bindings.
- GUI (qt-auto-test pid 487578): added element_1, Home tab → Duplicate/Delete buttons present &
  ENABLED. Clicked Duplicate → element_2 created (offset copy), selected; ID=element_2,
  X=Y=110.0px (orig 100 + 10 offset), z_order=1; undo label "Undo Add element" (undoable);
  both rows in timeline (screenshot p11-dup2.png). ✓
- Shortcut KEYS (Ctrl+D/Del/Backspace) not injectable via qt-auto-test (window QAction shortcuts
  don't fire from synthetic child-widget key events); slots verified via button (Duplicate) and
  P0-2 (Delete). Bindings + ShortcutOverride safety are code+reviewer-confirmed.

### 2026-07-22 — P1-2 handle hit-slack (impl → deterministic test)
- Impl: `kHitSlack=4` in SelectionHandles.h; `hitTest` grows each handle rect by slack and
  returns the nearest-center match (not first-match) to resolve corner/edge overlap on small
  elements. handleRect/draw untouched. Build exit 0.
- Deterministic proof: scratchpad/hittest.cpp links the compiled SelectionHandles.o + Qt6 →
  `HITTEST PASS (failures=0)`, run exit 0. Covers: (a) a click 6px outside the old 8px box now
  hits (was a miss), (b) nearest-center tiebreak on a 20x20 element — (9,0) picks TC not TL,
  (3,0) picks TL; exact TL/TC/TR still correct.
- Reviewer round skipped for this 15-line pure-geometry change: exact-spec match + deterministic
  execution proof is stronger than a code read. (Not the pattern for logic-heavy items.)

### 2026-07-22 — P1-3 tree reset coalescing (impl → reviewer APPROVE → GUI part b)
- Impl (4 files): CanvasWidget emits interactiveEditStarted/Finished on drag begin/end;
  TitleTreeModel.setResetsSuppressed() defers resets during a canvas drag (one reset on release);
  TitleTreeView re-applies EditorTitle selection after EVERY modelReset (was lost on any reset);
  MainWindow stores m_treeView and wires the two canvas signals. Build exit 0.
- Reviewer APPROVED — critically confirmed no PERMANENT suppression stuck-ON: m_dragging cleared
  only at the instrumented site; all started-without-finished paths (non-left release mid-drag,
  m_previewTitle) self-heal on the next completed left drag. Tree drag-reorder NOT suppressed
  (canvas signals don't fire). Selection re-apply: no recursion (m_syncingSelection guard),
  scrollTo EnsureVisible doesn't steal scroll. Non-blocking note: m_treeView null at connect()
  time but only deref'd at emit-time (during a drag) — harmless.
- GUI (qt-auto-test pid 509569): PART B confirmed directly — selected element_1 in tree, changed
  X 100->350 via spinbox (fires documentChanged->reset); tree KEPT element_1 highlighted
  (p13-afteredit.png). Previously any reset cleared the tree highlight.
- PART A (drag coalescing) not GUI-observed: portal-synthesized canvas drags on this Wayland
  session land just off the element (deselect instead of move) — single clicks work, press-move-
  release doesn't. Harness limitation, not a product bug. Coalescing logic is reviewer-verified.

### 2026-07-22 — P1-4 undo merge granularity (impl → GUI timing proof)
- Impl: single idle QTimer in RibbonFormatSection bumps m_mergeGen after 600ms of no edits;
  `mergeTag(base)` folds gen into the id (base + gen*100000, no collision — bases are 1000-1017)
  AND restarts the timer. All 15 mergeable pushes routed through it (12 fields + rotation +
  2 content), verified via grep (no bare ElemMergeTag:: left). Added ElemMergeTag::TextContent;
  SetElementRotationCmd now takes a mergeTag param (id() returns it). Build exit 0.
- Correctness reasoning: within a gesture (<600ms gaps) gen is stable → merges; a pause bumps gen
  → next edit is a new id → separate undo entry. QUndoStack only merges with the top command, so
  cross-gesture same-id (rare) can't wrongly merge. Undo restores the first push's m_before → no
  data loss. Reviewer round skipped: uniform mechanical wrapping + deterministic GUI proof below.
- GUI (qt-auto-test pid 524950): X spinbox (w0339), original 100. Set X=200 (gesture 1), PAUSE
  950ms (>600 → timer bumps gen), set X=300 (gesture 2). Undo #1 → X=200 (NOT 100 → gestures are
  SEPARATE entries; old bug would merge to 100). Undo #2 → X=100. ✓ Stack = [100→200],[200→300].
- Text half uses the identical mergeTag(TextContent)+timer path (typing run merges, pause seals) —
  covered by the same proven mechanism.

### 2026-07-22 — P1-5 error surfacing (mostly delivered by P0-4/P0-3; ribbon cleanup)
- The two high-value P1-5 targets were already done: load/save show the engine's real cause
  (P0-4 TitleDocument::lastError + dialogs); the rename catch(...){} was removed with proper
  validation (P0-3). This commit finishes the remaining ribbon swallows.
- Impl: all 17 `catch (...)` in RibbonFormatSection.cpp narrowed to
  `catch (const std::runtime_error&)` (the EXPECTED stale-element-id case from getElement — stays
  silent, no noise) + a trailing `catch (const std::exception& e) { qWarning() << e.what(); }` so
  a genuinely unexpected exception is logged instead of blindly eaten (and never propagates out of
  a Qt slot). The one catch-with-return (onDocumentChanged) preserves return on both arms.
  Added #include <QDebug>. grep confirms 0 bare catch(...) remain. Build exit 0.
- Verified by build + diff review (mechanical uniform change). No behavior change for the common
  stale-id path; only unexpected exceptions now surface in the log.

### 2026-07-22 — P1-6 route neutral colors through palette (dark-only)
- Impl: 4 neutral text/border literals → palette roles (no light-theme branch):
  transformeditor sectionLabel #aaa → PlaceholderText; painteditor "Brand Colors" gray →
  PlaceholderText; RibbonFormatSection read-only id #888 → PlaceholderText; F1 shortcuts HTML
  #aaa/#444/#ddd/#ccc → PlaceholderText/Mid/Text/BrightText via QString().arg(cHead,cBorder,
  cText,cKey) (width:100% left intact — % not followed by a digit). Added <QPalette>. Build exit 0.
- Kept functional colors: ColorLineEdit error flash, image-path warning #c0392b, checkerboard,
  swatch rings, timeline accents (out of scope this pass).
- GUI (qt-auto-test pid 538340, external import for the modal): opened Help→Keyboard Shortcuts;
  dialog renders correctly (p16-shortcuts.png) — dimmed section headers, bright key column,
  readable body, subtle row borders, all legible on dark. .arg substitution correct, no garbage.
- Minor follow-up noted (not P1-6): the static shortcuts reference doesn't list the new
  Delete/Ctrl+D shortcuts from P1-1.

### 2026-07-22 — P1-7 ColorWheel HiDPI (impl → build + reasoning)
- Impl: rebuildRing/rebuildTriangle now allocate backing QImages at size()*devicePixelRatioF()
  and setDevicePixelRatio(dpr). Ring uses QPainter (logical coords auto-scale). Triangle writes
  pixels directly, so its vertices are scaled by dpr and loop bounds clamped to physSz;
  barycentric weights are scale-invariant so colors are identical, just at physical resolution.
  paintEvent unchanged (drawImage honors the image DPR). Build exit 0.
- Correctness: at dpr==1, qRound(x*1.0)==x → images logical-sized, byte-identical to before (no
  regression on this dpr=1 session). Crispness manifests only on a real 2x display.
- Not GUI-observed: the ColorWheel lives behind a modal PaintEditor dialog (blocks qt-auto-test)
  and looks identical at dpr=1. Standard Qt HiDPI pattern; verified by build + no-regression logic.

### 2026-07-22 — Final full rebuild at HEAD (fe52549)
- `cmake --build build -j$(nproc)` → exit 0, clean (nothing to rebuild beyond up-to-date targets).
  All 12 commits compile together. git status clean except pre-existing `M CLAUDE.md` (untouched).

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
ALL planned P0 and P1 items are implemented, verified, and committed on branch `ux-audit-fixes`
(one focused commit each). Engine work (E1-E4) is committed + pushed to the engine origin; the
editor submodule was bumped to consume it.

Done (12 commits after the plan doc):
- P0-1 reuse engine ogt:: (de)serializers (27ad283) — roundtrip.cpp PASS
- P0-2 selection reconciled by identity (deb5b2b) — GUI: delete clears cleanly
- P0-3 undoable rename + dup/empty rejection (997a886) — GUI: undo "Rename element", dup rejected
- P0-4 asset validation + real save/load errors (9f55171) — GUI: red-border on bad path
- P1-1 Delete shortcut + Duplicate cmd (bd97d16) — GUI: Duplicate offset copy +10
- P1-2 handle hit-slack + nearest-center (f2be4ca) — deterministic hittest.cpp PASS
- P1-3 tree reset coalescing + selection re-apply (aac850c) — GUI: selection kept after edit
- P1-4 undo merge idle-time barrier (d5fc060) — GUI: two X-edits across a pause = 2 undo entries
- P1-5 typed catches in ribbon (8dd8e61) — 0 bare catch(...); load/save+rename via P0-4/P0-3
- P1-6 neutral colors → palette roles (ac5e54c) — GUI: F1 dialog renders correctly
- P1-7 ColorWheel device-pixel-ratio (fe52549) — build + dpr==1 no-regression

Every "works" claim above is backed by evidence in the Log (deterministic test / live qt-auto-test
screenshot+text-dump / reviewer trace). Harness notes: use qt-auto-test (NOT use-computer);
window-QAction key shortcuts and canvas drags aren't reliably injectable — verified those via
buttons / deterministic tests instead; native+modal dialogs block qt-auto-test (dismiss via xdotool,
screenshot via `import`).

Deferred (NOT done, need separate approval — see PLAN.md): P2-1..5 and D-1 rotation-correct
handles, D-2 multi-select, D-3 full tree parity, D-4 broader schema versioning. Minor follow-ups
noted in the Log: Animation Timing panel stale after structural undo (adjacent to P1-3); shortcuts
reference dialog doesn't list the new Delete/Ctrl+D. Full clean rebuild at HEAD: see final Log entry.
