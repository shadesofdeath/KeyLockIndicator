# Development notes

Everything a contributor needs that does not belong on the [README](../README.md):
requirements, build, architecture, settings storage, deviations from the
specification, and the measurements behind the design decisions.

---

## Requirements

**To run:** Windows 10 1809 (build 17763) or newer, x64.

**To build:** Visual Studio 2022 (MSVC v143) or 2026 (v145), Windows SDK 10.0.22621
or newer, CMake 3.25+, Ninja. Verified configuration: VS 2026 Community, MSVC
19.51, Windows SDK 10.0.26100, Ninja.

There are no third-party dependencies, no package manager and no submodules. The
only bundled library is `external/WinDark` (dark-mode theming for the settings
window), which is vendored in-tree — see deviation 1.

---

## Build

```powershell
tools\build.ps1 -Config Release
```

Manually:

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

Output: `build/release/KeyLockIndicator.exe`.

Add `-DKLI_PACKAGED=ON` for the MSIX variant, which uses `windows.startupTask`
instead of the `Run` key for autostart. `cpack` produces the portable ZIP.

Compiler settings: `/std:c++20 /permissive- /W4 /MT`, static CRT, LTCG,
`/OPT:REF /OPT:ICF`. **The build must stay warning-free at `/W4`;**
`#pragma warning(disable: ...)` is not allowed anywhere in the tree.

If the icons are missing, CMake runs `tools/make_icons.ps1` once during
configuration.

### House rules (spec §9)

These are not style preferences — they are enforced constraints:

- Warning-free at `/W4`; suppressing a warning instead of fixing it is forbidden.
- No raw `new`/`delete` — use `kli::ComPtr` and the `kli::unique_*` wrappers.
- No exceptions thrown; failures travel as `HRESULT`/`bool`.
- No global variables (file-scope `static` inside a `.cpp` is fine).
- All strings are `wchar_t`; the whole program is Unicode.
- Each `.cpp` stays at or under 400 lines — split it and register the new file in
  `CMakeLists.txt` when it grows past that.
- Comments are written in Turkish.

---

## Architecture

The dependency direction is one-way and must not be violated (spec §2):

```
KeyMonitor ───┐
ThemeWatcher ─┤
LayoutMonitor ┼──► App ──► TrayIcon
Settings ─────┘        └──► OsdWindow ──► OsdRenderer ──► IconGeometry
```

Modules do not know about each other. All wiring happens in `App` through
`std::function` callbacks. In particular **`OsdWindow` knows neither `KeyMonitor`
nor `Settings`** — everything it needs arrives as an injected `OsdConfig`, and the
enum values on both sides are locked together with `static_assert`.

| File | Responsibility |
|---|---|
| `main.cpp` | `WinMain`, DPI, single-instance mutex, COM, message loop |
| `App.*` | Coordinator + hidden host window, all callback wiring |
| `AppEvents.cpp` | Callback bodies split out of `App.cpp` |
| `AppCommands.cpp` | Tray commands, applying settings, building `OsdConfig` |
| `AppFilter.*` | Foreground executable lookup for the exclusion list |
| `KeyMonitor.*` | 70 ms `WM_TIMER` polling, event per changed key |
| `LayoutMonitor.*` | Keyboard layout change detection on the same poll tick |
| `ThemeWatcher.*` | `AppsUseLightTheme` via `RegNotifyChangeKeyValue` **and** `WM_SETTINGCHANGE` |
| `OsdWindow.*` | Layered DComp window, lifetime, re-trigger logic |
| `OsdShow.cpp` | Show/hide flow and dwell timers |
| `OsdPosition.cpp` | Nine-point grid, custom position, pick-on-screen mode |
| `OsdBadge.cpp` | Persistent badge mode |
| `OsdDevice.cpp` | D3D11/D2D/DComp chain, surface, frame, device-loss recovery |
| `OsdAnimation.cpp` | `IDCompositionAnimation` cubic curves, opacity/scale |
| `OsdRenderer.*` | Card, shadow, border, icon and text drawing |
| `OsdText.cpp` | DirectWrite layout and drawing |
| `OsdMetrics.h` | Single source of truth for every view mode's geometry |
| `IconGeometry.*` / `IconShapes.*` | Cached `ID2D1PathGeometry` per key and state |
| `TrayIcon.*` | `Shell_NotifyIcon`, context menu, `TaskbarCreated` recovery |
| `Settings.*` | Field table, load/save, range clamping, import/export |
| `SettingsStore.*` | Two back ends (registry / `.ini`) behind one enum-switched API |
| `SettingsDialog.*` | Modeless settings window themed through WinDark |
| `SettingsFields.cpp` | Control population and combo-box ↔ enum mapping |
| `SettingsExclude.cpp` | Excluded-apps list, file pickers, import/export dialogs |
| `SettingsPaint.*` | Background, Rufus-style section rules, `WM_CTLCOLOR*` colours |
| `HotkeyBox.*` | Read-only EDIT subclassed into a hotkey capture box |
| `ScreenReader.*` | `UiaRaiseNotificationEvent`, loaded on demand |
| `Autostart.*` | `Run` key / `StartupTask` |
| `MonitorUtil.*` | DPI, work area, active monitor, full-screen detection |
| `OemCheck.*` | OEM OSD agent scan + one-time warning |
| `Localization.*` | Reads `RT_STRING` blocks directly by language id |
| `Theme.*` | Palette tables, including the high-contrast palette |
| `Util.h` | `ComPtr`, `unique_*`, `HR()` macros, DPI helpers |

