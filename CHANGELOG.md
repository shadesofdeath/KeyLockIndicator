# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
