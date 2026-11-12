# yMonitory

[日本語版 README](README.ja.md)

A small always-on-top overlay for Windows that shows CPU, GPU and memory usage as numbers and graphs.

## Features

- CPU, GPU and memory usage as percentages, each in its own section with a graph of the last 60 samples. By default, the values update every second
- GPU usage is read with NVIDIA's NVML library, which comes with the NVIDIA driver and takes almost no CPU to read. On PCs without NVML, or where NVML cannot report GPU usage, yMonitory reads the `GPU Engine` performance counters that Task Manager also uses, which take a few milliseconds of CPU per sample. On a PC with neither, or with `ShowGpu=false`, the GPU section is not shown and nothing is read. `GPU: --` means the value could not be read for that sample
- Memory is shown on one line as two values: `RAM` is the share of physical memory in use, the same value as in Task Manager. `Commit` is committed memory as a share of the commit limit (physical memory plus page files). When `Commit` nears 100%, programs can fail to allocate memory even if physical memory is free. The memory graph shows both: `RAM` as a filled area and `Commit` as a line on top of it
- Clicks pass through the overlay to the window below. Hold Ctrl to move it or open its menu
- The overlay fades out as the mouse pointer comes within 100 px of it
- Size, position, opacity, update interval, colors and whether to show GPU usage are set in `config.ini`

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
| `Width`, `Height` | `200`, `150` | Size in pixels |
| `Opacity` | `200` | Opacity of the whole overlay, from 0 to 255 |
| `UpdateIntervalMs` | `1000` | Time between samples in milliseconds |
| `ColorText`, `ColorBackground`, `ColorGraphCpu`, `ColorGraphGpu`, `ColorGraphRam`, `ColorGraphCommit` | `#FFFFFFFF`, `#00000080`, `#00FFFF80`, `#A0FFFFCC`, `#FF00FF`, `#FFA0FFCC` | Colors as `#RRGGBBAA` or `#RRGGBB` |
| `ShowGpu` | `true` | Show GPU usage. With `false`, GPU usage is not read at all |
| `GhostModeEnabled` | `true` | Fade out when the mouse pointer comes near |

## Known limitations

- The right-click menu is in Japanese
- While Ctrl is held, the overlay takes mouse clicks, so a Ctrl+click on a window under the overlay goes to the overlay instead
- A position set by dragging is often not kept for the next start. To fix the position, set `X` and `Y` in `config.ini`
- When yMonitory is updated with Scoop, `config.ini` returns to the defaults
- The GPU value means slightly different things depending on its source. With NVML it is the share of time the GPU was running work, as `nvidia-smi` shows it. With the performance counters it is the busiest engine (3D, copy, video and so on), as Task Manager shows it
- On a PC with an NVIDIA GPU and another GPU, such as a laptop with integrated graphics, only the NVIDIA GPU is shown
- If you downloaded yMonitory from Releases and keep a `config.ini` from v1.1.1 or earlier, it has `Height=100`, which leaves each of the three sections about 33 px tall. Set `Height=150` to use the current default

## Uninstall

1. If `スタートアップ登録` is checked, uncheck it in the menu
2. Choose `Exit` in the menu
3. If you installed with Scoop, run `scoop uninstall ymonitory`. If you downloaded it from Releases, delete `yMonitory.exe` and `config.ini`

## Build

Requires the Visual Studio C++ build tools. In a developer command prompt for x64 (for example, after running `VsDevCmd.bat -arch=x64`), run the following in the repository folder. The output is `build\yMonitory.exe`.

```bat
mkdir build
rc /fo build\yMonitory.res src\yMonitory.rc
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fobuild\ /Febuild\yMonitory.exe src\main.cpp src\Config.cpp src\SystemMonitor.cpp src\GpuMonitor.cpp src\OverlayWindow.cpp build\yMonitory.res /link user32.lib gdi32.lib shell32.lib pdh.lib d2d1.lib dwrite.lib advapi32.lib /SUBSYSTEM:WINDOWS
```

## License

[MIT License](LICENSE)
