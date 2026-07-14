# Linux ビルド手順（Tsugaru / TOWNSEMU 派生）

この文書は `/home/flex/build/TOWNSEMU` での **再現可能ビルド** 用です。
upstream の [captainys/TOWNSEMU](https://github.com/captainys/TOWNSEMU) CI（`.github/workflows/continuous.yml`）に合わせています。

## クイックスタート

```bash
cd /home/flex/build/TOWNSEMU
./scripts/build-linux.sh --check-deps   # 依存確認のみ
./scripts/build-linux.sh --cui          # CUI のみ（推奨・最初の確認）
./scripts/build-linux.sh --townsui      # TownsUI（上部メニューバー + 画面埋め込み）
./scripts/build-linux.sh --all            # CUI + TownsUI + GUI
```

成果物:

| コンポーネント | パス |
|----------------|------|
| CUI | `build/main_cui/Tsugaru_CUI` |
| **TownsUI**（派生・メニューバー付き） | `build/townsui/TownsUI` |
| GUI（upstream） | `gui/build/main_gui/Tsugaru_GUI` |

GUI 起動時は **CUI と GUI を同じディレクトリに置く**（readme 記載どおり）。

```bash
cp build/main_cui/Tsugaru_CUI gui/build/main_gui/
cd gui/build/main_gui
./Tsugaru_GUI
```

## 依存パッケージ

### openSUSE Tumbleweed / Leap

```bash
sudo zypper install \
  cmake gcc-c++ make git ccache \
  alsa-lib-devel python3 \
  libX11-devel mesa-libGL-devel mesa-libGLU-devel
```

| パッケージ | 用途 |
|------------|------|
| `alsa-lib-devel` | 音声（ALSA）。無いと無音ビルドになる |
| `mesa-libGL-devel` `mesa-libGLU-devel` | OpenGL / GLU（`GL/glu.h`）— CUI の画面表示に必要 |
| `libX11-devel` | X11 / GLX |
| `python3` | CMake テスト・一部スクリプト |

### Debian / Ubuntu

```bash
sudo apt-get update
sudo apt-get install \
  cmake g++ make git ccache \
  libasound2-dev python3 \
  libglu1-mesa-dev mesa-common-dev libx11-dev
```

### Fedora

```bash
sudo dnf install \
  cmake gcc-c++ make git ccache \
  alsa-lib-devel python3 \
  libX11-devel mesa-libGL-devel mesa-libGLU-devel
```

## TownsUI（派生 UI）

Tsugaru_CUI と同じ **VM スレッド + UI スレッド** 構成で、画面上部にメニューバーを付けた派生フロントエンドです。
（Tsugaru_GUI のような CUI 子プロセス方式ではなく、コアをプロセス内で動かします。）

```
┌─────────────────────────────────────┐
│ [RUN][PAUSE][POFF][QUIT]  メニュー   │  ← UI スレッド（コマンドキュー）
├─────────────────────────────────────┤
│                                     │
│      FM TOWNS 画面（埋め込み）       │  ← VM スレッド + 描画スレッド
│                                     │
├─────────────────────────────────────┤
│ ステータスバー（CD/FD 等）           │
└─────────────────────────────────────┘
```

ビルド:

```bash
./scripts/build-linux.sh --townsui
```

実行例:

```bash
./build/townsui/TownsUI /path/to/ROM_DIR \
  -CD /path/to/game.cue \
  -FREQ 16 -YESWAIT
```

メニューバー左から: **RUN / PAUSE / POFF（電源オフ）/ QUIT**。

ソース: `src/townsui/`（`menubar_connection.*`, `towns_menu_ui_thread.*`, `main.cpp`）

## 手動ビルド（スクリプトと同等）

### CUI（`Tsugaru_CUI`）

```bash
cd /home/flex/build/TOWNSEMU
mkdir -p build && cd build
cmake ../src -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel "$(nproc)" --target Tsugaru_CUI
```

### GUI（`Tsugaru_GUI`）

GUI には **別リポジトリ `public`** が必要です（CI と同じ）。

```bash
cd /home/flex/build/TOWNSEMU/gui/src
git clone https://github.com/captainys/public.git   # 初回のみ

cd /home/flex/build/TOWNSEMU/gui
mkdir -p build && cd build
cmake ../src -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel "$(nproc)" --target Tsugaru_GUI
```

## 実行例（CUI）

ROM パスは環境に合わせて変更してください。

```bash
./build/main_cui/Tsugaru_CUI /path/to/ROM_DIR \
  -CD /path/to/game.cue \
  -FREQ 16 -YESWAIT
```

## トラブルシュート

### `GL/glu.h: そのようなファイルやディレクトリはありません`

OpenGL / GLU 開発パッケージ不足。上記 `mesa-libGLU-devel`（openSUSE）をインストール。

### `ALSA library not found` / 音が出ない

`alsa-lib-devel` を入れて **クリーンビルド** してください。

```bash
./scripts/build-linux.sh --clean --cui
```

ビルドログに `yssimplesound_linux_alsa.cpp` がコンパイルされていれば ALSA 有効です。
`yssimplesound_nownd.cpp` だけなら無音ビルドです。

### ccache で `Permission denied`

```bash
export CCACHE_DISABLE=1
```

または `~/.ccache` の権限を修正。`scripts/build-linux.sh` はデフォルトで ccache を無効化します。

### `gui/src/public` がない

GUI ビルド前に:

```bash
git clone https://github.com/captainys/public.git gui/src/public
```

## High-Fidelity モード（参考）

Windows 3.1 等の実験用。通常の FM TOWNS ゲーム互換調整では不要です。

```bash
cd srchf   # upstream に srchf がある場合
mkdir -p ../build_hf && cd ../build_hf
cmake ../srchf -DCMAKE_BUILD_TYPE=Release
cmake --build . --target Tsugaru_CUI
```

## クリーンビルド

```bash
./scripts/build-linux.sh --clean
./scripts/build-linux.sh --all
```

または `rm -rf build gui/build` 後に再ビルド。
