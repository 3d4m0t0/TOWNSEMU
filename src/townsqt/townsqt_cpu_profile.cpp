#include "townsqt_cpu_profile.h"

#include <QCoreApplication>

#include <algorithm>

namespace
{
QString Tr(const char *text)
{
	return QCoreApplication::translate("TownsQtCpuProfile",text);
}

struct CpuEntry
{
	TownsQtCpuKind kind;
	const char *id;
	const char *label;
	int max_mem_mb;
	bool cdrom_2x;
};

const CpuEntry kCpus[]={
    {TownsQtCpuKind::I386DX,"386dx",QT_TRANSLATE_NOOP("TownsQtCpuProfile","80386DX"),64,false},
    {TownsQtCpuKind::I386SX,"386sx",QT_TRANSLATE_NOOP("TownsQtCpuProfile","80386SX"),10,false},
    {TownsQtCpuKind::I486SX,"486sx",QT_TRANSLATE_NOOP("TownsQtCpuProfile","80486SX"),64,false},
    {TownsQtCpuKind::I486DX,"486dx",QT_TRANSLATE_NOOP("TownsQtCpuProfile","80486DX"),64,true},
    {TownsQtCpuKind::Pentium,"pentium",QT_TRANSLATE_NOOP("TownsQtCpuProfile","Pentium"),64,true},
    {TownsQtCpuKind::Marty,"marty",QT_TRANSLATE_NOOP("TownsQtCpuProfile","Marty"),6,false},
};

constexpr int kCpuCount=static_cast<int>(sizeof(kCpus)/sizeof(kCpus[0]));

const CpuEntry &Entry(TownsQtCpuKind kind)
{
	for(const CpuEntry &e : kCpus)
	{
		if(e.kind==kind)
		{
			return e;
		}
	}
	return kCpus[3]; // 486DX
}

bool ContainsKind(const std::vector<TownsQtCpuKind> &list,TownsQtCpuKind kind)
{
	return std::find(list.begin(),list.end(),kind)!=list.end();
}
}

int TownsQtCpuKindCount()
{
	return kCpuCount;
}

TownsQtCpuKind TownsQtCpuKindAt(int index)
{
	index=std::clamp(index,0,kCpuCount-1);
	return kCpus[index].kind;
}

TownsQtCpuKind TownsQtCpuKindDefault()
{
	return TownsQtCpuKind::I486DX;
}

const char *TownsQtCpuKindId(TownsQtCpuKind kind)
{
	return Entry(kind).id;
}

QString TownsQtCpuKindLabel(TownsQtCpuKind kind)
{
	return Tr(Entry(kind).label);
}

TownsQtCpuKind TownsQtCpuKindFromId(const QString &id)
{
	for(const CpuEntry &e : kCpus)
	{
		if(0==id.compare(QString::fromLatin1(e.id),Qt::CaseInsensitive))
		{
			return e.kind;
		}
	}
	return TownsQtCpuKindDefault();
}

TownsQtCpuKind TownsQtCpuKindFromTownsType(unsigned int towns_type)
{
	switch(towns_type)
	{
	case TOWNSTYPE_2_UX:
	case TOWNSTYPE_2_UG:
	case TOWNSTYPE_MARTY:
		return (TOWNSTYPE_MARTY==towns_type) ? TownsQtCpuKind::Marty : TownsQtCpuKind::I386SX;
	case TOWNSTYPE_2_HR:
	case TOWNSTYPE_2_UR:
	case TOWNSTYPE_2_MA:
		return TownsQtCpuKind::I486SX;
	case TOWNSTYPE_2_MX:
	case TOWNSTYPE_2_ME:
	case TOWNSTYPE_2_MF_FRESH:
		return TownsQtCpuKind::I486DX;
	case TOWNSTYPE_2_HC:
		return TownsQtCpuKind::Pentium;
	case TOWNSTYPE_2_CX:
	case TOWNSTYPE_2_HG:
	case TOWNSTYPE_MODEL1_2:
	case TOWNSTYPE_1F_2F:
	case TOWNSTYPE_10F_20F:
	default:
		return TownsQtCpuKind::I386DX;
	}
}

unsigned int TownsQtCpuKindTownsType(TownsQtCpuKind kind,int sys_rom_level)
{
	switch(kind)
	{
	case TownsQtCpuKind::Marty:
		return TOWNSTYPE_MARTY;
	case TownsQtCpuKind::I386SX:
		// L20+ packaging = UG era; L10 = UX.
		return (20<=sys_rom_level) ? TOWNSTYPE_2_UG : TOWNSTYPE_2_UX;
	case TownsQtCpuKind::I486SX:
		return TOWNSTYPE_2_HR;
	case TownsQtCpuKind::I486DX:
		return TOWNSTYPE_2_MX;
	case TownsQtCpuKind::Pentium:
		return TOWNSTYPE_2_HC;
	case TownsQtCpuKind::I386DX:
	default:
		if(20<=sys_rom_level)
		{
			return TOWNSTYPE_2_HG;
		}
		if(10<=sys_rom_level)
		{
			return TOWNSTYPE_2_CX;
		}
		return TOWNSTYPE_1F_2F;
	}
}

int TownsQtCpuKindMaxMemMb(TownsQtCpuKind kind)
{
	return Entry(kind).max_mem_mb;
}

