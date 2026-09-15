#include "townsqt_paths.h"

#include <QDir>
#include <QFile>
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

QString TownsQtPaths::stateSaveImageDir()
{
	return stateSaveDir()+QStringLiteral("/image");
}

QString TownsQtPaths::imageDir()
{
	/*! XDG Pictures: ~/.config/user-dirs.dirs → XDG_PICTURES_DIR (locale name). */
	QString pictures=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if(pictures.isEmpty())
	{
		pictures=QDir::homePath()+QStringLiteral("/Pictures");
	}
	return pictures;
}

QString TownsQtPaths::contentLibraryFilePath()
{
	return configDir()+QStringLiteral("/content_library.json");
}

QString TownsQtPaths::contentIconsDir()
{
	return configDir()+QStringLiteral("/content_icons");
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
		const auto migratePrefix=[&](const QString &fromPrefix,const QString &toPrefix){
			const QFileInfoList files=stateDir.entryInfoList(
			    QStringList{fromPrefix+QStringLiteral("*.TState")},
			    QDir::Files);
			for(const QFileInfo &fi : files)
			{
				const QString base=fi.completeBaseName();
				if(true!=base.startsWith(fromPrefix))
				{
					continue;
				}
				const QString newName=toPrefix+base.mid(fromPrefix.size())+QStringLiteral(".TState");
				const QString dest=stateSave+QStringLiteral("/")+newName;
				if(true!=QFile::exists(dest))
				{
					(void)QDir().rename(fi.absoluteFilePath(),dest);
				}
			}
		};
		// Legacy auto-resume names → state0_* (slot 0 / auto-resume).
		migratePrefix(QStringLiteral("snap0_"),QStringLiteral("state0_"));
		migratePrefix(QStringLiteral("stateauto_"),QStringLiteral("state0_"));
	}
	if(true!=dir.mkpath(stateSaveImageDir()))
	{
		return false;
	}
	{
		QDir imgDir(stateSaveImageDir());
		const auto migrateImgPrefix=[&](const QString &fromPrefix,const QString &toPrefix){
			const QFileInfoList files=imgDir.entryInfoList(
			    QStringList{fromPrefix+QStringLiteral("*.png")},
			    QDir::Files);
			for(const QFileInfo &fi : files)
			{
				const QString base=fi.completeBaseName();
				if(true!=base.startsWith(fromPrefix))
				{
					continue;
				}
				const QString newName=toPrefix+base.mid(fromPrefix.size())+QStringLiteral(".png");
				const QString dest=stateSaveImageDir()+QStringLiteral("/")+newName;
				if(true!=QFile::exists(dest))
				{
					(void)QDir().rename(fi.absoluteFilePath(),dest);
				}
			}
		};
		migrateImgPrefix(QStringLiteral("snap0_"),QStringLiteral("state0_"));
		migrateImgPrefix(QStringLiteral("stateauto_"),QStringLiteral("state0_"));
	}
	if(true!=dir.mkpath(imageDir()))
	{
		return false;
	}
	if(true!=dir.mkpath(contentIconsDir()))
	{
		return false;
	}
	return true;
}
