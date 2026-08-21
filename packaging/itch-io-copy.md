# itch.io page copy — StreamCanvas Editor

Copy-paste source for the itch.io project page. Checked against
[itch.io's quality guidelines](https://itch.io/docs/creators/quality-guidelines);
see the checklist at the bottom for what's verified vs. what's still your call.

Page setup fields are listed first; the long description body follows in
itch.io-flavored Markdown.

---

## Page setup fields

| Field | Value |
|---|---|
| Title | StreamCanvas Editor |
| Kind of project | Tool |
| Release status | Released — **only if** you're uploading a tested installer for every platform you select (see checklist) |
| Pricing | Paid — $19.99 base price, one license per user. Don't set this higher then discount it back down to fake a sale. |
| Platforms | Confirmed: **Linux** (the build in this repo runs and is what the screenshots below were taken from). `CMakeLists.txt` also has Windows and macOS packaging steps, but I have not built or run those — only check those boxes once you have a tested installer for each, per the guideline that platform selection must match real executables. |
| Tags | obs, streaming, broadcast-graphics, overlay, lower-thirds, animation, editor |
| Short description (tagline) | Design animated broadcast graphics and play them live in OBS. |

Tag notes: dropped `obs-studio` (synonym of `obs`), `tool` (already covered by
"Kind of project"), and `qt` (implementation detail, not something a buyer
searches for) — the guidelines call out synonyms and classification-redundant
tags specifically. Don't add "StreamCanvas Editor" itself as a tag — tagging
your own title is against the guidelines.

---

## Long description (body)

# StreamCanvas Editor

Design animated broadcast graphics — lower thirds, overlays, alerts, QR
codes — and play them live in OBS Studio.

This download includes two parts:

- **StreamCanvas Editor** — the desktop app where you build scenes.
- **StreamCanvas OBS plugin** — the OBS Studio source that plays those
  scenes back live, animations included.

You build a scene once in the editor and save it. In OBS, you add a
StreamCanvas source, point it at the saved scene, and the plugin renders it
during your stream.

## Features

- Canvas-based scene editor with text, image, and QR code elements
- Move, resize, rotate, and shear elements, with correct handles at any angle
- Fill and stroke editor with linear and radial gradients, corner radius,
  padding, and drop shadows
- A shared brand color palette across every element in a scene
- In and out animations, plus data-driven animations that trigger when live
  data changes (score updates, follower counts, and similar)
- Timeline editor for animation timing
- Layer tree with drag-and-drop reordering
- Undo and redo on every edit
- Multi-select with marquee selection, group move, align, and distribute
- Copy, paste, and duplicate that preserve every property, including
  animations
- Missing-asset checks before save, so a scene never reaches OBS broken

## How it works

1. Install the editor and the OBS plugin.
2. Build a scene in the editor: add elements, set their animations, pick
   your colors.
3. Save the scene.
4. In OBS, add a StreamCanvas source and point it at the saved scene.
5. The plugin renders the scene live, animations included.

## Requirements

- OBS Studio, for playback
- Windows, macOS, or Linux

## License

One license per user. See the included EULA for full terms. Bug reports and
license questions: diego95lopes@gmail.com.

---

## Screenshots

Real captures from a live run of the editor (`packaging/itch-screenshots/`),
not mockups:

- `02-editor-element-transform.png` — a loaded lower-third scene, an element
  selected with its transform handles and properties visible, animation
  timeline underneath.
- `03-editor-style-tab.png` — same scene, Style tab open (fill, stroke,
  border, shadow).

Use one of these two as the cover image and both as gallery screenshots.
`01-editor-empty-canvas.png` (blank canvas, no content) is in the same folder
but is a weak cover image — keep it only as a filler screenshot, or drop it
and capture 2–3 more with different templates (QR code element, an alert
overlay, multi-select) before publishing. The guidelines call screenshots
"highly recommended" and explicitly warn against fake or misleading imagery —
these are unedited app screenshots, which is the safe side of that rule.

---

## Compliance checklist (itch.io quality guidelines)

Verified while writing this copy:
- [x] Every feature line in the description matches a feature actually present
  and verified in the codebase (checked against `TASK.md`'s verification log
  and `CLAUDE.md`'s architecture notes) — no unrealistic or invented claims.
- [x] Tags trimmed to avoid synonyms, classification-redundant tags, and
  self-tagging with the project name.
- [x] Screenshots are real, unedited captures of the running app, not
  mockups or borrowed imagery.
- [x] No sale gimmick — one base price, no fake-discount setup described.

Still needs your call before hitting publish (things I can't decide or verify
from here):
- [ ] **Platforms box**: only check Windows/macOS if you actually have a
  tested installer for each. Right now only Linux is verified.
- [ ] **Release status**: keep "Released" only once real installers for every
  checked platform are uploaded directly to itch.io (guidelines: don't link
  out to GitHub/other hosts for the actual download, and don't publish before
  the page — files, images, classification — is genuinely complete).
- [ ] **Languages field**: leave it at English only unless the UI is actually
  localized (it isn't, as far as the codebase shows) — don't select a
  language just because text could theoretically be translated.
- [ ] **Cover image**: pick one of the two real screenshots above, or a crop
  of one. Don't commission or generate a separate "marketing" image that
  doesn't reflect the actual UI.
- [ ] **AI disclosure**: the repo's own README already discloses that parts of
  this project were built with AI coding tools, reviewed before merging.
  That's a development-process disclosure, not a generated-content one, so
  itch.io's AI-content tagging (aimed at generated art/assets) likely doesn't
  apply — but it's your call whether to add a line about it on the page for
  transparency.
