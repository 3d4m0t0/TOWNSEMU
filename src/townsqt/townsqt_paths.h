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
QString configFilePath();
/*! Per-disc mouse-coord profiles: <configDir>/profiles/fp_XXXXXXXX.ini */
QString profilesDir();
/*! User mouse-integration presets: <configDir>/mouse_presets/mouse_XXXXXXXX.ini */
QString mousePresetsDir();

/*! Create configDir, romsDir, blankFdDir, hddDir, profilesDir, and mousePresetsDir if missing. */
bool ensureLayout();
}
