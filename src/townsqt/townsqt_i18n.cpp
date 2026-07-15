#include "townsqt_i18n.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

#include <cstdio>

namespace
{

const char *const kEmbeddedLocales[]={"en","ja"};

bool IsEmbeddedLocale(const QString &locale)
{
	for(const char *embedded : kEmbeddedLocales)
	{
		if(locale==QLatin1String(embedded))
		{
			return true;
		}
	}
	return false;
}

QString NormalizeLocaleTag(QString tag)
{
	tag=tag.trimmed().replace(QLatin1Char('-'),QLatin1Char('_'));
	if(0==tag.compare(QStringLiteral("jp"),Qt::CaseInsensitive))
	{
		return QStringLiteral("ja");
	}
	if(0==tag.compare(QStringLiteral("zh"),Qt::CaseInsensitive) ||
	   tag.startsWith(QStringLiteral("zh_hans"),Qt::CaseInsensitive) ||
	   tag.startsWith(QStringLiteral("zh_cn"),Qt::CaseInsensitive))
	{
		return QStringLiteral("zh_CN");
	}
	if(tag.startsWith(QStringLiteral("zh_tw"),Qt::CaseInsensitive) ||
	   tag.startsWith(QStringLiteral("zh_hk"),Qt::CaseInsensitive) ||
	   tag.startsWith(QStringLiteral("zh_mo"),Qt::CaseInsensitive) ||
	   tag.startsWith(QStringLiteral("zh_hant"),Qt::CaseInsensitive))
	{
		return QStringLiteral("zh_TW");
	}
	if(0==tag.compare(QStringLiteral("C"),Qt::CaseInsensitive) ||
	   0==tag.compare(QStringLiteral("POSIX"),Qt::CaseInsensitive))
	{
		return QStringLiteral("en");
	}
	return tag;
}

QString ResolveUiLocaleTag(void)
{
	const QByteArray forced=qgetenv("TOWNSQT_LANG");
	if(!forced.isEmpty())
	{
		return NormalizeLocaleTag(QString::fromUtf8(forced));
	}

	const QLocale locale=QLocale::system();
	switch(locale.language())
	{
	case QLocale::Japanese:
		return QStringLiteral("ja");
	case QLocale::English:
		return QStringLiteral("en");
	case QLocale::Korean:
		return QStringLiteral("ko");
	case QLocale::German:
		return QStringLiteral("de");
	case QLocale::French:
		return QStringLiteral("fr");
	case QLocale::Spanish:
		return QStringLiteral("es");
	case QLocale::Chinese:
		{
			const QString name=locale.name();
			if(name.startsWith(QStringLiteral("zh_TW"),Qt::CaseInsensitive) ||
			   name.startsWith(QStringLiteral("zh_HK"),Qt::CaseInsensitive) ||
			   name.startsWith(QStringLiteral("zh_MO"),Qt::CaseInsensitive) ||
			   name.startsWith(QStringLiteral("zh_Hant"),Qt::CaseInsensitive))
			{
				return QStringLiteral("zh_TW");
			}
			if(name.startsWith(QStringLiteral("zh_CN"),Qt::CaseInsensitive) ||
			   name.startsWith(QStringLiteral("zh_Hans"),Qt::CaseInsensitive))
			{
				return QStringLiteral("zh_CN");
			}
			break;
		}
	default:
		break;
	}
	return NormalizeLocaleTag(locale.name());
}

QStringList LocaleCandidates(const QString &locale_tag)
{
	QStringList candidates;
	const QString normalized=NormalizeLocaleTag(locale_tag);
	if(!normalized.isEmpty())
	{
		candidates << normalized;
	}
	const int sep=normalized.indexOf(QLatin1Char('_'));
	if(sep>0)
	{
		const QString language=normalized.left(sep);
		if(!candidates.contains(language))
		{
			candidates << language;
		}
	}
	return candidates;
}

QStringList TranslationSearchPaths(void)
{
	QStringList paths;

	const QByteArray env_dir=qgetenv("TOWNSQT_TRANSLATIONS_DIR");
	if(!env_dir.isEmpty())
	{
		paths << QDir::cleanPath(QString::fromLocal8Bit(env_dir));
	}

	const QStringList data_dirs=
	    QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
	for(const QString &data_dir : data_dirs)
	{
		const QString path=data_dir+QStringLiteral("/townsqt/translations");
		if(!paths.contains(path))
		{
			paths << path;
		}
	}

	const QString app_dir=QCoreApplication::applicationDirPath();
	const QString rel_share=
	    QDir(app_dir).filePath(QStringLiteral("../share/townsqt/translations"));
	const QString local_dir=QDir(app_dir).filePath(QStringLiteral("translations"));
	for(const QString &path : {rel_share,local_dir})
	{
		const QString cleaned=QDir::cleanPath(path);
		if(!paths.contains(cleaned))
		{
			paths << cleaned;
		}
	}

	return paths;
}

class TownsQtJsonTranslator final : public QTranslator
{
public:
	explicit TownsQtJsonTranslator(QObject *parent=nullptr) : QTranslator(parent) {}

	bool isLoaded(void) const { return !map_.isEmpty(); }
	int entryCount(void) const { return map_.size(); }
	const QString &loadedLocale(void) const { return loaded_locale_; }
	const QString &loadedFrom(void) const { return loaded_from_; }

