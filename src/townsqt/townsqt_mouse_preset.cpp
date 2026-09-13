#include "townsqt_mouse_preset.h"

#include "mouse_coord_write_scan.h"
#include "townsqt_paths.h"

#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>

namespace
{
QString FileNameForFingerprint(unsigned int fingerprintHash32)
{
	return QStringLiteral("mouse_%1.ini")
	    .arg(fingerprintHash32,8,16,QLatin1Char('0'));
}

QString UserPresetDir(void)
{
	return TownsQtPaths::mousePresetsDir();
}

QString UserPresetPath(unsigned int fingerprintHash32)
{
	return UserPresetDir()+QLatin1Char('/')+FileNameForFingerprint(fingerprintHash32);
}

QStringList SystemSearchDirs(void)
{
	QStringList dirs;
	const QStringList dataDirs=
	    QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
	for(const QString &dataDir : dataDirs)
	{
		const QString path=dataDir+QStringLiteral("/townsqt/mouse_presets");
		if(!dirs.contains(path))
		{
			dirs << path;
		}
	}
	const QString appDir=QCoreApplication::applicationDirPath();
	const QString relShare=
	    QDir::cleanPath(QDir(appDir).filePath(QStringLiteral("../share/townsqt/mouse_presets")));
	const QString local=QDir::cleanPath(QDir(appDir).filePath(QStringLiteral("mouse_presets")));
	for(const QString &path : {relShare,local})
	{
		if(!dirs.contains(path))
		{
			dirs << path;
		}
	}
	return dirs;
}

bool ReadUtf8File(const QString &path,QByteArray &out)
{
	QFile file(path);
	if(true!=file.open(QIODevice::ReadOnly))
	{
		return false;
	}
	out=file.readAll();
	return true!=out.isEmpty();
}

/*! Map preset text onto [mouse_coord] for Profile::FromIniString.
    Accepts [mouse_preset], [mouse_coord], or keys with no section.
    Drops [machine] and any other sections. */
std::string NormalizePresetIni(const QByteArray &bytes)
{
	const QString text=QString::fromUtf8(bytes);
	QStringList kept;
	bool inMouse=false;
	bool seenSection=false;
	for(QString line : text.split(QLatin1Char('\n')))
	{
		if(true==line.endsWith(QLatin1Char('\r')))
		{
			line.chop(1);
		}
		const QString trimmed=line.trimmed();
		if(true==trimmed.isEmpty() ||
		   true==trimmed.startsWith(QLatin1Char('#')) ||
		   true==trimmed.startsWith(QLatin1Char(';')))
		{
			continue;
		}
		if(true==trimmed.startsWith(QLatin1Char('[')))
		{
			seenSection=true;
			inMouse=(QStringLiteral("[mouse_preset]")==trimmed ||
			         QStringLiteral("[mouse_coord]")==trimmed);
			continue;
		}
		if(true!=seenSection || true==inMouse)
		{
			kept << line;
		}
	}
	if(true==kept.isEmpty())
	{
		return {};
	}
	std::string out="[mouse_coord]\n";
	for(const QString &line : kept)
	{
		out += line.toStdString();
		out += '\n';
	}
	return out;
}

QVariantMap ProfileToMouseMap(const MouseCoordWriteScan::Profile &p)
{
	QVariantMap out;
	out.insert(QStringLiteral("prof_integration_mode"),p.integrationMode);
	out.insert(QStringLiteral("prof_offset_x"),p.offsetX);
	out.insert(QStringLiteral("prof_offset_y"),p.offsetY);
	out.insert(QStringLiteral("prof_invert_x"),p.invertX);
	out.insert(QStringLiteral("prof_invert_y"),p.invertY);
	out.insert(QStringLiteral("prof_wait_feedback"),p.waitFeedback);
	out.insert(QStringLiteral("prof_stop_soft_write"),p.stopSoftWrite);
	out.insert(QStringLiteral("prof_app_exec_name"),
	           QString::fromStdString(p.appExecName));
	out.insert(QStringLiteral("prof_app_exec_hash"),
	           static_cast<uint>(p.appExecHash32));
	for(unsigned int i=0; i<MouseCoordWriteScan::MAX_COORD_PAIRS; ++i)
	{
		const auto &pr=p.pair[i];
		const QString base=QStringLiteral("prof_pair%1").arg(i);
		out.insert(base+QStringLiteral("_x"),static_cast<uint>(pr.physX));
		out.insert(base+QStringLiteral("_y"),static_cast<uint>(pr.physY));
		if(true==pr.hasDsOff)
		{
			out.insert(base+QStringLiteral("_ds_off_x"),static_cast<uint>(pr.dsOffX));
			out.insert(base+QStringLiteral("_ds_off_y"),static_cast<uint>(pr.dsOffY));
			out.insert(base+QStringLiteral("_ds_sel"),static_cast<uint>(pr.dsSelector));
			out.insert(base+QStringLiteral("_has_ds_off"),true);
		}
		out.insert(base+QStringLiteral("_scale_x"),pr.scaleX);
		out.insert(base+QStringLiteral("_scale_y"),pr.scaleY);
		if(true==pr.hasRangeX)
		{
			out.insert(base+QStringLiteral("_min_x"),pr.rangeMinX);
			out.insert(base+QStringLiteral("_max_x"),pr.rangeMaxX);
		}
		if(true==pr.hasRangeY)
		{
			out.insert(base+QStringLiteral("_min_y"),pr.rangeMinY);
			out.insert(base+QStringLiteral("_max_y"),pr.rangeMaxY);
		}
	}
	return out;
}

bool ParsePresetBytes(const QByteArray &bytes,unsigned int fingerprintHash32,QVariantMap &out)
{
	const std::string normalized=NormalizePresetIni(bytes);
	if(true==normalized.empty())
	{
		return false;
	}
	MouseCoordWriteScan::Profile p;
	if(true!=MouseCoordWriteScan::Profile::FromIniString(normalized,p))
	{
		return false;
	}
	if(0!=p.discFingerprintHash32 && p.discFingerprintHash32!=fingerprintHash32)
	{
		return false;
	}
	if(true!=p.HasMouseIntegration() && 0==p.NumPairs() && true!=p.HasAppExecBind())
	{
		return false;
	}
	out=ProfileToMouseMap(p);
	return true;
}

QString HexU32(unsigned int v)
{
	return QStringLiteral("0x%1").arg(v,8,16,QLatin1Char('0'));
}

QString PresetIniFromProfile(unsigned int fingerprintHash32,const QVariantMap &profile)
{
	auto u32=[&](const char *key)->unsigned int
	{
		return profile.value(QString::fromLatin1(key)).toUInt();
	};
	auto i32=[&](const char *key,int fallback=0)->int
	{
		return profile.value(QString::fromLatin1(key),fallback).toInt();
	};
	auto flag=[&](const char *key,bool fallback=false)->int
	{
		return profile.value(QString::fromLatin1(key),fallback).toBool() ? 1 : 0;
	};

	QStringList lines;
	lines << QStringLiteral("[mouse_preset]");
	lines << QStringLiteral("disc_fingerprint_hash=%1").arg(HexU32(fingerprintHash32));
	lines << QStringLiteral("integration_mode=%1")
	             .arg(i32("prof_integration_mode",
	                      MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE));
	lines << QStringLiteral("pair0_x=%1").arg(HexU32(u32("prof_pair0_x")));
	lines << QStringLiteral("pair0_y=%1").arg(HexU32(u32("prof_pair0_y")));
	if(true==profile.value(QStringLiteral("prof_pair0_has_ds_off")).toBool() ||
	   true==profile.contains(QStringLiteral("prof_pair0_ds_off_x")))
	{
		lines << QStringLiteral("pair0_ds_off_x=%1")
		             .arg(HexU32(u32("prof_pair0_ds_off_x")));
		lines << QStringLiteral("pair0_ds_off_y=%1")
		             .arg(HexU32(u32("prof_pair0_ds_off_y")));
		const unsigned int sel=u32("prof_pair0_ds_sel");
		if(0!=sel)
		{
			lines << QStringLiteral("pair0_ds_sel=%1").arg(HexU32(sel));
		}
	}
	lines << QStringLiteral("pair0_min_x=%1").arg(i32("prof_pair0_min_x"));
	lines << QStringLiteral("pair0_max_x=%1").arg(i32("prof_pair0_max_x"));
	lines << QStringLiteral("pair0_min_y=%1").arg(i32("prof_pair0_min_y"));
	lines << QStringLiteral("pair0_max_y=%1").arg(i32("prof_pair0_max_y"));
	lines << QStringLiteral("pair0_scale_x=%1").arg(i32("prof_pair0_scale_x",1));
	lines << QStringLiteral("pair0_scale_y=%1").arg(i32("prof_pair0_scale_y",1));
	lines << QStringLiteral("offset_x=%1").arg(i32("prof_offset_x"));
	lines << QStringLiteral("offset_y=%1").arg(i32("prof_offset_y"));
	lines << QStringLiteral("invert_x=%1").arg(flag("prof_invert_x"));
	lines << QStringLiteral("invert_y=%1").arg(flag("prof_invert_y"));
	lines << QStringLiteral("wait_feedback=%1").arg(flag("prof_wait_feedback",true));
	lines << QStringLiteral("stop_soft_write=%1").arg(flag("prof_stop_soft_write"));
	const unsigned int pair1x=u32("prof_pair1_x");
	const unsigned int pair1y=u32("prof_pair1_y");
	const bool pair1Ds=
	    true==profile.value(QStringLiteral("prof_pair1_has_ds_off")).toBool() ||
	    true==profile.contains(QStringLiteral("prof_pair1_ds_off_x"));
	if((0!=pair1x && 0!=pair1y) || true==pair1Ds)
	{
		lines << QStringLiteral("pair1_x=%1").arg(HexU32(pair1x));
		lines << QStringLiteral("pair1_y=%1").arg(HexU32(pair1y));
		if(true==pair1Ds)
		{
			lines << QStringLiteral("pair1_ds_off_x=%1")
			             .arg(HexU32(u32("prof_pair1_ds_off_x")));
			lines << QStringLiteral("pair1_ds_off_y=%1")
			             .arg(HexU32(u32("prof_pair1_ds_off_y")));
			const unsigned int sel=u32("prof_pair1_ds_sel");
			if(0!=sel)
			{
				lines << QStringLiteral("pair1_ds_sel=%1").arg(HexU32(sel));
			}
		}
		lines << QStringLiteral("pair1_scale_x=%1").arg(i32("prof_pair1_scale_x",1));
		lines << QStringLiteral("pair1_scale_y=%1").arg(i32("prof_pair1_scale_y",1));
	}
	const QString appName=
	    profile.value(QStringLiteral("prof_app_exec_name")).toString().trimmed();
	if(true!=appName.isEmpty())
	{
		lines << QStringLiteral("app_exec_name=%1").arg(appName);
		lines << QStringLiteral("app_exec_hash=%1")
		             .arg(HexU32(u32("prof_app_exec_hash")));
	}
	return lines.join(QLatin1Char('\n'))+QLatin1Char('\n');
}
}

