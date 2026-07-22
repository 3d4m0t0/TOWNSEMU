#pragma once

#include "debug_text_window.h"

class QCheckBox;

/*! CD-ROM / CDDA cache debug log with category filters. */
class CdromMonitorWindow : public DebugTextWindow
{
	Q_OBJECT

public:
	enum LogCategory : unsigned int
	{
		CategoryCdrom=1u<<0,
		CategoryCdda=1u<<1,
		CategoryCache=1u<<2,
		CategoryAll=CategoryCdrom|CategoryCdda|CategoryCache,
	};

	explicit CdromMonitorWindow(QWidget *parent=nullptr);

	void appendMonitorLine(const QString &line);

private:
	void rebuildFilterMask();

	static unsigned int CategoryFromLinePrefix(const QString &line);

	QCheckBox *filter_cdrom_=nullptr;
	QCheckBox *filter_cdda_=nullptr;
	QCheckBox *filter_cache_=nullptr;
	unsigned int filter_mask_=CategoryAll;
};
