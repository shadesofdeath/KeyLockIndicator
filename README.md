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
<img src="docs/screenshots/settings.png" width="420" alt="Settings window">
</div>

<br>

## The indicators

Each key has its own vector icon, drawn in code — and **on and off are different
shapes**, not just different colours, so the state stays readable if you cannot
tell those colours apart.

<table>
<tr>
<th align="left">Key</th>
<th align="center" colspan="2">On dark backgrounds</th>
<th align="center" colspan="2">On light backgrounds</th>
</tr>
<tr>
<td align="left"><b>Caps Lock</b></td>
<td align="center"><img src="docs/images/tray_caps_on_dark.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_caps_off_dark.png" width="40"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_caps_on_light.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_caps_off_light.png" width="40"><br><sub>off</sub></td>
</tr>
<tr>
<td align="left"><b>Num Lock</b></td>
<td align="center"><img src="docs/images/tray_num_on_dark.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_num_off_dark.png" width="40"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_num_on_light.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_num_off_light.png" width="40"><br><sub>off</sub></td>
</tr>
<tr>
<td align="left"><b>Scroll Lock</b></td>
<td align="center"><img src="docs/images/tray_scroll_on_dark.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_scroll_off_dark.png" width="40"><br><sub>off</sub></td>
<td align="center"><img src="docs/images/tray_scroll_on_light.png" width="40"><br><sub>on</sub></td>
<td align="center"><img src="docs/images/tray_scroll_off_light.png" width="40"><br><sub>off</sub></td>
</tr>
</table>

The tray icon uses the same set and swaps automatically with your Windows theme.

<br>

## Features

- **Instant on-screen display** — a square card with a soft shadow, drawn with
  Direct2D and composited with true per-pixel alpha
- **Never steals focus** — keep typing; the caret does not move, and clicks pass
  straight through to whatever is underneath
- **Follows your theme** — light, dark, or whatever Windows is doing right now,
  switched live
- **Sharp at any scale** — per-monitor DPI aware, so it stays crisp at 125 %,
  150 % and 200 %
- **Stays out of games** — automatically suppressed in full-screen and
  presentation mode
- **Tunable** — duration, opacity, size, position, and which keys to watch
- **Multilingual** — the interface follows your Windows language
- **Feather-light** — a single statically linked file, and it releases its GPU
  resources while idle

<br>

## Install

1. Download the latest ZIP from [Releases](../../releases)
2. Unzip it anywhere
3. Run `KeyLockIndicator.exe`

That is the whole thing. It is portable — no installer, nothing left behind.
Right-click the tray icon for Settings, or to start it with Windows.

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
