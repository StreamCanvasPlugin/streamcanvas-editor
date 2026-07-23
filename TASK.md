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
### D-1 — Rotation/shear-correct canvas manipulation (plan approved; add rotate handle; full rot+shear resize)
- [x] D1-1: transform helper (elementToWidgetTransform / elementLinear / makeElementTransform) — VERIFIED (impl+reviewer APPROVE, numeric 0.0 error vs Cairo)
- [x] D1-2: rotation-aware SelectionHandles API + rotate handle (9th) — VERIFIED (impl+deterministic handles_test PASS; reviewer skipped per P1-2 pure-geometry precedent)
- [x] D1-3: overlay drawing via transform (polygon box, mapped handles) — VERIFIED (impl build exit 0; mechanical migration, orchestrator diff-reviewed)
- [x] D1-4: hit-test (inverse-map body) + direction-based cursors — VERIFIED (impl build exit 0; orchestrator diff-review; identity case reduces to old AABB, no regression)
- [x] D1-5a: full rot+shear resize math (inverse-map delta + anchor-fixed solve); snapping identity-only — VERIFIED (impl+reviewer APPROVE, deterministic resize_test PASS incl. 90°+shear anchor-fixed, re-run by orchestrator)
- [x] D1-5b: rotate drag mode (DragMode::Rotate, kRotateHandle press → SetElementRotationCmd) — VERIFIED (impl build exit 0 + rotate_test PASS; reviewer pass for the un-GUI-testable drag gesture)
- [x] D1-6: verification (4 deterministic tests + qt-auto-test GUI) — VERIFIED (rotation+shear overlay + hit-test confirmed live; drag gestures via deterministic tests)

### D-2 — Multi-select (canvas marquee + tree ExtendedSelection + group ops) (plan approved 2026-07-22)
Plan file: ~/.claude-personal/plans/plan-d-2-golden-jellyfish.md. Additive layer: keep
`EditorTitle::m_selection` as the active/anchor (ribbon + all single-select readers untouched),
add a pointer-anchored selection SET on top. Scope (user): include align/distribute; MOVE-ONLY on
multi (no group-resize box; single-select keeps full resize/rotate); ribbon shows the active element.
- [x] D2-1: multi-selection model in EditorTitle (selectedIndices/toggle/setMultiSelection/
      selectionSetChanged; validateSelection re-anchors the whole set) — VERIFIED (impl build exit 0
      + reviewer APPROVE: single-select regression-free, active-always-a-member invariant, no
      under-emit). Contract note: setMultiSelection promotes front() if activeIndex not in indices.
- [x] D2-2: canvas marquee + multi-element highlight (handle box only when count==1) — VERIFIED
      (impl build exit 0 + reviewer APPROVE). Reviewer caught + orchestrator fixed: gate hitHandle
      in mousePress + updateCursorForPos on selectionCount()<=1 so 2+ selection can't grab an
      invisible resize/rotate handle on the anchor (rebuild exit 0). Marquee uses
      QPainterPath::intersects (rotated-quad correct, both containment directions).
