# yMonitory

[日本語版 README](README.ja.md)

A small always-on-top overlay for Windows that shows CPU and memory usage as numbers and graphs.

## Features

- CPU usage and memory usage as percentages, with a graph of the last 60 samples for each. By default, the values update every second
- Clicks pass through the overlay to the window below. Hold Ctrl to move it or open its menu
- The overlay fades out as the mouse pointer comes within 100 px of it
- Size, position, opacity, update interval and colors are set in `config.ini`

## Install

With [Scoop](https://scoop.sh/):

```powershell
scoop bucket add yscoopy https://github.com/ycookiey/yscoopy
scoop install yscoopy/ymonitory
```

Or download `yMonitory.exe` from [Releases](https://github.com/ycookiey/yMonitory/releases) and run it. Requires 64-bit Windows.

## Usage

When `yMonitory.exe` starts, the overlay appears near the top-right corner of the primary display and an icon appears in the notification area.

- Move: hold Ctrl and drag the overlay with the left mouse button
- Menu: right-click the notification area icon, or hold Ctrl and right-click the overlay
  - `スタートアップ登録`: when checked, yMonitory starts when you sign in to Windows
  - `Exit`: closes yMonitory

## Settings

`config.ini` is in the same folder as `yMonitory.exe` and is written when yMonitory exits. Edit it while yMonitory is not running.

| Key | Default | Meaning |
|---|---|---|
| `X`, `Y` | `-1` | Position of the top-left corner in pixels. `-1` places the overlay near the top-right corner |
| `Width`, `Height` | `200`, `100` | Size in pixels |
| `Opacity` | `200` | Opacity of the whole overlay, from 0 to 255 |
| `UpdateIntervalMs` | `1000` | Time between samples in milliseconds |
| `ColorText`, `ColorBackground`, `ColorGraphCpu`, `ColorGraphRam` | `#FFFFFFFF`, `#00000080`, `#00FFFF80`, `#FF00FF` | Colors as `#RRGGBBAA` or `#RRGGBB` |
| `GhostModeEnabled` | `true` | Fade out when the mouse pointer comes near |

## Known limitations

- The right-click menu is in Japanese
- The memory value is committed memory as a percentage of the commit limit (the `% Committed Bytes In Use` performance counter), not the share of physical memory in use
- While Ctrl is held, the overlay takes mouse clicks, so a Ctrl+click on a window under the overlay goes to the overlay instead
- A position set by dragging is often not kept for the next start. To fix the position, set `X` and `Y` in `config.ini`
- When yMonitory is updated with Scoop, `config.ini` returns to the defaults

## Uninstall

1. If `スタートアップ登録` is checked, uncheck it in the menu
2. Choose `Exit` in the menu
3. If you installed with Scoop, run `scoop uninstall ymonitory`. If you downloaded it from Releases, delete `yMonitory.exe` and `config.ini`

## Build

Requires the Visual Studio C++ build tools. In a developer command prompt for x64 (for example, after running `VsDevCmd.bat -arch=x64`), run the following in the repository folder. The output is `build\yMonitory.exe`.

```bat
mkdir build
rc /fo build\yMonitory.res src\yMonitory.rc
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fobuild\ /Febuild\yMonitory.exe src\main.cpp src\Config.cpp src\SystemMonitor.cpp src\OverlayWindow.cpp build\yMonitory.res /link user32.lib gdi32.lib shell32.lib pdh.lib d2d1.lib dwrite.lib advapi32.lib /SUBSYSTEM:WINDOWS
```

## License

[MIT License](LICENSE)
