#pragma once

#include <QString>

/*! XDG-style paths for TownsQt (~/.config/townsqt on Linux). */
namespace TownsQtPaths
{
QString configDir();
QString romsDir();
QString blankFdDir();
QString cmosFilePath();
QString configFilePath();

/*! Create configDir and romsDir if missing. */
bool ensureLayout();
}
