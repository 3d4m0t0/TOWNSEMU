#include "hdd_settings_dialog.h"

#include "townsqt_paths.h"

#include "cpputil.h"

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
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int kDefaultHddSizeMb=100;
constexpr int kMinHddSizeMb=1;
constexpr int kMaxHddSizeMb=1024;

QString FormatByteSize(qint64 bytes)
{
	if(bytes>=1024LL*1024LL*1024LL)
	{
		return QString::number(bytes/(1024.0*1024.0*1024.0),'f',2)+QStringLiteral(" GB");
	}
	if(bytes>=1024LL*1024LL)
	{
		return QString::number(bytes/(1024.0*1024.0),'f',1)+QStringLiteral(" MB");
	}
	if(bytes>=1024LL)
	{
		return QString::number(bytes/1024.0,'f',1)+QStringLiteral(" KB");
	}
	return QString::number(bytes)+QStringLiteral(" B");
}
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
		row.create->setToolTip(
		    tr("Create a sparse image.\n"
		       "Logical size is as entered; disk usage starts small and grows on write."));
		row.compact=new QPushButton(tr("Compact"),this);
		row.compact->setToolTip(
		    tr("Make an existing image sparse by punching holes in zero regions.\n"
		       "Logical size is unchanged; only on-disk usage shrinks."));
		row.browse=new QPushButton(tr("Browse…"),this);
		row.remove=new QPushButton(tr("Remove"),this);

		grid->addWidget(row.enabled,slot,0);
		grid->addWidget(row.path,slot,1);
		grid->addWidget(row.create,slot,2);
		grid->addWidget(row.compact,slot,3);
		grid->addWidget(row.browse,slot,4);
		grid->addWidget(row.remove,slot,5);

		connect(row.enabled,&QCheckBox::toggled,this,[this,slot](bool){
			updateRowEnabled(slot);
		});
		connect(row.create,&QPushButton::clicked,this,[this,slot]{
			onCreateClicked(slot);
		});
		connect(row.compact,&QPushButton::clicked,this,[this,slot]{
			onCompactClicked(slot);
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
	    tr("New images are created sparse. File managers show logical size; actual disk usage stays small until data is written."),
	    this));
	layout->addWidget(new QLabel(
	    tr("Images are stored under %1 by default.").arg(TownsQtPaths::hddDir()),
	    this));

	auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
	connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);
	connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
	layout->addWidget(buttons);

	resize(560,sizeHint().height());
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
	const qint64 logical=info.size();
	const qint64 disk=cpputil::AllocatedFileBytes(fullPath.toStdString());
	row.path->setToolTip(
	    tr("Path: %1\nLogical size: %2\nDisk usage: %3")
	        .arg(fullPath,FormatByteSize(logical),FormatByteSize(disk)));
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
	const bool hasImage=!rows_[slot].fullPath.isEmpty();
	rows_[slot].path->setEnabled(on);
	rows_[slot].create->setEnabled(on);
	rows_[slot].compact->setEnabled(on && hasImage);
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

	const unsigned long long bytes=static_cast<unsigned long long>(size_mb)*1024ULL*1024ULL;
	return cpputil::CreateSparseBinaryFile(path.toStdString(),bytes);
}

void HddSettingsDialog::onCreateClicked(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	bool ok=false;
	const int size_mb=QInputDialog::getInt(
	    this,
	    tr("Create hard disk image"),
	    tr("Logical size in MB (%1–%2).\nCreated as a sparse image:").arg(kMinHddSizeMb).arg(kMaxHddSizeMb),
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

	const qint64 logical=QFileInfo(path).size();
	const qint64 disk=cpputil::AllocatedFileBytes(path.toStdString());
	QMessageBox::information(
	    this,
	    tr("Create hard disk image"),
	    tr("Created a sparse hard disk image.\n\n"
	       "Logical size (for FM TOWNS): %1\n"
	       "Disk usage on this computer: %2\n\n"
	       "File managers list logical size. Use the tooltip on the path field or `du -h` to check disk usage.")
	        .arg(FormatByteSize(logical),FormatByteSize(disk)));

	setSlotPath(slot,path);
	rows_[slot].enabled->setChecked(true);
	updateRowEnabled(slot);
}

void HddSettingsDialog::onCompactClicked(int slot)
{
	slot=std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
	const QString path=rows_[slot].fullPath;
	if(path.isEmpty())
	{
		return;
	}
	if(!QFileInfo::exists(path))
	{
		QMessageBox::warning(
		    this,
		    tr("Compact hard disk image"),
		    tr("The selected file does not exist."));
		return;
	}

	if(!cpputil::CompactBinaryFileToSparse(path.toStdString()))
	{
		QMessageBox::warning(
		    this,
		    tr("Compact hard disk image"),
		    tr("Failed to compact the hard disk image.\n"
		       "Sparse compaction is supported on Linux with ext4, XFS, or Btrfs."));
		return;
	}

	QMessageBox::information(
	    this,
	    tr("Compact hard disk image"),
	    tr("Compacted the hard disk image.\n"
	       "Logical size is unchanged; unused zero regions now use less disk space."));
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