bool TownsQtMousePreset::ExistsUser(unsigned int fingerprintHash32)
{
	if(0==fingerprintHash32)
	{
		return false;
	}
	return QFile::exists(UserPresetPath(fingerprintHash32));
}

bool TownsQtMousePreset::ExistsSystem(unsigned int fingerprintHash32)
{
	if(0==fingerprintHash32)
	{
		return false;
	}
	const QString name=FileNameForFingerprint(fingerprintHash32);
	for(const QString &dir : SystemSearchDirs())
	{
		if(true==QFile::exists(dir+QLatin1Char('/')+name))
		{
			return true;
		}
	}
	return QFile::exists(QStringLiteral(":/mouse_presets/")+name);
}

bool TownsQtMousePreset::Exists(unsigned int fingerprintHash32)
{
	return true==ExistsUser(fingerprintHash32) || true==ExistsSystem(fingerprintHash32);
}

bool TownsQtMousePreset::Save(
    unsigned int fingerprintHash32,const QVariantMap &profile,QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	if(0==fingerprintHash32)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("missing CD fingerprint");
		}
		return false;
	}
	const QString dir=UserPresetDir();
	if(true!=QDir().mkpath(dir))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=dir;
		}
		return false;
	}
	const QString path=UserPresetPath(fingerprintHash32);
	QFile file(path);
	if(true!=file.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=path;
		}
		return false;
	}
	const QByteArray bytes=PresetIniFromProfile(fingerprintHash32,profile).toUtf8();
	if(bytes.size()!=file.write(bytes))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=path;
		}
		return false;
	}
	return true;
}

