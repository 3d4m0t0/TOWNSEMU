#pragma once

#include <QString>
#include <QVector>

/*! User content-browser library (separate from fp_*.ini disc profiles). */
namespace TownsQtContentLibrary
{
struct Entry
{
	unsigned int fingerprint=0;
	QString profileFile;   // fp_XXXXXXXX.ini
	QString displayName;
	QString cdImagePath;
	QString iconPath;      // absolute or under content_icons/
	qint64 addedAtMs=0;
	qint64 lastLaunchedMs=0;
};

/*! Full library document (entries + last autosave pointer). */
struct Document
{
	QVector<Entry> entries;
	unsigned int lastAutosaveFingerprint=0;
	qint64 lastAutosaveMs=0;
};

Document LoadDocument(void);
bool SaveDocument(const Document &doc,QString *errorOut=nullptr);

QVector<Entry> Load(void);
bool Save(const QVector<Entry> &entries,QString *errorOut=nullptr);

int IndexOfFingerprint(const QVector<Entry> &entries,unsigned int fingerprint);
QString AbsoluteIconPath(const Entry &entry);
QString DefaultDisplayName(const QString &cdImagePath,unsigned int fingerprint);

/*! Copy image into content_icons/<fp>.<ext>; returns absolute dest or empty. */
QString CopyIconForFingerprint(unsigned int fingerprint,const QString &sourceImagePath,QString *errorOut=nullptr);

/*! Upsert by fingerprint. Requires existing disc profile. */
bool AddOrUpdateEntry(
    unsigned int fingerprint,
    const QString &cdImagePath,
    const QString &displayName,
    const QString &iconSourcePath,
    QString *errorOut=nullptr);

bool SetDisplayName(unsigned int fingerprint,const QString &displayName,QString *errorOut=nullptr);
/*! Copy image into content_icons and set icon_path. Empty source clears the icon. */
bool SetIcon(unsigned int fingerprint,const QString &iconSourcePath,QString *errorOut=nullptr);
bool RemoveEntry(unsigned int fingerprint,QString *errorOut=nullptr);
bool TouchLastLaunched(unsigned int fingerprint);
bool UpdateCdImagePath(unsigned int fingerprint,const QString &cdImagePath,QString *errorOut=nullptr);

/*! Record last successful auto-resume (state0_*) save for a library entry. */
bool RecordLastAutosave(unsigned int fingerprint);
unsigned int LastAutosaveFingerprint(void);
}
