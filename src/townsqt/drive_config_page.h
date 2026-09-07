#pragma once

#include <QWidget>

#include "townscmos_util.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

/*! CMOS drive-letter map + single-drive editor (embedded in Settings). */
class DriveConfigPage : public QWidget
{
	Q_OBJECT

public:
	struct Values
	{
		bool singleDrive=false;
		TownsCmos::DriveAssignEntry letters[TownsCmos::kDriveLetterCount];
	};

	explicit DriveConfigPage(QWidget *parent=nullptr);

	void setValues(const Values &values);
	Values values() const;

private:
	struct Row
	{
		QComboBox *type=nullptr;
		QSpinBox *unit=nullptr;
	};

	void syncUnitEnabled(int letter);

	QCheckBox *single_drive_=nullptr;
	Row rows_[TownsCmos::kDriveLetterCount];
};
