#include "townsqt_paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QString TownsQtPaths::configDir()
{
	return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
	    +QStringLiteral("/townsqt");
}

QString TownsQtPaths::romsDir()
{
	return configDir()+QStringLiteral("/roms");
}

QString TownsQtPaths::blankFdDir()
{
	return configDir()+QStringLiteral("/blank_fd");
}

QString TownsQtPaths::hddDir()
{
	return configDir()+QStringLiteral("/hdd");
}

QString TownsQtPaths::cmosFilePath()
{
	return configDir()+QStringLiteral("/cmos.bin");
}

QString TownsQtPaths::cmosDir()
{
	return configDir()+QStringLiteral("/cmos");
}

QString TownsQtPaths::cmosFilePathForDiscFingerprint(unsigned int fingerprintHash32)
{
	return cmosDir()+QStringLiteral("/cmos_%1.bin")
	    .arg(fingerprintHash32,8,16,QLatin1Char('0'));
}

QString TownsQtPaths::cmosFilePathForProfile(unsigned int fingerprintHash32)
{
	if(0==fingerprintHash32)
	{
		return cmosFilePath();
	}
	return cmosFilePathForDiscFingerprint(fingerprintHash32);
}

QString TownsQtPaths::configFilePath()
{
	return configDir()+QStringLiteral("/townsqt.conf");
}

QString TownsQtPaths::profilesDir()
{
	return configDir()+QStringLiteral("/profiles");
}

QString TownsQtPaths::mousePresetsDir()
{
	return configDir()+QStringLiteral("/mouse_presets");
}

QString TownsQtPaths::stateSaveDir()
{
	return configDir()+QStringLiteral("/statesave");
}

bool TownsQtPaths::ensureLayout()
{
	QDir dir;
	if(true!=dir.mkpath(configDir()))
	{
		return false;
	}
	if(true!=dir.mkpath(romsDir()))
	{
		return false;
	}
	if(true!=dir.mkpath(blankFdDir()))
	{
		return false;
	}
	if(true!=dir.mkpath(hddDir()))
	{
		return false;
	}
	const QString cmos=cmosDir();
	if(true!=dir.mkpath(cmos))
	{
		return false;
	}
	{
		// One-time migrate legacy <configDir>/cmos_XXXXXXXX.bin into cmos/.
		QDir config(configDir());
		const QFileInfoList legacyCmos=config.entryInfoList(
		    QStringList{QStringLiteral("cmos_*.bin")},
		    QDir::Files);
		for(const QFileInfo &fi : legacyCmos)
		{
			const QString dest=cmos+QStringLiteral("/")+fi.fileName();
			if(true==QFileInfo::exists(dest))
			{
				continue;
			}
			(void)QDir().rename(fi.absoluteFilePath(),dest);
		}
	}
	const QString profiles=profilesDir();
	const QString legacy=configDir()+QStringLiteral("/mouse_coord_profiles");
	if(true!=QDir(profiles).exists() && true==QDir(legacy).exists())
	{
		// One-time rename from the previous directory name.
		if(true!=QDir().rename(legacy,profiles))
		{
			if(true!=dir.mkpath(profiles))
			{
				return false;
			}
		}
	}
	else if(true!=dir.mkpath(profiles))
	{
		return false;
	}
	if(true!=dir.mkpath(mousePresetsDir()))
	{
		return false;
	}
	const QString stateSave=stateSaveDir();
	const QString legacySnapshots=configDir()+QStringLiteral("/snapshots");
	if(true!=QDir(stateSave).exists() && true==QDir(legacySnapshots).exists())
	{
		// One-time rename from the previous directory name.
		if(true!=QDir().rename(legacySnapshots,stateSave))
		{
			if(true!=dir.mkpath(stateSave))
			{
				return false;
			}
		}
	}
	else if(true!=dir.mkpath(stateSave))
	{
		return false;
	}
	{
		QDir stateDir(stateSave);
		const QFileInfoList legacyFiles=stateDir.entryInfoList(
		    QStringList{QStringLiteral("snap0_*.TState")},
		    QDir::Files);
		for(const QFileInfo &fi : legacyFiles)
		{
			const QString newName=QStringLiteral("state0_")+fi.fileName().mid(6);
			(void)QDir().rename(fi.absoluteFilePath(),stateSave+QStringLiteral("/")+newName);
		}
	}
	return true;
}