- [x] D2-3: canvas group move drag + keyboard nudge (N SetElementBoundsCmd under one macro) —
      VERIFIED (impl build exit 0 + reviewer APPROVE: group built at press; snapped active-delta
      applied to all members from captured origs, no frame accumulation; release commits changed
      members under one macro, revert-before-push captures correct before-value; m_dragGroup cleared
      on every exit path. Group arrow-nudge = one macro per press (no cross-press coalescing, noted).
- [x] D2-4: group delete + duplicate (id-based, index-shift-safe macros) — VERIFIED (impl build
      exit 0 + orchestrator diff-review; reviewer round skipped per P1-2/D1-3 mechanical-change
      precedent). delete: ids resolved BEFORE removal (RemoveElementCmd by-id, shift-safe), single
      fast-path unchanged, multi under one macro. duplicate: insertElementCopy split into
      insertElementCopyImpl (id-returning, no selection) + wrapper; multi loops under one macro then
      setMultiSelection(newIndices, back()) keeps copies selected. updateToolBarState gates on
      selectionCount()>0. NOTE: copy/cut still single-active (clipboard is one json) — deferred.
- [x] D2-5: tree ExtendedSelection two-way sync — VERIFIED (impl build exit 0 + orchestrator
      diff-review; recursion-safe both directions via m_syncingSelection guard — view→setMultiSelection
      →selectionSetChanged→guarded; editor→select()→guarded). ExtendedSelection; onViewSelectionChanged
      rebuilds from full selectedIndexes(); onEditorSelectionSetChanged (driven by selectionSetChanged)
      selects all rows + sets current to active; Title/None fallback preserved; context-menu Remove
      now multi-aware. Tree modifier-click is GUI-injectable → live-verified in final D2 pass.
- [x] D2-6: ribbon shows active + Ctrl+A select-all + toolbar state — VERIFIED (impl build exit 0 +
      orchestrator diff-review; trivial). Select All (Icons16::Action_SelectAll, QKeySequence::SelectAll)
      → setMultiSelection over all VisualElements; enabled when elements.size()>1; "N elements selected"
      status hint via selectionSetChanged. Ribbon already shows active element (onSelectionChanged
      unchanged, approved).
- [x] D2-7a: align/distribute MATH + canvas AABB helper + MainWindow apply — VERIFIED (impl build
      exit 0 + orchestrator diff-review + deterministic align_test PASS (failures=0): 6 align modes,
      distribute H/V, unsorted-index mapping, <3/empty edge cases). New AlignMath.h (pure, header-only,
      registered in CMakeLists); CanvasWidget::elementTitleAABB reuses elementLocalToTitle (rotation/
      shear-correct); MainWindow alignSelected(>=2)/distributeSelected(>=3) push SetElementBoundsCmd
      (no pre-mutate; push applies + captures before), no empty macro, single-change fast path.
- [x] D2-7b: Arrange ribbon UI (6 align + 2 distribute actions) + enable gating — VERIFIED (impl
      build exit 0 + orchestrator diff-review). Home→Arrange panel: 6 align (Action_Align* icons) +
      2 distribute (Action_Distribute* icons) in 3 makeSmallStack columns (3+3+2), connected to
      alignSelected(mode)/distributeSelected(bool); updateToolBarState gates align≥2, distribute≥3.

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

### 2026-07-22 — D1-1 transform helper (impl → reviewer APPROVE)
- Plan approved (add on-canvas rotate handle; resize honors full rotation+shear). Plan file:
  ~/.claude-personal/plans/let-s-plan-d-1-eventual-sonnet.md. Design: one QTransform
  (element-local 0..w,0..h → widget px) mirroring the engine Cairo render exactly.
- Impl (CanvasWidget.h/.cpp, +55/-0, purely additive): file-local static
  `makeElementTransform(...)` (testable), `elementToWidgetTransform(el)`,
  `elementLinear(el)` (pure rotation+shear linear part in title space, for resize R^-1).
  Existing globalBounds/titleToWidget/widgetToTitle/letterboxRect untouched. Not yet wired
  to any call site (later subtasks). `cmake --build build -j$(nproc)` → exit 0.
- Deterministic: session scratchpad transform_test.cpp (g++ -std=c++20 + Qt6Core/Gui) →
  `TRANSFORM TEST PASS (failures=0)`, exit 0. Covers identity case, 90° center-pivot
  invariance, lin.inverted()*lin==I.
- Reviewer APPROVED with an independent NUMERIC ground-truth: compiled makeElementTransform
  vs a from-scratch Cairo matrix built from types.hpp:46-54 → max error 0.000e+00 across
  corners/center/interior AND for non-trivial shear(0.3,-0.15) at 27° (pins shear-direction
  + rotation-sign empirically, not analytic-only). Compose-order, shear convention, rotation
  sign all bit-identical to Cairo. Bonus finding: nested elements are ALSO exact — engine
  RenderChildren runs OUTSIDE the parent transform block, so translation-only
  GetGlobalPosition suffices; the "nested parent-rotation out of scope" caveat is a non-issue
  for render-match fidelity.

### 2026-07-22 — D1-2 rotation-aware SelectionHandles + rotate handle (impl → deterministic)
- Impl (SelectionHandles.h/.cpp, +152/-0, purely additive OVERLOADS — old QRectF methods
  byte-identical so CanvasWidget compiles untouched): `handleCenterT(idx,xf,localRect)` maps
  0-7 via xf.map(handleCenter(localRect)) and idx==kRotateHandle(8) → knob kRotateGap(24px)
  beyond mapped top-center along the mapped up-vector (degenerate-length guard); transform
  overloads of handleRect/hitTest/draw. hitTest nearest-center-wins across 0-7 (rect+slack)
  plus rotate knob (circle radius+slack). draw: mapped-quad border (AA on) + connector line
  + knob circle + the 8 square handles (AA off, original two-pass style). New constants
  kRotateHandle/kRotateGap/kRotateRadius. `cmake --build build -j$(nproc)` → exit 0.
- Deterministic: session scratchpad handles_test.cpp (links real SelectionHandles.cpp+Qt6) →
  `HANDLES TEST PASS (failures=0)`, exit 0. Covers identity handle centers + knob position,
  90° center-invariance + knob distance ≈ kRotateGap, hitTest knob/TL/miss on identity+rotated.
- Reviewer round SKIPPED (P1-2 precedent): pure-additive geometry + passing deterministic
  execution proof + orchestrator's own diff read. Visual styling deferred to D1-6 GUI shot.
- No call sites migrated yet (old overloads still in use by CanvasWidget); they get deleted
  in D1-4 once paintEvent(draw) and hitHandle(hitTest) are migrated.

### 2026-07-22 — D1-3 overlay drawing via transform (impl → build)
- Impl (CanvasWidget.cpp): added file-local `localBoundsRect(el)`=QRectF(0,0,w,h);
  `drawElementOutlines` now maps the 4 local corners → QPolygonF and drawPolygon (dashed
  two-pass), AA flipped false→true for clean rotated edges; paintEvent selection draw now
  calls the `SelectionHandles::draw(p, elementToWidgetTransform(*ve), localBoundsRect(*ve),…)`
  transform overload. `cmake --build build -j$(nproc)` → exit 0.
- Grep confirms the only remaining old-QRectF-overload caller is hitHandle:358 (hitTest);
  draw(QRectF) fully migrated. Old public overloads become dead after D1-4 migrates hitHandle.
- Mechanical migration mirroring the already-tested D1-2 draw pattern; orchestrator diff-read,
  reviewer round folded into D1-6 GUI (visual).

### 2026-07-22 — D1-4 hit-test + cursors (impl → build)
- Impl (CanvasWidget.h/.cpp + SelectionHandles.h/.cpp): added `elementLocalToTitle(el)`
  (local→title, no letterbox); hitTest body now inverse-maps titlePt to local + tests
  0..w,0..h (with inverted(&ok) degenerate guard); hitHandle uses the transform hitTest
  overload (can return kRotateHandle); new `resizeCursorForAngle(dx,dy)` folds handle outward
  direction to [0,180) → Hor/FDiag/Ver/BDiag; updateCursorForPos resolves selVe once, rotate
  handle → Qt::CrossCursor, resize handles → direction-based cursor, body → inverse-mapped
  SizeAll; removed the old fixed kHandleCursors[8]. Deleted the three dead QRectF SelectionHandles
  overloads (kept file-local handleCenter). `cmake --build build -j$(nproc)` → exit 0.
- No-regression note: for identity transform elementLocalToTitle = translate(gpos) only, so the
  inverse-map hit/cursor test reduces exactly to the previous AABB test. Cursor folding verified
  by orchestrator (TL→FDiag, TR→BDiag). Interactive confirmation deferred to D1-6 GUI.
- grep confirms only transform-overload SelectionHandles calls remain.

### 2026-07-22 — D1-5a rotation/shear-correct resize (impl → reviewer APPROVE)
- Impl: NEW `ui/widgets/ResizeMath.h` (pure inline `resizemath::anchorNorm`+`resizeSolve`);
  registered header-only in CMakeLists (matches GradientHandlePainter.h convention). CanvasWidget
  press-branch captures m_dragAnchorTitle/m_dragOrigCenterTitle/m_dragParentOffset from
  elementLocalToTitle at drag start. applyResizeDrag rewritten: project mouse delta into local
  axes via elementLinear().inverted(), call resizeSolve (anchor-fixed for corner/edge; center-fixed
  for Ctrl), snapping gated to identity && snapping && !Ctrl (block otherwise byte-identical).
  mouseRelease/keyPress unchanged. `cmake --build build -j$(nproc)` → exit 0.
- Deterministic: scratchpad resize_test.cpp (links ResizeMath.h + Qt6 + cairo) →
  `RESIZE TEST PASS (failures=0)`, exit 0. Gates: identity==old behavior; anchor world FIXED
  under 90° rotation (corner) and under shearX=0.3+rot=20° and an edge handle; Ctrl keeps orig
  center fixed. Orchestrator independently rebuilt+re-ran → PASS exit 0.
- Reviewer APPROVED via independent algebraic derivation from makeElementTransform (R=Shear*Rotate
  matches elementLinear; anchor solve = correct inversion of W(anchorLocal1)=anchorTitle; Ctrl R
  terms cancel; identity reduces to old edge-fixed behavior so snapping stays valid). Non-blocking:
  Ctrl-resize no longer snaps (was odd; improvement) — SURFACE TO USER. Degenerate Rinv (shear
  det 0) → unprojected fallback, non-crashing.

### 2026-07-22 — D1-5b on-canvas rotate drag (impl → reviewer APPROVE)
- Impl: ResizeMath.h `rotateSolve(origDeg,startRad,curRad,snap15)`; DragMode::Rotate +
  m_dragStartAngle/m_dragOrigRotation. mousePress routes kRotateHandle→Rotate (before resize),
  capturing center (elementLocalToTitle.map(w/2,h/2)) + start angle; applyRotateDrag sets
  rotation live via SetRotation; mouseMove dispatches Rotate; mouseRelease reverts-then-pushes
  ONE SetElementRotationCmd(...,-1) discrete entry iff changed; keyPress/Release re-apply live
  (Shift=15° snap). `cmake --build build -j$(nproc)` → exit 0.
- Deterministic: scratchpad rotate_test.cpp → `ROTATE TEST PASS (failures=0)`, exit 0.
- Reviewer APPROVED (this gesture is NOT qt-auto-test-injectable, so review is the primary
  verification): undo revert-then-push correct (before=orig via revert, after=final; -1 disables
  merge — confirmed in UndoCommands.cpp); center-pivot capture → no press jump; routing/lifecycle
  clean; no leaked/shadowed state. KNOWN COSMETIC (ship-fine, follow-up): atan2 seam can store a
  rotation offset by ±360° (renders identically; only odd in the spinbox) — SURFACE TO USER.
- Full rebuild at all-6-subtasks HEAD: `cmake --build build -j$(nproc)` → exit 0. All four
  deterministic tests re-run together by orchestrator → transform/handles/resize/rotate all PASS.

### 2026-07-22 — D1-6 verification (deterministic ×4 + qt-auto-test GUI)
- Full editor rebuild at all-6-subtasks HEAD: `cmake --build build -j$(nproc)` → exit 0.
- All 4 scratchpad deterministic tests rebuilt + re-run together by orchestrator:
  transform_test / handles_test / resize_test / rotate_test → each `PASS (failures=0)`, exit 0.
- GUI (qt-auto-test pid 1071302, NOT use-computer): Insert→Rectangle added element_1; set
  Rotation=30° via ribbon spinbox (w0288) → selection box became a ROTATED QUAD exactly
  overlaying the rendered rotated rectangle, 8 handles on the rotated corners/edges, rotate knob
  projecting along the rotated up-vector (d1-04/05). Set Shear X → box tracked the sheared
  PARALLELOGRAM with handles+knob following (d1-07). Deselected (empty-canvas click), then
  clicked the rotated body → element_1 RESELECTED (d1-09), proving the inverse-map body hit-test
  on a rotated element. Screenshots in session scratchpad d1-*.png.
- NOT GUI-observed (Wayland canvas press-move-release not injectable — same limitation as P1-3):
  the resize-drag and rotate-drag GESTURES. Both covered by the deterministic resize_test/
  rotate_test (anchor-fixed under 90°+shear; rotate math) + reviewer APPROVE traces. Cursors
  (not visible in screenshots) covered by code review + build.

### 2026-07-22 — D-2 multi-select COMPLETE (7 subtasks) + full build + GUI verification
- Full rebuild at D-2 HEAD: `cmake --build build -j$(nproc)` → exit 0 (`ninja: no work to do`, all
  incremental compiles already clean across the 7 subtasks).
- Deterministic: scratchpad/align_test.cpp (links AlignMath.h + Qt6Core) → `ALIGN TEST PASS
  (failures=0)`, exit 0. Covers all 6 align modes (L/HC/R/T/VM/B numeric deltas), distribute H/V,
  unsorted-input→original-index mapping, and <3/empty edge cases.
- GUI (qt-auto-test pid 2016159, NOT use-computer; socket via `launch` preload — ptrace inject
  fails but isn't needed): editor launches clean showing the new **Select All** button (Clipboard)
  and the full **Arrange** panel (6 align + 2 distribute, correct icons), all disabled with no
  selection (g0-launch.png). Added 3 rectangles via Insert→Rectangle. Clicked **Select All** →
  all 3 tree rows highlighted (ExtendedSelection, D2-5), status bar "3 elements selected" (D2-6),
  ribbon shows the active element_3 (D2-6), align buttons ENABLED (≥2 gate, D2-7b) (g1). Spread the
  3 to distinct X/Y via the Transform spinboxes → 3 separate rectangles, all selected (g2). Clicked
  **Align Left** → all 3 left edges snapped to x=100 (element_3 X 700→100 live), selection preserved
  (g3-aligned.png); Undo button read **"Undo Align elements"** (ONE macro entry). Clicked Undo →
  ALL 3 reverted together to the spread layout in a single step (g4-undo.png), Redo="Redo Align
  elements". Confirms tree multi-select, Select All, status hint, ribbon-active, align enablement,
  align execution (UI→alignSelected→AlignMath), and single-step group-undo all work live.
- NOT GUI-observed (known Wayland harness limit, same as P1-3/D-1): the canvas MARQUEE and
  GROUP-MOVE DRAG gestures (press-move-release not injectable). Covered by reviewer APPROVE (D2-2,
  D2-3) + the marquee QPainterPath-intersects logic + group-move deterministic reasoning. The
  multi-highlight rendering path was reviewer-verified; on-canvas the 3 selected boxes were shown
  (highlight outline is subtle against the fill at this zoom).

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

**D-2 (multi-select) is now COMPLETE and UNCOMMITTED** on the same branch (D-1 is also complete +
uncommitted — see below). Implemented across 7 subtasks (orchestrator + implementer/reviewer),
build exit 0, verified. Design: additive layer — `EditorTitle::m_selection` stays the active/anchor
(ribbon + all single-select readers untouched); a pointer-anchored selection SET (`m_selectedElements`)
layers on top with `selectedIndices()/isSelected/selectionCount/setMultiSelection/toggle/addToSelection`
+ a `selectionSetChanged()` signal; `validateSelection()` re-anchors the whole set by pointer.
Scope (user-approved): includes align/distribute; MOVE-ONLY on multi (single-select keeps full
resize/rotate handles — the handle box + hitHandle are gated to `selectionCount()<=1`); ribbon shows
the active element. Changed files: `model/EditorTitle.{h,cpp}`, `ui/widgets/CanvasWidget.{h,cpp}`
(marquee via QPainterPath-intersects, multi-highlight, group-move drag, `elementTitleAABB`),
`ui/widgets/TitleTreeView.{h,cpp}` (ExtendedSelection two-way sync), `mainwindow.{h,cpp}` (group
delete/duplicate macros, Ctrl+A Select All, "N selected" status, Arrange panel + align/distribute),
new `ui/widgets/AlignMath.h` (pure, registered in CMakeLists), `CMakeLists.txt`, `TASK.md`.
Verified: full build exit 0; deterministic align_test PASS (all 6 align modes + distribute H/V +
edge cases); reviewer APPROVE on D2-1/D2-2/D2-3 (the state-machine + drag logic); orchestrator
diff-review on the mechanical macro/UI subtasks (D2-4/5/6/7b); live qt-auto-test GUI confirmed tree
multi-select, Select All, "3 elements selected" status, ribbon-active, align-button enablement,
Align Left execution, and single-step group-undo ("Undo Align elements"). Canvas marquee/group-move
DRAG gestures not Wayland-injectable (harness limit) — covered by reviewer + logic. Known
limitation surfaced: copy/cut of a multi-selection stays single-active (clipboard holds one JSON;
multi-clipboard deferred). Committed as 07961c8 (single focused D-2 commit — the subtasks
intermingle within the same files, so per-subtask splitting wasn't clean). Not pushed.

**D-1 (rotation/shear-correct canvas manipulation) is now COMPLETE and UNCOMMITTED** on the same
branch — implemented across 6 subtasks (orchestrator + implementer/reviewer), all verified, but
NOT yet committed (awaiting user go-ahead; no commit was requested). Changed files:
`ui/widgets/CanvasWidget.{h,cpp}`, `ui/widgets/SelectionHandles.{h,cpp}`, new
`ui/widgets/ResizeMath.h`, `CMakeLists.txt` (+ this TASK.md). One central `QTransform`
(elementToWidgetTransform, numerically proven 0.0-error vs the engine Cairo render) drives the
selection box, 8 handles, a new drag-to-rotate knob, inverse-map hit-test, direction-aware
cursors, and a full rotation+shear-correct resize (anchor-fixed solve) + on-canvas rotate drag
(SetElementRotationCmd). Snapping is gated to identity transforms. Verified: full build exit 0;
4 deterministic tests PASS (transform/handles/resize incl. 90°+shear anchor-fixed/rotate);
reviewer APPROVE on the two math-heavy subtasks (1, 5a) and the un-GUI-testable rotate drag (5b);
live qt-auto-test GUI confirmed the overlay tracks rotation+shear and the rotated-body hit-test.
Two items to surface to the user: (a) Ctrl-resize no longer snaps (intentional, minor improvement);
(b) cosmetic — the on-canvas rotate drag can store a rotation offset by ±360° (renders identically,
only odd in the spinbox); optional fmod normalization is a possible follow-up. Drag GESTURES were
not GUI-injectable on this Wayland session (harness limit) — proven deterministically + by review.

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

Deferred (NOT done, need separate approval — see PLAN.md): P2-1..5, D-2 multi-select, D-3 full
tree parity, D-4 broader schema versioning. (D-1 is now done — see above.) Minor follow-ups
noted in the Log: Animation Timing panel stale after structural undo (adjacent to P1-3); shortcuts
reference dialog doesn't list the new Delete/Ctrl+D. Full clean rebuild at HEAD: see final Log entry.
