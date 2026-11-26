# diffpick – Project Specification

Nov 12, 2026 · @Maroš Polák · revised Nov 16, 2026

## Overview and goals

diffpick is a small native desktop launcher for Windows and Linux. It replaces Meld's own "what to compare" selection with a faster, more forgiving one, then starts Meld with the right arguments. diffpick never computes diffs itself.

Goals:

- Pick 2 or 3 items (files, directories or pasted text) and open them in Meld in as few clicks as possible.
- Accept paths the way people actually paste or copy them: quoted, with `/` or `\`, as `file://` URIs, or as files copied in the file manager (Ctrl+C).
- Never nag about the wrong kind of item. When a file meets a folder, diffpick opens a file picker in that folder instead of showing an error.
- Compare one file against one or more files chosen from a folder, each in its own Meld tab.
- Start comparisons straight from the file manager (context menu, Send To, file manager scripts).
- Ship as a standalone program: no Python or other runtime required on Windows.

## Platforms, tech stack and dependencies

C++17 with Qt 6 Widgets, built with CMake. The application uses Qt Core, Qt Gui and Qt Widgets. On Windows it also uses system APIs for Send To shortcuts and registry removal.

| Area | Windows 10/11 (x64) | Linux (X11 and Wayland) |
| --- | --- | --- |
| Compiler | MSVC 2022 (release builds) | GCC 11+ or Clang 14+ |
| Build | CMake 3.22+, Ninja recommended | CMake 3.22+, Ninja recommended |
| Qt 6 | 6.8 LTS from the Qt online installer or `aqtinstall` | 6.4 or newer from distro packages (Debian 12, Ubuntu 24.04), e.g. `qt6-base-dev` |
| Runtime | Qt DLLs + platform/style plugins + compiler runtime, collected by `windeployqt` | Distro Qt 6 libs; AppImage via `linuxdeploy` for distros with older Qt |
| Native APIs | Per-user registry writes via `QSettings`, removal via `RegDeleteTreeW`, Send To shortcut via `IShellLinkW` | XDG dirs via `QStandardPaths` |
| Meld | Meld for Windows (`Meld.exe`) | `meld` in PATH or Flatpak `org.gnome.meld` |

The minimum supported Qt version is 6.4; the code must build with it. Qt is linked dynamically (LGPLv3). Built-in Qt features cover drag and drop, clipboard, native file dialogs, path completion and settings storage.

## Main window

One compact window with three slots and a compare button. The window opens with focus in the Left slot.

| Slot | Required | Meld pane |
| --- | --- | --- |
| Left | Yes | 1st |
| Middle | No (enables 3-way) | 2nd (base) |
| Right | Yes | 3rd, or 2nd in 2-way |

Each slot has:

- An editable path field with path completion (`QCompleter` with `QFileSystemModel`, files and folders) and a built-in clear button (`clearButtonEnabled`). Typing or pasting is the primary input; it is normalized as described below.
- A live status indicator next to the field: file, directory, pasted text, "checking…", or "not found" (red). Classification is debounced (300 ms) and runs off the GUI thread, so slow or disconnected network paths never freeze the window. No modal errors while typing.
- A **Browse…** button with a menu: **File…**, **Folder…**, **Text…**, **From clipboard**.
  The actions are numbered 1–5 (including **Open pasted texts folder** as 5) and can be chosen by pressing that number while the menu is open.
- Clicking the **Left**, **Right** or **Middle** title opens that slot's Browse menu.
- Drop target for files and folders dragged from the file manager.

Pasting multi-line text into a path field behaves like **From clipboard** for that slot, because a single-line field would otherwise keep only the first line.

Filling several slots at once (drop, clipboard, command line) always uses the same mapping:

| Items | Slots |
| --- | --- |
| 1 | The target slot (Left when coming from the command line) |
| 2 | Left, Right |
| 3 | Left, Middle, Right |
| More than 3 | Nothing is filled; a status message says why |

- Dropping a single item on a slot fills that slot. Dropping several items anywhere replaces all slots using the mapping above.
- Dropping plain text (not files) on a slot is handled like **From clipboard** for that slot.

Window-level controls:

- **Swap** exchanges Left and Right, including the editable pasted text and its line-ending setting.
- Alt+S clicks **Swap**.
- The top-level **Menu** contains **Recent**, **System integration** and a checkable **Close after launch** action.
- **Recent** contains the last 15 comparisons; picking one fills the slots. An entry stores the slot values as entered, so a file + folder entry opens the picker again. Entries whose paths no longer exist are hidden; existence is checked in the background at startup, and until the check finishes all entries are shown.
- **System integration** contains **Install**, **Remove** and **Repair**.
- **Close after launch** defaults to on and is remembered.
- **Settings** in the menu opens the settings dialog.
- **Compare** button, the default button. Enter triggers it, Esc closes the window.
- Ctrl+1, Ctrl+2, Ctrl+3 move focus to Left, Middle, Right.
- Alt+L, Alt+R and Alt+M open the Browse dropdown for Left, Right and Middle.
- Alt+C or Ctrl+Enter clicks **Compare**.
- File and folder dialogs start in the folder of the slot's current value, otherwise in the active saved browse location, then the last used folder, then the home folder. Missing directories are skipped.
- The first row above Left provides five browse locations. Empty slots have a **+** button to choose a folder. Clicking a saved location activates or deactivates it. **×** removes it. At most one location is active, and changes are saved immediately.

## Path input normalization

Every path from any source (typed, pasted, dropped, clipboard, command line, file manager) goes through two functions, both unit-tested:

- `normalizePath(input, baseDir)` – a pure string transformation. It reads only environment variables, the home folder and `baseDir`; it never touches the file system.
- `classify(path)` – returns file, directory or missing. It is the only step that touches the file system, and the UI calls it off the GUI thread.

`normalizePath()` steps, in order:

1. Trim whitespace and line breaks.
2. Strip one pair of surrounding double or single quotes, e.g. from Windows "Copy as path". Remove a single unpaired trailing `"`: Windows turns `"D:\"` from a context menu into `D:"`. A bare drive `X:` left after this becomes `X:\`. Trim again.
3. Convert `file://` URIs to local paths, including percent-decoding (`QUrl::toLocalFile`). On Windows `file://server/share/x` becomes `\\server\share\x`. Other schemes (`sftp://`, `smb://` …) are rejected as "not a local path".
4. On Windows strip a `\\?\` prefix (`\\?\UNC\` becomes `\\`). Then convert both `/` and `\` to `/`.
5. Expand a leading `~` to the home folder. On Windows also expand `%VAR%` when `VAR` is defined; undefined `%…%` and other `%` characters stay literal.
6. Resolve relative paths against `baseDir`. The command line passes the process's working directory (relevant for file manager scripts on Linux). Input typed in the GUI passes no `baseDir`, so a relative path stays unresolved and shows "not found".
7. `QDir::cleanPath`, and remove a trailing separator except on a root (`C:/`, `/`, `//server/share`).

The result uses `/`; the UI displays it with `QDir::toNativeSeparators`.

Rules:

