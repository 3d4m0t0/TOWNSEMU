#include "townsqt_content_library.h"

#include "townsqt_disc_statesave.h"
#include "townsqt_paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
QString FingerprintHex(unsigned int fingerprint)
{
	return QStringLiteral("%1").arg(fingerprint,8,16,QLatin1Char('0'));
}

QString ProfileFileName(unsigned int fingerprint)
{
	return QStringLiteral("fp_%1.ini").arg(FingerprintHex(fingerprint));
}

TownsQtContentLibrary::Entry EntryFromJson(const QJsonObject &obj)
{
	TownsQtContentLibrary::Entry e;
	const QString fpHex=obj.value(QStringLiteral("fingerprint")).toString();
	bool ok=false;
	e.fingerprint=fpHex.toUInt(&ok,16);
	if(true!=ok)
	{
		e.fingerprint=static_cast<unsigned int>(obj.value(QStringLiteral("fingerprint")).toVariant().toULongLong());
	}
	e.profileFile=obj.value(QStringLiteral("profile")).toString();
	e.displayName=obj.value(QStringLiteral("display_name")).toString();
	e.cdImagePath=obj.value(QStringLiteral("cd_image_path")).toString();
	e.iconPath=obj.value(QStringLiteral("icon_path")).toString();
	e.addedAtMs=static_cast<qint64>(obj.value(QStringLiteral("added_at_ms")).toDouble());
	e.lastLaunchedMs=static_cast<qint64>(obj.value(QStringLiteral("last_launched_ms")).toDouble());
	if(e.profileFile.isEmpty() && 0!=e.fingerprint)
	{
		e.profileFile=ProfileFileName(e.fingerprint);
	}
	return e;
}

QJsonObject EntryToJson(const TownsQtContentLibrary::Entry &e)
{
	QJsonObject obj;
	obj.insert(QStringLiteral("fingerprint"),FingerprintHex(e.fingerprint));
	obj.insert(QStringLiteral("profile"),e.profileFile);
	obj.insert(QStringLiteral("display_name"),e.displayName);
	obj.insert(QStringLiteral("cd_image_path"),e.cdImagePath);
	obj.insert(QStringLiteral("icon_path"),e.iconPath);
	obj.insert(QStringLiteral("added_at_ms"),static_cast<double>(e.addedAtMs));
	obj.insert(QStringLiteral("last_launched_ms"),static_cast<double>(e.lastLaunchedMs));
	return obj;
}

unsigned int FingerprintFromJsonValue(const QJsonValue &v)
{
	if(true==v.isString())
	{
		bool ok=false;
		const unsigned int fp=v.toString().toUInt(&ok,16);
		return ok ? fp : 0u;
	}
	return static_cast<unsigned int>(v.toVariant().toULongLong());
}

TownsQtContentLibrary::Document g_doc;
bool g_loaded=false;
bool g_dirty=false;

TownsQtContentLibrary::Document ReadDocumentFromDisk(void)
{
	TownsQtContentLibrary::Document doc;
	const QString path=TownsQtPaths::contentLibraryFilePath();
	QFile file(path);
	if(true!=file.open(QIODevice::ReadOnly))
	{
		return doc;
	}
	const QJsonDocument json=QJsonDocument::fromJson(file.readAll());
	if(true!=json.isObject() && true!=json.isArray())
	{
		return doc;
	}
	QJsonArray arr;
	if(true==json.isArray())
	{
		arr=json.array();
	}
	else
	{
		const QJsonObject root=json.object();
		arr=root.value(QStringLiteral("entries")).toArray();
		doc.lastAutosaveFingerprint=
		    FingerprintFromJsonValue(root.value(QStringLiteral("last_autosave_fingerprint")));
		doc.lastAutosaveMs=
		    static_cast<qint64>(root.value(QStringLiteral("last_autosave_ms")).toDouble());
	}
	doc.entries.reserve(arr.size());
	for(const QJsonValue &v : arr)
	{
		if(true!=v.isObject())
		{
			continue;
		}
		TownsQtContentLibrary::Entry e=EntryFromJson(v.toObject());
		if(0==e.fingerprint)
		{
			continue;
		}
		doc.entries.push_back(e);
	}
	return doc;
}

