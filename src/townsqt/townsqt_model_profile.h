#pragma once

#include <QString>
#include <vector>

#include "townsdef.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_rom_availability.h"

struct TownsQtModelGroup
{
	const char *id;
	const char *label;
	unsigned int towns_type;
	const char *const *hardware_bullets;
	int max_mem_mb;
	bool supports_fast_mode;
	bool cdrom_2x_capable;
	bool requires_sx_rom;
};

int TownsQtModelGroupCount();
const TownsQtModelGroup &TownsQtModelGroupAt(int index);
int TownsQtModelGroupDefaultIndex();
int TownsQtModelGroupIndexForTownsType(unsigned int towns_type);
unsigned int TownsQtModelGroupTownsType(int index);
QString TownsQtModelGroupLabel(int index);
QString TownsQtModelGroupDescription(int index);

QString TownsQtModelGroupId(int index);
int TownsQtModelGroupIndexForId(const QString &id);
int TownsQtModelGroupMartyIndex();
bool TownsQtModelGroupIsMarty(int index);
bool TownsQtModelGroupRequiresSxRom(int index);
int TownsQtModelGroupMaxMemMb(int index);
bool TownsQtModelGroupSupportsFastMode(int index);
bool TownsQtModelGroupCdRom2xCapable(int index);
int TownsQtModelGroupDefaultCdSpeed(int index);
/*! MX/ME/MF/HC-class high-res CRTC + high-res PCM. */
bool TownsQtModelGroupSupportsHighRes(int index);
/*! UG and later SCSI / peripheral I/O generation (not Marty). */
bool TownsQtModelGroupSupportsUgGenerationIO(int index);
/*! High-res when the model has it and SYS ROM can drive it. */
bool TownsQtModelGroupEffectiveHighRes(int index,TownsQtSysRomProfile profile);
bool TownsQtModelGroupEffectiveHighRes(int index,const QString &rom_dir);
/*! UG I/O when the model is UG+ or SYS ROM implies UG-generation I/O. */
bool TownsQtModelGroupEffectiveUgGenerationIO(int index,const QString &rom_dir);

/*! Model groups whose CPU and SYS-ROM packaging era match. */
std::vector<int> TownsQtModelGroupsAllowedForCpuAndSysRom(
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
int TownsQtModelGroupPreferredForCpuAndSysRom(
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
int TownsQtModelGroupClampToAllowed(
    int model_index,
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present);
