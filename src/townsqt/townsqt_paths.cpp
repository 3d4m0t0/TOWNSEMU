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
