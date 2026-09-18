#!/usr/bin/env python3
"""Extract tr()/translate() sources from TownsQt and rebuild en/ja JSON (then sync others)."""

from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT.parent
LOCALES = ("en", "ja", "de", "fr", "es", "ko", "zh_CN", "zh_TW")

FILE_CONTEXT = {
    "main_window": "MainWindow",
    "settings_dialog": "SettingsDialog",
    "drive_config_page": "DriveConfigPage",
    "mouse_coord_profile_page": "MouseCoordProfilePage",
    "mouse_coord_scan_window": "MouseCoordScanWindow",
    "audio_mixer_dialog": "AudioMixerDialog",
    "hdd_settings_dialog": "HddSettingsDialog",
    "cdrom_monitor_window": "CdromMonitorWindow",
    "debug_text_window": "DebugTextWindow",
    "emu_view": "EmuView",
    "emu_gl_view": "EmuGlView",
    "townsqt_gameport_options": "TownsQtGamePortOptions",
    "townsqt_rom_availability": "TownsQtRomAvailability",
    "townsqt_model_profile": "TownsQtModelProfile",
    "townsqt_cpu_profile": "TownsQtCpuProfile",
    "content_browser_widget": "ContentBrowserWidget",
}


def unescape(s: str) -> str:
    return (
        s.replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\'", "'")
        .replace("\\\\", "\\")
    )


def extract_concat_tr(text: str) -> list[str]:
    results: list[str] = []
    for m in re.finditer(r"\btr\s*\(", text):
        i = m.end()
        depth = 1
        start = i
        while i < len(text) and depth:
            c = text[i]
            if c == '"':
                i += 1
                while i < len(text):
                    if text[i] == "\\":
                        i += 2
                        continue
                    if text[i] == '"':
                        i += 1
                        break
                    i += 1
                continue
            if c == "'":
                i += 1
                while i < len(text):
                    if text[i] == "\\":
                        i += 2
                        continue
                    if text[i] == "'":
                        i += 1
                        break
                    i += 1
                continue
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        body = text[start : i - 1]
        parts = re.findall(r'"((?:\\.|[^"\\])*)"', body)
        if not parts:
            continue
        s = unescape("".join(parts))
        if s == "":
            continue
        results.append(s)
    return results


