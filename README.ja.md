# yMonitory

[English README](README.md)

Windows用の小さなオーバーレイ。CPU・GPU・メモリの使用率を、数値とグラフで常に最前面に表示する。

## 特徴

- CPU・GPU・メモリの使用率を百分率で表示し、それぞれの段に直近60回分の値をグラフで表示する。既定では値を1秒ごとに更新する
- GPUの使用率は、NVIDIAのドライバーに含まれるNVMLというライブラリーから取得する。取得にCPUをほとんど使わない。NVMLがないPCや、NVMLでGPUの使用率を取得できないPCでは、タスクマネージャーも使うパフォーマンスカウンターの`GPU Engine`から取得する。こちらは1回の取得にCPUを数ミリ秒使う。どちらもないPCや、`ShowGpu=false`にしたときは、GPUの段を表示せず、値も取得しない。`GPU: --`は、その回に値を取得できなかったことを示す
- メモリは2つの値を1行で表示する。`RAM`は物理メモリの使用率で、タスクマネージャーの値と同じ。`Commit`はコミット済みのメモリがコミットの上限(物理メモリとページファイルの合計)に占める割合。`Commit`が100%に近づくと、物理メモリが空いていても、プログラムがメモリを確保できずに失敗することがある。メモリのグラフは両方を重ねて表示する。`RAM`は塗りつぶした領域、`Commit`はその上に引いた線
- オーバーレイをクリックしても、下のウィンドウがクリックされる。Ctrlを押している間は、オーバーレイを移動したり、メニューを開いたりできる
- マウスポインターがオーバーレイから100px以内に近づくと、オーバーレイが薄くなる
- 大きさ・位置・不透明度・更新の間隔・色・GPUの使用率を表示するかどうかを`config.ini`で設定する

## インストール

[Scoop](https://scoop.sh/)でインストールする。

```powershell
scoop bucket add yscoopy https://github.com/ycookiey/yscoopy
scoop install yscoopy/ymonitory
```

または、[Releases](https://github.com/ycookiey/yMonitory/releases)から`yMonitory.exe`をダウンロードして起動する。64ビット版のWindowsが必要。

## 使い方

`yMonitory.exe`を起動すると、メインのディスプレイの右上の近くにオーバーレイが表示され、通知領域にアイコンが表示される。

- 移動: Ctrlを押しながら、オーバーレイを左ボタンでドラッグする
- メニュー: 通知領域のアイコンを右クリックするか、Ctrlを押しながらオーバーレイを右クリックする
  - `スタートアップ登録`: チェックを付けると、WindowsにサインインしたときにyMonitoryが起動する
  - `Exit`: yMonitoryを終了する

## 設定

`config.ini`は`yMonitory.exe`と同じフォルダにあり、yMonitoryの終了時に書き込まれる。yMonitoryを終了してから編集する。

| キー | 既定値 | 意味 |
|---|---|---|
| `X`, `Y` | `-1` | 左上の角の位置。単位はピクセル。`-1`にすると右上の近くに置く |
| `Width`, `Height` | `200`, `150` | 大きさ。単位はピクセル |
| `Opacity` | `200` | オーバーレイ全体の不透明度。0から255まで |
| `UpdateIntervalMs` | `1000` | 値を取得する間隔。単位はミリ秒 |
| `ColorText`, `ColorBackground`, `ColorGraphCpu`, `ColorGraphGpu`, `ColorGraphRam`, `ColorGraphCommit` | `#FFFFFFFF`, `#00000080`, `#00FFFF80`, `#A0FFFFCC`, `#FF00FF`, `#FFA0FFCC` | 色。`#RRGGBBAA`か`#RRGGBB`で書く |
| `ShowGpu` | `true` | GPUの使用率を表示するかどうか。`false`にすると、GPUの使用率を取得しない |
| `GhostModeEnabled` | `true` | マウスポインターが近づいたときに薄くするかどうか |

## 既知の制限

- Ctrlを押している間はオーバーレイがクリックを受け取るので、オーバーレイの下にあるウィンドウをCtrlを押しながらクリックしても、クリックはオーバーレイに届く
- ドラッグで動かした位置は、多くの場合、次の起動時に復元されない。位置を固定するには、`config.ini`の`X`と`Y`を設定する
- Scoopで更新すると、`config.ini`は既定値に戻る
- GPUの値の意味は、取得元によって少し違う。NVMLでは、GPUが処理を実行していた時間の割合で、`nvidia-smi`の値と同じ。パフォーマンスカウンターでは、最も使われているエンジン(3D・コピー・動画など)の使用率で、タスクマネージャーの値と同じ
- NVIDIAのGPUとほかのGPUの両方があるPC(内蔵GPUのあるノートPCなど)では、NVIDIAのGPUだけを表示する
- Releasesからダウンロードし、v1.1.1以前の`config.ini`を使い続けている場合は`Height=100`になっていて、3つの段がそれぞれ約33pxと狭くなる。今の既定の大きさにするには`Height=150`にする

## アンインストール

1. メニューの`スタートアップ登録`にチェックが付いていれば、チェックを外す
2. メニューの`Exit`で終了する
3. Scoopでインストールした場合は`scoop uninstall ymonitory`を実行する。Releasesからダウンロードした場合は`yMonitory.exe`と`config.ini`を削除する

## ビルド

Visual StudioのC++のビルドツールが必要。x64用の開発者コマンドプロンプトで、リポジトリのフォルダに移動して次を実行する。ビルドツールの`VsDevCmd.bat -arch=x64`を実行すると、開いているコマンドプロンプトでx64用のビルドツールを使える。出力先は`build\yMonitory.exe`。

```bat
mkdir build
rc /fo build\yMonitory.res src\yMonitory.rc
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fobuild\ /Febuild\yMonitory.exe src\main.cpp src\Config.cpp src\SystemMonitor.cpp src\GpuMonitor.cpp src\OverlayWindow.cpp build\yMonitory.res /link user32.lib gdi32.lib shell32.lib pdh.lib d2d1.lib dwrite.lib advapi32.lib /SUBSYSTEM:WINDOWS
```

## ライセンス

[MIT License](LICENSE)
