#pragma once

#include <QString>
#include <vector>

#include "townsdef.h"
#include "townsqt_rom_availability.h"

enum class TownsQtCpuKind
{
	I386DX=0,
	I386SX,
	I486SX,
	I486DX,
	Pentium,
	Marty,
};

int TownsQtCpuKindCount();
TownsQtCpuKind TownsQtCpuKindAt(int index);
TownsQtCpuKind TownsQtCpuKindDefault();
const char *TownsQtCpuKindId(TownsQtCpuKind kind);
QString TownsQtCpuKindLabel(TownsQtCpuKind kind);
TownsQtCpuKind TownsQtCpuKindFromId(const QString &id);
TownsQtCpuKind TownsQtCpuKindFromTownsType(unsigned int towns_type);

/*! Representative townsType for Machine ID / memory map, using SYS-ROM TownsOS level. */
unsigned int TownsQtCpuKindTownsType(TownsQtCpuKind kind,int sys_rom_level);
int TownsQtCpuKindMaxMemMb(TownsQtCpuKind kind);
bool TownsQtCpuKindCdRom2xCapable(TownsQtCpuKind kind);
int TownsQtCpuKindDefaultCdSpeed(TownsQtCpuKind kind);
bool TownsQtCpuKindIsMarty(TownsQtCpuKind kind);
bool TownsQtCpuKindRequiresSxMap(TownsQtCpuKind kind);

std::vector<TownsQtCpuKind> TownsQtCpuKindsAllowedForSysRom(
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
TownsQtCpuKind TownsQtCpuKindClampToAllowed(
    TownsQtCpuKind kind,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
TownsQtCpuKind TownsQtCpuKindPreferredForSysRom(
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
