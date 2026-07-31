# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

Every setting below defaults to the 1.0 behaviour, so an upgrading user sees no change
until they opt in.

### Added

- **Nine-point position grid.** `OsdPositionMode` grew from top/middle/bottom to a full
  3 × 3 grid; the edge margin now applies on both axes. The stored 1.0 values 0/1/2 already
  meant top-centre / centre / bottom-centre in the new grid, so no migration is needed.
- **Pick the position on screen.** "Pick on screen…" makes the card draggable
  (`OsdCustomX`/`OsdCustomY`, stored as a 0–10000 ratio so it survives resolution and
  monitor changes). Release commits, Esc or right-click cancels.
- **Monitor target.** `MonitorTarget` replaces the primary-only flag with cursor / primary /
  all monitors; the 1.0 flag is still migrated on read and written on save.
- **Global hotkey.** `RegisterHotKey` + `MOD_NOREPEAT`, off by default (Ctrl+Alt+L), with a
  visible warning when another application already owns the combination.
- **Three OSD view modes** (`OsdViewMode`): icon + text (180 × 180 DIP, the 1.0 look and the
  default), icon only (96 × 96 DIP) and a minimal bar (240 × 56 DIP, icon left, one line
  right). Surface size and drawing both derive from a single metrics table, so the card can
  never be clipped by a mismatch.
- **Persistent badge** (`PersistentBadge`, off by default). The card stays at the chosen
  position and tracks the key state live; dwell and animations are disabled, click-through
  is kept, and full-screen suppression still applies (polled once a second).
- **High contrast support.** `SPI_GETHIGHCONTRAST` is honoured: the palette is derived from
  `COLOR_WINDOW` / `COLOR_WINDOWTEXT` / `COLOR_HIGHLIGHT`, the card becomes fully opaque, the
  border doubles in thickness and the shadow is dropped. Tracked live via `WM_SETTINGCHANGE`.
- **Screen reader announcements** (`AnnounceToScreenReader`, off by default). Lock changes are
  announced with `UiaRaiseNotificationEvent` over a host provider obtained from the message
  window; `uiautomationcore.dll` is loaded on demand so users who leave the setting off never
  pay for it. The announcement is independent of the OSD being shown.
- **Per-application exclusions** (`ExcludedApps`, `REG_MULTI_SZ`, empty by default). The
  foreground executable is resolved with `GetForegroundWindow` →
  `GetWindowThreadProcessId` → `QueryFullProcessImageNameW` and matched case-insensitively
  against the list, right before the card is shown. The tray icon and the screen-reader
  announcement are deliberately *not* filtered: the user asked for no card, not for no
  information. Managed from Settings with a list box, a file picker and a Remove button.
- **Portable mode.** If `KeyLockIndicator.ini` sits next to the executable, every setting is
  read from and written to that file instead of `HKCU`. Registry and INI share one field
  table and one code path (`SettingsStore`, an enum-switched value store — no class
  hierarchy). "Start with Windows" keeps using the Run key, because that is the only
  user-level way Windows starts a program; the Settings window states this explicitly.
- **Import / export settings.** The same `.ini` format and the same code path as portable
  mode, driven by `GetSaveFileNameW` / `GetOpenFileNameW`. Imported settings apply
  immediately and refresh every control. A corrupt or partial file falls back to defaults
  field by field instead of failing (verified against a 9 KB binary-garbage file).
- **Keyboard layout indicator** (`WatchKeyboardLayout`, off by default). The active layout is
  read on the existing 70 ms poll tick — no new thread, no new timer, and `KeyMonitor`'s
  contract untouched (`LayoutMonitor` is a separate small watcher). The card shows the ISO
  639 code plus the real layout name resolved from the `Keyboard Layouts` registry branch,
  so "Turkish Q" and "Turkish F" are told apart. It has its own keyboard badge geometry.
- **25 interface languages** — tr, en, de, fr, es, it, pt-BR, pt-PT, ru, uk, pl, nl, cs, sk,
  sv, da, fi, nb, hu, ro, el, ja, ko, zh-CN, zh-TW. `Loc::Languages()` is the single source of
  truth for the picker, the stored code and the resource lookup; each entry is listed under
  its own endonym, and switching is live. Right-to-left languages are deliberately out of
  scope until the layout can be mirrored properly.
