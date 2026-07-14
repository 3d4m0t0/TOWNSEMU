#pragma once

#include <QString>

#include "townsdef.h"

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