bool WriteDocumentToDisk(const TownsQtContentLibrary::Document &doc,QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("ensureLayout failed");
		}
		return false;
	}
	QJsonArray arr;
	for(const TownsQtContentLibrary::Entry &e : doc.entries)
	{
		arr.append(EntryToJson(e));
	}
	QJsonObject root;
	root.insert(QStringLiteral("version"),1);
	root.insert(QStringLiteral("entries"),arr);
	if(0!=doc.lastAutosaveFingerprint)
	{
		root.insert(
		    QStringLiteral("last_autosave_fingerprint"),
		    FingerprintHex(doc.lastAutosaveFingerprint));
		root.insert(
		    QStringLiteral("last_autosave_ms"),
		    static_cast<double>(doc.lastAutosaveMs));
	}
	const QString path=TownsQtPaths::contentLibraryFilePath();
	QSaveFile file(path);
	if(true!=file.open(QIODevice::WriteOnly|QIODevice::Truncate))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=path;
		}
		return false;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	if(true!=file.commit())
	{
		if(nullptr!=errorOut)
		{
			*errorOut=path;
		}
		return false;
	}
	return true;
}
}

void TownsQtContentLibrary::EnsureLoaded(void)
{
	if(true==g_loaded)
	{
		return;
	}
	g_doc=ReadDocumentFromDisk();
	g_loaded=true;
	g_dirty=false;
}

bool TownsQtContentLibrary::FlushDirty(QString *errorOut)
{
	EnsureLoaded();
	if(true!=g_dirty)
	{
		return true;
	}
	if(true!=WriteDocumentToDisk(g_doc,errorOut))
	{
		return false;
	}
	g_dirty=false;
	return true;
}

TownsQtContentLibrary::Document TownsQtContentLibrary::LoadDocument(void)
{
	EnsureLoaded();
	return g_doc;
}

bool TownsQtContentLibrary::SaveDocument(const Document &doc,QString *errorOut)
{
	EnsureLoaded();
	g_doc=doc;
	if(true!=WriteDocumentToDisk(g_doc,errorOut))
	{
		return false;
	}
	g_dirty=false;
	return true;
}

QVector<TownsQtContentLibrary::Entry> TownsQtContentLibrary::Load(void)
{
	return LoadDocument().entries;
}

bool TownsQtContentLibrary::Save(const QVector<Entry> &entries,QString *errorOut)
{
	(void)errorOut;
	EnsureLoaded();
	g_doc.entries=entries;
	g_dirty=true;
	return true;
}

int TownsQtContentLibrary::IndexOfFingerprint(const QVector<Entry> &entries,unsigned int fingerprint)
{
	for(int i=0; i<entries.size(); ++i)
	{
		if(entries[i].fingerprint==fingerprint)
		{
			return i;
		}
	}
	return -1;
}

QString TownsQtContentLibrary::AbsoluteIconPath(const Entry &entry)
{
	if(entry.iconPath.isEmpty())
	{
		return QString();
	}
	const QFileInfo fi(entry.iconPath);
	if(true==fi.isAbsolute())
	{
		return fi.absoluteFilePath();
	}
	return QDir(TownsQtPaths::contentIconsDir()).filePath(entry.iconPath);
}

QString TownsQtContentLibrary::DefaultDisplayName(const QString &cdImagePath,unsigned int fingerprint)
{
	const QString base=QFileInfo(cdImagePath).completeBaseName();
	if(!base.isEmpty())
	{
		return base;
	}
	return QStringLiteral("fp_%1").arg(FingerprintHex(fingerprint));
}

QString TownsQtContentLibrary::CopyIconForFingerprint(
    unsigned int fingerprint,const QString &sourceImagePath,QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	if(0==fingerprint || sourceImagePath.isEmpty() || true!=QFile::exists(sourceImagePath))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("invalid icon source");
		}
		return {};
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("ensureLayout failed");
		}
		return {};
	}
	QString ext=QFileInfo(sourceImagePath).suffix().toLower();
	if(ext.isEmpty())
	{
		ext=QStringLiteral("png");
	}
	const QString destName=FingerprintHex(fingerprint)+QLatin1Char('.')+ext;
	const QString destPath=TownsQtPaths::contentIconsDir()+QLatin1Char('/')+destName;
	QFile::remove(destPath);
	if(true!=QFile::copy(sourceImagePath,destPath))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=destPath;
		}
		return {};
	}
	return destPath;
}