- A missing path is never launched. The slot shows "not found"; Compare explains which slot is wrong.
- On Linux, a backslash in input is treated as a separator. Real file names containing `\` are not supported.
- A trailing separator does not force a kind; the kind always comes from `classify()`.
- The field always shows the normalized native path after the user leaves it.
- Two slots hold "the same item" when their canonical paths (`QFileInfo::canonicalFilePath`) are equal, compared case-insensitively on Windows.

## File vs. directory handling

The wrong kind of item is never an error. A slot accepts a file or a folder; the kind is decided only at Compare. Where a choice is needed, diffpick opens the relevant folder so the user can pick.

| Situation | Behavior |
| --- | --- |
| **Folder…** clicked while the slot holds a file | Folder dialog opens at the file's parent folder. No warning. |
| **File…** clicked while the slot holds a folder | File dialog opens inside that folder. |
| Compare: 2 files, or 2 folders | Launch Meld directly. |
| Compare: 1 file and 1 folder | Open the **file picker** in that folder (see below). |
| Compare: 3-way with mixed kinds | Each folder slot, in the order Left, Middle, Right, opens the file picker in single-select mode, then a 3-way file comparison runs. Cancel in any picker cancels the whole comparison. |
| Compare: pasted text and a folder | Same as file and folder. Nothing is preselected, because pasted text has no real file name. |

### File picker for "file vs. folder"

1. A file dialog opens inside the folder from the folder slot, with multi-select enabled. Native dialogs are allowed.
2. If the folder contains a file with the same name as the compared file, it is preselected. Some native dialogs (e.g. the xdg-desktop-portal one) ignore preselection; this is accepted.
3. The user picks one or more files. Cancel launches nothing and keeps the window open.
4. Each picked file is compared with the fixed file in its own Meld tab: `meld --diff <fixed> <picked1> --diff <fixed> <picked2> …`. The fixed file stays on the side of the slot it came from. If the fixed file itself is picked, it is skipped.
5. More than 10 picked files asks once for confirmation before launching.

The same picker is used when this situation arrives from the command line or the file manager, e.g. "Compare with marked" where one item is a file and the other a folder.

## Pasted text and clipboard

Any slot can hold text instead of a path. The text is saved to a temporary file and that file's path goes into the slot.

- **Text…** opens an editor dialog: monospace font, no wrapping, undo, scrollbars. Ctrl+Enter applies, Esc cancels. If the slot already holds pasted text, the dialog opens with that text for editing.
- **From clipboard** checks, in this order:
  1. File URLs (`QMimeData::urls()`, e.g. files copied with Ctrl+C in Explorer or Nautilus): the local files fill slots using the mapping from "Main window".
  2. Text where every non-empty line (1 to 3 lines) normalizes to an existing absolute path, e.g. Explorer "Copy as path" with several files: the paths fill slots the same way.
  3. Anything else: the text is saved as pasted text in this slot.
- Storage location: `%TEMP%\diffpick\` on Windows, `~/.cache/diffpick/` on Linux. Not `/tmp`, because Flatpak Meld cannot see the host's `/tmp`.
- File name: `<YYYYMMDD-HHMMSS-mmm>/Text (left).txt`, `Text (middle).txt` or `Text (right).txt`, UTF-8 without BOM. Each save gets its own time-stamped subfolder, so the file name itself can be readable. Meld shows this name in every pane and tab, so no `--label` is needed.
- Line endings: **From clipboard** writes the text exactly as received. The Text… dialog works with `\n` internally (`QPlainTextEdit`); on save, text that originally used `\r\n` is written with `\r\n`, text that used only `\n` keeps `\n`, and text without line breaks uses the OS convention (`\r\n` on Windows, `\n` on Linux).
- Cleanup: on each start, delete pasted-text subfolders older than 7 days. Settings also has **Delete pasted texts now**. Pasted text may contain sensitive data and stays on disk until it is deleted.

## Launching Meld

Meld is found in this order; the first hit wins:

1. Command set in Settings.
2. `meld` / `Meld.exe` in `PATH`.
3. Windows: `%ProgramFiles%\Meld\Meld.exe`, `%ProgramFiles(x86)%\Meld\Meld.exe`, `%LOCALAPPDATA%\Programs\Meld\Meld.exe`.
4. Linux: Flatpak, if `flatpak info org.gnome.meld` succeeds → `flatpak run org.gnome.meld`.

The command in Settings is a command line split with `QProcess::splitCommand`: the first token is the program, the rest are arguments put before diffpick's own, e.g. `flatpak run org.gnome.meld`. Detection runs in the background on Compare and when checking the command in Settings.

Arguments used:

| Case | Command |
| --- | --- |
| 2 files or 2 folders | `meld A B` |
| 3-way | `meld LEFT MIDDLE RIGHT` |
| One file vs. several picked files | `meld --diff F X --diff F Y …` |

diffpick never passes `--label`; pasted text is identified by its file name (see "Pasted text and clipboard").

Launch rules:

- Start detached with `QProcess::startDetached`. diffpick does not wait for Meld and cannot detect failures after Meld has started.
- Pass arguments as a list, never through a shell.
- Validate before launching: 2–3 items, all existing, no item used twice (see the "same item" rule).
- Meld not found → message pointing to Settings, window stays open.
- Successful launch adds the comparison to Recent.

## System integration

Installed and removed from Settings or with `--install` / `--uninstall`. Per-user only, no admin rights.

| Entry | Windows | Linux |
| --- | --- | --- |
| Mark for compare | Submenu "Meld" in the context menu of files, folders and the folder background | Nautilus / Nemo / Caja script; Dolphin service menu |
| Compare with marked | Same submenu | Same as above |
| Open in diffpick | Same submenu, prefills the window | Same as above |
| Selected items (1–3) | Send To shortcut `.lnk` to `diffpick.exe --open --sort` (receives all selected items at once; the user clicks Compare in diffpick) | Script / service menu action with all selected items, `--compare --sort` |

Details:

- Windows registry keys: `HKCU\Software\Classes\*\shell\diffpick` (files), `…\Directory\shell\diffpick` (folders), `…\Directory\Background\shell\diffpick` (current folder, passed as `%V`). The submenu uses `MUIVerb`, an empty `SubCommands` value and a nested `shell` key with the three entries.
- Windows 11 shows classic context menu entries under "Show more options". This is accepted.
- Explorer runs a context menu verb once per selected item, so multi-select uses Send To. "Open in diffpick" on 3 selected items opens 3 windows (see Q3).
- Explorer and file managers pass selected items in an unspecified order, so `--sort` orders them by file name (natural order, case-insensitive) before assigning slots.
- "Compare with marked" is always visible, because static registry entries cannot be hidden. With nothing marked it shows an error message.
- Application and context menu icon: diffpick's blue owl holding the two compared documents.
- After installation, the result message lists the exact Windows registry keys and Send To shortcut, or the Linux script directories and Dolphin service-menu file that were created.
- Removal reports entries it could not delete. Missing entries count as already removed, and the recorded integration path is cleared only after successful removal.
- The Send To shortcut is created with `IShellLinkW` / `IPersistFile`, because `QFile::link` cannot store arguments.
- Install records the executable path in `integration/exePath`. If that path differs from the current executable (e.g. the zip folder was moved), the main window's status bar shows "System integration points to an old location" with a **Repair** button. Repair reinstalls the entries for the current path. The README tells users to run Remove before deleting the folder.
- Linux script folders: `~/.local/share/nautilus/scripts`, `~/.local/share/nemo/scripts`, `~/.config/caja/scripts`, written only for file managers found in `PATH`. Scripts are executable (`0755`).
- Dolphin: `~/.local/share/kio/servicemenus/diffpick.desktop`, executable, submenu "Diffpick", `MimeType=all/all;`.
- Linux scripts and the service menu call `$APPIMAGE` when diffpick runs as an AppImage, otherwise `QCoreApplication::applicationFilePath()`. The AppImage's temporary mount path is never written.
- Items from non-local locations (`sftp://`, `smb://` without a gvfs FUSE path) are rejected with an error message.
- The marked item is stored in settings and survives restarts.

