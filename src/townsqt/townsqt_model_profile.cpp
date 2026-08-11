#include "townsqt_model_profile.h"

#include <QCoreApplication>
#include <QString>

#include <algorithm>
#include <vector>

namespace
{
/*! One entry per Tsugaru townsType (same names as TownsTypeToStr / GUI profile list). */
const TownsQtModelGroup kModelGroups[]={
    // id matches TownsTypeToStr; label unused (TownsQtModelGroupLabel uses TownsTypeToStr).
    {"MODEL2", "MODEL2", TOWNSTYPE_MODEL1_2, nullptr, 64, false, false, false},
    {"2F",     "2F",     TOWNSTYPE_1F_2F,    nullptr, 64, false, false, false},
    {"20F",    "20F",    TOWNSTYPE_10F_20F,  nullptr, 64, false, false, false},
    {"UX",     "UX",     TOWNSTYPE_2_UX,     nullptr, 10, false, false, true},
    {"CX",     "CX",     TOWNSTYPE_2_CX,     nullptr, 64, true,  false, false},
    {"UG",     "UG",     TOWNSTYPE_2_UG,     nullptr, 10, true,  false, true},
    {"HG",     "HG",     TOWNSTYPE_2_HG,     nullptr, 64, true,  false, false},
    {"HR",     "HR",     TOWNSTYPE_2_HR,     nullptr, 64, true,  false, false},
    {"UR",     "UR",     TOWNSTYPE_2_UR,     nullptr, 64, true,  false, false},
    {"MA",     "MA",     TOWNSTYPE_2_MA,     nullptr, 64, true,  false, false},
    {"MX",     "MX",     TOWNSTYPE_2_MX,     nullptr, 64, true,  true,  false},
    {"ME",     "ME",     TOWNSTYPE_2_ME,     nullptr, 64, true,  true,  false},
    {"MF",     "MF",     TOWNSTYPE_2_MF_FRESH,nullptr,64, true,  true,  false},
    {"HC",     "HC",     TOWNSTYPE_2_HC,     nullptr, 64, true,  true,  false},
    {"MARTY",  "MARTY",  TOWNSTYPE_MARTY,    nullptr, 6,  false, false, false},
};

constexpr int kDefaultModelGroupIndex=10; // MX

bool ModelFitsSysRomEra(unsigned int towns_type,TownsQtSysRomProfile profile,int level)
{
	if(TownsQtSysRomProfile::PreV2Legacy==profile)
	{
		return TOWNSTYPE_MODEL1_2==towns_type ||
		       TOWNSTYPE_1F_2F==towns_type ||
		       TOWNSTYPE_10F_20F==towns_type;
	}
	if(0>level)
	{
		return true;
	}
	switch(towns_type)
	{
	case TOWNSTYPE_MODEL1_2:
	case TOWNSTYPE_1F_2F:
	case TOWNSTYPE_10F_20F:
		return level<10;
	case TOWNSTYPE_2_UX:
		return level<20;
	case TOWNSTYPE_2_CX:
		return level>=10 && level<20;
	case TOWNSTYPE_2_UG:
	case TOWNSTYPE_2_HG:
	case TOWNSTYPE_2_HR:
	case TOWNSTYPE_2_UR:
	case TOWNSTYPE_2_MA:
		return level>=20;
	case TOWNSTYPE_2_MX:
	case TOWNSTYPE_2_ME:
	case TOWNSTYPE_2_MF_FRESH:
		return level>=30;
	case TOWNSTYPE_2_HC:
		return level>=50;
	case TOWNSTYPE_MARTY:
		return true;
	default:
		return true;
	}
}

bool ModelFitsSysRomProfile(int model_index,TownsQtSysRomProfile profile,bool marty_ex_rom_present)
{
	if(TownsQtModelGroupIsMarty(model_index))
	{
		return marty_ex_rom_present;
	}
	if(true==TownsQtModelGroupRequiresSxRom(model_index))
	{
		return TownsQtSysRomProfile::ModernUnified==profile;
	}
	return true;
}

/*! Map legacy TownsQt model_group ids to current Tsugaru-name ids. */
int LegacyModelGroupIndexForId(const QString &id)
{
	if(QStringLiteral("gen1")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_1F_2F);
	}
	if(QStringLiteral("ux")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_UX);
	}
	if(QStringLiteral("ug")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_UG);
	}
	if(QStringLiteral("cx")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_CX);
	}
	if(QStringLiteral("hg")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_HG);
	}
	if(QStringLiteral("hr")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_HR);
	}
	if(QStringLiteral("ur_ma")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_UR);
	}
	if(QStringLiteral("mx_gen")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_MX);
	}
	if(QStringLiteral("hc")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_2_HC);
	}
	if(QStringLiteral("marty")==id)
	{
		return TownsQtModelGroupIndexForTownsType(TOWNSTYPE_MARTY);
	}
	return -1;
}
}

int TownsQtModelGroupCount()
{
	return static_cast<int>(sizeof(kModelGroups)/sizeof(kModelGroups[0]));
}

const TownsQtModelGroup &TownsQtModelGroupAt(int index)
{
	static const TownsQtModelGroup kFallback={
	    "MX","MX",TOWNSTYPE_2_MX,nullptr,64,true,true,false,
	};
	if(index<0 || TownsQtModelGroupCount()<=index)
	{
		return kFallback;
	}
	return kModelGroups[index];
}

int TownsQtModelGroupDefaultIndex()
{
	return kDefaultModelGroupIndex;
}

int TownsQtModelGroupIndexForTownsType(unsigned int towns_type)
{
	if(TOWNSTYPE_UNKNOWN==towns_type)
	{
		return kDefaultModelGroupIndex;
	}
	for(int i=0; i<TownsQtModelGroupCount(); ++i)
	{
		if(kModelGroups[i].towns_type==towns_type)
		{
			return i;
		}
	}
	return kDefaultModelGroupIndex;
}

