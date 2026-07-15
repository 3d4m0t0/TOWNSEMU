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

/*! Create configDir, romsDir, blankFdDir, and hddDir if missing. */
bool ensureLayout();
}
