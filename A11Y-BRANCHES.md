# OrcaSlicer accessibility: planned pull requests

State on 2026-10-30. Every branch below is on https://github.com/daiverd/OrcaSlicer (browse one at `https://github.com/daiverd/OrcaSlicer/tree/<branch>`). All of them sit on upstream main 52c50e626e; merged together they build on Windows and pass Orca's tests. From now on they are updated by merging main, never rebased, so you can branch from them. The number in a branch name is its planned order; a dotted number builds on the branch with the shorter number. Tested on Windows 11 with NVDA unless a line says otherwise. A number like #17068 is `https://github.com/OrcaSlicer/OrcaSlicer/pull/17068` (or the issue with that number).

`accessibility-preview` is upstream main with every branch below merged in, for trying all of it at once. It is rebuilt from scratch when the branches change, so branch from the individual branches, not from it.

## What can be worked on in parallel

Seven tracks. Within a track, a branch waits for the one it builds on; separate tracks don't wait for each other.

1. **Keyboard navigation:** #15290, then `01.02`, then `01.02.02`, `01.02.04` and `01.02.05` side by side. `01.02.03` also needs track 4's `03.02`.
2. **Widget names and roles:** `05` (#17068), then `05.02`, `05.03`, `05.04`, `05.05`, `05.06` and `05.07` side by side, then `05.02.02` after `05.02`.
3. **Web pages:** #17057, then `06.03`, then `06.02` (the checker).
4. **3D view:** `03`, then `03.02`.
5. **Standalone:** `02`, `07` (Orca part), `08`, `09`, `10` and `11`, each on its own.
6. **wxWidgets:** the five wxWidgets pull requests, then porting them to Orca's wxWidgets fork once its 3.3.4 branch exists (#16370).
7. **Not started:** macOS (VoiceOver) and Linux (Orca screen reader, AT-SPI) support for the widget work, building on #16788's macOS code; the Design tab.

Where parallel tracks touch the same code, the second to merge has conflicts to resolve:
- Tracks 1 and 2 overlap most: the main tab strip (`Notebook.cpp`, `Notebook.hpp`), `MainFrame.cpp`, `GUI_App.cpp`, `Plater.cpp`, `wxExtensions` and several widgets (`Button`, `ComboBox`, `TabCtrl`, `StaticBox`, `SpinInput`, `TextInput`). Expect real conflicts; keep these two tracks in close contact.
- Tracks 1 and 4 both change the shortcut list (`Shortcuts.cpp`), `Notebook.hpp` and `docs/HLSD/keyboard-shortcuts.md`.
- Track 2's `05.06` and track 3 both change the setup wizard's window (`WebGuideDialog.cpp`).
- Track 5 touches shared files only lightly: `10` adds to the shortcut list and `GLCanvas3D.cpp`, `08` shares `Preferences.cpp` and `SpinInput` with track 2, `09` and `11` share `Plater.cpp` and `MainFrame.cpp`. These conflicts are small.

Orca's contributing guide asks one contributor to keep only a few pull requests open at a time, so parallel work goes fastest when different people carry different tracks.

## Open upstream

| PR | Branch | What it does |
|---|---|---|
| #15290 | `a11y-pr1-keyboard-nav` | F6 and Shift+F6 move between the main areas of the window. Left, Right, Home and End move along the main tab strip; Enter or Space opens a tab and keeps focus on it. A focus ring shows the focused tab, for keyboard focus only. |
| #17057 (draft) | `accessibility-06-web-wizard` | The setup wizard, Select Printers and Select Filaments work with the keyboard and a screen reader: real buttons, checkboxes and radio buttons, headings, named search fields, spoken result counts, a vendor list dialog. Adds the shared `a11y.js` the other web pages use. |
| #17068 (draft) | `accessibility-05-widget-accessible` | Screen readers get names, roles and states from Orca's custom-drawn widgets: the main and sidebar tabs read as tabs with only the current one selected, drop-downs as combo boxes with their value, checkboxes and radio buttons with their state; icon-only buttons get names; a long setting tooltip becomes a short name plus a description. Under the hood, one shared accessible (`WidgetAccessible`) that any widget sets a role and state on; #16788's Windows Button accessible folds into it. Windows only; no keyboard changes. Built and tested; NVDA check of this exact build still to do. |

## Planned, in order

| Branch | Builds on | What it does |
|---|---|---|
| `accessibility-06.03-web-pages` | 06 (#17057) | The other web pages: Home, Project, Preset Bundle, Plugins and its Terminal, Speed Dial, Flushing volumes. Adds a shared popup menu helper. |
| `accessibility-02-statehandler-unbind` | main | One-line fix: StateHandler unbinds its focus and mouse handlers from the wrong window. |
| `accessibility-01.02-book-keyboard-nav` | #15290 | Removes keyboard traps in Tab order: the tab strips (main and Tabbook) become one Tab stop each with arrow keys inside, the object list and per-object settings grid let Tab out, hidden and decorative controls stop being Tab stops. DropDown popups get arrow-key navigation. Names several unnamed controls (slice and print buttons, License box, don't-show-again checkbox). |
| `accessibility-01.02.02-combobox-keys` | 01.02 | Closed combo boxes: Alt+Down, Alt+Up and F4 open the list; arrow browsing is safe and skips group headers. |
| `accessibility-01.02.03-arrow-policy` | 01.02 and 03.02 | Arrow keys move only inside composite widgets (tab strips, lists, toolbars), never between ordinary buttons. Page Up and Page Down page the settings list from a sidebar control. |
| `accessibility-01.02.04-dropdown-accessible` | 01.02 | DropDown popups are read as lists by screen readers, and the keyboard reaches grouped lists. |
| `accessibility-01.02.05-multiline-tab` | 01.02 | Tab leaves a multi-line text box (G-code fields) instead of being stuck in it. Windows. |
| `accessibility-05.02-widget-names-events` | 05 | Parameter fields named by their row label, sections as named groups, combo boxes report expanded or collapsed and announce value changes once, tab strips announce the tab they switch to, the sidebar icon buttons named. |
| `accessibility-05.02.02-option-icons` | 05.02 | The painted buttons beside a full-width setting are in Tab order; multi-line settings and the G-code editor's controls are named. |
| `accessibility-05.03-topbar-keyboard` | 05 | The top bar (File, Save, Undo, Redo and the rest) is reachable by keyboard and readable by screen readers. |
| `accessibility-05.04-unsaved-dialog` | 05 | The unsaved-changes dialog: correct Tab order and first focus, the changes list as one named Tab stop with arrow keys and named rows. |
| `accessibility-05.05-publish-dialog` | 05 | The publish dialog: checkboxes and filter field named, mixed-filament tabs named, the tab indicator dot announced. |
| `accessibility-05.06-webview-focus` | 05 | Keyboard focus enters and leaves embedded web pages instead of skipping over them or getting stuck; every web view is named; focus returns to the 3D view after the setup wizard. |
| `accessibility-05.07-printer-edit-keyboard` | 05 | The sidebar's printer edit button is reachable by keyboard, focus returns to the printer box after editing, the printer variant box is named. |
| `accessibility-03-canvas-keyboard` | main | In the 3D view, Tab and Shift+Tab move focus out of the canvas like everywhere else, instead of switching Prepare and Preview or collapsing the sidebar (both stay available as assignable shortcuts, and Ctrl+Tab now switches tabs). Updates the matching hint. |
| `accessibility-03.02-canvas-screenreader` | 03 | The canvas toolbars get keyboard focus stops and a focus ring, and screen readers read the focused toolbar item. The Preview layer and move sliders are announced. Windows. |
| `accessibility-08-preferences-dialog` | main | Preferences: controls named, one Tab stop per row, correct Tab order in the page strip, F1 opens the wiki page for the focused setting. SpinInput arrows named. |
| `accessibility-09-notifications` | main | Screen readers hear notification errors, slicing progress, and filaments added or removed in the sidebar. |
| `accessibility-10-transform-dialog` | main | A move, rotate and scale dialog for the selection, opened with Alt+Enter or from the object menu's Transform submenu: a keyboard and screen reader alternative to the gizmos. |
| `accessibility-11-view-menu` | main | Four options that were only in the 3D view's popup (zoom button, axes, wireframe, realistic view) are added to the View menu. |
| `accessibility-07-wx-dataview` (Orca part) | main | The object list and its rename editor are named. Its wxWidgets fix is listed below. |

## Fixes that belong in wxWidgets

Prepared as wxWidgets pull requests (not yet opened); they reach Orca when its wxWidgets fork moves to 3.3.4 (#16370).

- The object list exposes no rows to screen readers on Windows: `wxDataViewCtrl`'s accessible is attached to the control instead of its main window.
- `wxIAccessible` lacks `IOleWindow`, which makes UI Automation tools show duplicate children.
- `wxDataViewCtrl` reports the tree expand/collapse key as a keyboard shortcut, so NVDA reads a direction key after every row.
- Tabbing into an Edge web view focuses its first or last element, and tabbing out follows wx's tab order (two pull requests).

## Overtaken by events

- `accessibility-06-web-content`: split into `06-web-wizard` (#17057) and `06.03-web-pages`. Delete.
- `accessibility-04-wx-iolewindow` and the wx patch in `accessibility-07-wx-dataview`: Orca no longer patches wxWidgets in its own tree; it builds from the `OrcaSlicer/Orca-deps-wxWidgets` fork. Replaced by the wxWidgets pull requests above. 07's three Orca-side commits stay.
- `accessibility-06.02-web-check` (static checker for the web pages, plus a CI job): 7 of its 9 commits are old copies of the web page work, superseded by #17057 and 06.03. Its 2 own commits need moving onto 06.03, and the checker needs exemptions for the Export Presets dialog and the Home page staff picks, which 06.03 leaves out.
- Partly overtaken, to trim before opening:
  - `05.06`: "Give the filament-only and printer-only guide pages a real title" is also in #17057; it drops out when #17057 merges. Part of its web view focus handling may become unnecessary once Orca uses wxWidgets with the web view Tab fixes; to be rechecked then.
  - `01.02.03`: "Let Ctrl+Tab through the sidebar's page-key handler" only matters if someone rebinds Ctrl+Tab, now that #16301 handles it as a global shortcut.

## Not covered yet

- The Design tab (parametric CAD, new in upstream main) has had no accessibility work.
- macOS and Linux screen readers: the widget work above is Windows-only, apart from #16788's macOS code.
- ImGui overlays other than move, rotate and scale.
