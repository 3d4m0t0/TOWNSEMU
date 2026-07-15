#include "townsqt_model_profile.h"

#include <QCoreApplication>

#include <algorithm>

namespace
{
QString Tr(const char *text)
{
	return QCoreApplication::translate("TownsQtModelProfile",text);
}

const char *kBulletsGen1[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386DX (machine ID: MODEL2 / 2F / 20F)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Standard memory map (386DX)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE not supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","High-resolution CRTC not supported"),
    nullptr,
};
const char *kBulletsUx[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386SX"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","386SX memory map (different ROM/VRAM layout)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 10 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE not supported"),
    nullptr,
};
const char *kBulletsUg[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386SX (UG generation)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","386SX memory map"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 10 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","UG and later SCSI / CPU peripheral I/O behavior"),
    nullptr,
};
const char *kBulletsCx[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386DX (CX)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Standard memory map (386DX)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","UG-generation SCSI extensions not supported"),
    nullptr,
};
const char *kBulletsHg[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386DX (HG)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Standard memory map (386DX)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","UG and later SCSI / CPU peripheral I/O behavior"),
    nullptr,
};
const char *kBulletsHr[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80486SX"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Standard memory map (386DX family)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE / UG SCSI supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","High-resolution CRTC not supported (pre-MX)"),
    nullptr,
};
const char *kBulletsUrMa[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80486DX (UR / MA)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Standard memory map"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FAST MODE / UG SCSI supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","High-resolution CRTC not supported (pre-MX)"),
    nullptr,
};
const char *kBulletsMxGen[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80486DX (MX / ME / MF)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","High-resolution CRTC / display output registers"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","CD-ROM 2x speed supported"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    nullptr,
};
const char *kBulletsHc[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Pentium (HC, machine ID report)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","Same high-resolution and CD features as MX generation"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM up to 64 MB"),
    nullptr,
};
const char *kBulletsMarty[]={
    QT_TRANSLATE_NOOP("TownsQtModelProfile","FM Towns Marty"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","80386SX + Marty-specific ROM mapping"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","No SCSI (I/O disabled)"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","EX-ROM image required"),
    QT_TRANSLATE_NOOP("TownsQtModelProfile","RAM limited up to before the OS ROM"),
    nullptr,
};

const TownsQtModelGroup kModelGroups[]={
    {"gen1",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","1st gen 386DX (MODEL2 / 2F / 20F)"),
     TOWNSTYPE_1F_2F,kBulletsGen1,64,false,false,false},
    {"ux",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","386SX (UX)"),
     TOWNSTYPE_2_UX,kBulletsUx,10,false,false,true},
    {"ug",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","386SX (UG)"),
     TOWNSTYPE_2_UG,kBulletsUg,10,true,false,true},
    {"cx",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","386DX (CX)"),
     TOWNSTYPE_2_CX,kBulletsCx,64,true,false,false},
    {"hg",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","386DX (HG)"),
     TOWNSTYPE_2_HG,kBulletsHg,64,true,false,false},
    {"hr",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","486SX (HR)"),
     TOWNSTYPE_2_HR,kBulletsHr,64,true,false,false},
    {"ur_ma",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","486DX (UR / MA)"),
     TOWNSTYPE_2_UR,kBulletsUrMa,64,true,false,false},
    {"mx_gen",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","486DX high-res (MX / ME / MF)"),
     TOWNSTYPE_2_MX,kBulletsMxGen,64,true,true,false},
    {"hc",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","Pentium (HC)"),
     TOWNSTYPE_2_HC,kBulletsHc,64,true,true,false},
    {"marty",
     QT_TRANSLATE_NOOP("TownsQtModelProfile","Marty"),
     TOWNSTYPE_MARTY,kBulletsMarty,6,false,false,false},
};

constexpr int kDefaultModelGroupIndex=7;

QString BulletsToHtml(const char *const *bullets)
{
	QString html;
	for(int i=0; nullptr!=bullets[i]; ++i)
	{
		if(0<i)
		{
			html+=QStringLiteral("<br>");
		}
		html+=QStringLiteral("&#183; ")
		    +Tr(bullets[i]).toHtmlEscaped();
	}
	return html;
}
}

int TownsQtModelGroupCount()
{
	return static_cast<int>(sizeof(kModelGroups)/sizeof(kModelGroups[0]));
}

const TownsQtModelGroup &TownsQtModelGroupAt(int index)
{
	static const TownsQtModelGroup kFallback={
	    "mx_gen",
	    QT_TRANSLATE_NOOP("TownsQtModelProfile","486DX high-res (MX / ME / MF)"),
	    TOWNSTYPE_2_MX,
	    kBulletsMxGen,
	    64,
	    true,
	    true,
	    false,
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

	switch(towns_type)
	{
	case TOWNSTYPE_MODEL1_2:
	case TOWNSTYPE_10F_20F:
		return 0;
	case TOWNSTYPE_2_UR:
	case TOWNSTYPE_2_MA:
		return 6;
	case TOWNSTYPE_2_ME:
	case TOWNSTYPE_2_MF_FRESH:
		return 7;
	default:
		break;
	}

	return kDefaultModelGroupIndex;
}

unsigned int TownsQtModelGroupTownsType(int index)
{
	return TownsQtModelGroupAt(index).towns_type;
}

QString TownsQtModelGroupLabel(int index)
{
	return Tr(TownsQtModelGroupAt(index).label);
}

QString TownsQtModelGroupDescription(int index)
{
	return BulletsToHtml(TownsQtModelGroupAt(index).hardware_bullets);
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