bool TownsQtMousePreset::LoadUser(unsigned int fingerprintHash32,QVariantMap &out)
{
	out.clear();
	if(0==fingerprintHash32)
	{
		return false;
	}
	QByteArray bytes;
	return true==ReadUtf8File(UserPresetPath(fingerprintHash32),bytes) &&
	       true==ParsePresetBytes(bytes,fingerprintHash32,out);
}

bool TownsQtMousePreset::LoadSystem(unsigned int fingerprintHash32,QVariantMap &out)
{
	out.clear();
	if(0==fingerprintHash32)
	{
		return false;
	}
	const QString name=FileNameForFingerprint(fingerprintHash32);
	QByteArray bytes;
	for(const QString &dir : SystemSearchDirs())
	{
		if(true==ReadUtf8File(dir+QLatin1Char('/')+name,bytes) &&
		   true==ParsePresetBytes(bytes,fingerprintHash32,out))
		{
			return true;
		}
	}
	return true==ReadUtf8File(QStringLiteral(":/mouse_presets/")+name,bytes) &&
	       true==ParsePresetBytes(bytes,fingerprintHash32,out);
}

bool TownsQtMousePreset::Load(unsigned int fingerprintHash32,QVariantMap &out)
{
	if(true==LoadUser(fingerprintHash32,out))
	{
		return true;
	}
	return LoadSystem(fingerprintHash32,out);
}