## Command-line interface

All paths given on the command line go through `normalizePath()` with the working directory as `baseDir`. Errors in non-interactive modes are shown as a message box, because file managers have no console.
Operation switches (`--open`, `--compare`, `--mark`, `--compare-with-marked`, `--install`, `--uninstall`) are mutually exclusive. More than three positional items is an error before the window opens. `--sort` is a modifier, not an operation mode.

| Command | Behavior |
| --- | --- |
| `diffpick` | Open the window empty. |
| `diffpick A [B [C]]` or `--open A [B [C]]` | Open the window prefilled, using the slot mapping from "Main window". |
| `diffpick --compare [--sort] A B [C]` | Launch Meld directly; file vs. folder opens the file picker, and Cancel exits without launching. One path → open the window prefilled. More than 3 → error. `--sort` orders items by file name first. Used by Send To and file manager scripts. |
| `diffpick --mark A` | Remember A. Silent. |
| `diffpick --compare-with-marked B` | Compare the marked item (Left) with B (Right). The mark is kept, so several items can be compared against it. Nothing marked, marked item missing, or B is the marked item → error message. |
| `diffpick --install` / `--uninstall` | Add or remove system integration and print or show the result. |
| `diffpick --help`, `--version` | Standard Qt `QCommandLineParser` output (a message box on Windows). |

## Diagnostics

Diagnostic logging is opt-in and disabled by default. When enabled in Settings, diffpick writes `diffpick.log` to the pasted-text folder. It records the application/CMake version, Git tag and short hash at startup, each launched command line, Meld detection results and every error. On start the file is truncated to its last 256 KB. Disabling logging stops new writes but does not delete the existing log. The content of pasted text is never logged.

## Settings and persistence

Stored with `QSettings` in INI format: `%APPDATA%\diffpick\diffpick.ini` on Windows, `~/.config/diffpick/diffpick.ini` on Linux. On first start after the rename, settings are copied from the legacy `meldpick` location when no diffpick settings exist yet.

