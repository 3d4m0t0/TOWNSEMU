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
	void onBrowseClicked(int slot);
	void onRemoveClicked(int slot);

private:
	struct Row
	{
		QCheckBox *enabled=nullptr;
		QLineEdit *path=nullptr;
		QPushButton *create=nullptr;
		QPushButton *browse=nullptr;
		QPushButton *remove=nullptr;
	};

	void updateRowEnabled(int slot);
	bool createBlankHddImage(const QString &path,int size_mb);

	Row rows_[TownsQtSettings::kHddSlotCount];
};
