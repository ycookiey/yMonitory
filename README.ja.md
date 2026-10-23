# yMonitory

[English](README.md)

Windows用の小さなオーバーレイ。CPUとメモリの使用率を、数値とグラフで常に最前面に表示する。

## 特徴

- CPUの使用率とメモリの使用率を百分率で表示し、それぞれ直近60回分の値をグラフで表示する。既定では値を1秒ごとに更新する
- オーバーレイをクリックしても、下のウィンドウがクリックされる。Ctrlを押している間は、オーバーレイを移動したり、メニューを開いたりできる
- マウスポインターがオーバーレイから100px以内に近づくと、オーバーレイが薄くなる
- 大きさ・位置・不透明度・更新の間隔・色を`config.ini`で設定する

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
| `Width`, `Height` | `200`, `100` | 大きさ。単位はピクセル |
| `Opacity` | `200` | オーバーレイ全体の不透明度。0から255まで |
| `UpdateIntervalMs` | `1000` | 値を取得する間隔。単位はミリ秒 |
| `ColorText`, `ColorBackground`, `ColorGraphCpu`, `ColorGraphRam` | `#FFFFFFFF`, `#00000080`, `#00FFFF80`, `#FF00FF` | 色。`#RRGGBBAA`か`#RRGGBB`で書く |
| `GhostModeEnabled` | `true` | マウスポインターが近づいたときに薄くするかどうか |

## 既知の制限

- メモリの値はコミット済みのメモリがコミットの上限に占める割合で、物理メモリの使用率ではない。値はパフォーマンスカウンターの`% Committed Bytes In Use`から取得する
- Ctrlを押している間はオーバーレイがクリックを受け取るので、オーバーレイの下にあるウィンドウをCtrlを押しながらクリックしても、クリックはオーバーレイに届く
- ドラッグで動かした位置は、多くの場合、次の起動時に復元されない。位置を固定するには、`config.ini`の`X`と`Y`を設定する
- Scoopで更新すると、`config.ini`は既定値に戻る

## アンインストール

1. メニューの`スタートアップ登録`にチェックが付いていれば、チェックを外す
2. メニューの`Exit`で終了する
3. Scoopでインストールした場合は`scoop uninstall ymonitory`を実行する。Releasesからダウンロードした場合は`yMonitory.exe`と`config.ini`を削除する

## ビルド

Visual StudioのC++のビルドツールが必要。x64用の開発者コマンドプロンプトで、リポジトリのフォルダに移動して次を実行する。ビルドツールの`VsDevCmd.bat -arch=x64`を実行すると、開いているコマンドプロンプトでx64用のビルドツールを使える。出力先は`build\yMonitory.exe`。

```bat
mkdir build
rc /fo build\yMonitory.res src\yMonitory.rc
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fobuild\ /Febuild\yMonitory.exe src\main.cpp src\Config.cpp src\SystemMonitor.cpp src\OverlayWindow.cpp build\yMonitory.res /link user32.lib gdi32.lib shell32.lib pdh.lib d2d1.lib dwrite.lib advapi32.lib /SUBSYSTEM:WINDOWS
```

## ライセンス

[MIT License](LICENSE)