	bool loadLocale(const QString &locale_tag)
	{
		map_.clear();
		loaded_locale_.clear();
		loaded_from_.clear();

		for(const QString &candidate : LocaleCandidates(locale_tag))
		{
			if(true==IsEmbeddedLocale(candidate))
			{
				const QString resource_path=
				    QStringLiteral(":/i18n/townsqt_%1.json").arg(candidate);
				if(true==loadFromResource(resource_path))
				{
					loaded_locale_=candidate;
					loaded_from_=resource_path;
					return true;
				}
				continue;
			}

			for(const QString &dir : TranslationSearchPaths())
			{
				const QString file_path=
				    dir+QStringLiteral("/townsqt_")+candidate+QStringLiteral(".json");
				if(true==loadFromFile(file_path))
				{
					loaded_locale_=candidate;
					loaded_from_=file_path;
					return true;
				}
			}
		}
		return false;
	}

	QString translate(const char *context,const char *sourceText,const char *disambiguation,
	                  int n) const override
	{
		Q_UNUSED(disambiguation);
		Q_UNUSED(n);
		if(nullptr==context || nullptr==sourceText)
		{
			return {};
		}
		const QByteArray key=QByteArray(context)+'\0'+sourceText;
		const auto it=map_.constFind(key);
		if(it!=map_.constEnd())
		{
			return it.value();
		}
		return {};
	}

private:
	bool loadFromResource(const QString &resource_path)
	{
		QFile file(resource_path);
		if(false==file.open(QIODevice::ReadOnly))
		{
			return false;
		}
		return ingestJson(file.readAll());
	}

	bool loadFromFile(const QString &file_path)
	{
		QFile file(file_path);
		if(false==file.open(QIODevice::ReadOnly))
		{
			return false;
		}
		return ingestJson(file.readAll());
	}

	bool ingestJson(const QByteArray &bytes)
	{
		const QJsonDocument doc=QJsonDocument::fromJson(bytes);
		const QJsonArray array=
		    doc.isObject() ? doc.object().value(QStringLiteral("translations")).toArray()
		                   : doc.array();
		if(true==array.isEmpty())
		{
			return false;
		}

		QHash<QByteArray,QString> next;
		for(const QJsonValue &entry : array)
		{
			if(false==entry.isObject())
			{
				continue;
			}
			const QJsonObject obj=entry.toObject();
			const QString context=obj.value(QStringLiteral("context")).toString();
			const QString source=obj.value(QStringLiteral("source")).toString();
			const QString translation=obj.value(QStringLiteral("translation")).toString();
			if(true==context.isEmpty() || true==source.isEmpty() || true==translation.isEmpty())
			{
				continue;
			}
			next.insert(context.toUtf8()+'\0'+source.toUtf8(),translation);
		}
		if(true==next.isEmpty())
		{
			return false;
		}
		map_=std::move(next);
		return true;
	}

	QHash<QByteArray,QString> map_;
	QString loaded_locale_;
	QString loaded_from_;
};

bool LoadQtBaseTranslator(QApplication &app,const QString &locale_tag)
{
	if(true==locale_tag.startsWith(QStringLiteral("en")))
	{
		return false;
	}

	QTranslator *qt_translator=new QTranslator(&app);
	const QString translations_path=
	    QLibraryInfo::path(QLibraryInfo::TranslationsPath);
	for(const QString &candidate : LocaleCandidates(locale_tag))
	{
		if(true==qt_translator->load(QStringLiteral("qtbase_")+candidate,translations_path))
		{
			app.installTranslator(qt_translator);
			return true;
		}
	}

	delete qt_translator;
	return false;
}

bool InstallAppTranslator(QApplication &app,const QString &locale_tag,
                          const char *requested_label)
{
	auto *app_translator=new TownsQtJsonTranslator(&app);
	if(true==app_translator->loadLocale(locale_tag))
	{
		app.installTranslator(app_translator);
		std::fprintf(stderr,"Tsugaru_QT: UI language: %s (%d strings, %s)\n",
		             app_translator->loadedLocale().toUtf8().constData(),
		             app_translator->entryCount(),
		             app_translator->loadedFrom().toUtf8().constData());
		return true;
	}

	std::fprintf(stderr,
	             "Tsugaru_QT: warning: UI translations for %s could not be loaded; "
	             "falling back to English\n",
	             requested_label);
	delete app_translator;
	return false;
}

} // namespace

void TownsQtInstallTranslators(QCoreApplication &app)
{
	// QApplication required for Qt base translators; cast is safe from main().
	auto &qapp=static_cast<QApplication &>(app);
	const QString requested=ResolveUiLocaleTag();
	const QByteArray requested_label=requested.toUtf8();

	if(true==requested.startsWith(QStringLiteral("en")))
	{
		if(true==InstallAppTranslator(qapp,QStringLiteral("en"),requested_label.constData()))
		{
			return;
		}
		std::fprintf(stderr,"Tsugaru_QT: UI language: English\n");
		return;
	}

	LoadQtBaseTranslator(qapp,requested);
	if(true==InstallAppTranslator(qapp,requested,requested_label.constData()))
	{
		return;
	}

	LoadQtBaseTranslator(qapp,QStringLiteral("en"));
	if(false==InstallAppTranslator(qapp,QStringLiteral("en"),requested_label.constData()))
	{
		std::fprintf(stderr,"Tsugaru_QT: UI language: English (source strings)\n");
	}
}
