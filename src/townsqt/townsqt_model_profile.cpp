#include "townsqt_model_profile.h"

#include <algorithm>

namespace
{
const char *kBulletsGen1[]={
    "80386DX（マシンID: MODEL2 / 2F / 20F）",
    "標準メモリマップ（386DX）",
    "RAM 最大 64 MB",
    "FAST MODE 非対応",
    "高解像度 CRTC 非対応",
    nullptr,
};
const char *kBulletsUx[]={
    "80386SX",
    "386SX 用メモリマップ（ROM/VRAM の配置が異なる）",
    "RAM 最大 10 MB",
    "FAST MODE 非対応",
    nullptr,
};
const char *kBulletsUg[]={
    "80386SX（UG 世代）",
    "386SX 用メモリマップ",
    "RAM 最大 10 MB",
    "FAST MODE 対応",
    "UG 以降の SCSI / CPU 周辺 I/O 挙動",
    nullptr,
};
const char *kBulletsCx[]={
    "80386DX（CX）",
    "標準メモリマップ（386DX）",
    "RAM 最大 64 MB",
    "FAST MODE 対応",
    "UG 世代 SCSI 拡張は非対応",
    nullptr,
};
const char *kBulletsHg[]={
    "80386DX（HG）",
    "標準メモリマップ（386DX）",
    "RAM 最大 64 MB",
    "FAST MODE 対応",
    "UG 以降 SCSI / CPU 周辺 I/O 挙動",
    nullptr,
};
const char *kBulletsHr[]={
    "80486SX",
    "標準メモリマップ（386DX 系）",
    "RAM 最大 64 MB",
    "FAST MODE / UG SCSI 対応",
    "高解像度 CRTC 非対応（MX 未満）",
    nullptr,
};
const char *kBulletsUrMa[]={
    "80486DX（UR / MA）",
    "標準メモリマップ",
    "RAM 最大 64 MB",
    "FAST MODE / UG SCSI 対応",
    "高解像度 CRTC 非対応（MX 未満）",
    nullptr,
};
const char *kBulletsMxGen[]={
    "80486DX（MX / ME / MF）",
    "高解像度 CRTC / 画像出力レジスタ対応",
    "CD-ROM 2 倍速対応",
    "RAM 最大 64 MB",
    nullptr,
};
const char *kBulletsHc[]={
    "Pentium（HC、マシンID 報告）",
    "MX 世代と同様の高解像度・CD 機能",
    "RAM 最大 64 MB",
    nullptr,
};
const char *kBulletsMarty[]={
    "FM Towns Marty",
    "80386SX + Marty 専用 ROM マッピング",
    "SCSI 非搭載（I/O 無効）",
    "EX-ROM イメージが必要",
    "RAM は OS ROM 手前まで",
    nullptr,
};

const TownsQtModelGroup kModelGroups[]={
    {"gen1", "初代 386DX（MODEL2 / 2F / 20F）", TOWNSTYPE_1F_2F, kBulletsGen1, 64, false, false, false},
    {"ux", "386SX（UX）", TOWNSTYPE_2_UX, kBulletsUx, 10, false, false, true},
    {"ug", "386SX（UG）", TOWNSTYPE_2_UG, kBulletsUg, 10, true, false, true},
    {"cx", "386DX（CX）", TOWNSTYPE_2_CX, kBulletsCx, 64, true, false, false},
    {"hg", "386DX（HG）", TOWNSTYPE_2_HG, kBulletsHg, 64, true, false, false},
    {"hr", "486SX（HR）", TOWNSTYPE_2_HR, kBulletsHr, 64, true, false, false},
    {"ur_ma", "486DX（UR / MA）", TOWNSTYPE_2_UR, kBulletsUrMa, 64, true, false, false},
    {"mx_gen", "486DX 高解像度（MX / ME / MF）", TOWNSTYPE_2_MX, kBulletsMxGen, 64, true, true, false},
    {"hc", "Pentium（HC）", TOWNSTYPE_2_HC, kBulletsHc, 64, true, true, false},
    {"marty", "Marty", TOWNSTYPE_MARTY, kBulletsMarty, 6, false, false, false},
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
		    +QString::fromUtf8(bullets[i]).toHtmlEscaped();
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
	    "486DX 高解像度（MX / ME / MF）",
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
