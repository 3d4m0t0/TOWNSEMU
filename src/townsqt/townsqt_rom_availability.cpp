#include "townsqt_rom_availability.h"

#include "townsqt_model_profile.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include <cstring>

namespace
{
constexpr qint64 kMartyExRomSize=512*1024;
constexpr qint64 kSysRomSize=256*1024;

QString CompareFileNameCaseInsensitive(const QString &dir_name,const char *canonical_name)
{
	const QString exact=dir_name+QLatin1Char('/')+QString::fromLatin1(canonical_name);
	if(QFileInfo::exists(exact))
	{
		return exact;
	}

	QDir dir(dir_name);
	const QStringList entries=dir.entryList(QDir::Files);
	const QString needle=QString::fromLatin1(canonical_name);
	for(const QString &entry : entries)
	{
		if(0==entry.compare(needle,Qt::CaseInsensitive))
		{
			return dir.filePath(entry);
		}
	}
	return exact;
}

bool RomImageValid(const QString &path,qint64 expected_size)
{
	const QFileInfo info(path);
	return info.exists() && info.isFile() && info.size()==expected_size;
}

bool ExtractRomFromAllRom(const QString &all_rom_path,const char *tag7,qint64 expected_size,QByteArray &out)
{
	QFile file(all_rom_path);
	if(!file.open(QIODevice::ReadOnly))
	{
		return false;
	}
	const QByteArray all_roms=file.readAll();
	file.close();

	size_t ptr=0;
	while(ptr+16<=static_cast<size_t>(all_roms.size()))
	{
		const char *tag=all_roms.constData()+ptr;
		const auto len=static_cast<qint64>(
		    static_cast<unsigned char>(all_roms[ptr+12]) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+13]))<<8) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+14]))<<16) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+15]))<<24));
		if(len<0 || all_roms.size()<static_cast<qint64>(ptr+16)+len)
		{
			break;
		}
		if(0==memcmp(tag,tag7,7) && len==expected_size)
		{
			out=all_roms.mid(static_cast<qint64>(ptr+16),len);
			return out.size()==expected_size;
		}
		ptr+=16+static_cast<size_t>(len);
	}
	return false;
}

QByteArray LoadRomImageFromDir(const QString &rom_dir,const char *canonical_name,const char *tag7,qint64 expected_size)
{
	if(rom_dir.isEmpty())
	{
		return {};
	}

	const QString path=CompareFileNameCaseInsensitive(rom_dir,canonical_name);
	if(RomImageValid(path,expected_size))
	{
		QFile file(path);
		if(file.open(QIODevice::ReadOnly))
		{
			return file.readAll();
		}
	}

	QByteArray from_all;
	const QString all_rom_path=CompareFileNameCaseInsensitive(rom_dir,"FMT_ALL.ROM");
	if(ExtractRomFromAllRom(all_rom_path,tag7,expected_size,from_all))
	{
		return from_all;
	}
	return {};
}

bool LoadMartyExRomsFromAllRom(const QString &all_rom_path,qint64 sizes[4])
{
	QFile file(all_rom_path);
	if(!file.open(QIODevice::ReadOnly))
	{
		return false;
	}
	const QByteArray all_roms=file.readAll();
	file.close();

	size_t ptr=0;
	while(ptr+16<=static_cast<size_t>(all_roms.size()))
	{
		const char *tag=all_roms.constData()+ptr;
		int marty_index=-1;
		if(0==memcmp(tag,"MAR_EX0",7))
		{
			marty_index=0;
		}
		else if(0==memcmp(tag,"MAR_EX1",7))
		{
			marty_index=1;
		}
		else if(0==memcmp(tag,"MAR_EX2",7))
		{
			marty_index=2;
		}
		else if(0==memcmp(tag,"MAR_EX3",7))
		{
			marty_index=3;
		}

		const auto len=static_cast<qint64>(
		    static_cast<unsigned char>(all_roms[ptr+12]) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+13]))<<8) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+14]))<<16) |
		    (static_cast<unsigned int>(static_cast<unsigned char>(all_roms[ptr+15]))<<24));
		if(len<0 || all_roms.size()<static_cast<qint64>(ptr+16)+len)
		{
			break;
		}
		if(0<=marty_index)
		{
			sizes[marty_index]=len;
		}
		ptr+=16+static_cast<size_t>(len);
	}

	return kMartyExRomSize==sizes[0] &&
	       kMartyExRomSize==sizes[1] &&
	       kMartyExRomSize==sizes[2] &&
	       kMartyExRomSize==sizes[3];
}