bool TownsQtContentLibrary::AddOrUpdateEntry(
    unsigned int fingerprint,
    const QString &cdImagePath,
    const QString &displayName,
    const QString &iconSourcePath,
    QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	if(0==fingerprint || cdImagePath.isEmpty())
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("missing fingerprint or CD path");
		}
		return false;
	}
	if(true!=TownsQtDiscStateSave::ProfileExistsForFingerprint(fingerprint))
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("disc profile missing");
		}
		return false;
	}
	QString iconDest;
	if(!iconSourcePath.isEmpty())
	{
		iconDest=CopyIconForFingerprint(fingerprint,iconSourcePath,errorOut);
		if(iconDest.isEmpty())
		{
			return false;
		}
	}
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	const bool isNew=(0>idx);
	Entry e;
	if(true!=isNew)
	{
		e=g_doc.entries[idx];
	}
	else
	{
		e.fingerprint=fingerprint;
		e.addedAtMs=QDateTime::currentMSecsSinceEpoch();
	}
	e.profileFile=ProfileFileName(fingerprint);
	e.cdImagePath=cdImagePath;
	e.displayName=displayName.trimmed().isEmpty() ?
	    DefaultDisplayName(cdImagePath,fingerprint) : displayName.trimmed();
	if(!iconDest.isEmpty())
	{
		e.iconPath=QFileInfo(iconDest).fileName();
	}
	if(true!=isNew)
	{
		g_doc.entries[idx]=e;
		g_dirty=true;
		return true;
	}
	g_doc.entries.push_back(e);
	/*! New registration: persist immediately so a crash before exit still keeps the entry. */
	if(true!=WriteDocumentToDisk(g_doc,errorOut))
	{
		g_doc.entries.removeLast();
		return false;
	}
	g_dirty=false;
	return true;
}

bool TownsQtContentLibrary::SetDisplayName(unsigned int fingerprint,const QString &displayName,QString *errorOut)
{
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	g_doc.entries[idx].displayName=displayName.trimmed();
	if(g_doc.entries[idx].displayName.isEmpty())
	{
		g_doc.entries[idx].displayName=
		    DefaultDisplayName(g_doc.entries[idx].cdImagePath,fingerprint);
	}
	g_dirty=true;
	return true;
}

bool TownsQtContentLibrary::SetIcon(
    unsigned int fingerprint,const QString &iconSourcePath,QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	if(iconSourcePath.isEmpty())
	{
		g_doc.entries[idx].iconPath.clear();
		g_dirty=true;
		return true;
	}
	const QString iconDest=CopyIconForFingerprint(fingerprint,iconSourcePath,errorOut);
	if(iconDest.isEmpty())
	{
		return false;
	}
	g_doc.entries[idx].iconPath=QFileInfo(iconDest).fileName();
	g_dirty=true;
	return true;
}

bool TownsQtContentLibrary::RemoveEntry(unsigned int fingerprint,QString *errorOut)
{
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	g_doc.entries.removeAt(idx);
	if(g_doc.lastAutosaveFingerprint==fingerprint)
	{
		g_doc.lastAutosaveFingerprint=0;
		g_doc.lastAutosaveMs=0;
	}
	g_dirty=true;
	return true;
}

bool TownsQtContentLibrary::TouchLastLaunched(unsigned int fingerprint)
{
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	if(0>idx)
	{
		return false;
	}
	g_doc.entries[idx].lastLaunchedMs=QDateTime::currentMSecsSinceEpoch();
	g_dirty=true;
	return true;
}

bool TownsQtContentLibrary::UpdateCdImagePath(
    unsigned int fingerprint,const QString &cdImagePath,QString *errorOut)
{
	EnsureLoaded();
	const int idx=IndexOfFingerprint(g_doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	g_doc.entries[idx].cdImagePath=cdImagePath;
	g_dirty=true;
	return true;
}

bool TownsQtContentLibrary::RecordLastAutosave(unsigned int fingerprint)
{
	if(0==fingerprint)
	{
		return false;
	}
	EnsureLoaded();
	if(0>IndexOfFingerprint(g_doc.entries,fingerprint))
	{
		return false;
	}
	g_doc.lastAutosaveFingerprint=fingerprint;
	g_doc.lastAutosaveMs=QDateTime::currentMSecsSinceEpoch();
	g_dirty=true;
	return true;
}

unsigned int TownsQtContentLibrary::LastAutosaveFingerprint(void)
{
	EnsureLoaded();
	return g_doc.lastAutosaveFingerprint;
}