| Key | Content | Default |
| --- | --- | --- |
| `meld/command` | Custom Meld command line | empty = auto-detect |
| `ui/closeAfterLaunch` | Checkbox state | `true` |
| `ui/lastDir` | Last folder used in a dialog | home folder |
| `ui/browseLocations` | Five saved browse location slots | empty |
| `ui/activeBrowseLocation` | Active slot index, or -1 | -1 |
| `ui/geometry` | Window size and position | system default |
| `history` | Last 15 comparisons as slot values (Left, Middle, Right), newest first, duplicates removed | empty |
| `marked` | Item marked from the file manager | empty |
| `integration/exePath` | Executable path that system integration points to | empty = not installed |
| `diagnostics/logEnabled` | Whether diagnostic logging is active | `false` |

The Settings dialog shows the detected Meld command, has **Choose…** and **Auto-detect** buttons, and shows the pasted-text folder path with **Delete pasted texts now**. **Install**, **Remove** and **Repair** are in **Menu > System integration**.
**Settings file** displays the full INI path. Its **Open folder** button opens the containing directory in the file manager.

## Build, packaging and project layout

The core code (`PathUtil`, `MeldLauncher`, `PastedText`, `Settings`) is widget-free and can be unit-tested with Qt Test.

```
diffpick/
  CMakeLists.txt
  src/
    Main.cpp              // CLI parsing, mode dispatch
    PathUtil.{hpp,cpp}      // normalizePath(), classify()
    MeldLauncher.{hpp,cpp}  // find Meld, build args, startDetached
    PastedText.{hpp,cpp}    // save, cleanup
    Settings.{hpp,cpp}      // QSettings wrapper, history, marked item
    Log.{hpp,cpp}           // diffpick.log
    IntegrationWin.cpp    // registry submenu + Send To (IShellLinkW)
    IntegrationLinux.cpp  // scripts + Dolphin service menu
    MainWindow.{hpp,cpp}    // slots, recent, swap
    SlotWidget.{hpp,cpp}    // path field, status, browse menu, drop
    TextDialog.{hpp,cpp}
    SettingsDialog.{hpp,cpp}
  tests/
    tst_pathutil.cpp
    tst_meldargs.cpp
```

### Local builds

`CMakePresets.json` defines the `windows-release` and `linux-release` configure presets for Ninja Release builds. Each preset is listed only on its host platform. Their build directories are `build-Windows` and `build-Linux`. Use the configure preset with `cmake --preset`, then pass the build directory to `cmake --build`. CI uses the shared presets.

The local Windows 11 setup uses MSYS2 UCRT64 with GCC 16 and Qt 6.11 (`mingw-w64-ucrt-x86_64-qt6-base`). The local `windows-msys2` configure preset inherits `windows-release` and sets the Qt and compiler paths in the git-ignored `CMakeUserPresets.json`.

```powershell
cmake --preset windows-msys2
cmake --build build-Windows
ctest --test-dir build-Windows --output-on-failure
```

If you run a test executable directly with MSYS2, put `C:\msys64\ucrt64\bin` first in `PATH`. Git's `mingw64/bin` may supply an older `libstdc++-6.dll` and cause exit code `0xc0000139`. CTest prepends Qt's binary directory automatically on Windows.

Run these commands from the repository root on Linux or in WSL. The local WSL environment is Ubuntu 26.04 with GCC 15 and Qt 6.10.

```sh
cmake --preset linux-release
cmake --build build-Linux
ctest --test-dir build-Linux --output-on-failure
```

### Build configuration

- Set the application version in `cmake/DiffpickVersion.cmake`. CI checks that a pushed release tag matches `v<version>`.
- `cmake/BuildConfig.hpp.in` supplies the version and configure time. `cmake/GitInfo.hpp.in` supplies the Git tag and commit, refreshed on each build with a `-dirty` suffix for uncommitted changes. Code reads this metadata through `BuildInfo`.
- Build options, warnings, compiler caching, packaging, and the configure summary live under `cmake/`. Link `Diffpick::ProjectOptions` and `Diffpick::ProjectWarnings` to every new target. Set `-DDIFFPICK_WARNINGS_AS_ERRORS=ON` to treat warnings as errors. Test targets are defined in `tests/CMakeLists.txt`.

### CI and packaging