bool IsFreeCompatibleDxSysRom(const QByteArray &sys_rom)
{
	return sys_rom.contains("VOICEVOX") ||
	       sys_rom.contains(QString::fromUtf8("四国めたん").toUtf8()) ||
	       sys_rom.contains("Shikoku Methane");
}

bool IsDxOnlySysRomLayout(const QByteArray &sys_rom)
{
	if(sys_rom.size()<224*1024)
	{
		return true;
	}
	qint64 ff_count=0;
	for(int i=0; i<224*1024; ++i)
	{
		if(static_cast<unsigned char>(sys_rom[i])==0xff)
		{
			++ff_count;
		}
	}
	// Classic 386DX packs keep the first 224 KB empty; they cannot boot in the 386SX map.
	return ff_count>(224*1024*98/100);
}

bool HasModernSxSysRomVersion(const QByteArray &sys_rom)
{
	// V06 L01 EXT-BOOT images are too old for TownsQt UX/UG (e.g. with V2.x CD boot).
	return sys_rom.contains("V2.") ||
	       sys_rom.contains("2MXBOOT") ||
	       sys_rom.contains("L10") ||
	       sys_rom.contains("L20") ||
	       sys_rom.contains("L30") ||
	       sys_rom.contains("L40") ||
	       sys_rom.contains("L51");
}

bool IsSxBootCapableSysRom(const QByteArray &sys_rom)
{
	if(kSysRomSize!=sys_rom.size())
	{
		return false;
	}
	if(0xff==static_cast<unsigned char>(sys_rom[0xF7]))
	{
		return false;
	}
	if(true==IsDxOnlySysRomLayout(sys_rom))
	{
		return false;
	}
	return HasModernSxSysRomVersion(sys_rom);
}

bool IsVersionChar(char c)
{
	return ('0'<=c && c<='9') || ('A'<=c && c<='Z') || ('a'<=c && c<='z') || '.'==c;
}

QString ExtractSysRomVersionLabel(const QByteArray &sys_rom)
{
	for(int i=0; i+6<sys_rom.size(); ++i)
	{
		const unsigned char b0=static_cast<unsigned char>(sys_rom[i]);
		const unsigned char b1=static_cast<unsigned char>(sys_rom[i+1]);
		const unsigned char b2=static_cast<unsigned char>(sys_rom[i+2]);
		const unsigned char b3=static_cast<unsigned char>(sys_rom[i+3]);
		const unsigned char b4=static_cast<unsigned char>(sys_rom[i+4]);
		const unsigned char b5=static_cast<unsigned char>(sys_rom[i+5]);
		const unsigned char b6=static_cast<unsigned char>(sys_rom[i+6]);
		if('V'==b0 && '0'<=b1 && b1<='9' && '0'<=b2 && b2<='9' &&
		   ' '==b3 && 'L'==b4 && '0'<=b5 && b5<='9' && '0'<=b6 && b6<='9')
		{
			return QString::fromLatin1(sys_rom.constData()+i,7);
		}
	}

	static const char *kKnownMarkers[]={
	    "V2.1L51",
	    "V2.1L50",
	    "V2.1L40",
	    "V2.1L31",
	    "V2.1L30",
	    "V2.1L20",
	    "V2.1L10B",
	    "V2.1L10",
	    "2MXBOOT",
	};
	for(const char *marker : kKnownMarkers)
	{
		if(sys_rom.contains(marker))
		{
			return QString::fromLatin1(marker);
		}
	}

	if(sys_rom.contains("EXT-BOOT"))
	{
		return QStringLiteral("EXT-BOOT");
	}
	if(sys_rom.contains("V2."))
	{
		const int idx=sys_rom.indexOf("V2.");
		int end=idx+3;
		while(end<sys_rom.size() && IsVersionChar(sys_rom[end]) && end-idx<16)
		{
			++end;
		}
		return QString::fromLatin1(sys_rom.constData()+idx,end-idx);
	}
	return {};
}