Strings live in `res/strings.rc`, which pulls in four part files
(`strings_core/west/east/asia.rc`) so no single file becomes unmanageable.
`Loc::Languages()` is the single source of truth for the language list, the stored
code and the resource language id.

---

## Settings storage

Root: `HKCU\Software\ShadesOfDeath\KeyLockIndicator`. Changes apply immediately.

If a file named `KeyLockIndicator.ini` sits next to the executable, that file is
used instead of the registry (portable mode). Both back ends share one field table
and one load/save code path, which is also what import/export uses. Autostart keeps
using the `Run` key in both modes — see the note in `Autostart.cpp`.

| Value | Type | Default | Range / meaning |
|---|---|---|---|
| `ShowOsd` | DWORD | 1 | 0/1 |
| `WatchCaps` | DWORD | 1 | 0/1 |
| `WatchNum` | DWORD | 1 | 0/1 |
| `WatchScroll` | DWORD | 0 | 0/1 |
| `WatchKeyboardLayout` | DWORD | 0 | 0/1 |
| `OsdDurationMs` | DWORD | 1400 | 800–4000 |
| `OsdOpacity` | DWORD | 82 | 60–100 |
| `OsdScalePercent` | DWORD | 100 | 75–150 |
| `OsdPosition` | DWORD | 0 | 0=top-centre, 1=centre, 2=bottom-centre, 3=top-left, 4=top-right, 5=middle-left, 6=middle-right, 7=bottom-left, 8=bottom-right, 9=custom |
| `OsdCustomX` / `OsdCustomY` | DWORD | 5000 | 0–10000, ratio of the work area |
| `OsdTopMarginDip` | DWORD | 72 | 0–400 |
| `OsdViewMode` | DWORD | 0 | 0=icon+text, 1=icon only, 2=bar |
| `PersistentBadge` | DWORD | 0 | 0/1 |
| `AnnounceToScreenReader` | DWORD | 0 | 0/1 |
| `ThemeMode` | DWORD | 0 | 0=system, 1=light, 2=dark |
| `SuppressFullscreen` | DWORD | 1 | 0/1 |
| `OsdMonitorTarget` | DWORD | 0 | 0=cursor, 1=primary, 2=all |
| `PrimaryMonitorOnly` | DWORD | 0 | 1.0 legacy; migrated on read, still written on save |
| `ShowOnlyWhenTurnedOn` | DWORD | 0 | 0/1 |
| `TrayIconKey` | DWORD | 0 | 0=Caps, 1=Num, 2=Scroll |
| `TrayLeftClickAction` | DWORD | 0 | 0=show OSD, 1=open settings, 2=nothing |
| `HotkeyEnabled` | DWORD | 0 | 0/1 |
| `HotkeyMods` | DWORD | `MOD_CONTROL\|MOD_ALT` | `MOD_*` bits |
| `HotkeyVk` | DWORD | `'L'` | virtual key code |
| `Language` | SZ | `auto` | `auto` or a code from `Loc::Languages()` |
| `ExcludedApps` | MULTI_SZ | *(empty)* | executable names, max 64 |
| `OemWarningShown` | DWORD | 0 | state, not a setting |

