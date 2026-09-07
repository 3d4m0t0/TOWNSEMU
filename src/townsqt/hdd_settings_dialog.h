#pragma once

#include <QDialog>
#include <QString>

#include "townsqt_settings.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class HddSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	struct Slot
	{
		bool enabled=false;
		QString path;
	};

	/*! mountedCdImagePath: when set, HD0 Create defaults to that CD's basename + ".hd". */
	explicit HddSettingsDialog(const Slot slots[TownsQtSettings::kHddSlotCount],
	                           QWidget *parent=nullptr,
	                           const QString &mountedCdImagePath=QString());

	void copySlotsTo(Slot out[TownsQtSettings::kHddSlotCount]) const;

private Q_SLOTS:
	void onCreateClicked(int slot);
	void onCompactClicked(int slot);
	void onBrowseClicked(int slot);
	void onRemoveClicked(int slot);

private:
	struct Row
	{
		QCheckBox *enabled=nullptr;
		QLineEdit *path=nullptr;
		QString fullPath;
		QPushButton *create=nullptr;
		QPushButton *compact=nullptr;
		QPushButton *browse=nullptr;
		QPushButton *remove=nullptr;
	};

	void setSlotPath(int slot,const QString &fullPath);
	void updateRowEnabled(int slot);
	bool createBlankHddImage(const QString &path,int size_mb);

	Row rows_[TownsQtSettings::kHddSlotCount];
	/*! Basename (no extension) of the mounted CD, if any — used for HD0 Create default name. */
	QString cd_image_base_name_;
};