/*! TownsOS / SYS-ROM "Lxx" level, or -1 if unknown.
    Correspondence (system software packaging):
      V2.1 L10  → CX / UX (pre-UG peripheral I/O)
      V2.1 L20+ → UG / HR / HG / UR and later */
int ParseSysRomLevelNumber(const QString &label)
{
	if(label.isEmpty() || label==QStringLiteral("EXT-BOOT"))
	{
		return -1;
	}
	if(label.contains(QStringLiteral("2MXBOOT"),Qt::CaseInsensitive))
	{
		return 30; // MX-era boot marker → treat as post-UG
	}
	const int lpos=label.lastIndexOf(QLatin1Char('L'));
	if(0>lpos || lpos+1>=label.size())
	{
		return -1;
	}
	int level=0;
	int digits=0;
	for(int i=lpos+1; i<label.size(); ++i)
	{
		const QChar c=label.at(i);
		if(true!=c.isDigit())
		{
			break;
		}
		level=level*10+c.digitValue();
		++digits;
	}
	return (0<digits) ? level : -1;
}

bool SysRomImpliesUgGenerationIOFromImage(const QByteArray &sys_rom)
{
	if(kSysRomSize!=sys_rom.size())
	{
		return false;
	}
	// Free compatible ROMs are used as MX-class substitutes in TownsQt.
	if(true==IsFreeCompatibleDxSysRom(sys_rom))
	{
		return true;
	}

	const QString label=ExtractSysRomVersionLabel(sys_rom);
	const int level=ParseSysRomLevelNumber(label);
	if(0<=level)
	{
		// L20 = UG/HR/HG/UR packaging era; L10 = CX/UX.
		return 20<=level;
	}

	// Fallback markers (ordered so L10 is not matched via shorter probes alone).
	static const char *kPostUgMarkers[]={
	    "2MXBOOT","V2.1L51","V2.1L50","V2.1L40","V2.1L31","V2.1L30","V2.1L20",
	};
	for(const char *marker : kPostUgMarkers)
	{
		if(sys_rom.contains(marker))
		{
			return true;
		}
	}
	return false;
}

TownsQtSysRomProfile ClassifySysRomImage(const QByteArray &sys_rom)
{
	if(kSysRomSize!=sys_rom.size())
	{
		return TownsQtSysRomProfile::Missing;
	}
	if(true==IsFreeCompatibleDxSysRom(sys_rom))
	{
		return TownsQtSysRomProfile::FreeDx;
	}
	if(true==HasModernSxSysRomVersion(sys_rom))
	{
		const bool sx_boot=0xff!=static_cast<unsigned char>(sys_rom[0xF7]);
		const bool dx_only=IsDxOnlySysRomLayout(sys_rom);
		if(sx_boot && !dx_only)
		{
			return TownsQtSysRomProfile::ModernUnified;
		}
		return TownsQtSysRomProfile::ModernDxOnly;
	}
	if(0xff!=static_cast<unsigned char>(sys_rom[0xF7]) &&
	   !IsDxOnlySysRomLayout(sys_rom))
	{
		return TownsQtSysRomProfile::PreV2Legacy;
	}
	if(true==IsDxOnlySysRomLayout(sys_rom))
	{
		return TownsQtSysRomProfile::ModernDxOnly;
	}
	return TownsQtSysRomProfile::Missing;
}
}

QString TownsQtRomAvailability::ResolveRomImagePath(const QString &rom_dir,const char *canonical_name)
{
	if(rom_dir.isEmpty())
	{
		return QString::fromLatin1(canonical_name);
	}
	return CompareFileNameCaseInsensitive(rom_dir,canonical_name);
}

bool TownsQtRomAvailability::MartyExRomPresent(const QString &rom_dir)
{
	if(rom_dir.isEmpty())
	{
		return false;
	}

	const bool individual=
	    RomImageValid(ResolveRomImagePath(rom_dir,"MAR_EX0.ROM"),kMartyExRomSize) &&
	    RomImageValid(ResolveRomImagePath(rom_dir,"MAR_EX1.ROM"),kMartyExRomSize) &&
	    RomImageValid(ResolveRomImagePath(rom_dir,"MAR_EX2.ROM"),kMartyExRomSize) &&
	    RomImageValid(ResolveRomImagePath(rom_dir,"MAR_EX3.ROM"),kMartyExRomSize);
	if(individual)
	{
		return true;
	}

	qint64 sizes[4]={-1,-1,-1,-1};
	const QString all_rom_path=ResolveRomImagePath(rom_dir,"FMT_ALL.ROM");
	if(!QFileInfo::exists(all_rom_path))
	{
		return false;
	}
	return LoadMartyExRomsFromAllRom(all_rom_path,sizes);
}