unsigned int TownsQtModelGroupTownsType(int index)
{
	return TownsQtModelGroupAt(index).towns_type;
}

QString TownsQtModelGroupLabel(int index)
{
	return QString::fromStdString(TownsTypeToStr(TownsQtModelGroupTownsType(index)));
}

QString TownsQtModelGroupDescription(int index)
{
	(void)index;
	return QString();
}

QString TownsQtModelGroupId(int index)
{
	return QString::fromUtf8(TownsQtModelGroupAt(index).id);
}

int TownsQtModelGroupIndexForId(const QString &id)
{
	const QByteArray needle=id.toUtf8();
	for(int i=0; i<TownsQtModelGroupCount(); ++i)
	{
		if(needle==kModelGroups[i].id)
		{
			return i;
		}
	}
	const int legacy=LegacyModelGroupIndexForId(id);
	if(0<=legacy)
	{
		return legacy;
	}
	// Also accept TownsTypeToStr via StrToTownsType.
	const unsigned int towns_type=StrToTownsType(id.toStdString());
	if(TOWNSTYPE_UNKNOWN!=towns_type)
	{
		return TownsQtModelGroupIndexForTownsType(towns_type);
	}
	return kDefaultModelGroupIndex;
}

int TownsQtModelGroupMartyIndex()
{
	for(int i=0; i<TownsQtModelGroupCount(); ++i)
	{
		if(TOWNSTYPE_MARTY==kModelGroups[i].towns_type)
		{
			return i;
		}
	}
	return -1;
}

bool TownsQtModelGroupIsMarty(int index)
{
	return TOWNSTYPE_MARTY==TownsQtModelGroupAt(index).towns_type;
}

bool TownsQtModelGroupRequiresSxRom(int index)
{
	return TownsQtModelGroupAt(index).requires_sx_rom;
}

int TownsQtModelGroupMaxMemMb(int index)
{
	return std::max(1,TownsQtModelGroupAt(index).max_mem_mb);
}

bool TownsQtModelGroupSupportsFastMode(int index)
{
	return TownsQtModelGroupAt(index).supports_fast_mode;
}

bool TownsQtModelGroupCdRom2xCapable(int index)
{
	return TownsQtModelGroupAt(index).cdrom_2x_capable;
}

int TownsQtModelGroupDefaultCdSpeed(int index)
{
	return TownsQtModelGroupCdRom2xCapable(index) ? 2 : 0;
}

bool TownsQtModelGroupSupportsHighRes(int index)
{
	const unsigned int tt=TownsQtModelGroupTownsType(index);
	return TOWNSTYPE_2_MX==tt ||
	       TOWNSTYPE_2_ME==tt ||
	       TOWNSTYPE_2_MF_FRESH==tt ||
	       TOWNSTYPE_2_HC==tt;
}

bool TownsQtModelGroupSupportsUgGenerationIO(int index)
{
	if(true==TownsQtModelGroupIsMarty(index))
	{
		return false;
	}
	return TOWNSTYPE_2_UG<=TownsQtModelGroupTownsType(index);
}

bool TownsQtModelGroupEffectiveHighRes(int index,TownsQtSysRomProfile profile)
{
	return TownsQtModelGroupSupportsHighRes(index) &&
	       TownsQtRomAvailability::SysRomSupportsHighResCrtc(profile);
}

bool TownsQtModelGroupEffectiveHighRes(int index,const QString &rom_dir)
{
	return TownsQtModelGroupEffectiveHighRes(
	    index,
	    TownsQtRomAvailability::ClassifySysRom(rom_dir));
}

bool TownsQtModelGroupEffectiveUgGenerationIO(int index,const QString &rom_dir)
{
	return TownsQtModelGroupSupportsUgGenerationIO(index) ||
	       TownsQtRomAvailability::SysRomImpliesUgGenerationIO(rom_dir);
}

std::vector<int> TownsQtModelGroupsAllowedForCpuAndSysRom(
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	std::vector<int> out;
	for(int i=0; i<TownsQtModelGroupCount(); ++i)
	{
		if(TownsQtCpuKindFromTownsType(TownsQtModelGroupTownsType(i))!=cpu)
		{
			continue;
		}
		if(true!=ModelFitsSysRomProfile(i,profile,marty_ex_rom_present))
		{
			continue;
		}
		if(true!=ModelFitsSysRomEra(TownsQtModelGroupTownsType(i),profile,sys_rom_level))
		{
			continue;
		}
		out.push_back(i);
	}
	return out;
}

int TownsQtModelGroupPreferredForCpuAndSysRom(
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	const auto allowed=TownsQtModelGroupsAllowedForCpuAndSysRom(
	    cpu,profile,sys_rom_level,marty_ex_rom_present);
	if(true==allowed.empty())
	{
		return TownsQtModelGroupDefaultIndex();
	}
	const int hint=TownsQtModelGroupIndexForTownsType(
	    TownsQtCpuKindTownsType(cpu,sys_rom_level));
	for(int idx : allowed)
	{
		if(idx==hint)
		{
			return idx;
		}
	}
	return allowed.front();
}

int TownsQtModelGroupClampToAllowed(
    int model_index,
    TownsQtCpuKind cpu,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	const auto allowed=TownsQtModelGroupsAllowedForCpuAndSysRom(
	    cpu,profile,sys_rom_level,marty_ex_rom_present);
	for(int idx : allowed)
	{
		if(idx==model_index)
		{
			return model_index;
		}
	}
	return TownsQtModelGroupPreferredForCpuAndSysRom(
	    cpu,profile,sys_rom_level,marty_ex_rom_present);
}