Every value added after 1.0 defaults to the 1.0 behaviour, so an upgrading user
sees no change until they opt in.

The settings window deliberately avoids `GROUPBOX` (the Win32 group frame draws a
bright etched rectangle in dark mode). It uses the Rufus pattern instead: a section
caption plus a one-pixel rule running to the edge of that caption's column
(`SettingsPaint.cpp`). `SysTabControl32` was evaluated and rejected for the same
reason as `msctls_hotkey32`: it themes its own strip through uxtheme, ignores
`WM_CTLCOLOR*` and leaves a light grey band in dark mode.

---

## Deviations from the specification

Each with its reason. Measurements were taken on this machine (Intel iGPU,
2560×1440 at 150 %, x64 Release).

1. **`external/WinDark` is not a git submodule.** Spec §1 lists it as one; the named
   library could not be found in any public source and is not on disk either. A
   dependency-free implementation filling the same role is vendored into the same
   directory: Win32 + uxtheme + dwmapi only, calling undocumented uxtheme ordinals
   under a "could not resolve → silently no-op" rule. If the real submodule ever
   turns up, **only `external/WinDark` changes** and `src/` is untouched. Hence no
   `.gitmodules`.

2. **`LockKey` / `LockState` live in the leaf header `src/LockTypes.h`.** Spec §4.1
   shows them inside `KeyMonitor.h`, but §2's "OsdWindow does not know KeyMonitor"
   rule cannot be broken. `KeyMonitor.h` includes the leaf header, so the
   declaration surface from §4.1 is preserved.

3. **`OsdPlacement` is declared separately in `OsdWindow.h`.** `OsdWindow` may not
   depend on `Settings.h` (§2); `App` translates between the two enums, whose
   numeric values are identical and locked with `static_assert`.

4. **Extra link libraries.** `comctl32` (Common Controls 6 APIs the §8 manifest
   mandates: trackbar, updown, `TaskDialogIndirect`), `wtsapi32`
   (`WTSRegisterSessionNotification`, §6), `dxguid` (`CLSID_D2D1Shadow` is declared
   with `DEFINE_GUID` in `d2d1effects.h`, so the definition lives in that library),
   `oleaut32` (BSTRs for the UIA announcement), `comdlg32` (file pickers),
   `shlwapi` (`SHLoadIndirectString` for keyboard layout names).
   `uiautomationcore.lib` is deliberately **not** linked: the announcement API is
   resolved at runtime with `LoadLibraryExW(..., SEARCH_SYSTEM32)`, so users who
   leave the setting off pay nothing and the Windows 10 1709 API baseline stops
   being a problem.

5. **Text antialiasing is `D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE`.** ClearType subpixel
   filtering does not work on a transparent per-pixel-alpha surface and produces
   exactly the black-fringe artefact §11 forbids.

6. **`ID2D1DeviceContext::SetDpi` is called explicitly.** `SetTarget` does not inherit
   the context DPI from the target bitmap (a bitmap's DPI only applies when it is a
   *source*). Without it the context stays at 96 DPI and every DIP maps 1:1 to a
   pixel — the card would draw 180 px on a 366 px surface at 150 % instead of 270.

7. **`IDCompositionSurface::EndDraw` must precede `Commit`.** DComp rejects a `Commit`
   issued while a surface draw is still open, with
   `DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED` (0x88980801).

8. **`IDCompositionScaleTransform` needs an explicit scale of 1.0**, otherwise the
   visual is never composited at all.

