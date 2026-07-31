<div align="center">

<img src="docs/images/app.png" width="112" alt="KeyLock Indicator">

# KeyLock Indicator

**Know your lock keys. Without looking down.**

A tiny Windows tray utility that shows Caps Lock, Num Lock and Scroll Lock
on screen — the moment they change.

<br>

![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=flat-square&logo=windows&logoColor=white)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Win32](https://img.shields.io/badge/Win32-native-1a1a1a?style=flat-square)
![No dependencies](https://img.shields.io/badge/dependencies-none-2ea44f?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-1a1a1a?style=flat-square)

</div>

<br>

## What it does

Your keyboard has no lights, or you never look at them. You start typing and
suddenly EVERYTHING IS SHOUTING. KeyLock Indicator puts a small, clean card on
your screen the instant a lock key changes, then gets out of the way.

It sits in the tray, needs no runtime, installs nothing, and stays invisible
until it has something to tell you.

<br>

## Screenshots

| Dark theme | Light theme |
|:---:|:---:|
| <img src="docs/screenshots/osd-dark.png" width="260" alt="OSD, dark theme"> | <img src="docs/screenshots/osd-light.png" width="260" alt="OSD, light theme"> |

<div align="center">
<img src="docs/screenshots/settings.png" width="760" alt="Settings window">
</div>

<br>

## The indicators

Every key gets its own mark, and **on and off are different shapes** — not just
different colours — so the state stays readable if you cannot tell those colours
apart. Below is the tray artwork, in the dark and light variants the app swaps
between as your Windows theme changes.

<table>
<tr>
<th align="left">Key</th>
<th align="center" colspan="2">Dark taskbar</th>
<th align="center" colspan="2">Light taskbar</th>
</tr>
<tr>
<td align="left"><b>Caps Lock</b></td>
<td align="center"><img src="docs/images/tray_caps_on_dark.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_caps_off_dark.png" width="46"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_caps_on_light.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_caps_off_light.png" width="46"><br><sub>off</sub></td>
</tr>
<tr>
<td align="left"><b>Num Lock</b></td>
<td align="center"><img src="docs/images/tray_num_on_dark.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_num_off_dark.png" width="46"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_num_on_light.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_num_off_light.png" width="46"><br><sub>off</sub></td>
</tr>
<tr>
<td align="left"><b>Scroll Lock</b></td>
<td align="center"><img src="docs/images/tray_scroll_on_dark.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_scroll_off_dark.png" width="46"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_scroll_on_light.png" width="46"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_scroll_off_light.png" width="46"><br><sub>off</sub></td>
</tr>
</table>

The card uses matching shapes, drawn in code as vector geometry so they stay sharp
at any scale.

<br>

## Features

- **Instant on-screen display** — a card with a soft shadow, drawn with
  Direct2D and composited with true per-pixel alpha
- **Three looks** — icon + text (the default), icon only, or a slim one-line bar
- **Or keep it up** — persistent badge mode parks a small live indicator at the
  spot you pick and leaves it there
- **Never steals focus** — keep typing; the caret does not move, and clicks pass
  straight through to whatever is underneath
- **Follows your theme** — light, dark, or whatever Windows is doing right now,
  switched live
- **High contrast aware** — turns on high contrast and the card goes fully
  opaque with system colours and a strong border, switched live
- **Talks to screen readers** — optional UI Automation announcements when a lock
  key changes, independent of whether the card is shown
- **Sharp at any scale** — per-monitor DPI aware (v2); the card is re-rendered at
  the scale of whichever monitor it appears on, never stretched
- **Stays out of games** — automatically suppressed in full-screen and
  presentation mode, badge mode included
- **Per-app exceptions** — name the apps that should never see the card; while
  one of them is in front, the OSD stays quiet
- **Keyboard layout alerts** — optional card when the active layout changes
  (TR-Q ↔ US and friends), with the real layout name from Windows
- **Tunable** — duration, opacity, size, position, and which keys to watch
- **Multilingual** — 25 languages, each listed under its own name, switched
  live without closing Settings
- **Truly portable** — drop a `KeyLockIndicator.ini` next to the exe and every
  setting lives in that file instead of the registry
- **Take your settings with you** — export and import the same `.ini` from the
  Settings window
- **Feather-light** — a single statically linked file, and it releases its GPU
  resources while idle

<br>

## Feature matrix

Every item from the original wish list, with an honest verdict.
✅ done · ⚠️ partial · ❌ not implemented.

<details>
<summary><b>Full feature matrix (33 items)</b></summary>

<br>

| # | Feature | Status | Notes |
|---:|---|:---:|---|
| 1 | Caps Lock state | ✅ | Watched by default |
| 2 | Num Lock state | ✅ | Watched by default |
| 3 | Scroll Lock state | ✅ | Supported; **off by default**, since most keyboards no longer use it |
| 4 | Insert / overwrite mode | ❌ | Deliberately not shipped — the bit does not mean "overwrite". See the note below the table |
| 5 | On-screen display | ✅ | Direct2D card on a DirectComposition surface with per-pixel alpha |
| 6 | Nine-point position grid | ✅ | Corners, edges and centre; the edge margin applies on both axes |
| 7 | Free drag-and-drop placement | ✅ | "Pick on screen…"; stored as a ratio, so it survives resolution changes |
| 8 | Display duration setting | ✅ | 800–4000 ms |
| 9 | Fade in / fade out animation | ✅ | Cubic-eased opacity and scale, driven by DirectComposition |
| 10 | OSD size setting | ✅ | 75–150 %; text is re-rasterised at the target size, not stretched |
| 11 | OSD opacity setting | ✅ | 60–100 % |
| 12 | Light / dark / system theme | ✅ | Follows Windows live, with a manual override |
| 13 | Icon-only view | ✅ | Small square card |
| 14 | Icon + text view | ✅ | The default |
| 15 | Minimal bar view | ✅ | Slim one-line bar, icon on the left |
| 16 | Persistent badge mode | ✅ | Off by default; stays on screen and tracks state live |
| 17 | Monitor choice (cursor / primary / all) | ✅ | "All" drives one composition chain per monitor |
| 18 | Per-application exclusion list | ✅ | Matched on the foreground executable name, case-insensitively |
| 19 | Full-screen detection and auto-hide | ✅ | `SHQueryUserNotificationState`; applies to badge mode too |
| 20 | Dynamic tray icon | ✅ | Reflects the chosen key's live state |
| 21 | Tray context menu | ✅ | Settings · Start with Windows · Pause · About · Exit, themed |
| 22 | Tray icon theme variants | ✅ | Separate light and dark artwork, swapped with the system theme |
| 23 | Start with Windows | ✅ | `Run` key, or `windows.startupTask` in the MSIX build; detects policy locks |
| 24 | Single-instance lock | ✅ | A second launch brings the running instance's Settings forward |
| 25 | Portable mode | ✅ | Drop a `KeyLockIndicator.ini` next to the exe; autostart still uses the registry |
| 26 | Global hotkey to summon the OSD | ✅ | Off by default (Ctrl+Alt+L); warns when the combination is already taken |
| 27 | Turkish / English interface | ✅ | Exceeded — **25 languages**, switched live |
| 28 | Keyboard layout change indicator | ✅ | Off by default; resolves the real layout name, so TR-Q and TR-F are told apart |
| 29 | High contrast theme support | ✅ | System colours, opaque card, thicker border, no shadow; tracked live |
| 30 | Screen reader announcements | ✅ | Off by default; `UiaRaiseNotificationEvent`, DLL loaded on demand |
| 31 | Per-monitor DPI v2 | ✅ | Declared in the manifest, with a runtime fallback |
| 32 | Import / export settings | ✅ | Same `.ini` format as portable mode; a corrupt file falls back per field |
| 33 | Low memory and idle CPU | ⚠️ | Idle is a fraction of a megabyte once the GPU chain is released. Not literally zero CPU — a 70 ms timer polls the key state. While the card is visible, memory is dominated by graphics driver heaps and exceeds the original target |

**On item 4.** Reading `VK_INSERT` is easy; the problem is what the bit means.
Windows keeps no global overwrite state — overwrite is each application's private
editing mode. The bit is really "how many times Insert was pressed, mod 2", and
Ctrl+Insert (copy) and Shift+Insert (paste) flip it too, so a user who copies and
pastes would be told "Overwrite ON". Applications also start in insert mode no
matter what the bit says. An indicator that is systematically wrong would undermine
the three that are correct, so it was left out rather than shipped as a
half-truth. The reasoning is written up in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

</details>

<br>

## Languages

The interface ships in 25 languages, each listed under its own name. Pick one in
Settings and it applies immediately — no restart, no reopening the window. Left on
**Automatic**, it follows your Windows display language.

Čeština · Dansk · Deutsch · Ελληνικά · English · Español · Suomi · Français ·
Magyar · Italiano · 日本語 · 한국어 · Norsk bokmål · Nederlands · Polski ·
Português (Brasil) · Português (Portugal) · Română · Русский · Slovenčina ·
Svenska · Türkçe · Українська · 简体中文 · 繁體中文

<br>

## Install

1. Download the latest ZIP from [Releases](../../releases)
2. Unzip it anywhere
3. Run `KeyLockIndicator.exe`

That is the whole thing. It is portable — no installer, nothing left behind.
Right-click the tray icon for Settings, or to start it with Windows.

**Want it fully portable?** Create an empty file called `KeyLockIndicator.ini`
next to `KeyLockIndicator.exe`. Every setting is then read from and written to
that file, so the whole app travels on a USB stick. The Settings window always
tells you which of the two it is using. ("Start with Windows" still uses the
registry — that is how Windows starts programs, and there is no file-based
equivalent; unticking the box removes the entry again.)

<br>

## Good to know

- The OSD cannot draw over the UAC prompt or the secure desktop. That is a
  Windows security boundary, not a bug.
- Some exclusive full-screen DirectX games may cover it.
- Inside a VM guest window, lock state can differ from the host.

<br>

<div align="center">
<sub>

Building from source, architecture and measurements:
**[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)**

MIT licensed · © 2026 ShadesOfDeath

</sub>
</div>
