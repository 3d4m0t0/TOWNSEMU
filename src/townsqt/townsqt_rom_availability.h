#pragma once

#include <QString>

enum class TownsQtSysRomProfile
{
	Missing,
	FreeDx,
	PreV2Legacy,
	ModernDxOnly,
	ModernUnified,
};

namespace TownsQtRomAvailability
{
QString ResolveRomImagePath(const QString &rom_dir,const char *canonical_name);
bool MartyExRomPresent(const QString &rom_dir);
TownsQtSysRomProfile ClassifySysRom(const QString &rom_dir);
QString SysRomSummary(const QString &rom_dir);
/*! True when a V2-era (or later) 386SX-capable FMT_SYS.ROM is available. */
bool SxSystemRomPresent(const QString &rom_dir);
bool ModelGroupAllowedForSysRom(int model_index,
                                TownsQtSysRomProfile profile,
                                bool marty_ex_rom_present);
int PreferredModelGroupForSysRom(TownsQtSysRomProfile profile);
}
