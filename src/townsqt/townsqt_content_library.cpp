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
}

TownsQtContentLibrary::Document TownsQtContentLibrary::LoadDocument(void)
{
	Document doc;
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
		Entry e=EntryFromJson(v.toObject());
		if(0==e.fingerprint)
		{
			continue;
		}
		doc.entries.push_back(e);
	}
	return doc;
}

bool TownsQtContentLibrary::SaveDocument(const Document &doc,QString *errorOut)
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
	for(const Entry &e : doc.entries)
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

QVector<TownsQtContentLibrary::Entry> TownsQtContentLibrary::Load(void)
{
	return LoadDocument().entries;
}

bool TownsQtContentLibrary::Save(const QVector<Entry> &entries,QString *errorOut)
{
	Document doc=LoadDocument();
	doc.entries=entries;
	return SaveDocument(doc,errorOut);
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
	QVector<Entry> entries=Load();
	const int idx=IndexOfFingerprint(entries,fingerprint);
	Entry e;
	if(0<=idx)
	{
		e=entries[idx];
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
	if(0<=idx)
	{
		entries[idx]=e;
	}
	else
	{
		entries.push_back(e);
	}
	return Save(entries,errorOut);
}

bool TownsQtContentLibrary::SetDisplayName(unsigned int fingerprint,const QString &displayName,QString *errorOut)
{
	QVector<Entry> entries=Load();
	const int idx=IndexOfFingerprint(entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	entries[idx].displayName=displayName.trimmed();
	if(entries[idx].displayName.isEmpty())
	{
		entries[idx].displayName=
		    DefaultDisplayName(entries[idx].cdImagePath,fingerprint);
	}
	return Save(entries,errorOut);
}

bool TownsQtContentLibrary::SetIcon(
    unsigned int fingerprint,const QString &iconSourcePath,QString *errorOut)
{
	if(nullptr!=errorOut)
	{
		errorOut->clear();
	}
	QVector<Entry> entries=Load();
	const int idx=IndexOfFingerprint(entries,fingerprint);
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
		entries[idx].iconPath.clear();
		return Save(entries,errorOut);
	}
	const QString iconDest=CopyIconForFingerprint(fingerprint,iconSourcePath,errorOut);
	if(iconDest.isEmpty())
	{
		return false;
	}
	entries[idx].iconPath=QFileInfo(iconDest).fileName();
	return Save(entries,errorOut);
}

bool TownsQtContentLibrary::RemoveEntry(unsigned int fingerprint,QString *errorOut)
{
	Document doc=LoadDocument();
	const int idx=IndexOfFingerprint(doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	doc.entries.removeAt(idx);
	if(doc.lastAutosaveFingerprint==fingerprint)
	{
		doc.lastAutosaveFingerprint=0;
		doc.lastAutosaveMs=0;
	}
	return SaveDocument(doc,errorOut);
}

bool TownsQtContentLibrary::TouchLastLaunched(unsigned int fingerprint)
{
	QVector<Entry> entries=Load();
	const int idx=IndexOfFingerprint(entries,fingerprint);
	if(0>idx)
	{
		return false;
	}
	entries[idx].lastLaunchedMs=QDateTime::currentMSecsSinceEpoch();
	return Save(entries,nullptr);
}

bool TownsQtContentLibrary::UpdateCdImagePath(
    unsigned int fingerprint,const QString &cdImagePath,QString *errorOut)
{
	Document doc=LoadDocument();
	const int idx=IndexOfFingerprint(doc.entries,fingerprint);
	if(0>idx)
	{
		if(nullptr!=errorOut)
		{
			*errorOut=QStringLiteral("entry not found");
		}
		return false;
	}
	doc.entries[idx].cdImagePath=cdImagePath;
	return SaveDocument(doc,errorOut);
}

bool TownsQtContentLibrary::RecordLastAutosave(unsigned int fingerprint)
{
	if(0==fingerprint)
	{
		return false;
	}
	Document doc=LoadDocument();
	if(0>IndexOfFingerprint(doc.entries,fingerprint))
	{
		return false;
	}
	doc.lastAutosaveFingerprint=fingerprint;
	doc.lastAutosaveMs=QDateTime::currentMSecsSinceEpoch();
	return SaveDocument(doc,nullptr);
}

unsigned int TownsQtContentLibrary::LastAutosaveFingerprint(void)
{
	return LoadDocument().lastAutosaveFingerprint;
}