- GitHub Actions CI builds and runs the tests on every push and pull request: Windows (MSVC 2022, Qt 6.8.3) and Linux (Ubuntu 24.04, distro Qt 6.4). Platform-specific tests run only on their platform. The Windows job publishes the portable ZIP, and a separate Ubuntu 22.04/Qt 6.4.3 job builds and smoke-tests the AppImage; both packages are uploaded as workflow artifacts.
- A pushed `vX.Y.Z` tag must match the CMake project version. After Windows, Linux, and AppImage jobs succeed, CI creates a draft GitHub release with the portable ZIP and the AppImage attached as separate assets. Reruns fill missing assets in a draft and leave published releases untouched. Review the draft and publish it manually. The draft uses the unsigned Windows build. After SignPath is configured, replace that asset with the verified signed ZIP before publishing a signed release.
- Windows: `cmake --build build-Windows --target package_windows` runs `windeployqt`, resolves the MSYS2/MinGW runtime dependencies, verifies that no imported DLL is missing, and creates `build-Windows/package/diffpick-<version>-windows-x64.zip`. The zip contains the Qt LGPL and third-party license texts and where to get Qt sources. The exe is unsigned for now (see Q4). A WiX or Inno Setup installer is optional, later.
- Linux: `cmake --install` to `~/.local`; the `package_appimage` target uses pinned linuxdeploy and Qt-plugin releases to create `build-Linux/package/diffpick-<version>-linux-x86_64.AppImage`, including X11 and Wayland platform plugins.

## Development rules

Read this specification before changing behavior. Update it in the same change when a decision changes. Surface unresolved questions before implementing a decision. These development rules take precedence over conflicting requirements in this document.

- Write code, comments, commit messages, and UI strings in English. Wrap UI strings in `tr()` for translation with Qt Linguist. Talk to the user in Slovak.
- Keep platform-specific code in `IntegrationWin.cpp` or `IntegrationLinux.cpp`, or guard it with `Q_OS_WIN` or `Q_OS_LINUX`. Build it only on its platform.
- Get the user's approval before adding a Qt module or other library. Windows SDK system APIs are allowed in `IntegrationWin.cpp`.
- Add a test in `tests/` for each change to `normalizePath()`, `classify()`, or Meld argument building.
- State assumptions that affect the result. Clarify ambiguous requirements and explain meaningful tradeoffs before making a decision.
- Use the simplest implementation that meets the request. Match the existing style, keep edits focused, and remove code made unused by the change.
- Set verifiable success criteria. For multi-step changes, identify how each step will be checked. Reproduce bugs where feasible and verify the result with the relevant tests.

### Code style

Apply to all new and edited code. Use `.clang-format` for indentation, braces, and the 100-column limit. In changes unrelated to formatting, format only the edited lines, for example with `git clang-format`.

- Use braces for every `if`, `else`, `for`, `while`, and `do` body, including single statements. Put opening braces for functions and control blocks on their own lines. Start `else`, `catch`, and the closing `while` of a `do` loop on new lines.
- Check errors and invalid inputs first. Handle them with guard clauses that return or continue early, then keep the successful path at the outer indentation level. Omit `else` after a branch that already returned or continued.
- Leave a blank line after a complete control block when another statement, declaration, or preprocessor condition follows. Keep attached clauses, enclosing closing braces, switch labels, and preprocessor branch endings adjacent to the preceding block.
- Put a blank line after `#endif` when code follows, and name the condition on it (`#endif // Q_OS_WIN`).
- Declare and define the destructor before the constructors, and teardown before setup (`stop()` before `start()`), in the header and in the `.cpp`. Order header sections `public`, `protected`, `private`.
- Use the `.hpp` extension for C++ headers, including generated headers.
- Write type aliases as `using Name = Type;`, never `typedef`.

### Verification

- Use the local Windows and WSL builds for their respective tests. WSL covers `IntegrationLinux.cpp` compilation and unit and widget tests. Both local Qt versions are newer than 6.4, so verify the minimum version separately, for example in Linux CI.
- Verify Explorer context menu and Send To behavior, actual Linux file-manager integration (Nautilus, Nemo, Caja, and Dolphin), Flatpak Meld, and Wayland in the corresponding desktop environment. Include manual steps for behavior the automated tests cannot cover.

## Acceptance criteria

