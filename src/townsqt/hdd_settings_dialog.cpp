#include "hdd_settings_dialog.h"

#include "townsqt_paths.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace
{
constexpr int kDefaultHddSizeMb=100;
constexpr int kMinHddSizeMb=1;
constexpr int kMaxHddSizeMb=1024;
}

HddSettingsDialog::HddSettingsDialog(const Slot slots[TownsQtSettings::kHddSlotCount],QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Hard disk drive settings"));
	setModal(true);

	auto *layout=new QVBoxLayout(this);
	auto *grid=new QGridLayout();
	grid->setHorizontalSpacing(8);
	grid->setVerticalSpacing(6);
	grid->setColumnStretch(1,1);

	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		auto &row=rows_[slot];
		row.enabled=new QCheckBox(tr("HD%1").arg(slot),this);
		row.enabled->setChecked(slots[slot].enabled);
		row.path=new QLineEdit(this);
		row.path->setReadOnly(true);
		row.path->setPlaceholderText(tr("No image"));
		setSlotPath(slot,slots[slot].path);
		row.create=new QPushButton(tr("Create"),this);
		row.browse=new QPushButton(tr("Browse…"),this);
		row.remove=new QPushButton(tr("Remove"),this);

		grid->addWidget(row.enabled,slot,0);
		grid->addWidget(row.path,slot,1);
		grid->addWidget(row.create,slot,2);
		grid->addWidget(row.browse,slot,3);
		grid->addWidget(row.remove,slot,4);

		connect(row.enabled,&QCheckBox::toggled,this,[this,slot](bool){
			updateRowEnabled(slot);
		});
		connect(row.create,&QPushButton::clicked,this,[this,slot]{
			onCreateClicked(slot);
		});
		connect(row.browse,&QPushButton::clicked,this,[this,slot]{
			onBrowseClicked(slot);
		});
		connect(row.remove,&QPushButton::clicked,this,[this,slot]{
			onRemoveClicked(slot);
		});
		updateRowEnabled(slot);
	}

	layout->addLayout(grid);
	layout->addWidget(new QLabel(
	    tr("Images are stored under %1 by default.").arg(TownsQtPaths::hddDir()),
	    this));

	auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
	connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);
	connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
	layout->addWidget(buttons);

	resize(480,sizeHint().height());
}

void HddSettingsDialog::setSlotPath(int slot,const QString &fullPath)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	auto &row=rows_[slot];
	row.fullPath=fullPath;
	if(fullPath.isEmpty())
	{
		row.path->clear();
		row.path->setToolTip(QString());
		return;
	}
	const QFileInfo info(fullPath);
	row.path->setText(info.fileName());
	row.path->setToolTip(fullPath);
}

void HddSettingsDialog::copySlotsTo(Slot out[TownsQtSettings::kHddSlotCount]) const
{
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		out[slot].enabled=rows_[slot].enabled->isChecked();
		out[slot].path=rows_[slot].fullPath;
	}
}

void HddSettingsDialog::updateRowEnabled(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	const bool on=rows_[slot].enabled->isChecked();
	rows_[slot].path->setEnabled(on);
	rows_[slot].create->setEnabled(on);
	rows_[slot].browse->setEnabled(on);
	rows_[slot].remove->setEnabled(on);
}

bool HddSettingsDialog::createBlankHddImage(const QString &path,int size_mb)
{
	if(size_mb<kMinHddSizeMb || size_mb>kMaxHddSizeMb)
	{
		return false;
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		return false;
	}

	QSaveFile file(path);
	if(!file.open(QIODevice::WriteOnly))
	{
		return false;
	}

	std::vector<char> zero(1024*1024,0);
	for(int i=0; i<size_mb; ++i)
	{
		if(static_cast<qint64>(zero.size())!=file.write(zero.data(),static_cast<qint64>(zero.size())))
		{
			file.cancelWriting();
			return false;
		}
	}
	return file.commit();
}

void HddSettingsDialog::onCreateClicked(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	bool ok=false;
	const int size_mb=QInputDialog::getInt(
	    this,
	    tr("Create hard disk image"),
	    tr("Size in MB (%1–%2):").arg(kMinHddSizeMb).arg(kMaxHddSizeMb),
	    kDefaultHddSizeMb,
	    kMinHddSizeMb,
	    kMaxHddSizeMb,
	    1,
	    &ok);
	if(!ok)
	{
		return;
	}

	if(true!=TownsQtPaths::ensureLayout())
	{
		QMessageBox::warning(
		    this,
		    tr("Create hard disk image"),
		    tr("Failed to create the hard disk directory."));
		return;
	}

	const QString default_name=QStringLiteral("hd%1_%2mb.hd").arg(slot).arg(size_mb);
	QString path=QFileDialog::getSaveFileName(
	    this,
	    tr("Create hard disk image"),
	    TownsQtPaths::hddDir()+QStringLiteral("/")+default_name,
	    tr("Hard disk images (*.hd *.hdi *.hdm *.bin);;All files (*)"),
	    nullptr,
	    QFileDialog::DontConfirmOverwrite);
	if(path.isEmpty())
	{
		return;
	}
	if(QFileInfo(path).suffix().isEmpty())
	{
		path+=QStringLiteral(".hd");
	}

	if(QFileInfo::exists(path) &&
	   QMessageBox::Yes!=QMessageBox::question(
	       this,
	       tr("Create hard disk image"),
	       tr("The file already exists. Overwrite it?"),
	       QMessageBox::Yes|QMessageBox::No,
	       QMessageBox::No))
	{
		return;
	}

	if(!createBlankHddImage(path,size_mb))
	{
		QMessageBox::warning(
		    this,
		    tr("Create hard disk image"),
		    tr("Failed to create the hard disk image."));
		return;
	}

	setSlotPath(slot,path);
	rows_[slot].enabled->setChecked(true);
	updateRowEnabled(slot);
}

void HddSettingsDialog::onBrowseClicked(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	QString start=rows_[slot].fullPath;
	if(start.isEmpty())
	{
		start=TownsQtPaths::hddDir();
	}
	else
	{
		start=QFileInfo(start).absolutePath();
	}

	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("Select hard disk image"),
	    start,
	    tr("Hard disk images (*.hd *.hdi *.hdm *.bin);;All files (*)"));
	if(path.isEmpty())
	{
		return;
	}
	setSlotPath(slot,path);
	rows_[slot].enabled->setChecked(true);
	updateRowEnabled(slot);
}

void HddSettingsDialog::onRemoveClicked(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	setSlotPath(slot,QString());
	rows_[slot].enabled->setChecked(false);
	updateRowEnabled(slot);
}
