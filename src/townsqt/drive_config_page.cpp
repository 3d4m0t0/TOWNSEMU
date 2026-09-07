#include "drive_config_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
enum TypeComboRole
{
	TypeUnassigned=0,
	TypeFd=1,
	TypeScsi=2,
	TypeRom=3,
};

constexpr int kLettersPerColumn=8;

int TypeToComboIndex(unsigned char type)
{
	switch(type)
	{
	case TownsCmos::kTypeFd:
		return TypeFd;
	case TownsCmos::kTypeScsi:
		return TypeScsi;
	case TownsCmos::kTypeRom:
		return TypeRom;
	default:
		return TypeUnassigned;
	}
}

unsigned char ComboIndexToType(int index)
{
	switch(index)
	{
	case TypeFd:
		return TownsCmos::kTypeFd;
	case TypeScsi:
		return TownsCmos::kTypeScsi;
	case TypeRom:
		return TownsCmos::kTypeRom;
	default:
		return TownsCmos::kTypeUnassigned;
	}
}

void MakeExpandingField(QWidget *w)
{
	w->setMinimumWidth(0);
	w->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
}
}

DriveConfigPage::DriveConfigPage(QWidget *parent)
	: QWidget(parent)
{
	auto *root=new QVBoxLayout(this);
	root->setContentsMargins(0,0,0,0);
	root->setSpacing(6);

	single_drive_=new QCheckBox(tr("Single-drive mode (disable FD1)"),this);
	single_drive_->setToolTip(
	    tr("Towns CMOS flag at I/O 0x328C. When on, FD1 is unavailable."));
	root->addWidget(single_drive_);

	auto *hint=new QLabel(
	    tr("Drive letter assignments (A–P). Apply or OK writes VM CMOS RAM and the "
	       "active CMOS file. Towns OS usually picks them up after reset/boot."),
	    this);
	hint->setWordWrap(true);
	root->addWidget(hint);

	auto *scroll=new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

	auto *inner=new QWidget(scroll);
	auto *columns=new QHBoxLayout(inner);
	columns->setContentsMargins(0,0,0,0);
	columns->setSpacing(8);

	auto addLetterColumn=[&](int letterBegin){
		auto *colHost=new QWidget(inner);
		auto *grid=new QGridLayout(colHost);
		grid->setContentsMargins(0,0,0,0);
		grid->setHorizontalSpacing(8);
		grid->setVerticalSpacing(4);
		grid->setColumnStretch(0,0);
		grid->setColumnStretch(1,1);
		grid->setColumnStretch(2,1);
		const int letterColW=
		    QFontMetrics(font()).horizontalAdvance(QStringLiteral("W:"))+4;
		grid->setColumnMinimumWidth(0,letterColW);

		auto *type_hdr=new QLabel(tr("Type"),colHost);
		auto *unit_hdr=new QLabel(tr("Unit"),colHost);
		type_hdr->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
		unit_hdr->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
		grid->addWidget(type_hdr,0,1);
		grid->addWidget(unit_hdr,0,2);

		for(int row=0; row<kLettersPerColumn; ++row)
		{
			const int i=letterBegin+row;
			auto &entry=rows_[i];
			auto *letter=new QLabel(
			    QStringLiteral("%1:").arg(QChar(QLatin1Char('A'+i))),colHost);
			letter->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
			letter->setFixedWidth(letterColW);
			letter->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

			entry.type=new QComboBox(colHost);
			entry.type->addItem(tr("Unassigned"),TypeUnassigned);
			entry.type->addItem(tr("FD"),TypeFd);
			entry.type->addItem(tr("SCSI"),TypeScsi);
			entry.type->addItem(tr("ROM"),TypeRom);
			entry.type->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
			entry.type->setMinimumContentsLength(1);
			MakeExpandingField(entry.type);

			entry.unit=new QSpinBox(colHost);
			entry.unit->setRange(0,6);
			entry.unit->setValue(0);
			entry.unit->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
			MakeExpandingField(entry.unit);

			grid->addWidget(letter,row+1,0);
			grid->addWidget(entry.type,row+1,1);
			grid->addWidget(entry.unit,row+1,2);

			connect(entry.type,QOverload<int>::of(&QComboBox::currentIndexChanged),
			        this,[this,i](int){
				        syncUnitEnabled(i);
			        });
			syncUnitEnabled(i);
		}
		grid->setRowStretch(kLettersPerColumn+1,1);
		columns->addWidget(colHost,1);
	};

	addLetterColumn(0);

	auto *sep=new QFrame(inner);
	sep->setFrameShape(QFrame::VLine);
	sep->setFrameShadow(QFrame::Sunken);
	columns->addWidget(sep);

	addLetterColumn(kLettersPerColumn);

	scroll->setWidget(inner);
	root->addWidget(scroll,1);
}

void DriveConfigPage::setValues(const Values &values)
{
	single_drive_->setChecked(values.singleDrive);
	for(int i=0; i<TownsCmos::kDriveLetterCount; ++i)
	{
		rows_[i].type->setCurrentIndex(TypeToComboIndex(values.letters[i].type));
		if(true!=TownsCmos::IsUnassigned(values.letters[i]))
		{
			rows_[i].unit->setValue(
			    std::min(6,static_cast<int>(values.letters[i].unit)));
		}
		else
		{
			rows_[i].unit->setValue(0);
		}
		syncUnitEnabled(i);
	}
}

DriveConfigPage::Values DriveConfigPage::values() const
{
	Values v;
	v.singleDrive=single_drive_->isChecked();
	for(int i=0; i<TownsCmos::kDriveLetterCount; ++i)
	{
		const unsigned char type=ComboIndexToType(rows_[i].type->currentIndex());
		v.letters[i].type=type;
		if(TownsCmos::kTypeUnassigned==type)
		{
			v.letters[i].unit=TownsCmos::kTypeUnassigned;
		}
		else
		{
			v.letters[i].unit=static_cast<unsigned char>(rows_[i].unit->value());
		}
	}
	return v;
}

void DriveConfigPage::syncUnitEnabled(int letter)
{
	if(letter<0 || letter>=TownsCmos::kDriveLetterCount)
	{
		return;
	}
	auto &row=rows_[letter];
	const bool assigned=TypeUnassigned!=row.type->currentIndex();
	row.unit->setEnabled(assigned);
}
