#include "cdrom_monitor_window.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

CdromMonitorWindow::CdromMonitorWindow(QWidget *parent)
    :DebugTextWindow(QObject::tr("CD-ROM monitor"),parent)
{
	if(nullptr==layout())
	{
		return;
	}
	auto *filter_row=new QHBoxLayout();
	filter_row->setContentsMargins(0,0,0,0);
	filter_row->setSpacing(12);

	filter_cdrom_=new QCheckBox(tr("CD-ROM"),this);
	filter_cdda_=new QCheckBox(tr("CDDA"),this);
	filter_cache_=new QCheckBox(tr("CDDA cache"),this);

	filter_cdrom_->setChecked(true);
	filter_cdda_->setChecked(true);
	filter_cache_->setChecked(true);

	filter_row->addWidget(filter_cdrom_);
	filter_row->addWidget(filter_cdda_);
	filter_row->addWidget(filter_cache_);
	filter_row->addStretch(1);

	auto connect_filter=[this](QCheckBox *box){
		if(nullptr!=box)
		{
			connect(box,&QCheckBox::toggled,this,[this](bool){ rebuildFilterMask(); });
		}
	};
	connect_filter(filter_cdrom_);
	connect_filter(filter_cdda_);
	connect_filter(filter_cache_);

	auto *vbox=qobject_cast<QVBoxLayout*>(layout());
	if(nullptr!=vbox)
	{
		vbox->insertLayout(0,filter_row);
	}
	rebuildFilterMask();
}

void CdromMonitorWindow::rebuildFilterMask()
{
	filter_mask_=0;
	if(nullptr!=filter_cdrom_ && filter_cdrom_->isChecked())
	{
		filter_mask_|=CategoryCdrom;
	}
	if(nullptr!=filter_cdda_ && filter_cdda_->isChecked())
	{
		filter_mask_|=CategoryCdda;
	}
	if(nullptr!=filter_cache_ && filter_cache_->isChecked())
	{
		filter_mask_|=CategoryCache;
	}
}

unsigned int CdromMonitorWindow::CategoryFromLinePrefix(const QString &line)
{
	if(line.startsWith(QStringLiteral("[CDROM]")) ||
	   line.startsWith(QStringLiteral("CDROM ")))
	{
		return CategoryCdrom;
	}
	if(line.startsWith(QStringLiteral("[CDDA]")) ||
	   line.startsWith(QStringLiteral("GETSTATE ")))
	{
		return CategoryCdda;
	}
	if(line.startsWith(QStringLiteral("[CACHE]")) ||
	   line.startsWith(QStringLiteral("CDDA cache ")) ||
	   line.startsWith(QStringLiteral("CDDA same-track")))
	{
		return CategoryCache;
	}
	// Unknown lines: hide unless every category is enabled (avoid filter no-op).
	return 0;
}

void CdromMonitorWindow::appendMonitorLine(const QString &line)
{
	if(line.isEmpty())
	{
		return;
	}
	const unsigned int cat=CategoryFromLinePrefix(line);
	if(0==cat || 0==(filter_mask_&cat))
	{
		return;
	}
	appendLine(line);
}
