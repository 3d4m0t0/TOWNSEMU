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
/*! True when SYS ROM is V2-era commercial (high-res CRTC BIOS support). */
bool SysRomSupportsHighResCrtc(TownsQtSysRomProfile profile);
bool SysRomSupportsHighResCrtc(const QString &rom_dir);
/*! TownsOS / SYS-ROM "Lxx" level (e.g. 10, 20, 51), or -1 if unknown. */
int SysRomTownsOsLevel(const QString &rom_dir);
/*! True when SYS ROM version implies UG-generation SCSI / peripheral I/O
    (TownsOS V2.1L20+ era: UG/HR/HG/UR and later; not CX/UX V2.1L10). */
bool SysRomImpliesUgGenerationIO(const QString &rom_dir);
bool ModelGroupAllowedForSysRom(int model_index,
                                TownsQtSysRomProfile profile,
                                bool marty_ex_rom_present);
int PreferredModelGroupForSysRom(TownsQtSysRomProfile profile);
}