- **MSIX packaging pipeline** (`tools/make_msix.ps1`). Builds the packaged binary, stages it,
  generates `resources.pri` with `makepri` and packs an `.msix` with `makeappx`. Three modes,
  because "install it here" and "upload it to the Store" need different packages:
  `-Register` sideloads the staging folder under Developer Mode with no certificate and no
  admin rights, `-Sign` produces a self-signed package for local distribution, and `-Store`
  produces an **unsigned** package — the Store re-signs with its own certificate and rejects
  one you signed yourself. Identity values are written into the staged manifest through the
  XML DOM, so the real publisher GUID never enters version control. `-Wack` runs the App
  Certification Kit.
- **Scale variants for every MSIX asset** — `.scale-125/150/200/400` for all seven tiles plus
  `.targetsize-16/24/32/48/256` (plated and unplated) for the 44 × 44 logo, 45 files in all,
  generated by `tools/make_icons.ps1` from the same geometry as the tray icons. Without them
  Windows upscales the 100 % asset and the tile edges go soft on any display above 100 %.

### Changed

- **Settings window is now two columns** (596 × 294 DLU instead of 310 × 407). The single
  column had already outgrown a 150 %-scaled 1080p work area, and this round adds four more
  controls. `SysTabControl32` was evaluated and rejected: it draws its tab strip through
  uxtheme, ignores `WM_CTLCOLOR*` and leaves a light grey band in dark mode — the same
  problem already documented for `msctls_hotkey32`. Two columns fix the height (−28 %) with
  no owner-draw code and stay closer to the dense single-page Rufus layout.
- **The packaged build now lands in `build/<Config>-msix/`** instead of sharing
  `build/<Config>/` with the portable build. Sharing one directory meant toggling `-Packaged`
  silently replaced the binary at a path that looked unchanged, so which variant you had was
  only discoverable by running it — and packaging was impossible while the portable build sat
  in the tray holding a lock on the file.
- **`AppxManifest.xml` declares all 25 interface languages** rather than only `tr-TR` and
  `en-US`. That list is what the Store shows as supported languages and what users search on;
  the previous two-language declaration predated the jump to 25 and would have hidden the app
  from 23 language filters. Default is now `en-US`, matching the binary's fallback, and
  Chinese is declared by script (`zh-Hans`/`zh-Hant`) because the shell matches on script
  rather than region.

### Documentation

- README is now a product page rather than a developer document: icon header, badges, real
  screenshots of the OSD in both themes and of the Settings window, a tray-icon gallery
  (three keys × on/off × light/dark), the language list, and an honest 33-item feature
  matrix that marks what is missing as missing.
- Build instructions, architecture, the source-file map, the settings-value table, the
  deviations from the specification and the measurements behind them moved to
  `docs/DEVELOPMENT.md`, including the reasoning for not shipping an Insert/overwrite
  indicator.
- `docs/STORE.md` covers the whole Microsoft Store path: the three packaging modes, the
  identity values that must come from Partner Center, the `runFullTrust` justification text,
  the listing checklist (age rating, privacy policy, screenshots, accessibility declaration)
  and the two App Certification Kit warnings that are safe to ignore for a desktop-bridge app.
  It also records why the package deliberately ships no `README.md` and no `.ini` — an `.ini`
  beside the executable would switch the app into portable mode, and an MSIX install directory
  is read-only, so settings would fail to save with no visible error.
- README gained an Install section split into ZIP / MSIX / fully-portable, a navigation row
  under the badges, and a Store-ready badge.

### Fixed

- **Icon changes never reached the executable.** `res/app.rc` embeds thirteen `.ico` files,
  but CMake's RC dependency scan does not follow them, so editing an icon left ninja
  reporting "no work to do" and the *old* artwork linked into the binary. The failure was
  silent — the build succeeded, only the product was wrong — and it was caught by comparing
  `RT_ICON` resources in a freshly packaged `.exe` against the files on disk, which were
  identical when they should not have been. `OBJECT_DEPENDS` on `res/app.rc` now names the
  icons and the four included `strings_*.rc` files explicitly.
