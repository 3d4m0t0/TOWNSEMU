#include "townsqt_disc_statesave.h"

#include "townsqt_paths.h"
#include "discimg.h"

#include <QFile>

#include <cstdio>

namespace TownsQtDiscStateSave
{
unsigned int FingerprintForDiscPath(const QString &cdPath)
{
	if(cdPath.isEmpty())
	{
		return 0;
	}
	DiscImage disc;
	if(DiscImage::ERROR_NOERROR!=disc.Open(cdPath.toStdString()))
	{
		return 0;
	}
	const DiscIdentity id=disc.ComputeIdentity();
	if(true!=id.hasFingerprint)
	{
		return 0;
	}
	return id.fingerprintHash32;
}

QString PathForFingerprint(unsigned int fingerprintHash32)
{
	if(0==fingerprintHash32)
	{
		return QString();
	}
	char hex[32];
	snprintf(hex,sizeof(hex),"state0_%08x",fingerprintHash32);
	return TownsQtPaths::stateSaveDir()
	    +QStringLiteral("/")
	    +QString::fromLatin1(hex)
	    +QStringLiteral(".TState");
}

QString ProfilePathForFingerprint(unsigned int fingerprintHash32)
{
	if(0==fingerprintHash32)
	{
		return QString();
	}
	char hex[32];
	snprintf(hex,sizeof(hex),"fp_%08x",fingerprintHash32);
	return TownsQtPaths::profilesDir()
	    +QStringLiteral("/")
	    +QString::fromLatin1(hex)
	    +QStringLiteral(".ini");
}

bool ProfileExistsForFingerprint(unsigned int fingerprintHash32)
{
	const QString path=ProfilePathForFingerprint(fingerprintHash32);
	return !path.isEmpty() && QFile::exists(path);
}

bool ProfileExistsForDiscPath(const QString &cdPath)
{
	const unsigned int fp=FingerprintForDiscPath(cdPath);
	return ProfileExistsForFingerprint(fp);
}

QString StartupStateSavePathForDisc(const QString &cdPath)
{
	if(cdPath.isEmpty() || true!=ProfileExistsForDiscPath(cdPath))
	{
		return QString();
	}
	const unsigned int fp=FingerprintForDiscPath(cdPath);
	const QString statePath=PathForFingerprint(fp);
	if(statePath.isEmpty() || true!=QFile::exists(statePath))
	{
		return QString();
	}
	return statePath;
}

QString ResumeStatePathForDisc(const QString &cdPath)
{
	if(cdPath.isEmpty())
	{
		return QString();
	}
	const unsigned int fp=FingerprintForDiscPath(cdPath);
	return PathForFingerprint(fp);
}

QString ManualStateSlotPath(int slot)
{
	if(slot<1 || 9<slot)
	{
		return QString();
	}
	char name[16];
	snprintf(name,sizeof(name),"state%d",slot);
	return TownsQtPaths::stateSaveDir()
	    +QStringLiteral("/")
	    +QString::fromLatin1(name)
	    +QStringLiteral(".TState");
}
}
