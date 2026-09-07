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
/*! Per-disc VM state saves: <configDir>/statesave/state0_XXXXXXXX.TState */
QString stateSaveDir();

/*! Create configDir, romsDir, blankFdDir, hddDir, cmosDir, profilesDir, mousePresetsDir, and stateSaveDir if missing. */
bool ensureLayout();
}
