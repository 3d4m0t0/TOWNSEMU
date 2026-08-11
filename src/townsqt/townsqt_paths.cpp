#include "townsqt_paths.h"

#include <QDir>
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
	return true;
}
