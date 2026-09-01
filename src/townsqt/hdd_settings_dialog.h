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

	explicit HddSettingsDialog(const Slot slots[TownsQtSettings::kHddSlotCount],
	                           QWidget *parent=nullptr);

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
};