def extract_entries() -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []
    for f in sorted(SRC.rglob("*.cpp")):
        stem = f.stem
        ctx = FILE_CONTEXT.get(stem)
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(
            r'(?:QCoreApplication::)?translate\s*\(\s*"([^"]+)"\s*,\s*"((?:\\.|[^"\\])*)"',
            text,
        ):
            entries.append((m.group(1), unescape(m.group(2))))
        for m in re.finditer(
            r'QT_TRANSLATE_NOOP\s*\(\s*"([^"]+)"\s*,\s*"((?:\\.|[^"\\])*)"',
            text,
        ):
            entries.append((m.group(1), unescape(m.group(2))))
        if ctx is None:
            continue
        for s in extract_concat_tr(text):
            entries.append((ctx, s))
    for f in sorted(SRC.rglob("*.h")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(
            r'(?:QCoreApplication::)?translate\s*\(\s*"([^"]+)"\s*,\s*"((?:\\.|[^"\\])*)"',
            text,
        ):
            entries.append((m.group(1), unescape(m.group(2))))

    seen: set[tuple[str, str]] = set()
    ordered: list[tuple[str, str]] = []
    for e in entries:
        if e in seen:
            continue
        seen.add(e)
        ordered.append(e)
    return ordered


def load_map(path: Path) -> dict[tuple[str, str], str]:
    if not path.exists():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    return {
        (e["context"], e["source"]): e["translation"]
        for e in data.get("translations", [])
    }


def has_cjk(s: str) -> bool:
    return any("\u3040" <= c <= "\u30ff" or "\u4e00" <= c <= "\u9fff" for c in s)


# Explicit Japanese for sources that were English-only or newly added.
JA_FORCE: dict[tuple[str, str], str] = {
    ("MainWindow", "Starting…"): "起動中…",
    ("MainWindow", "Restarting the emulator…"): "エミュレーターを再起動しています…",
    ("MainWindow", "Emulator restarted"): "エミュレーターを再起動しました",
    ("MainWindow", "&Restart"): "再起動(&R)",
    ("MainWindow", "Hard disk drive settings…"): "ハードディスク設定…",
    (
        "MainWindow",
        "Hard disk settings saved. They take effect after restart.",
    ): "ハードディスク設定を保存しました。再起動後に反映されます。",
    (
        "MainWindow",
        "Hard disk settings saved (CMOS D: = SCSI unit 0). Restart to recognize HD0.",
    ): "ハードディスク設定を保存しました（CMOS D: = SCSI ユニット 0）。再起動後に HD0 を認識します。",
    (
        "MainWindow",
        "HD image created, but failed to set CMOS D: = SCSI unit 0.",
    ): "HD イメージは作成しましたが、CMOS の D: = SCSI ユニット 0 設定に失敗しました。",
    ("MainWindow", "Drive configuration"): "ドライブ構成",
    (
        "MainWindow",
        "Failed to update CMOS (file missing or not writable).",
    ): "CMOS を更新できませんでした（ファイルがないか書き込めません）。",
    (
        "MainWindow",
        "Could not apply mouse profile (need a disc profile).",
    ): "マウスプロファイルを適用できませんでした（ディスクプロファイルが必要です）。",
    ("MainWindow", "Failed to load CD image"): "CD イメージの読み込みに失敗しました",
    (
        "MainWindow",
        "App-specific Phys integration is active. Middle button switches to mouse capture; middle again restores.",
    ): "アプリ別 Phys 統合が有効です。中ボタンでマウスキャプチャへ切替、もう一度で戻ります。",
    (
        "MainWindow",
        "Mouse BIOS (MOS) integration is active. Middle button switches to mouse capture; middle again restores.",
    ): "Mouse BIOS (MOS) 統合が有効です。中ボタンでマウスキャプチャへ切替、もう一度で戻ります。",
    (
        "HddSettingsDialog",
        "Changes take effect after the emulator is restarted.",
    ): "変更はエミュレーター再起動後に反映されます。",
    (
        "HddSettingsDialog",
        "TownsOS: create with 1 partition + format (127 MB, max size)",
    ): "TownsOS: 1パーティション＋フォーマットで作成（127 MB・最大サイズ）",
    (
        "HddSettingsDialog",
        "Writes Towns HD IPL, one full-size partition, and an empty FAT16.\n"
        "For HD0, CMOS is set to D: = SCSI unit 0 after OK.\n"
        "Restart the emulator to recognize the drive.",
    ): "Towns HD IPL・最大サイズの1パーティション・空の FAT16 を書き込みます。\n"
    "HD0 の場合、OK 後に CMOS で D: = SCSI ユニット 0 を設定します。\n"
    "ドライブ認識にはエミュレーターの再起動が必要です。",
    (
        "HddSettingsDialog",
        "Create a 127 MB TownsOS image with one full-size partition and an empty format?\n"
        "For HD0, D: will be set to SCSI unit 0 in CMOS when you press OK.",
    ): "127 MB の TownsOS イメージを、最大サイズの1パーティションと空フォーマットで作成しますか？\n"
    "HD0 の場合、OK 時に CMOS で D: = SCSI ユニット 0 を設定します。",
    (
        "HddSettingsDialog",
        "Created a TownsOS-formatted sparse hard disk image (1 partition).\n\n",
    ): "TownsOS 形式のスパース HD イメージを作成しました（1パーティション）。\n\n",
    (
        "HddSettingsDialog",
        "Created a sparse hard disk image.\n\n",
    ): "スパースのハードディスクイメージを作成しました。\n\n",
    (
        "HddSettingsDialog",
        "Logical size (for FM TOWNS): %1\n"
        "Disk usage on this computer: %2\n\n"
        "File managers list logical size. Use the tooltip on the path field or `du -h` to check disk usage.",
    ): "論理サイズ（FM TOWNS 向け）: %1\n"
    "実ディスク使用量: %2\n\n"
    "ファイルマネージャは論理サイズを表示します。パス欄のツールチップまたは `du -h` で実使用量を確認できます。",
    (
        "HddSettingsDialog",
        "\n\nAfter OK: CMOS D: = SCSI unit 0 (HD0). Restart to recognize the drive.",
    ): "\n\nOK 後: CMOS D: = SCSI ユニット 0（HD0）。再起動後にドライブを認識します。",
    ("MainWindow", "FPS display"): "FPS表示",
    (
        "MainWindow",
        "Show FPS, emulation Hz, and present-queue stats in the window title.",
    ): "ウィンドウタイトルに FPS・エミュレーション Hz・表示キュー統計を表示します。",
    ("MainWindow", "Profile enabled"): "プロファイル有効",
    (
        "MainWindow",
        "A disc profile is loaded and “Use disc profiles” is on.",
    ): "ディスクプロファイルが読み込まれ、「ディスクプロファイルを使用」がオンです。",
    (
        "MainWindow",
        " — %1 FPS | %2 Hz | P:%3 C:%4 lag:%5",
    ): " — %1 FPS | %2 Hz | P:%3 C:%4 lag:%5",
    ("MainWindow", "(new — will save for current CD)"): "（新規 — 現在のCD用に保存）",
    ("MainWindow", "Mouse Integration"): "マウス統合",
    (
        "MainWindow",
        "Mouse integration: the host pointer controls the Towns pointer directly.",
    ): "マウス統合：ホストのポインタが Towns のポインタを直接動かします。",
    ("MainWindow", "Mouse Integration (Profile)"): "マウス統合（プロファイル）",
    (
        "MainWindow",
        "Mouse profile: memory-write mouse integration.",
    ): "マウスプロファイル：メモリ書き込みによるマウス統合です。",
    ("MainWindow", "Mouse Capture"): "マウスキャプチャ",
    ("MainWindow", "Middle button to release"): "中ボタンで解除",
    (
        "MainWindow",
        "Mouse capture is active. "
        "Press the middle mouse button to release capture.",
    ): "マウスキャプチャ中です。マウス中ボタンで解除します。",
    ("MainWindow", "Mouse Capture (released)"): "マウスキャプチャ（解除）",
    ("MainWindow", "Click to capture"): "クリックでキャプチャ",
    (
        "MainWindow",
        "Mouse capture released. Click the screen to start capture; "
        "press the middle mouse button to release it.",
    ): "マウスキャプチャ解除中。画面をクリックで開始、中ボタンで解除します。",
    ("MainWindow", "Mouse capture"): "マウスキャプチャ",
    ("MainWindow", "Mouse capture (released)"): "マウスキャプチャ（解除）",
    ("MainWindow", "Mouse integration (Mouse BIOS)"): "マウス統合(マウスBIOS)",
    ("MainWindow", "Mouse integration (app-specific)"): "マウス統合(アプリ別設定)",
    (
        "MainWindow",
        "Mouse integration (Mouse BIOS): the host pointer controls the Towns "
        "pointer via Mouse BIOS.",
    ): "マウス統合(マウスBIOS)：Mouse BIOS 経由でホストのポインタが Towns のポインタを動かします。",
    (
        "MainWindow",
        "Mouse integration (app-specific): Game Phys drives host→guest mapping "
        "(memory write or gameport).",
    ): "マウス統合(アプリ別設定)：Game Phys によるホスト→ゲスト対応（メモリ書き込みまたはゲームポート）。",
    ("MouseCoordProfilePage", "Default"): "規定",
    (
        "MouseCoordProfilePage",
        "Default: no MOS → mouse capture; MOS present → Mouse BIOS absolute. "
        "If MOS is present but unused → fall back to mouse capture.",
    ): "規定: MOS無し→マウスキャプチャ、MOS有り→マウスBIOS絶対。"
    "MOS有りでも未使用判定ならマウスキャプチャへ落とす。",
    ("MouseCoordProfilePage", "Mouse capture"): "マウスキャプチャ",
    ("MouseCoordProfilePage", "Mouse integration (Mouse BIOS)"): "マウス統合(マウスBIOS)",
    ("MouseCoordProfilePage", "Mouse integration (app-specific)"): "マウス統合(アプリ別設定)",
    ("MouseCoordProfilePage", "Mouse integration (app-specific settings)"): "マウス統合(アプリ別設定)",
    ("MouseCoordProfilePage", "Clear"): "クリア",
    ("MouseCoordProfilePage", "Load preset"): "プリセット読込み",
    ("MouseCoordProfilePage", "Save to file"): "ファイルへ保存",
    (
        "MouseCoordProfilePage",
        "Load a mouse-integration preset for this CD fingerprint.\n"
        "Enabled when mouse_XXXXXXXX.ini is found. Apply or OK saves it to the disc profile.",
    ): "このCDフィンガープリント向けのマウス統合プリセットを読み込みます。\n"
    "mouse_XXXXXXXX.ini があるとき有効です。適用またはOKでディスクプロファイルへ保存します。",
    (
        "MouseCoordProfilePage",
        "Write the current app-specific mouse settings to\n"
        "~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini.\n"
        "Enabled when app-specific Phys is set.",
    ): "現在のアプリ別マウス設定を\n"
    "~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini へ書き出します。\n"
    "アプリ別の Phys が設定されているとき有効です。",
    (
        "MouseCoordProfilePage",
        "Could not write mouse preset:\n%1",
    ): "マウスプリセットを書き込めませんでした：\n%1",
    (
        "MouseCoordProfilePage",
        "Clear Game Phys / Phys 2 / offset / scale / invert / Bind. "
        "You can Apply or OK with Phys unset.",
    ): "Game Phys／Phys 2／オフセット／スケール／反転／紐付けを消します。"
    "Phys 未設定のまま適用／OK で保存できます。",
    ("MouseCoordProfilePage", "Bind"): "紐付け",
    ("SettingsDialog", "Window scale:"): "ウインドウ倍率：",
    ("SettingsDialog", "Sprite transfer speed:"): "スプライト転送速度：",
    ("MouseCoordProfilePage", "Memory write"): "メモリ書込み",
    (
        "MouseCoordProfilePage",
        "On: poke Game Phys in guest RAM (min/max clamp + scale on store).\n"
        "Off: gameport Δ = (host × scale) − guest (1:1 raw coords; min/max unused).",
    ): "オン: Game Phys をゲスト RAM へ書き込み（min/max クランプ＋スケール）。\n"
    "オフ: ゲームポートΔ = (ホスト × scale) − ゲスト（1:1 生座標、min/max 未使用）。",
    ("MouseCoordProfilePage", "Stop writing to Mouse BIOS soft-cursor phys"):
    "マウスBIOSのソフトカーソルphysへの書込みを停止",
    ("MouseCoordProfilePage", "Wait for gameport input to apply"): "ゲームポート入力の反映を待つ",
    (
        "MouseCoordProfilePage",
        "When on, do not write Mouse BIOS soft coordinates while MOS is alive. "
        "Game Phys / gameport still follow app-specific settings.",
    ): "オン時は MOS 生存中でも マウスBIOS soft 座標へ書き込みません。"
    "Game Phys／ゲームポートはアプリ別設定どおり動作します。",
    (
        "MouseCoordProfilePage",
        "When on, hold the next gameport packet until Phys updates or the port is idle. "
        "Turn off for continuous refill (may oscillate on delayed guest feedback). "
        "Only used for gameport mode (memory write off).",
    ): "オン時は Phys 更新またはポート消費まで次のゲームポートパケットを保留します。"
    "オフにすると連続投入します（ゲスト反映が遅いと振動することがあります）。"
    "ゲームポート動作時のみ有効（メモリ書き込みオン時は無効）。",
    (
        "MouseCoordProfilePage",
        "Mouse capture: always mouse capture "
        "(starts released; click to capture). "
        "Exclusive with Mouse BIOS absolute and app-specific apply.",
    ): "マウスキャプチャ: 常にマウスキャプチャ（解除状態で開始、クリックでキャプチャ）。"
    "マウスBIOS絶対およびアプリ別適用とは排他。",
    (
        "MouseCoordProfilePage",
        "Mouse integration (Mouse BIOS): no MOS → mouse capture; "
        "MOS present → Mouse BIOS absolute. "
        "MOS unused detection is not used. "
        "Game Phys and offset/invert are not used.",
    ): "マウス統合(マウスBIOS): MOS無し→マウスキャプチャ、MOS有り→マウスBIOS絶対。"
    "MOS未使用判定は行わない。Game Phys・オフセット・反転は使いません。",
    (
        "MouseCoordProfilePage",
        "Mouse integration (Mouse BIOS): unavailable while Mouse BIOS soft cursor "
        "is not resolvable.",
    ): "マウス統合(マウスBIOS): Mouse BIOS soft カーソルが解決できない間は使えません。",
    (
        "MouseCoordProfilePage",
        "Mouse integration (app-specific, game port): "
        "no MOS → mouse capture; MOS present → Mouse BIOS absolute. "
        "Bound EXE/EXP start → Game Phys Δ (per settings, including wait-feedback). "
        "While MOS lives, MOS soft is written immediately unless "
        "“Stop soft write” is on. "
        "Enable “Memory write” to poke RAM instead.",
    ): "マウス統合(アプリ別・ゲームポート): MOS無し→マウスキャプチャ、MOS有り→マウスBIOS絶対。"
    "BindしたEXE/EXP起動→Game Phys Δ（設定どおり・反映待ち含む）。"
    "MOS生存中のMOS側は「soft書き込み停止」オフ時のみ即時soft書き込み。"
    "「メモリ書き込み」でRAM書き込みに切替。",
    (
        "MouseCoordProfilePage",
        "Mouse integration (app-specific, memory write): "
        "no MOS → mouse capture; MOS present → Mouse BIOS absolute. "
        "Bound EXE/EXP start → Game Phys poke (per settings). "
        "While MOS lives, MOS soft is written immediately unless "
        "“Stop soft write” is on; Phys still follows settings. "
        "Under DOS extenders (RUN386), Bind the .EXP payload when shown.",
    ): "マウス統合(アプリ別・メモリ書き込み): MOS無し→マウスキャプチャ、MOS有り→マウスBIOS絶対。"
    "BindしたEXE/EXP起動→Game Phys書き込み（設定どおり）。"
    "MOS生存中のMOS側は「soft書き込み停止」オフ時のみ即時soft書き込み（Physは設定どおり）。"
    "DOSエクステンダ(RUN386)時は表示中の.EXPをBind。",
    (
        "MainWindow",
        "Mouse capture ON — list refresh / prune (profile paused).",
    ): "マウスキャプチャON — 一覧の更新／整理（プロファイル一時停止）",
    (
        "MainWindow",
        "Mouse capture OFF — profile mouse mode restored.",
    ): "マウスキャプチャOFF — プロファイルのマウスモードを復元",
    (
        "MainWindow",
        "Scan on — adding candidates (mouse capture ON). ESC to stop.",
    ): "スキャンON — 候補を追加中（マウスキャプチャON）。ESCで停止",
    (
        "MainWindow",
        "Scan off — capture may stay on for prune.",
    ): "スキャンOFF — 整理のためキャプチャはONのままのことがあります",
    (
        "MainWindow",
        "Stopped (ESC) — profile mouse mode restored.",
    ): "停止（ESC）— プロファイルのマウスモードを復元",
    ("MainWindow", "Applied and saved mouse profile."): "マウスプロファイルを適用・保存しました。",
    (
        "MainWindow",
        "Could not apply mouse profile (check phys addresses).",
    ): "マウスプロファイルを適用できませんでした（phys アドレスを確認）。",
    (
        "MainWindow",
        "Check one X and one Y candidate, then Update.",
    ): "X と Y の候補を1つずつ選んでから Update してください。",
    (
        "MainWindow",
        "Updated Mouse integration setting from selected X/Y.",
    ): "選択した X/Y をマウス統合設定に反映しました。",
    (
        "MainWindow",
        "Create a disc profile in Settings → Basics first.",
    ): "先に設定 → 基本構成でディスクプロファイルを作成してください。",
    ("MainWindow", "Could not create disc profile."): "ディスクプロファイルを作成できませんでした。",
    ("MainWindow", "Could not delete disc profile."): "ディスクプロファイルを削除できませんでした。",
    ("MainWindow", "Disc profile deleted."): "ディスクプロファイルを削除しました。",
    ("MainWindow", "Disc profile clock updated."): "ディスクプロファイルのクロックを更新しました。",
    (
        "MainWindow",
        "Pause the emulator and browse registered disc profiles / state saves.\n"
        "Close resumes the current session; choosing an entry ends it and boots that title.\n"
        "Shortcut: Alt+Z (toggle).",
    ): "エミュレータを一時停止し、登録済みディスクプロファイル／ステートを閲覧します。\n"
    "閉じると現在のセッションを再開、エントリ選択で終了してそのタイトルを起動します。\n"
    "ショートカット: Alt+Z（開閉）。",
    
    ("MainWindow", "Audio mixer…"): "オーディオミキサー…",
    ("MainWindow", "Mouse integration debug"): "マウス統合デバッグ",
    ("SettingsDialog", "Mouse integration"): "マウス統合",
    ("SettingsDialog", "Display / Audio"): "表示 / オーディオ",
    ("SettingsDialog", "Create profile"): "プロファイルを作成",
    ("SettingsDialog", "Delete profile"): "プロファイルを削除",
    ("SettingsDialog", "Delete disc profile"): "ディスクプロファイルの削除",
    (
        "SettingsDialog",
        "Delete the disc profile \"%1\"?\n"
        "Per-disc machine and mouse settings will be removed.\n"
        "This cannot be undone.",
    ): "ディスクプロファイル「%1」を削除しますか？\n"
    "ディスクごとのマシン／マウス設定が失われます。\n"
    "この操作は取り消せません。",
    (
        "SettingsDialog",
        "Per-disc mouse integration saved with the disc profile (fp_XXXXXXXX.ini).\n"
        "Create a profile on the Basics tab first. Apply or OK writes changes immediately\n"
        "(no emulator restart). Use Scan… to open Memory scan (Settings closes until Cancel or Update).",
    ): "ディスクごとのマウス統合。ディスクプロファイル（fp_XXXXXXXX.ini）に保存されます。\n"
    "先に基本構成タブでプロファイルを作成してください。適用または OK で即時反映します"
    "（エミュレータ再起動なし）。\n"
    "「スキャン…」でメモリスキャンを開きます（キャンセルまたは Update まで設定は閉じます）。",
    (
        "SettingsDialog",
        "Open Memory scan to find guest RAM cursor coordinates.\n"
        "Settings closes while the scan window is open.",
    ): "ゲスト RAM のカーソル座標を探すメモリスキャンを開きます。\n"
    "スキャンウインドウ表示中は設定を閉じます。",
    ("SettingsDialog", "Scan…"): "スキャン…",
    (
        "SettingsDialog",
        "Mount a CD image to edit mouse integration.",
    ): "マウス統合を編集するには CD イメージをマウントしてください。",
    (
        "SettingsDialog",
        "Create a disc profile on the Basics tab first.",
    ): "先に基本構成タブでディスクプロファイルを作成してください。",
    (
        "SettingsDialog",
        "No CD mounted. Disc profiles apply only while a CD image is loaded.",
    ): "CD がマウントされていません。ディスクプロファイルは CD イメージ読込中のみ有効です。",
    (
        "SettingsDialog",
        "No profile for this disc. Create one to save per-disc settings from Basics.",
    ): "このディスクのプロファイルがありません。基本構成の内容から作成してください。",
    (
        "SettingsDialog",
        "Editing disc profile (selection highlight). Clock, memory, ports and options save to the profile;\n"
        "CPU and model stay global.",
    ): "ディスクプロファイル編集中（選択色の枠内）。クロック・メモリ・ポート・オプションはプロファイルへ保存し、\n"
    "CPU とモデルはグローバルのままです。",
    (
        "SettingsDialog",
        "Editing the disc profile (fp_XXXXXXXX.ini). Apply or OK saves profile fields here\n"
        "(clock, memory, ports, options). CPU and model stay global (townsqt.conf) and are\n"
        "not stored in the profile. Memory / fidelity changes restart the emulator.",
    ): "ディスクプロファイル (fp_XXXXXXXX.ini) を編集中です。適用または OK でここに表示している項目\n"
    "（クロック、メモリ、ポート、オプション）をプロファイルへ保存します。CPU とモデルは\n"
    "グローバル (townsqt.conf) のままプロファイルには保存しません。メモリ／忠実度の変更は再起動します。",
    ("SettingsDialog", "Open content browser on startup"): "起動時にコンテンツブラウザを開く",
    (
        "SettingsDialog",
        "When off, cold-start with global settings (HD configuration is kept;\n"
        "CD / FD are not remounted). When on, open the content browser on startup.",
    ): "オフのときはグローバル設定でコールドスタートします（HD構成は保持、CD / FDは再マウントしません）。\n"
    "オンのときは起動時にコンテンツブラウザを開きます。",
    ("ContentBrowserWidget", "Last autosave"): "最後の自動保存",
    ("ContentBrowserWidget", "Double-click to resume"): "ダブルクリックで再開",
    (
        "ContentBrowserWidget",
        "AUTOSAVE · %1 — double-click to resume",
    ): "AUTOSAVE · %1 — ダブルクリックで再開",
    ("SettingsDialog", "Enable auto-resume"): "オートレジューム有効",
    (
        "SettingsDialog",
        "With a disc profile, save/restore on eject and exit.\n"
        "Same-CD manual restart skips resume.",
    ): "ディスクプロファイルがあるとき、取り出し・終了で自動保存し次回再開。\n"
    "同じ CD の手動再起動ではスキップ。",
    ("SettingsDialog", "Compress state data"): "ステートデータ圧縮",
    (
        "SettingsDialog",
        "Compress new .TState saves (zlib).\n"
        "Load still accepts uncompressed files.",
    ): "新規 .TState を zlib 圧縮。\n"
    "読み込みは非圧縮も可。",
    ("SettingsDialog", "Boot drive"): "起動ディスク",
    ("SettingsDialog", "Drive configuration"): "ドライブ構成",
    (
        "DriveConfigPage",
        "Single-drive mode (disable FD1)",
    ): "シングルドライブモード（FD1 を無効）",
    (
        "DriveConfigPage",
        "Towns CMOS flag at I/O 0x328C. When on, FD1 is unavailable.",
    ): "Towns CMOS の I/O 0x328C フラグ。オンだと FD1 は使えません。",
    (
        "DriveConfigPage",
        "Drive letter assignments (A–P). Apply or OK writes VM CMOS RAM and the "
        "active CMOS file. Towns OS usually picks them up after reset/boot.",
    ): "ドライブレター割り当て（A–P）。適用または OK で VM の CMOS RAM と有効な CMOS ファイルに書き込みます。"
    "Towns OS では通常リセット／再起動後に反映されます。",
    ("DriveConfigPage", "Type"): "種類",
    ("DriveConfigPage", "Unit"): "ユニット",
    ("DriveConfigPage", "Unassigned"): "未割り当て",
    ("DriveConfigPage", "FD"): "FD",
    ("DriveConfigPage", "SCSI"): "SCSI",
    ("DriveConfigPage", "ROM"): "ROM",
    (
        "SettingsDialog",
        "Editing Basics defaults (townsqt.conf). Apply or OK saves here.\n"
        "Memory, boot drive, and CPU fidelity changes restart the emulator; game-port changes apply immediately.\n"
        "Create a disc profile when a CD is mounted to save per-disc settings\n"
        "(FD0 / FD1 / HD0–HD6 mounts, and CMOS under cmos/cmos_XXXXXXXX.bin).",
    ): "基本構成の既定値 (townsqt.conf) を編集中です。適用または OK でここに保存します。\n"
    "メモリ・起動ディスク・CPU 忠実度の変更はエミュレーターを再起動し、ゲームポートの変更は即時反映されます。\n"
    "CD マウント中にディスクプロファイルを作成すると、ディスクごとの設定を保存できます\n"
    "（FD0 / FD1 / HD0–HD6 のマウントと cmos/cmos_XXXXXXXX.bin の CMOS）。",
    (
        "SettingsDialog",
        "Editing the disc profile (fp_XXXXXXXX.ini, selection highlight).\n"
        "Apply or OK saves clock, boot drive, memory, ports, options,\n"
        "and FD0 / FD1 / HD0–HD6 mount state to the profile (restored on next load).\n"
        "CMOS (drive letters, single drive) uses cmos/cmos_XXXXXXXX.bin for this disc — set in Towns SETUP.\n"
        "CPU and model stay global in townsqt.conf and are not stored in the profile.\n"
        "Memory, boot drive, and CPU fidelity changes restart the emulator.",
    ): "ディスクプロファイル (fp_XXXXXXXX.ini、選択色の枠) を編集中です。\n"
    "適用または OK でクロック・起動ディスク・メモリ・ポート・オプション、\n"
    "および FD0 / FD1 / HD0–HD6 のマウント状態をプロファイルへ保存します（次回読込時に復元）。\n"
    "CMOS（ドライブレター・シングルドライブ）はこのディスク用の cmos/cmos_XXXXXXXX.bin を使います — Towns SETUP で設定。\n"
    "CPU とモデルは townsqt.conf のグローバル設定のままで、プロファイルには保存しません。\n"
    "メモリ・起動ディスク・CPU 忠実度の変更はエミュレーターを再起動します。",
    (
        "SettingsDialog",
        "No disc profile for this CD yet. Use Create profile to save per-disc settings\n"
        "(including FD0, FD1, and HD0–HD6 mount state for restore).\n"
        "A profile also gets its own CMOS file (cmos/cmos_XXXXXXXX.bin) for Towns SETUP.\n"
        "Until then, Apply or OK saves Basics defaults to townsqt.conf.\n"
        "Memory, boot drive, and CPU fidelity changes restart the emulator; game-port changes apply immediately.",
    ): "この CD のディスクプロファイルはまだありません。「プロファイルを作成」でディスクごとの設定を保存できます\n"
    "（復元用に FD0 / FD1 / HD0–HD6 のマウント状態も含みます）。\n"
    "プロファイルには Towns SETUP 用の CMOS ファイル (cmos/cmos_XXXXXXXX.bin) も付きます。\n"
    "作成するまでは、適用または OK で基本構成の既定を townsqt.conf へ保存します。\n"
    "メモリ・起動ディスク・CPU 忠実度の変更は再起動し、ゲームポートの変更は即時反映されます。",
    (
        "SettingsDialog",
        "Edits the active CMOS (global cmos.bin or profile cmos/cmos_XXXXXXXX.bin).\n"
        "Apply or OK updates VM CMOS RAM and the file. Towns OS usually needs reset/boot.",
    ): "有効な CMOS（グローバル cmos.bin またはプロファイルの cmos/cmos_XXXXXXXX.bin）を編集します。\n"
    "適用または OK で VM の CMOS RAM とファイルを更新します。Towns OS では通常リセット／再起動が必要です。",
    (
        "SettingsDialog",
        "Mouse integration is configured for this disc profile.",
    ): "このディスクプロファイルにはマウス統合が設定されています。",
    (
        "SettingsDialog",
        "Choose an operation type and Game Phys as needed, then Apply or OK.",
    ): "必要に応じて操作の種類と Game Phys を選び、適用または OK してください。",
    ("MouseCoordProfilePage", "Mouse operation type"): "マウス操作の種類",
    ("MouseCoordProfilePage", "Soft X"): "Soft X",
    ("MouseCoordProfilePage", "Axis"): "軸",
    ("MouseCoordProfilePage", "Phys"): "Phys",
    ("MouseCoordProfilePage", "Min"): "Min",
    ("MouseCoordProfilePage", "Offset"): "オフセット",
    (
        "MouseCoordProfilePage",
        "Offset from live DS.base (WC-style). Prefer this over absolute Phys when guest layout shifts with memory/CMOS/HDD changes.",
    ): "ライブの DS.base からのオフセット（WC 方式）。メモリ／CMOS／HDD 変更でゲスト配置がずれるときは、絶対 Phys よりこちらを推奨。",
    ("MouseCoordProfilePage", "Scale"): "Scale",
    ("MouseCoordProfilePage", "Invert"): "反転",
    ("MouseCoordProfilePage", "Invert X"): "X を反転",
    ("MouseCoordProfilePage", "Invert Y"): "Y を反転",
    ("MouseCoordProfilePage", "Scale X"): "Scale X",
    ("SettingsDialog", "PCM LPF"): "PCM LPF",
    (
        "SettingsDialog",
        "Prefetch only the CDDA audio needed ahead of playback and keep
"
        "playing through data reads without interrupting. The value is how
"
        "many seconds until playback is considered finished.",
    ): "CDDA を必要な分だけ先読みし、データ読み出し中も再生を中断せず続けます。
"
       "数値は、再生終了とみなすまでの秒数です。",
    (
        "SettingsDialog",
        "Even while Mouse BIOS is running, if it is unused, end mouse integration\n"
        "and switch to mouse capture.",
    ): "Mouse BIOS 動作中でも未使用ならマウス統合を終了し、マウスキャプチャへ切り替えます。",
    (
        "SettingsDialog",
        "Directly rewrite guest coordinates to reduce input lag.",
    ): "ゲスト座標を直接書き換えて入力遅延を減らします。",
    ("SettingsDialog", "Faster mouse integration"): "高速マウス統合",
    (
        "SettingsDialog",
        "Mouse BIOS integration that writes guest memory to reduce latency.",
    ): "マウスBIOSを使った統合で、メモリ書込みを行い遅延を低減します。",
    (
        "SettingsDialog",
        "Absolute mouse edge assist (exit vector)",
    ): "絶対マウス縁アシスト（Exit vector）",
    (
        "SettingsDialog",
        "MOS / app-specific only. When the host leaves the picture, extend the last\n"
        "in-picture motion to the rim; the free axis still follows while the pointer\n"
        "stays over the window.",
    ): "MOS / アプリ固有のみ。ホストが絵から外れたとき、最後の絵内移動を縁まで延長。\n"
    "ポインタが窓上にある間は自由軸が追従します。",
    ("SettingsDialog", "CDDA cache:"): "CDDAキャッシュ:",
    ("SettingsDialog", "MOS unused detection (test)"): "MOS未使用検出（試験）",
    ("SettingsDialog", "High-res / high-res PCM"): "高解像度 / 高解像度PCM",
    ("SettingsDialog", "UG SCSI / peripheral I/O"): "UG SCSI / 周辺I/O",
    ("SettingsDialog", "Disabled"): "無効",
    ("SettingsDialog", "Enabled"): "有効",
    ("SettingsDialog", "Pretend 386DX"): "386DX として扱う",
    ("SettingsDialog", "Fast SCSI"): "高速SCSI",
    ("SettingsDialog", "Fast FD"): "高速FD",
    ("SettingsDialog", "MIDI output"): "MIDI出力",
    ("SettingsDialog", "SoundFont"): "SoundFont",
    ("SettingsDialog", "port"): "ポート",
    ("SettingsDialog", "FluidSynth"): "FluidSynth",
    ("SettingsDialog", "ALSA"): "ALSA",
    ("SettingsDialog", "(select)"): "（選択）",
    ("AudioMixerDialog", "Audio mixer"): "オーディオミキサー",
    ("AudioMixerDialog", "Close"): "閉じる",
    ("CdromMonitorWindow", "CD-ROM monitor"): "CD-ROMモニター",
    ("DebugTextWindow", "Clear"): "クリア",
    ("DebugTextWindow", "Close"): "閉じる",
    ("MouseCoordScanWindow", "Memory scan"): "メモリスキャン",
    ("MouseCoordScanWindow", "Scan"): "スキャン",
    ("MouseCoordScanWindow", "Mouse capture ON"): "マウスキャプチャON",
    ("MouseCoordScanWindow", "Clear min max"): "min/maxクリア",
    ("MouseCoordScanWindow", "Clear candidates"): "候補クリア",
    ("MouseCoordScanWindow", "Remove unselected"): "未選択を削除",
    ("MouseCoordScanWindow", "Update"): "Update",
    ("MouseCoordScanWindow", "Watch"): "監視",
    ("MouseCoordScanWindow", "Chase"): "追跡",
    (
        "MouseCoordScanWindow",
        "Delete rows with none of Watch / Chase / X / Y / X2 / Y2 checked.",
    ): "監視 / 追跡 / X / Y / X2 / Y2 のいずれも付いていない行を削除します。",
    (
        "MouseCoordScanWindow",
        "Scan RAM for new coordinate candidates (also turns Mouse capture ON).\n"
        "Move the mouse in the emu view.  ESC stops Scan and capture.",
    ): "RAMを走査して座標候補を追加します（マウスキャプチャもON）。\n"
    "エミュ画面上でマウスを動かしてください。ESCでスキャンとキャプチャを停止します。",
    (
        "MouseCoordScanWindow",
        "Alone: refresh list values and drop unrelated candidates (no new Scan picks).\n"
        "Forced while Scan is on.  Uncheck or ESC restores the profile mouse mode.",
    ): "単独：一覧の値を更新し、無関係な候補を落とします（新規スキャンなし）。\n"
    "スキャン中は強制ON。チェック解除またはESCでプロファイルのマウスモードに戻ります。",
    (
        "MouseCoordScanWindow",
        "Reset observed min..max on all candidates.\n"
        "Starts again from the next sampled value.",
    ): "全候補の観測 min..max をリセットします。\n次のサンプルから測り直します。",
    (
        "MouseCoordScanWindow",
        "Copy the checked X/Y pair (and min..max) into Mouse integration.\n"
        "If X2/Y2 are also checked, fill Phys 2 as well.\n"
        "Closes Memory scan and reopens Settings. Does not save — use Apply or OK there.",
    ): "チェックした X/Y 組（と min..max）をマウス統合へコピーします。\n"
    "X2/Y2 もチェックしていれば Phys 2 も埋めます。\n"
    "メモリスキャンを閉じて設定を開きます。保存はしません — 設定で適用 / OK してください。",
    (
        "MouseCoordScanWindow",
        "Close Memory scan and return to Settings → Mouse integration.",
    ): "メモリスキャンを閉じ、設定 → マウス統合へ戻ります。",
    ("MouseCoordScanWindow", "Cancel"): "キャンセル",
    ("MouseCoordScanWindow", "X2"): "X2",
    ("MouseCoordScanWindow", "Y2"): "Y2",
    (
        "MouseCoordScanWindow",
        "Scan = add candidates (turns Mouse capture ON).\n"
        "Mouse capture ON alone = refresh values / prune noise (no new picks).\n"
        "Watch = keep in list after Scan stops.\n"
        "Chase = follow guest-writer SOURCE; moves to the new SOURCE when found.\n"
        "Also adds nearby phys (±32 bytes) whose value Δ matches host/IO mouse Δ.\n"
        "X/Y = primary Game Phys pair.  X2/Y2 = optional Phys 2 pair.\n"
        "Green phys = MOS soft-cursor words (writable if set as Phys).\n"
        "Blue phys = disc-profile pair.  span = max−min; oversized rows are pruned.\n"
        "ESC ends capture (+ Scan).",
    ): "スキャン = 候補を追加（マウスキャプチャON）。\n"
    "マウスキャプチャONのみ = 値の更新／ノイズ整理（新規追加なし）。\n"
    "監視 = スキャン停止後も一覧に残す。\n"
    "追跡 = 書き込み元SOURCEを追従し、見つかると新しいSOURCEへ移る。\n"
    "周辺±32バイトでホスト／IOマウスΔに合わせて変化する phys も追加します。\n"
    "X/Y = 主の Game Phys。X2/Y2 = 任意の Phys 2。\n"
    "緑の phys = MOS soft-cursor（Phys に入れれば書き込み対象）。\n"
    "青の phys = ディスクプロファイルの組。span = max−min。極端に大きい行は整理。\n"
    "ESCでキャプチャ（＋スキャン）終了。",
    (
        "MouseCoordProfilePage",
        "Memory scan finds guest RAM coordinates; use Scan… above, then Update there "
        "to copy X/Y and ranges. All fields are manually adjustable; "
        "Settings Apply or OK writes into the disc profile.",
    ): "上部の「スキャン…」でメモリスキャンを開き、そこでの Update で X/Y と範囲をコピーできます。"
    "各項目は手動でも調整可能です。設定の適用 / OK でディスクプロファイルへ書き込みます。",
    ("MouseCoordProfilePage", "Phys 2"): "Phys 2",
    (
        "MouseCoordProfilePage",
        "Optional second X/Y phys. Direct-write pokes the same mapped value into both pairs.",
    ): "任意の2組目 X/Y phys。メモリ書き込み時は同じマップ値を両方の組へ書き込みます。",
}


def pick_ja(
    key: tuple[str, str],
    old_ja: dict[tuple[str, str], str],
    old_ja_by_source: dict[str, str],
) -> str:
    ctx, source = key
    if key in JA_FORCE:
        return JA_FORCE[key]
    prev = old_ja.get(key)
    if prev and has_cjk(prev):
        return prev
    if prev and prev != source:
        # Keep non-identical Latin translations (brand names etc.) only if intentional;
        # prefer CJK when available via source lookup.
        by_src = old_ja_by_source.get(source)
        if by_src and has_cjk(by_src):
            return by_src
        return prev
    by_src = old_ja_by_source.get(source)
    if by_src and has_cjk(by_src):
        return by_src
    # Keep identical for proper nouns / units / filter patterns.
    return source


def write_json(path: Path, entries: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump({"translations": entries}, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def sync_others(
    order: list[tuple[str, str]],
    en_map: dict[tuple[str, str], str],
) -> None:
    for locale in LOCALES:
        if locale in ("en", "ja"):
            continue
        path = ROOT / f"townsqt_{locale}.json"
        old = load_map(path)
        synced: list[dict[str, str]] = []
        for key in order:
            ctx, source = key
            existing = old.get(key)
            if existing and existing.strip():
                translation = existing
            else:
                translation = en_map.get(key, source)
            synced.append(
                {"context": ctx, "source": source, "translation": translation}
            )
        write_json(path, synced)
        print(f"wrote {path.name} ({len(synced)} entries)")


def main() -> None:
    ordered = extract_entries()
    print(f"extracted {len(ordered)} unique entries")

    old_en = load_map(ROOT / "townsqt_en.json")
    old_ja = load_map(ROOT / "townsqt_ja.json")
    old_ja_by_source: dict[str, str] = {}
    for (_ctx, source), translation in old_ja.items():
        if has_cjk(translation) and source not in old_ja_by_source:
            old_ja_by_source[source] = translation

    en_entries: list[dict[str, str]] = []
    ja_entries: list[dict[str, str]] = []
    en_map: dict[tuple[str, str], str] = {}
    still_en = 0
    for key in ordered:
        ctx, source = key
        en_entries.append(
            {"context": ctx, "source": source, "translation": source}
        )
        en_map[key] = source
        ja = pick_ja(key, old_ja, old_ja_by_source)
        ja_entries.append({"context": ctx, "source": source, "translation": ja})
        if ja == source and any(c.isalpha() and ord(c) < 128 for c in source):
            if not has_cjk(source):
                # Count likely-untranslated UI English
                if len(source) > 2 and source not in {
                    "X",
                    "Y",
                    "FM",
                    "PCM",
                    "CDDA",
                    "MIDI",
                    "BIOS",
                    "CPU",
                    "MID",
                    "HIGH",
                    "ALSA",
                    "FluidSynth",
                    "SoundFont",
                    "Update",
                    "phys",
                    "sz",
                    "value",
                    "span",
                    "changes",
                    "min..max",
                    "Marty",
                    "80386SX",
                    "80486SX",
                    "80386FPU",
                    "Tsugaru_QT",
                    "OK",
                    "x",
                }:
                    still_en += 1

    write_json(ROOT / "townsqt_en.json", en_entries)
    write_json(ROOT / "townsqt_ja.json", ja_entries)
    print(f"wrote townsqt_en.json ({len(en_entries)})")
    print(f"wrote townsqt_ja.json ({len(ja_entries)})")
    print(f"ja entries still identical to English source (excl. keep-list): {still_en}")
    sync_others(ordered, en_map)


if __name__ == "__main__":
    main()