9. **The device chain is released while idle (deviation from §6).** §6 says to build
   the chain at startup and never release it, justified by "~200 ms latency on the
   first press". Measured instead:

   | Stage | Working set | Private bytes |
   |---|---|---|
   | Before D3D | 7.5 MB | 1.4 MB |
   | D3D11 device | 33.0 MB | 34.3 MB |
   | Full chain + frame | 51.7 MB | 48.5 MB |
   | Chain released | 28.6 MB | 14.6 MB |
   | + working-set trim | **0.2 MB** | 14.6 MB |

   Almost all of it is the driver's own heaps (`igc-default64.dll`, a 74 MB shader
   compiler; `igd10um64xe.dll`, a 19 MB UMD) — the application's own allocations are
   negligible. Rebuilding the chain plus the first frame measures **18–22 ms**
   (driver DLLs stay loaded, shader cache stays warm), so §6's 200 ms premise does
   not hold, while §11's "< 8 MB when hidden" target is unreachable with a live
   D3D11 device under any circumstances. The chain is therefore released **5 seconds**
   after the OSD hides and the working set is trimmed; rapid consecutive presses
   share one chain. Worst-case added latency is ~22 ms, far inside §11's 100 ms
   budget. Tunable via `kOsdIdleTeardownMs` in `src/Messages.h`.

10. **The `< 500 KB` executable target is not met.** Code plus static CRT is a small
    fraction of the binary; the bulk is icon resources — twelve tray icons, each
    carrying 48 px and 256 px frames the tray never asks for (the tray requests at
    most `SM_CXSMICON`, which is 32 px at 200 % scaling). Dropping those two frames
    from the twelve tray icons would bring the file back under the target. It was
    **not** done, because the icons are user-supplied artwork; it remains a
    one-step change if the size ever matters.

11. **The build is verified with MSVC v145 (VS 2026);** the spec says v143 (VS 2022).
    Nothing in the source is version-specific and both are `/std:c++20` compatible.

---

## Known limitations

Documented per spec §12, not worked around:

- The OSD cannot draw over the UAC prompt or the Secure Desktop (`uiAccess=false`).
  `uiAccess="true"` would require a signed certificate and installation under
  Program Files, and is incompatible with Store distribution.
- Some exclusive full-screen DirectX games may cover it.
- Inside a virtual machine guest window (VMware/VirtualBox), lock state can differ
  from the host.
- Right-to-left interface languages are out of scope until the dialog layout can be
  mirrored properly.

### Why there is no Insert / overwrite indicator

Reading the bit is trivial — `GetKeyState(VK_INSERT) & 1` toggles just as reliably
as the other three under the existing 70 ms poll. The feature was rejected because
the bit does not mean what such an indicator would claim:

- **It is not an overwrite flag.** Ctrl+Insert (copy) and Shift+Insert (paste) both
  flip it. A user who copies and pastes would be told "Overwrite ON".
- **Windows has no global overwrite state.** Caps/Num/Scroll are kernel state driving
  keyboard LEDs; overwrite is each application's private editing mode, so there is no
  shared source of truth to read.
- **The focused application usually ignores the bit.** Windows 11 Notepad has no
  overwrite mode at all, Word's Overtype is a separate option not bound to Insert by
  default, and browsers ignore the key entirely. Applications start in insert mode
  regardless of the bit, so the indicator would be re-falsified on every launch and
  focus change.

An indicator that is systematically wrong would undermine the credibility of the
three that are correct. If it is ever added, the only honest form is a separate
`LockKey::Insert` labelled "Insert (key toggle)" — never "Overwrite" — defaulted to
off, with the caveat spelled out in Settings.

---

## Verification

The behaviour claims in the README were checked by measurement rather than
inspection. Highlights from the last full pass:

- Clean rebuild from an empty build directory: all targets, zero warnings, zero
  errors at `/W4`.
- No `HR FAIL` output during a capture covering startup and Caps/Num/Scroll changes
  (the capture mechanism itself was proven against a known writer, so the silence is
  real and not a broken probe).
- Rapid repeated presses create exactly one OSD window — no accumulation.
- Focus is never taken: the foreground window stays put and typing continues to land
  in it while the card is visible; `WindowFromPoint` over the card returns the window
  underneath, confirming click-through.
- No GDI, USER or handle growth across repeated show cycles.
- Working set falls back to a fraction of a megabyte once the idle teardown runs.
- All nine grid positions measured against hand-computed coordinates.
- Settings round-trip verified per field: registry → dialog → registry → reopen.

Three defects found this way during 1.0, all listed above as deviations 6–8.
