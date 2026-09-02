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
/*! Per-disc VM state saves: <configDir>/statesave/state0_XXXXXXXX.TState */
QString stateSaveDir();

/*! Create configDir, romsDir, blankFdDir, hddDir, profilesDir, mousePresetsDir, and stateSaveDir if missing. */
bool ensureLayout();
}