bool TownsQtCpuKindCdRom2xCapable(TownsQtCpuKind kind)
{
	return Entry(kind).cdrom_2x;
}

int TownsQtCpuKindDefaultCdSpeed(TownsQtCpuKind kind)
{
	return TownsQtCpuKindCdRom2xCapable(kind) ? 2 : 0;
}

bool TownsQtCpuKindIsMarty(TownsQtCpuKind kind)
{
	return TownsQtCpuKind::Marty==kind;
}

bool TownsQtCpuKindRequiresSxMap(TownsQtCpuKind kind)
{
	return TownsQtCpuKind::I386SX==kind || TownsQtCpuKind::Marty==kind;
}

std::vector<TownsQtCpuKind> TownsQtCpuKindsAllowedForSysRom(
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	std::vector<TownsQtCpuKind> out;
	auto add=[&](TownsQtCpuKind k){
		if(true!=ContainsKind(out,k))
		{
			out.push_back(k);
		}
	};

	if(true==marty_ex_rom_present)
	{
		add(TownsQtCpuKind::Marty);
	}

	switch(profile)
	{
	case TownsQtSysRomProfile::Missing:
		add(TownsQtCpuKind::I486DX);
		return out;
	case TownsQtSysRomProfile::FreeDx:
		add(TownsQtCpuKind::I386DX);
		add(TownsQtCpuKind::I486DX);
		return out;
	case TownsQtSysRomProfile::PreV2Legacy:
		// Gen1 (Model 1/2/F/H/S): 386DX only. 386SX needs UX/UG memory map + dedicated BIOS.
		add(TownsQtCpuKind::I386DX);
		return out;
	case TownsQtSysRomProfile::ModernDxOnly:
		// DX-only image: no 386SX map.
		if(0<=sys_rom_level && sys_rom_level<20)
		{
			add(TownsQtCpuKind::I386DX);
			return out;
		}
		if(0<=sys_rom_level && sys_rom_level<30)
		{
			add(TownsQtCpuKind::I386DX);
			add(TownsQtCpuKind::I486SX);
			return out;
		}
		if(0<=sys_rom_level && sys_rom_level<50)
		{
			add(TownsQtCpuKind::I486SX);
			add(TownsQtCpuKind::I486DX);
			return out;
		}
		add(TownsQtCpuKind::I486SX);
		add(TownsQtCpuKind::I486DX);
		if(50<=sys_rom_level || 0>sys_rom_level)
		{
			add(TownsQtCpuKind::Pentium);
		}
		return out;
	case TownsQtSysRomProfile::ModernUnified:
	default:
		break;
	}

	// ModernUnified / unknown modern: TownsOS packaging eras (SX map capable).
	if(0<=sys_rom_level && sys_rom_level<20)
	{
		// V2.1L10 → CX (386DX) / UX (386SX)
		add(TownsQtCpuKind::I386DX);
		add(TownsQtCpuKind::I386SX);
		return out;
	}
	if(0<=sys_rom_level && sys_rom_level<30)
	{
		// V2.1L20 → UG (386SX) / HR·HG·UR (486SX)
		add(TownsQtCpuKind::I386SX);
		add(TownsQtCpuKind::I386DX);
		add(TownsQtCpuKind::I486SX);
		return out;
	}
	if(0<=sys_rom_level && sys_rom_level<50)
	{
		// V2.1L30/L40 → MA/MX/ME…
		add(TownsQtCpuKind::I486SX);
		add(TownsQtCpuKind::I486DX);
		return out;
	}
	if(50<=sys_rom_level)
	{
		add(TownsQtCpuKind::I486SX);
		add(TownsQtCpuKind::I486DX);
		add(TownsQtCpuKind::Pentium);
		return out;
	}

	// Level unknown on unified ROM: offer the common late set.
	add(TownsQtCpuKind::I386SX);
	add(TownsQtCpuKind::I386DX);
	add(TownsQtCpuKind::I486SX);
	add(TownsQtCpuKind::I486DX);
	add(TownsQtCpuKind::Pentium);
	return out;
}

TownsQtCpuKind TownsQtCpuKindPreferredForSysRom(
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	const auto allowed=TownsQtCpuKindsAllowedForSysRom(profile,sys_rom_level,marty_ex_rom_present);
	if(true==allowed.empty())
	{
		return TownsQtCpuKindDefault();
	}
	// Prefer 486DX when available (TownsQt default), else last (newest) non-Marty, else first.
	if(true==ContainsKind(allowed,TownsQtCpuKind::I486DX))
	{
		return TownsQtCpuKind::I486DX;
	}
	for(auto it=allowed.rbegin(); it!=allowed.rend(); ++it)
	{
		if(TownsQtCpuKind::Marty!=*it)
		{
			return *it;
		}
	}
	return allowed.front();
}

TownsQtCpuKind TownsQtCpuKindClampToAllowed(
    TownsQtCpuKind kind,
    TownsQtSysRomProfile profile,
    int sys_rom_level,
    bool marty_ex_rom_present)
{
	const auto allowed=TownsQtCpuKindsAllowedForSysRom(profile,sys_rom_level,marty_ex_rom_present);
	if(true==ContainsKind(allowed,kind))
	{
		return kind;
	}
	return TownsQtCpuKindPreferredForSysRom(profile,sys_rom_level,marty_ex_rom_present);
}