- [ ] `"C:/Users/x/a.txt"`, `C:\Users\x\a.txt` and `file:///C:/Users/x/a.txt` all resolve to the same file on Windows.
- [ ] `\\server\share\x`, `//server/share/x` and `file://server/share/x` resolve to the same path on Windows.
- [ ] `D:"` (Explorer's form of `"D:\"`) resolves to `D:\`.
- [ ] `%NOTSET%\a.txt` keeps `%NOTSET%` literally.
- [ ] `~/proj\src/` resolves to `~/proj/src` as a directory on Linux.
- [ ] A missing path never starts Meld and the slot shows "not found".
- [ ] Typing a path on a disconnected network drive never freezes the window.
- [ ] Folder… on a slot holding a file opens the dialog in the file's folder, without any warning.
- [ ] File + folder at Compare opens the picker in that folder with the same-named file preselected (where the dialog supports it).
- [ ] Picking 3 files opens one Meld window with 3 tabs, each comparing against the fixed file.
- [ ] Pasted text compares correctly and Meld shows it as "Text (left).txt" (or middle/right) in every pane and tab, including tabs from `--diff`.
- [ ] Pasted text with `\r\n` edited in Text… is saved with `\r\n`.
- [ ] Clipboard holding a path uses the path; any other clipboard text is used as pasted text.
- [ ] Two files copied with Ctrl+C in Explorer or Nautilus fill Left and Right with From clipboard.
- [ ] Mark + "Compare with marked" works from Explorer, Nautilus and Dolphin.
- [ ] Send To with 1 to 3 selected items opens diffpick with the slots prefilled; Meld starts only after the user clicks Compare. For `b.txt` and `a.txt`, `a.txt` is on the left.
- [ ] After moving the zip folder, diffpick shows the repair notice and Repair makes the context menu work again.
- [ ] Linux integration installed from the AppImage still works after diffpick is restarted.
- [ ] `--uninstall` removes every registry key, shortcut, script and service menu that `--install` created.
- [ ] Works with Flatpak Meld on Linux, including pasted text.
- [ ] Unit tests cover `normalizePath()`, `classify()` and Meld argument building on both platforms.
- [ ] Builds and passes tests with Qt 6.4 on Linux and Qt 6.8 on Windows.

## Known limitations

- Flatpak Meld sees the host's `/usr` and parts of `/etc` only under `/run/host`, so comparing such files fails inside Meld. Native `meld` is recommended for system files.
- Windows 11 shows the entries only under "Show more options".
- Some native file dialogs ignore preselection in the file picker.
- diffpick cannot detect Meld failing after it has started.
- Linux file names containing `\` are not supported.

## Open questions

- **Q1 – resolved Nov 16, 2026:** Meld applies `-L` only to the first comparison (`meld/meldapp.py`, `tab.set_labels(options.label)`). Verified with Meld 3.24.0 for Windows: with `-L LBL-1 -L LBL-2 --diff a b --diff a c` the first tab shows the labels and the second shows `a.txt — c.txt`. diffpick therefore does not use `--label` at all; pasted text gets a readable file name instead. Paths with Slovak diacritics and `\r\n` files work in Meld for Windows. Flatpak Meld was not tested (no Linux machine with Meld); the label code is the same Python code.
- **Q2 – partly resolved Nov 16, 2026:** On Windows (Meld 3.24.0) `--newtab` has no effect: every launch, with or without it, starts its own process and window. On Linux, Meld's source opens a second invocation in the already running instance (a new window, or a tab in the active window with `--newtab`); this is not verified. No setting for now; revisit after a Linux test.
- **Q3:** Single instance. Today each invocation is a separate process and window. A single instance (e.g. "Open in diffpick" fills the open window) needs `QLocalServer`, i.e. QtNetwork, a new dependency.
- **Q4 – provider selected:** SignPath Foundation is the planned signing provider. The project has Windows executable metadata and an artifact configuration ready for onboarding. See the [Code signing policy](code-signing.md) for the remaining setup.
- **Q5 – resolved Nov 22, 2026:** diffpick is released under the MIT License. Qt and other bundled runtime components retain their respective licenses and notices.

### Out of scope

- Computing or showing diffs inside diffpick.
- Diff tools other than Meld. No abstraction for them until a second tool is actually added.
- Git or other VCS revision selection.
- macOS.
- Windows 11 modern context menu (`IExplorerCommand` / sparse package).
- Thunar (`uca.xml`) and other file managers not listed above.
