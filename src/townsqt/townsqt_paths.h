#pragma once

#include <QString>

/*! XDG-style paths for TownsQt (~/.config/townsqt on Linux). */
namespace TownsQtPaths
{
QString configDir();
QString romsDir();
QString blankFdDir();
QString hddDir();
QString cmosFilePath();
/*! Directory for per-disc-profile CMOS files: <configDir>/cmos */
QString cmosDir();
/*! Per-disc-profile CMOS: <configDir>/cmos/cmos_XXXXXXXX.bin (disc fingerprint). */
QString cmosFilePathForDiscFingerprint(unsigned int fingerprintHash32);
/*! Shared cmos.bin (config root) when hash is 0; otherwise under cmos/. */
QString cmosFilePathForProfile(unsigned int fingerprintHash32);
QString configFilePath();
/*! Per-disc mouse-coord profiles: <configDir>/profiles/fp_XXXXXXXX.ini */
QString profilesDir();
/*! User mouse-integration presets: <configDir>/mouse_presets/mouse_XXXXXXXX.ini */
QString mousePresetsDir();
/*! Per-disc VM state saves: <configDir>/statesave/state0_XXXXXXXX.TState (auto-resume / slot 0) */
QString stateSaveDir();
/*! State-save screenshots: <configDir>/statesave/image/stateN_XXXXXXXX.png */
QString stateSaveImageDir();
/*! Manual Tools→Screenshot: XDG Pictures (QStandardPaths::PicturesLocation),
    e.g. ~/Pictures or ~/ピクチャ — <CDROM basename>_NN.png (NN=00..99) */
QString imageDir();
/*! Content browser library JSON: <configDir>/content_library.json */
QString contentLibraryFilePath();
/*! Content browser icons: <configDir>/content_icons/ */
QString contentIconsDir();

/*! Create configDir, romsDir, blankFdDir, hddDir, cmosDir, profilesDir, mousePresetsDir,
    stateSaveDir, stateSaveImageDir, Pictures (imageDir), and contentIconsDir if missing. */
bool ensureLayout();
}
