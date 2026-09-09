# Tsugaru_QT — FM TOWNS / Marty エミュレータ ”津軽” (Qt)

**版 Tsugaru20260522-qt 1.1.0**

CaptainYS 作 FM TOWNS / Marty エミュレータ [Tsugaru](https://github.com/captainys/TOWNSEMU) の **Qt 6** フロントエンドです。

### English

**Tsugaru_QT — FM TOWNS / Marty Emulator ”津軽” (Qt frontend)**

**Version Tsugaru20260522-qt 1.1.0**

A **Qt 6** frontend for CaptainYS's FM TOWNS / Marty emulator [Tsugaru](https://github.com/captainys/TOWNSEMU).

## AI の利用について

本プロジェクトでは AI 支援 IDE [Cursor](https://cursor.com/) を利用し、生成されたコードやデザインパターンを必要に応じて取り入れています。採用した生成物は、いずれも制作者がレビュー・修正・統合しています。

紹介プログラム経由の登録用リンク（**紹介リンク**）: [cursor.com/referral?code=TI3UQLE9PFH3](https://cursor.com/referral?code=TI3UQLE9PFH3)  
このリンクから登録すると Cursor 側の紹介特典が適用される場合がありますが、Tsugaru_QT の開発・配布とは無関係です。

### English

This project uses the AI-assisted IDE [Cursor](https://cursor.com/). Generated code and design patterns are incorporated where helpful; the maintainer reviews, revises, and integrates all adopted material.

Referral registration link: [cursor.com/referral?code=TI3UQLE9PFH3](https://cursor.com/referral?code=TI3UQLE9PFH3). Cursor's referral program may apply at sign-up; this is unrelated to the development or distribution of Tsugaru_QT.

---

## 仕様

`Tsugaru_CUI` からの主な差分は次のとおりです。

* **Qt メニューバー UI** — フロントエンドに Qt 6 を使い、一般的なメニューバー形式の操作画面にしています。ゲームポート機器の切り替え、全画面表示、スプライト転送速度などはメニューから設定できます。
* **タイミング** — エミュレータの進行を実時間に合わせる処理を見直しました。音声のテンポがぶれにくく、画面の同期（VSYNC）も安定します。既定は実時間待ち（CUI の `-YESWAIT` 相当）で、遅れた分を一気に取り戻す動作はしません。
* **ディスクプロファイル** — マウントした CD ごとに設定を保存します。同じディスクをマウントして再起動すると、保存した内容が読み込まれます。フロッピー（FD0 / FD1）や HDD のマウント状況も保存され、次回起動時に再現されます。マウス統合の設定も、このプロファイルに含まれます。CMOS もプロファイルごとに保存されます。
* **ドライブ構成** — ゲストの SETUP を使わず、設定から Towns 上のドライブレター／シングルドライブ（CMOS）を編集できます。
* **HDD イメージ** — 上流 Tsugaru と同様、新規作成はスパース形式です。論理サイズは指定どおりで、未書き込み領域の実ディスク使用量は小さく、書き込みに応じて増えます。TownsOS 向けに 127 MB・1 パーティション＋フォーマット済みイメージを作るオプションもあります。旧形式の密なイメージは HDD 設定の **Compact** で変換できます。
* **マウス統合** — ゲストメモリへ座標を書き込むことで、ポインタ操作の遅延を抑えています。アプリ別統合では DS.base からのオフセットを使え、CMOS 変更など環境が変わっても追従しやすくしています。操作設定は **規定** と **マウスキャプチャ** に簡略化（規定時の優先はアプリ別 Phys → Mouse BIOS (MOS) → キャプチャ）。アプリ別／MOS 統合中でもマウス中ボタンでキャプチャに切り替えられます。オフセット対応のプリセットを同梱しています。
* **CPU コア及びメモリウェイト** — 互換モードと 16 MHz は i386 命令タイミング、それ以外は i486 相当です。i486 用とは別に各命令の実行サイクル数を用意することで、i386 の動作速度をエミュレート（再現）しています。メモリウェイトも RAM／VRAM で再現し、互換モードをより実機に寄せています（下表）。
* **オートレジューム** — ディスクプロファイルが有効なとき、終了時の状態をステートセーブで残し、次回起動時に再現します。
* **CDDA キャッシュ** — CD 音源（CDDA）を先読みキャッシュし、データトラックの読み込み中も演奏を止めない仕組みです。
* **CD イメージ** — Tsugaru が扱える CD-ROM イメージに加え、`.chd` 形式にも対応しています。
* **MIDI** — FluidSynth によるソフトウェア音源出力です。利用には別途パッケージが必要です。現在、SysEx は GS 音源向けのみ処理します。SoundFont は GS 対応のものを推奨します。
* **Wayland idle-inhibit** — 実行中は画面スリープやスクリーンセーバーを抑止します。
* **UI 多言語化 (i18n)** — en / ja / zh-CN / zh-TW / ko / de / fr / es（en と ja はバイナリ埋め込み、他は `share/townsqt/translations/` の JSON。`TOWNSQT_LANG` またはシステムロケールで選択）

| 名称 | CPU設定 | クロック | ウェイト |
|---|---|---|---|
| 互換モード | i386 | 16 MHz | あり（RAM / VRAM） |
| 16 MHz FAST | i386 | 16 MHz | VRAM |
| 他 | i486 | それ以外 | なし |

### English

Main differences from `Tsugaru_CUI`:

* **Qt menu-bar UI** — The frontend uses Qt 6 with a conventional menu-bar layout. Game-port devices, fullscreen, sprite transfer speed, and similar options are available from the menu.
* **Timing** — Real-time pacing was reworked so audio tempo stays steady and emulated VSYNC does not wobble. By default the emulator waits for real time (same idea as CUI `-YESWAIT`) and does not catch up a time deficit in one burst.
* **Disc profiles** — Settings are saved per mounted CD. Mount the same disc and restart to load them. Floppy (FD0 / FD1) and HDD mount state is stored and restored on the next launch. Mouse-integration settings are part of the profile. CMOS is also stored per profile.
* **Drive configuration** — Edit Towns drive letters / single-drive (CMOS) from Settings without running guest SETUP.
* **HDD images** — Same as upstream Tsugaru: new images are created sparse. Logical size matches your choice; on-disk usage starts small and grows on write. Optional TownsOS 127 MB image with one partition + empty format. Use **Compact** in HDD settings to convert older dense images.
* **Mouse integration** — Writing coordinates into guest memory reduces pointer latency. App-specific mode can use a DS.base offset so CMOS and other environment changes are less likely to break mapping. Modes are simplified to **Default** and **Mouse capture** (Default priority: app Phys → Mouse BIOS (MOS) → capture). Middle button switches to capture even during app/MOS integration. Offset-aware presets are bundled.
* **CPU core and memory wait** — Compatible and 16 MHz use i386 instruction timing; other speeds use i486-class timing. A separate per-instruction cycle table (distinct from i486) emulates i386 execution speed. RAM/VRAM waits are also modeled, bringing Compatible mode closer to real hardware (see table).
* **Auto-resume** — When a disc profile is active, saves state on exit and restores it on the next launch.
* **CDDA cache** — CD audio (CDDA) is prefetched so data-track reads do not interrupt playback.
* **CD images** — In addition to the CD-ROM image formats Tsugaru already supports, `.chd` is accepted.
* **MIDI** — Software synthesis via FluidSynth (a separate package is required). Only GS-oriented SysEx is handled at present. A GS SoundFont is recommended.
* **Wayland idle-inhibit** — Suppresses screen sleep / screensaver while running.
* **UI localization (i18n)** — en / ja / zh-CN / zh-TW / ko / de / fr / es (en and ja embedded in the binary; others load JSON from `share/townsqt/translations/`; select via `TOWNSQT_LANG` or system locale)

| Name | CPU | Clock | Wait |
|---|---|---|---|
| Compatible | i386 | 16 MHz | yes (RAM / VRAM) |
| 16 MHz FAST | i386 | 16 MHz | VRAM |
| Other | i486 | otherwise | none |

---

## 変更履歴 / Changelog

### v1.1.0

* **マウス統合更新**
  1. アプリ別マウス統合にオフセットを採用し、CMOS の変更など環境が変わったときにも対応できるようにしました。
  2. プリセットを上記オフセット対応に更新しました。
  3. マウス操作設定を **規定** と **マウスキャプチャ** に簡略化。アプリ別統合・MOS 統合中でも、マウス中ボタンでマウスキャプチャに切り替えられます。
* **ドライブ構成** — ゲストアプリを使わずに TOWNS 上のドライブ構成を設定できます。CMOS はプロファイルごとに保存されます。
* **HD イメージ** — 新規作成を上流 Tsugaru と同じスパースイメージに寄せました。あわせて TownsOS 向け 127 MB 初期化済みイメージ作成のオプションを用意しました。
* **CPU コア及びメモリウェイト** — i486 用とは別に各命令の実行サイクル数を用意し、i386 の動作速度をエミュレート（再現）しました。あわせてメモリウェイトも RAM／VRAM で再現し、互換モードをより実機に寄せています（上表）。
* **オートレジューム** — プロファイルが有効な場合、ステートセーブを利用して終了時の状態を次回起動時に再現します。
* **バグ修正** — CDDA キャッシュ周りの判定・挙動を修正、ほか。

### English

* **Mouse integration** — App-specific offset so CMOS and other environment changes are handled more gracefully; presets updated for offset; modes simplified to Default / Mouse capture; middle button switches to capture during app/MOS integration.
* **Drive configuration** — Configure the TOWNS drive layout without guest apps; CMOS is stored per disc profile.
* **HD images** — Aligned new image creation with upstream Tsugaru sparse images; optional 127 MB TownsOS-initialized image.
* **CPU core and memory wait** — Emulates i386 speed with a separate per-instruction cycle table (distinct from i486), plus RAM/VRAM waits that bring Compatible mode closer to real hardware (see table above).
* **Auto-resume** — With a disc profile active, restore the previous session via state save on the next launch.
* **Bug fixes** — CDDA cache decision logic and behavior, and other fixes.

---

## Requirements / 必要環境

- Linux (X11 or Wayland), C++17 compiler, CMake ≥ 3.16
- **Qt 6** (`Widgets`, `OpenGLWidgets`)
- ALSA (`alsa-lib`) for audio and MIDI
- OpenGL / GLU, X11

### openSUSE

```bash
sudo zypper install cmake gcc-c++ make git ccache \
  qt6-base-devel qt6-opengl-devel \
  alsa-lib-devel libX11-devel mesa-libGL-devel mesa-libGLU-devel python3
```

### Debian / Ubuntu

```bash
sudo apt-get install cmake g++ make git ccache \
  qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libglu1-mesa-dev \
  libasound2-dev libx11-dev python3
```

### Fedora

```bash
sudo dnf install cmake gcc-c++ make git ccache \
  qt6-qtbase-devel qt6-qtbase-gui \
  alsa-lib-devel libX11-devel mesa-libGL-devel mesa-libGLU-devel python3
```

---

## Build / ビルド

```bash
cd /path/to/TOWNSEMU
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)" --target TownsQt
```

Output binary: `build/townsqt/Tsugaru_QT`

If Qt 6 is not found, the `TownsQt` target is skipped (the rest of the project
still builds). See also [`../../BUILD_LINUX.md`](../../BUILD_LINUX.md).

---

## Install / インストール

```bash
# to ~/.local (no root)
./scripts/install-linux.sh --build --user

# system-wide to /usr (uses sudo)
./scripts/install-linux.sh --system
```

Installed layout (prefix `/usr`):

```
bin/Tsugaru_QT
share/applications/townsqt.desktop
share/icons/hicolor/*/apps/townsqt.png
share/townsqt/translations/townsqt_*.json
```

---

## First run / 初回起動

Tsugaru_QT needs an FM TOWNS ROM set (compatible with the emulator UNZ; a free
ROM set is available from <http://ysflight.com/FM/towns/FreeTOWNS/e.html>).

Place the ROM files in:

```
~/.config/townsqt/roms/
```

Then launch:

```bash
Tsugaru_QT
```

You can also pass a CD/ROM directory and images on the command line — all
`Tsugaru_CUI` options are accepted:

```bash
Tsugaru_QT /path/to/ROM_DIR -CD /path/to/game.cue -FREQ 16 -YESWAIT
```

---

## FluidSynth を使う場合

MIDI 出力は FluidSynth によるソフトウェア音源で再生できます。ビルド時の依存はなく、`libfluidsynth.so.3` を実行時に読み込むため、FluidSynth のランタイムと SoundFont を別途インストールしてください。SoundFont は GS 対応のものを推奨します（SysEx は GS 向けのみ処理します）。

```bash
# openSUSE
sudo zypper install fluidsynth fluid-soundfont-gm
# Debian / Ubuntu
sudo apt-get install libfluidsynth3 fluid-soundfont-gm
# Fedora
sudo dnf install fluidsynth-libs fluid-soundfont-gm
```

設定ダイアログで MIDI 出力に FluidSynth を選択し、必要なら SoundFont を指定してください。SoundFont は次の順で検索されます。

1. 設定ダイアログで指定したパス
2. 環境変数 `TOWNSQT_MIDI_SOUNDFONT`
3. 環境変数 `FLUIDSYNTH_SOUNDFONT`
4. `/usr/share/soundfonts/FluidR3_GM.sf2`、`/usr/share/sounds/sf2/FluidR3_GM.sf2`、`~/.soundfonts/default.sf2` などの一般的な場所

### English

MIDI output can be rendered in software with FluidSynth. There is no build-time dependency: `libfluidsynth.so.3` is loaded at runtime, so install the FluidSynth runtime package and a SoundFont separately. A GS SoundFont is recommended (only GS-oriented SysEx is handled).

```bash
# openSUSE
sudo zypper install fluidsynth fluid-soundfont-gm
# Debian / Ubuntu
sudo apt-get install libfluidsynth3 fluid-soundfont-gm
# Fedora
sudo dnf install fluidsynth-libs fluid-soundfont-gm
```

Select FluidSynth as the MIDI output in Settings, and pick a SoundFont there if needed. The SoundFont is searched in this order:

1. Path set in the Settings dialog
2. `TOWNSQT_MIDI_SOUNDFONT` environment variable
3. `FLUIDSYNTH_SOUNDFONT` environment variable
4. Common locations such as `/usr/share/soundfonts/FluidR3_GM.sf2`, `/usr/share/sounds/sf2/FluidR3_GM.sf2`, `~/.soundfonts/default.sf2`

---

## Configuration files / 設定ファイル

| Path | Purpose |
|------|---------|
| `~/.config/townsqt/townsqt.conf` | UI settings (QSettings INI) |
| `~/.config/townsqt/cmos.bin` | CMOS RAM |
| `~/.config/townsqt/roms/` | ROM images |
| `~/.config/townsqt/hdd/` | SCSI hard-disk images (sparse raw `.hd`; use **Compact** in HDD settings to shrink older dense images) |
| `~/.config/townsqt/blank_fd/` | blank floppy images |

UI language: `TOWNSQT_LANG` (e.g. `ja`, `en`, `de`, `fr`, `es`, `ko`,
`zh_CN`, `zh_TW`) or the system locale. Extra translations directory:
`TOWNSQT_TRANSLATIONS_DIR`.

---

## Troubleshooting / トラブルシュート

- **`GL/glu.h: No such file`** — install the GLU dev package
  (`mesa-libGLU-devel` / `libglu1-mesa-dev`).
- **No audio** — install `alsa-lib-devel` / `libasound2-dev` and rebuild clean.
- **No MIDI sound with FluidSynth** — install the FluidSynth runtime and a
  SoundFont (see “FluidSynth を使う場合”), or set `TOWNSQT_MIDI_SOUNDFONT`.

---

## ライセンス

### 系譜とドキュメント

| 版 | 説明 | ドキュメント |
|---|---|---|
| [captainys/TOWNSEMU](https://github.com/captainys/TOWNSEMU)（Tsugaru） | FM TOWNS / Marty エミュレータ本体 | リポジトリルート [`readme.md`](../../readme.md)、[`LICENSE`](../../LICENSE) |
| 本リポジトリ `src/townsqt/` | 上流 Tsugaru 向け Linux Qt 6 フロントエンド（`Tsugaru_QT`） | `README.md`（本ファイル） |

ファイルによって適用されるライセンスが異なります。

### TownsQt で新規追加したコード

`src/townsqt/` および Linux 向けインストール規則（`src/cmake/TownsQtInstall.cmake`、`scripts/install-linux.sh` 等）、本フロントエンドで新規に追加したファイルは、上流 Tsugaru と同じ **3 条項 BSD ライセンス** です（[`LICENSE`](../../LICENSE) 参照）。

### 上流 Tsugaru から引き継いだコード

`src/towns/`、`src/cpu/`、`src/main_cui/` 等、オリジナル Tsugaru から引き継いだファイルは **CaptainYS（Soji Yamakawa）の 3 条項 BSD ライセンス** に従います（[`LICENSE`](../../LICENSE)、[`readme.md`](../../readme.md)「Source Code」節）。

* 再配布時は著作権表示と免責条項を保持してください。
* 著作権者名による製品の推奨・宣伝に、事前の書面による許可が必要です。
* 本ソフトウェアは「現状のまま」提供され、いかなる保証もありません。

### ROM イメージ

ROM は **所有する実機から吸い出したもの** を使うのが最も正確です。実機をお持ちでない場合は、上流 readme に記載のフリー互換 ROM セット（<http://ysflight.com/FM/towns/FreeTOWNS/e.html>）が利用できます。Marty を再現するには Marty から抜き出した ROM が必要です（[`readme.md`](../../readme.md)「ROMS」「Marty」節）。

`Tsugaru_QT` を使用する場合も、上記 ROM に関する条件は **同等に適用** されます。

### 第三者ライブラリ

* **miniaudio** (`third_party/miniaudio/`) — パブリックドメインまたは MIT No Attribution（miniaudio 同梱ヘッダの表記に従う）
* **Wayland idle-inhibit protocol** (`src/townsqt/third_party/wayland-protocols/`) — MIT License

各ライブラリの利用・再配布は、それぞれのライセンス条件に従ってください。

### English

**Lineage and documentation**

| Version | Description | Documentation |
|---|---|---|
| [captainys/TOWNSEMU](https://github.com/captainys/TOWNSEMU) (Tsugaru) | FM TOWNS / Marty emulator core | Root [`readme.md`](../../readme.md), [`LICENSE`](../../LICENSE) |
| This repo `src/townsqt/` | Linux Qt 6 frontend (`Tsugaru_QT`) | `README.md` (this file) |

Different parts of the tree are covered by different licenses.

**Newly added TownsQt code** — Files under `src/townsqt/` and Linux install rules added for this frontend are under the same **3-clause BSD License** as upstream Tsugaru ([`LICENSE`](../../LICENSE)).

**Inherited Tsugaru code** — Core emulator sources (`src/towns/`, `src/cpu/`, `src/main_cui/`, etc.) remain under **CaptainYS (Soji Yamakawa)'s 3-clause BSD License** ([`LICENSE`](../../LICENSE), [`readme.md`](../../readme.md) “Source Code”).

**ROM images** — ROMs extracted from hardware you own give the best fidelity. A free compatible ROM set is linked from upstream readme; Marty emulation requires Marty ROMs ([`readme.md`](../../readme.md) “ROMS”, “Marty”). The same conditions apply when using `Tsugaru_QT`.

**Third-party libraries** — miniaudio (public domain or MIT No Attribution per its header); Wayland idle-inhibit protocol (MIT). Redistribute each component according to its own license terms.