- **The committed icons were stale**, generated by an earlier version of `make_icons.ps1`
  that wrote uncompressed DIB frames for every size below 256 px. Regenerating them stores
  all 78 frames as PNG: identical geometry and pixels, 336 KB → 68 KB on disk and
  335 KB → 67 KB of `RT_ICON` in the binary. With that gone the executable is **494.5 KB**,
  under the 500 KB target in spec §0 for the first time; the build script had been warning
  about the overrun on every build.
- The Settings window kept the previous theme's background when it opened with, or switched
  to, a theme different from the last one: controls repainted but the client area was never
  erased again. The background is now invalidated explicitly after the theme is applied.
- The excluded-apps list box showed a white sunken frame in dark mode. The dialog manager
  adds `WS_EX_CLIENTEDGE` to list boxes on its own and neither `DarkMode_Explorer` nor
  `DarkMode_CFD` darkens it; the style is now removed and the frame is drawn by the
  application in the theme colour.

## 1.0.0

First release.

### Added

- **On-screen display.** Square 180 × 180 DIP card with a 12 DIP corner radius, 1 DIP inner
  border and a Direct2D drop shadow, drawn with Direct2D + DirectWrite onto a
  DirectComposition surface for true per-pixel alpha.
- **Cubic-eased animation.** 130 ms opacity and scale fade-in, a configurable dwell, 200 ms
  fade-out. A key press while the card is visible updates the content and restarts the dwell
  timer instead of replaying the animation.
- **Vector icons generated in code.** Three keys × two states = six cached
  `ID2D1PathGeometry` objects. The *on* and *off* states are separate geometry, so the state is
  readable without relying on colour.
- **Key monitoring without a keyboard hook.** 70 ms `WM_TIMER` polling reading the low bit of
  `GetKeyState`, which also covers external keyboards, the on-screen keyboard and RDP. The
  initial state is read without firing a callback, so no OSD appears at startup. Silent resync
  after session change and resume from sleep.
- **Tray icon** reflecting the live key state, with a multi-line tooltip for all three keys, a
  themed context menu (Settings · Start with Windows · Pause the OSD · About · Exit) and
  automatic re-add when Explorer restarts.
- **Live theme tracking** of `AppsUseLightTheme` through both `RegNotifyChangeKeyValue` and
  `WM_SETTINGCHANGE`, with a Light/Dark override in Settings.
- **Settings window** themed through `external/WinDark`, using Rufus-style section headers
  instead of group boxes. Every change applies immediately and is persisted under
  `HKCU\Software\ShadesOfDeath\KeyLockIndicator`.
- **Settings:** OSD master switch, per-key monitoring, dwell duration, card opacity, card size,
  screen position, edge margin, theme mode, full-screen suppression, primary-monitor-only,
  "only when switched on", tray icon key, tray left-click action, language, restore defaults.
- **About window** with version, author and licence, themed like the settings window.
- **Localisation** in Turkish and English, read directly from the `RT_STRING` resource blocks by
  language id; `auto` follows `GetUserDefaultUILanguage()`. Language changes apply live.
- **Autostart** via the `Run` key, or `windows.startupTask` in the MSIX build
  (`-DKLI_PACKAGED=ON`), including detection of a policy-locked state.
- **Multi-monitor and DPI handling:** Per-Monitor DPI Aware V2, the OSD follows the monitor under
  the cursor, and full-screen/presentation mode is detected via `SHQueryUserNotificationState`.
- **OEM conflict check** scanning for preinstalled ASUS, Lenovo, HP, Acer and Dell indicator
  agents on first run, with a one-time dismissable warning.
- **Single instance.** A second launch brings the running instance's settings window forward.
- **Build:** CMake + Ninja, MSVC, `/std:c++20 /permissive- /W4 /MT`, static CRT, LTCG,
  `/OPT:REF /OPT:ICF`, plus `tools/build.ps1` and a portable ZIP target via CPack.

### Known issues

See *Known limitations* and *Deviations from spec* in the [README](README.md).
