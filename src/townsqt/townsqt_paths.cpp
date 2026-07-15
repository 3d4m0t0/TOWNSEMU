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
	return true;
}