bool TownsQtRomAvailability::SxSystemRomPresent(const QString &rom_dir)
{
	const QByteArray sys_rom=LoadRomImageFromDir(rom_dir,"FMT_SYS.ROM","FMT_SYS",kSysRomSize);
	if(kSysRomSize!=sys_rom.size())
	{
		return false;
	}
	if(true==IsFreeCompatibleDxSysRom(sys_rom))
	{
		return false;
	}
	return IsSxBootCapableSysRom(sys_rom);
}

bool TownsQtRomAvailability::SysRomSupportsHighResCrtc(TownsQtSysRomProfile profile)
{
	return TownsQtSysRomProfile::ModernUnified==profile ||
	       TownsQtSysRomProfile::ModernDxOnly==profile;
}

bool TownsQtRomAvailability::SysRomSupportsHighResCrtc(const QString &rom_dir)
{
	return SysRomSupportsHighResCrtc(ClassifySysRom(rom_dir));
}

bool TownsQtRomAvailability::SysRomImpliesUgGenerationIO(const QString &rom_dir)
{
	const QByteArray sys_rom=LoadRomImageFromDir(rom_dir,"FMT_SYS.ROM","FMT_SYS",kSysRomSize);
	return SysRomImpliesUgGenerationIOFromImage(sys_rom);
}

int TownsQtRomAvailability::SysRomTownsOsLevel(const QString &rom_dir)
{
	const QByteArray sys_rom=LoadRomImageFromDir(rom_dir,"FMT_SYS.ROM","FMT_SYS",kSysRomSize);
	if(kSysRomSize!=sys_rom.size())
	{
		return -1;
	}
	return ParseSysRomLevelNumber(ExtractSysRomVersionLabel(sys_rom));
}

TownsQtSysRomProfile TownsQtRomAvailability::ClassifySysRom(const QString &rom_dir)
{
	const QByteArray sys_rom=LoadRomImageFromDir(rom_dir,"FMT_SYS.ROM","FMT_SYS",kSysRomSize);
	return ClassifySysRomImage(sys_rom);
}

bool TownsQtRomAvailability::ModelGroupAllowedForSysRom(int model_index,
                                                       TownsQtSysRomProfile profile,
                                                       bool marty_ex_rom_present)
{
	if(model_index==TownsQtModelGroupMartyIndex())
	{
		return marty_ex_rom_present;
	}
	switch(profile)
	{
	case TownsQtSysRomProfile::ModernDxOnly:
	case TownsQtSysRomProfile::FreeDx:
	case TownsQtSysRomProfile::PreV2Legacy:
		return !TownsQtModelGroupRequiresSxRom(model_index);
	case TownsQtSysRomProfile::ModernUnified:
		return true;
	default:
		return false;
	}
}

int TownsQtRomAvailability::PreferredModelGroupForSysRom(TownsQtSysRomProfile profile)
{
	switch(profile)
	{
	case TownsQtSysRomProfile::ModernDxOnly:
	case TownsQtSysRomProfile::FreeDx:
	case TownsQtSysRomProfile::PreV2Legacy:
	case TownsQtSysRomProfile::ModernUnified:
	case TownsQtSysRomProfile::Missing:
	default:
		return TownsQtModelGroupDefaultIndex();
	}
}

QString TownsQtRomAvailability::SysRomSummary(const QString &rom_dir)
{
	const QByteArray sys_rom=LoadRomImageFromDir(rom_dir,"FMT_SYS.ROM","FMT_SYS",kSysRomSize);
	if(kSysRomSize!=sys_rom.size())
	{
		return QCoreApplication::translate("TownsQtRomAvailability","FMT_SYS.ROM: not found");
	}
	const QString version=ExtractSysRomVersionLabel(sys_rom);
	if(version.isEmpty())
	{
		return QCoreApplication::translate("TownsQtRomAvailability","FMT_SYS.ROM: unknown");
	}
	return QCoreApplication::translate("TownsQtRomAvailability","FMT_SYS.ROM: %1")
	    .arg(version);
}
