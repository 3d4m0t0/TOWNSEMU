#include "mouse_coord_scan_window.h"

#include <algorithm>

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QColor>
#include <QFontDatabase>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
int WordAsCoord(unsigned int v)
{
	return (int)(short)(v&0xffffu);
}

QString FmtCoordWord(unsigned int v)
{
	return QString::number(WordAsCoord(v));
}

unsigned int ParseCoordText(const QString &text,bool *ok)
{
	bool localOk=false;
	if(nullptr==ok)
	{
		ok=&localOk;
	}
	*ok=false;
	if(text.startsWith(QStringLiteral("0x"),Qt::CaseInsensitive))
	{
		const unsigned int raw=text.mid(2).toUInt(ok,16);
		if(true==*ok)
		{
			return raw&0xffffu;
		}
		return 0;
	}
	const int signedVal=text.toInt(ok);
	if(true==*ok)
	{
		return (unsigned int)(short)signedVal;
	}
	return 0;
}
}

void AddUniquePhys(QVector<unsigned int> &list,unsigned int phys)
{
	if(0==phys || list.contains(phys))
	{
		return;
	}
	list.append(phys);
}

namespace
{
enum
{
	COL_WATCH,
	COL_CHASE,
	COL_X,
	COL_Y,
	COL_X2,
	COL_Y2,
	COL_PHYS,
	COL_SIZE,
	COL_VALUE,
	COL_RANGE,
	COL_SPAN,
	COL_HITS,
	NUM_COL
};

constexpr int kSortRole=Qt::UserRole+1;

class SortableItem : public QTableWidgetItem
{
public:
	using QTableWidgetItem::QTableWidgetItem;
	bool operator<(const QTableWidgetItem &other) const override
	{
		const QVariant a=data(kSortRole);
		const QVariant b=other.data(kSortRole);
		if(true==a.isValid() && true==b.isValid())
		{
			return a.toULongLong()<b.toULongLong();
		}
		return QTableWidgetItem::operator<(other);
	}
};
}

MouseCoordScanWindow::MouseCoordScanWindow(QWidget *parent)
	:DebugTextWindow(tr("Memory scan"),parent)
{
	// Exclusive with Settings: title only (no close/min/max). Use Cancel or Update.
	setWindowFlags(Qt::Window|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
	setCloseButtonVisible(false);

	auto *bar=new QWidget(this);
	auto *row=new QHBoxLayout(bar);
	row->setContentsMargins(0,0,0,0);

	scan_btn_=new QPushButton(tr("Scan"),bar);
	scan_btn_->setCheckable(true);
	scan_btn_->setToolTip(
	    tr("Scan RAM for new coordinate candidates (also turns Mouse capture ON).\n"
	       "Move the mouse in the emu view.  ESC stops Scan and capture."));
	capture_btn_=new QPushButton(tr("Mouse capture"),bar);
	capture_btn_->setCheckable(true);
	capture_btn_->setToolTip(
	    tr("Alone: refresh list values and drop unrelated candidates (no new Scan picks).\n"
	       "Forced while Scan is on.  Press again or ESC restores the profile mouse mode."));
	auto *clear_range_btn=new QPushButton(tr("Clear min max"),bar);
	clear_range_btn->setToolTip(
	    tr("Reset observed min..max on all candidates.\n"
	       "Starts again from the next sampled value."));
	auto *clear_scan_btn=new QPushButton(tr("Clear candidates"),bar);
	auto *keep_only_btn=new QPushButton(tr("Remove unselected"),bar);
	keep_only_btn->setToolTip(tr("Delete rows with none of Watch / Chase / X / Y / X2 / Y2 checked."));
	auto *update_profile_btn=new QPushButton(tr("Update"),bar);
	update_profile_btn->setToolTip(
	    tr("Copy the checked X/Y pair (and min..max) into Mouse integration.\n"
	       "If X2/Y2 are also checked, fill Phys 2 as well.\n"
	       "Closes Memory scan and reopens Settings. Does not save — use Apply or OK there."));
	auto *cancel_btn=new QPushButton(tr("Cancel"),bar);
	cancel_btn->setToolTip(
	    tr("Close Memory scan and return to Settings → Mouse integration."));

	row->addWidget(scan_btn_);
	row->addWidget(capture_btn_);
	row->addWidget(clear_range_btn);
	row->addWidget(clear_scan_btn);
	row->addWidget(keep_only_btn);
	row->addStretch(1);
	row->addWidget(update_profile_btn);
	row->addWidget(cancel_btn);

	cand_table_=new QTableWidget(0,NUM_COL,this);
	{
		const QFont mono=QFontDatabase::systemFont(QFontDatabase::FixedFont);
		cand_table_->setFont(mono);
	}
	cand_table_->setHorizontalHeaderLabels(
	    {tr("Watch"),tr("Chase"),tr("X"),tr("Y"),tr("X2"),tr("Y2"),
	     tr("phys"),tr("sz"),tr("value"),tr("min..max"),tr("span"),tr("changes")});
	cand_table_->verticalHeader()->setVisible(true);
	// Fixed row height — ResizeToContents reflows every value update and flickers.
	cand_table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	cand_table_->verticalHeader()->setDefaultSectionSize(
	    cand_table_->fontMetrics().height()+14);
	cand_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	cand_table_->horizontalHeader()->setMinimumSectionSize(24);
	cand_table_->horizontalHeader()->setStretchLastSection(false);
	cand_table_->horizontalHeader()->setSortIndicatorShown(true);
	cand_table_->setSortingEnabled(true);
	cand_table_->setSelectionMode(QAbstractItemView::SingleSelection);
	cand_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	cand_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	cand_table_->setWordWrap(false);
	cand_table_->setTextElideMode(Qt::ElideNone);
	cand_table_->setMinimumHeight(220);
	cand_table_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
	cand_table_->setToolTip(
	    tr("Scan = add candidates (turns Mouse capture ON).\n"
	       "Mouse capture ON alone = refresh values / prune noise (no new picks).\n"
	       "Watch = keep in list after Scan stops.\n"
	       "Chase = follow guest-writer SOURCE; moves to the new SOURCE when found.\n"
	       "Also adds nearby phys (±32 bytes) whose value Δ matches host/IO mouse Δ.\n"
	       "X/Y = primary Game Phys pair.  X2/Y2 = optional Phys 2 pair.\n"
	       "Green phys = MOS soft-cursor words (writable if set as Phys).\n"
	       "Blue phys = disc-profile pair.  span = max−min; oversized rows are pruned.\n"
	       "ESC ends capture (+ Scan)."));

	connect(scan_btn_,&QPushButton::toggled,this,[this](bool on){
		if(true==on)
		{
			// Scan always needs host Δ → turn capture on with it.
			syncCaptureChecked(true);
			Q_EMIT captureToggled(true);
		}
		if(nullptr!=cand_table_)
		{
			cand_table_->setSortingEnabled(!on);
		}
		Q_EMIT scanToggled(on);
	});
	connect(capture_btn_,&QPushButton::toggled,this,[this](bool on){
		if(true!=on && true==scanChecked())
		{
			// Capture alone can stay without Scan; Scan cannot stay without capture.
			scan_btn_->blockSignals(true);
			scan_btn_->setChecked(false);
			scan_btn_->blockSignals(false);
			if(nullptr!=cand_table_)
			{
				cand_table_->setSortingEnabled(true);
			}
			Q_EMIT scanToggled(false);
		}
		Q_EMIT captureToggled(on);
	});
	connect(cand_table_,&QTableWidget::itemChanged,
	        this,&MouseCoordScanWindow::onTableItemChanged);

	if(auto *lay=qobject_cast<QVBoxLayout *>(layout()))
	{
		lay->insertWidget(0,bar);
		lay->insertWidget(1,cand_table_,2);
	}

	connect(clear_range_btn,&QPushButton::clicked,this,[this](){
		clearCandidateRanges();
		Q_EMIT clearRangesRequested();
	});
	connect(clear_scan_btn,&QPushButton::clicked,this,[this](){
		clearCandidateTable();
		Q_EMIT clearRequested();
	});
	connect(keep_only_btn,&QPushButton::clicked,this,[this](){
		const QVariantList keep=keptPhysList();
		if(true==keep.isEmpty())
		{
			return;
		}
		cand_table_->setSortingEnabled(false);
		cand_table_->blockSignals(true);
		for(int r=cand_table_->rowCount()-1; 0<=r; --r)
		{
			const unsigned int phys=physAtRow(r);
			auto marked=[&](int col)->bool
			{
				auto *it=cand_table_->item(r,col);
				return nullptr!=it && Qt::Checked==it->checkState();
			};
			if(true!=marked(COL_WATCH) && true!=marked(COL_CHASE) &&
			   true!=marked(COL_X) && true!=marked(COL_Y) &&
			   true!=marked(COL_X2) && true!=marked(COL_Y2))
			{
				checked_watch_.removeAll(phys);
				checked_chase_.removeAll(phys);
				checked_x_.removeAll(phys);
				checked_y_.removeAll(phys);
				checked_x2_.removeAll(phys);
				checked_y2_.removeAll(phys);
				profile_marked_.remove(phys);
				cand_table_->removeRow(r);
			}
		}
		cand_table_->blockSignals(false);
		cand_table_->setSortingEnabled(true);
		syncWatchChaseToCore();
		Q_EMIT keepOnlyRequested(keep);
	});
	connect(update_profile_btn,&QPushButton::clicked,this,&MouseCoordScanWindow::editProfileRequested);
	connect(cancel_btn,&QPushButton::clicked,this,&MouseCoordScanWindow::cancelRequested);

	setFocusPolicy(Qt::StrongFocus);
	cand_table_->sortItems(COL_HITS,Qt::DescendingOrder);
	fitColumnsToLabels();
	resize(820,520);
	// Vertical resize only — lock width after the initial layout.
	setFixedWidth(width());
}

void MouseCoordScanWindow::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	setFixedWidth(width());
}

void MouseCoordScanWindow::closeEvent(QCloseEvent *event)
{
	// Ignore Alt+F4 / system close — use Cancel or Update.
	event->ignore();
}

void MouseCoordScanWindow::syncCaptureChecked(bool on)
{
	if(nullptr==capture_btn_ || capture_btn_->isChecked()==on)
	{
		return;
	}
	capture_btn_->blockSignals(true);
	capture_btn_->setChecked(on);
	capture_btn_->blockSignals(false);
}

void MouseCoordScanWindow::setScanChecked(bool on)
{
	if(nullptr==scan_btn_)
	{
		return;
	}
	if(scan_btn_->isChecked()!=on)
	{
		scan_btn_->blockSignals(true);
		scan_btn_->setChecked(on);
		scan_btn_->blockSignals(false);
	}
	if(nullptr!=cand_table_)
	{
		cand_table_->setSortingEnabled(!on);
	}
}

void MouseCoordScanWindow::setCaptureChecked(bool on)
{
	syncCaptureChecked(on);
}

bool MouseCoordScanWindow::scanChecked(void) const
{
	return nullptr!=scan_btn_ && scan_btn_->isChecked();
}

bool MouseCoordScanWindow::captureChecked(void) const
{
	return nullptr!=capture_btn_ && capture_btn_->isChecked();
}

void MouseCoordScanWindow::keyPressEvent(QKeyEvent *event)
{
	if(nullptr!=event && Qt::Key_Escape==event->key() &&
	   (true==scanChecked() || true==captureChecked()))
	{
		setScanChecked(false);
		syncCaptureChecked(false);
		Q_EMIT scanStopRequested();
		event->accept();
		return;
	}
	DebugTextWindow::keyPressEvent(event);
}

void MouseCoordScanWindow::fitColumnsToLabels(void)
{
	if(nullptr==cand_table_)
	{
		return;
	}
	const QFontMetrics fm(cand_table_->font());
	auto textW=[&](const QString &sample)->int
	{
		return fm.horizontalAdvance(sample);
	};
	auto headerW=[&](int col)->int
	{
		if(QTableWidgetItem *h=cand_table_->horizontalHeaderItem(col))
		{
			// Sort indicator + padding.
			return textW(h->text())+28;
		}
		return 0;
	};
	auto setCol=[&](int col,const QString &contentSample,int extraPad)
	{
		const int w=std::max(headerW(col),textW(contentSample)+extraPad);
		cand_table_->setColumnWidth(col,w);
	};
	// Checkbox columns: header + check indicator.
	setCol(COL_WATCH,tr("Watch"),36);
	setCol(COL_CHASE,tr("Chase"),36);
	setCol(COL_X,QStringLiteral("X"),36);
	setCol(COL_Y,QStringLiteral("Y"),36);
	setCol(COL_X2,QStringLiteral("X2"),36);
	setCol(COL_Y2,QStringLiteral("Y2"),36);
	// Numeric columns (mono): size to worst-case visible samples.
	setCol(COL_PHYS,QStringLiteral("00000000"),20);
	setCol(COL_SIZE,QStringLiteral("4"),20);
	setCol(COL_VALUE,QStringLiteral("-32768"),20);
	setCol(COL_RANGE,QStringLiteral("-32768..32767"),20);
	setCol(COL_SPAN,QStringLiteral("65535"),20);
	setCol(COL_HITS,QStringLiteral("999999"),20);
}

int MouseCoordScanWindow::rowForPhys(unsigned int phys) const
{
	for(int r=0; r<cand_table_->rowCount(); ++r)
	{
		if(physAtRow(r)==phys)
		{
			return r;
		}
	}
	return -1;
}

unsigned int MouseCoordScanWindow::physAtRow(int row) const
{
	auto *it=cand_table_->item(row,COL_PHYS);
	return nullptr!=it ? it->data(Qt::UserRole).toUInt() : 0;
}

bool MouseCoordScanWindow::rangeAtRow(int row,unsigned int &minVal,unsigned int &maxVal) const
{
	auto *it=cand_table_->item(row,COL_RANGE);
	if(nullptr==it)
	{
		return false;
	}
	if(it->data(Qt::UserRole).toBool())
	{
		minVal=it->data(Qt::UserRole+1).toUInt();
		maxVal=it->data(Qt::UserRole+2).toUInt();
		return true;
	}
	const QString text=it->text();
	const int sep=text.indexOf(QStringLiteral(".."));
	if(sep<0 || QStringLiteral("-")==text)
	{
		return false;
	}
	bool okMin=false,okMax=false;
	minVal=ParseCoordText(text.left(sep),&okMin);
	maxVal=ParseCoordText(text.mid(sep+2),&okMax);
	return okMin && okMax;
}

void MouseCoordScanWindow::captureGameCursorRangeSnap(int col,unsigned int phys,int row)
{
	unsigned int minV=0,maxV=0;
	const bool hasRange=rangeAtRow(row,minV,maxV);
	if(COL_X==col)
	{
		snap_x_has_range_=hasRange;
		if(true==hasRange)
		{
			snap_x_min_=minV;
			snap_x_max_=maxV;
		}
	}
	else if(COL_Y==col)
	{
		snap_y_has_range_=hasRange;
		if(true==hasRange)
		{
			snap_y_min_=minV;
			snap_y_max_=maxV;
		}
	}
	(void)phys;
}

void MouseCoordScanWindow::clearGameCursorRangeSnap(int col)
{
	if(COL_X==col)
	{
		snap_x_has_range_=false;
	}
	else if(COL_Y==col)
	{
		snap_y_has_range_=false;
	}
}

QTableWidgetItem *MouseCoordScanWindow::MakeTextItem(
    const QString &text,bool numericSort,qulonglong sortKey) const
{
	auto *it=new SortableItem(text);
	it->setFlags(it->flags()&~Qt::ItemIsEditable);
	if(true==numericSort)
	{
		it->setData(kSortRole,sortKey);
	}
	return it;
}

bool MouseCoordScanWindow::isKeptPhys(unsigned int phys) const
{
	return 0!=phys &&
	       (checked_watch_.contains(phys) || checked_chase_.contains(phys) ||
	        checked_x_.contains(phys) || checked_y_.contains(phys) ||
	        checked_x2_.contains(phys) || checked_y2_.contains(phys));
}

void MouseCoordScanWindow::refreshCheckStates(void)
{
	if(nullptr==cand_table_)
	{
		return;
	}
	const bool wasBlocked=cand_table_->signalsBlocked();
	if(true!=wasBlocked)
	{
		cand_table_->blockSignals(true);
	}
	for(int r=0; r<cand_table_->rowCount(); ++r)
	{
		const unsigned int phys=physAtRow(r);
		if(0==phys)
		{
			continue;
		}
		auto setCol=[&](int col,bool on)
		{
			if(auto *it=cand_table_->item(r,col))
			{
				const Qt::CheckState want=on ? Qt::Checked : Qt::Unchecked;
				if(it->checkState()!=want)
				{
					it->setCheckState(want);
				}
			}
		};
		setCol(COL_WATCH,checked_watch_.contains(phys));
		setCol(COL_CHASE,checked_chase_.contains(phys));
		setCol(COL_X,checked_x_.contains(phys));
		setCol(COL_Y,checked_y_.contains(phys));
		setCol(COL_X2,checked_x2_.contains(phys));
		setCol(COL_Y2,checked_y2_.contains(phys));
	}
	if(true!=wasBlocked)
	{
		cand_table_->blockSignals(false);
	}
}

void MouseCoordScanWindow::updateCandidates(const QVariantList &cands)
{
	if(nullptr==cand_table_)
	{
		return;
	}
	cand_table_->setUpdatesEnabled(false);
	cand_table_->setSortingEnabled(false);
	cand_table_->blockSignals(true);
	QSet<unsigned int> seen;
	int displayRow=0;
	for(const QVariant &v : cands)
	{
		const QVariantMap row=v.toMap();
		const unsigned int phys=row.value(QStringLiteral("phys")).toUInt();
		if(0==phys)
		{
			continue;
		}
		seen.insert(phys);
		int r=rowForPhys(phys);
		const bool isNew=(0>r);
		if(true==isNew)
		{
			r=displayRow;
			if(r>cand_table_->rowCount())
			{
				r=cand_table_->rowCount();
			}
			cand_table_->insertRow(r);
			auto makeCheck=[&](int col)
			{
				auto *cx=new QTableWidgetItem;
				cx->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
				cx->setCheckState(Qt::Unchecked);
				cand_table_->setItem(r,col,cx);
			};
			makeCheck(COL_WATCH);
			makeCheck(COL_CHASE);
			makeCheck(COL_X);
			makeCheck(COL_Y);
			makeCheck(COL_X2);
			makeCheck(COL_Y2);
			auto *ph=MakeTextItem(
			    QStringLiteral("%1").arg(phys,8,16,QLatin1Char('0')),true,phys);
			ph->setData(Qt::UserRole,phys);
			cand_table_->setItem(r,COL_PHYS,ph);
			cand_table_->setItem(r,COL_SIZE,MakeTextItem(QString(),true,0));
			cand_table_->setItem(r,COL_VALUE,MakeTextItem(QString(),true,0));
			cand_table_->setItem(r,COL_RANGE,MakeTextItem(QString(),false,0));
			cand_table_->setItem(r,COL_SPAN,MakeTextItem(QString(),true,0));
			cand_table_->setItem(r,COL_HITS,MakeTextItem(QString(),true,0));
			if(true==row.value(QStringLiteral("from_profile")).toBool())
			{
				const bool firstSeen=true!=profile_marked_.contains(phys);
				profile_marked_.insert(phys);
				if(true==firstSeen)
				{
					if(true==row.value(QStringLiteral("profile_x")).toBool())
					{
						if(true==checked_x_.isEmpty() &&
						   true!=cleared_x_.contains(phys))
						{
							AddUniquePhys(checked_x_,phys);
							captureGameCursorRangeSnap(COL_X,phys,r);
						}
						else if(true!=checked_x_.contains(phys) &&
						        true==checked_x2_.isEmpty() &&
						        true!=cleared_x2_.contains(phys))
						{
							AddUniquePhys(checked_x2_,phys);
						}
					}
					if(true==row.value(QStringLiteral("profile_y")).toBool())
					{
						if(true==checked_y_.isEmpty() &&
						   true!=cleared_y_.contains(phys))
						{
							AddUniquePhys(checked_y_,phys);
							captureGameCursorRangeSnap(COL_Y,phys,r);
						}
						else if(true!=checked_y_.contains(phys) &&
						        true==checked_y2_.isEmpty() &&
						        true!=cleared_y2_.contains(phys))
						{
							AddUniquePhys(checked_y2_,phys);
						}
					}
				}
			}
		}
		if(true==row.value(QStringLiteral("user_watch")).toBool())
		{
			AddUniquePhys(checked_watch_,phys);
		}
		if(true==row.value(QStringLiteral("user_chase")).toBool())
		{
			AddUniquePhys(checked_chase_,phys);
		}

		auto setNum=[&](int col,const QString &text,qulonglong key)
		{
			auto *it=cand_table_->item(r,col);
			if(nullptr==it)
			{
				return;
			}
			if(it->text()!=text)
			{
				it->setText(text);
			}
			if(it->data(kSortRole).toULongLong()!=key)
			{
				it->setData(kSortRole,key);
			}
		};
		const unsigned int size=row.value(QStringLiteral("size")).toUInt();
		const unsigned int value=row.value(QStringLiteral("value")).toUInt();
		const unsigned int hits=row.value(QStringLiteral("hits")).toUInt();
		setNum(COL_SIZE,QString::number(size),size);
		setNum(COL_VALUE,FmtCoordWord(value),value&0xffffu);
		QString rangeStr=QStringLiteral("-");
		QString spanStr=QStringLiteral("-");
		unsigned int minV=0,maxV=0;
		unsigned int span=0;
		if(row.value(QStringLiteral("has_range")).toBool())
		{
			minV=row.value(QStringLiteral("min")).toUInt();
			maxV=row.value(QStringLiteral("max")).toUInt();
			rangeStr=QStringLiteral("%1..%2").arg(FmtCoordWord(minV)).arg(FmtCoordWord(maxV));
			const int sMin=WordAsCoord(minV);
			const int sMax=WordAsCoord(maxV);
			span=static_cast<unsigned int>(sMax-sMin);
			spanStr=QString::number(static_cast<int>(span));
		}
		if(auto *rangeIt=cand_table_->item(r,COL_RANGE))
		{
			if(rangeIt->text()!=rangeStr)
			{
				rangeIt->setText(rangeStr);
			}
			rangeIt->setData(Qt::UserRole,row.value(QStringLiteral("has_range")).toBool());
			rangeIt->setData(Qt::UserRole+1,minV);
			rangeIt->setData(Qt::UserRole+2,maxV);
		}
		setNum(COL_SPAN,spanStr,span);
		setNum(COL_HITS,QString::number(hits),hits);
		if(auto *ph=cand_table_->item(r,COL_PHYS))
		{
			if(true==row.value(QStringLiteral("soft")).toBool())
			{
				// MOS / TBIOS soft-cursor word.
				ph->setBackground(QColor(200,235,200));
			}
			else if(true==row.value(QStringLiteral("from_profile")).toBool())
			{
				ph->setBackground(QColor(220,235,255));
			}
			else
			{
				ph->setBackground(QBrush());
			}
		}
		++displayRow;
	}
	if(true!=scanChecked())
	{
		for(int row=cand_table_->rowCount()-1; 0<=row; --row)
		{
			const unsigned int phys=physAtRow(row);
			if(0==phys || true==seen.contains(phys) || true==isKeptPhys(phys))
			{
				continue;
			}
			cand_table_->removeRow(row);
		}
	}
	refreshCheckStates();
	cand_table_->blockSignals(false);
	// Keep sort off while scanning; restore when idle (was left off after every refresh).
	cand_table_->setSortingEnabled(true!=scanChecked());
	cand_table_->setUpdatesEnabled(true);
}

void MouseCoordScanWindow::markFollowedSources(const QVariantList &physList)
{
	bool changed=false;
	for(const QVariant &v : physList)
	{
		const unsigned int phys=v.toUInt();
		if(0==phys)
		{
			continue;
		}
		// Newly found SOURCE takes over 追跡 (seed cleared via clearChaseFlags).
		const int chaseBefore=checked_chase_.size();
		AddUniquePhys(checked_chase_,phys);
		if(checked_chase_.size()!=chaseBefore)
		{
			changed=true;
		}
	}
	if(true==changed)
	{
		refreshCheckStates();
		syncWatchChaseToCore();
	}
}

void MouseCoordScanWindow::clearChaseFlags(const QVariantList &physList)
{
	bool changed=false;
	for(const QVariant &v : physList)
	{
		const unsigned int phys=v.toUInt();
		if(0==phys)
		{
			continue;
		}
		if(0!=checked_chase_.removeAll(phys))
		{
			changed=true;
		}
	}
	if(true==changed)
	{
		refreshCheckStates();
		syncWatchChaseToCore();
	}
}

void MouseCoordScanWindow::applyProfileChecks(const QVariantList &cands)
{
	bool profileChanged=false;
	for(const QVariant &v : cands)
	{
		const QVariantMap row=v.toMap();
		const unsigned int phys=row.value(QStringLiteral("phys")).toUInt();
		if(0==phys || true!=row.value(QStringLiteral("from_profile")).toBool())
		{
			continue;
		}
		const bool firstSeen=true!=profile_marked_.contains(phys);
		profile_marked_.insert(phys);
		if(true!=firstSeen)
		{
			continue;
		}
		profileChanged=true;
		if(true==row.value(QStringLiteral("profile_x")).toBool())
		{
			if(true==checked_x_.isEmpty() && true!=cleared_x_.contains(phys))
			{
				AddUniquePhys(checked_x_,phys);
				const int r=rowForPhys(phys);
				if(0<=r)
				{
					captureGameCursorRangeSnap(COL_X,phys,r);
				}
			}
			else if(true!=checked_x_.contains(phys) &&
			        true==checked_x2_.isEmpty() &&
			        true!=cleared_x2_.contains(phys))
			{
				AddUniquePhys(checked_x2_,phys);
			}
		}
		if(true==row.value(QStringLiteral("profile_y")).toBool())
		{
			if(true==checked_y_.isEmpty() && true!=cleared_y_.contains(phys))
			{
				AddUniquePhys(checked_y_,phys);
				const int r=rowForPhys(phys);
				if(0<=r)
				{
					captureGameCursorRangeSnap(COL_Y,phys,r);
				}
			}
			else if(true!=checked_y_.contains(phys) &&
			        true==checked_y2_.isEmpty() &&
			        true!=cleared_y2_.contains(phys))
			{
				AddUniquePhys(checked_y2_,phys);
			}
		}
	}
	if(true==profileChanged)
	{
		refreshCheckStates();
	}
}

void MouseCoordScanWindow::resetForDiscChange(void)
{
	clearCandidateTable();
}

void MouseCoordScanWindow::clearCandidateTable()
{
	if(nullptr==cand_table_)
	{
		return;
	}
	cand_table_->setSortingEnabled(false);
	cand_table_->blockSignals(true);
	cand_table_->setRowCount(0);
	cand_table_->blockSignals(false);
	cand_table_->setSortingEnabled(true);
	checked_watch_.clear();
	checked_chase_.clear();
	checked_x_.clear();
	checked_y_.clear();
	checked_x2_.clear();
	checked_y2_.clear();
	profile_marked_.clear();
	cleared_x_.clear();
	cleared_y_.clear();
	cleared_x2_.clear();
	cleared_y2_.clear();
	snap_x_has_range_=false;
	snap_y_has_range_=false;
	syncWatchChaseToCore();
}

void MouseCoordScanWindow::clearCandidateRanges()
{
	snap_x_has_range_=false;
	snap_y_has_range_=false;
	snap_x_min_=0;
	snap_x_max_=0;
	snap_y_min_=0;
	snap_y_max_=0;
	if(nullptr==cand_table_)
	{
		return;
	}
	cand_table_->blockSignals(true);
	for(int r=0; r<cand_table_->rowCount(); ++r)
	{
		if(auto *rangeIt=cand_table_->item(r,COL_RANGE))
		{
			rangeIt->setText(QStringLiteral("-"));
			rangeIt->setData(Qt::UserRole,false);
			rangeIt->setData(Qt::UserRole+1,0);
			rangeIt->setData(Qt::UserRole+2,0);
		}
	}
	cand_table_->blockSignals(false);
}

void MouseCoordScanWindow::syncWatchChaseToCore(void)
{
	Q_EMIT watchPhysChanged(watchPhysList());
	Q_EMIT chasePhysChanged(chasePhysList());
}

void MouseCoordScanWindow::onTableItemChanged(QTableWidgetItem *item)
{
	if(nullptr==item)
	{
		return;
	}
	const int col=item->column();
	if(COL_WATCH!=col && COL_CHASE!=col &&
	   COL_X!=col && COL_Y!=col && COL_X2!=col && COL_Y2!=col)
	{
		return;
	}
	const unsigned int phys=physAtRow(item->row());
	if(0==phys)
	{
		return;
	}
	QVector<unsigned int> *list=nullptr;
	if(COL_WATCH==col)
	{
		list=&checked_watch_;
	}
	else if(COL_CHASE==col)
	{
		list=&checked_chase_;
	}
	else if(COL_X==col)
	{
		list=&checked_x_;
	}
	else if(COL_Y==col)
	{
		list=&checked_y_;
	}
	else if(COL_X2==col)
	{
		list=&checked_x2_;
	}
	else
	{
		list=&checked_y2_;
	}
	const bool axisCol=
	    COL_X==col || COL_Y==col || COL_X2==col || COL_Y2==col;
	if(Qt::Checked==item->checkState())
	{
		if(true==axisCol)
		{
			// One selection per axis column.
			for(int r=0; r<cand_table_->rowCount(); ++r)
			{
				if(r==item->row())
				{
					continue;
				}
				if(auto *other=cand_table_->item(r,col))
				{
					if(Qt::Checked==other->checkState())
					{
						other->setCheckState(Qt::Unchecked);
					}
				}
			}
			list->clear();
		}
		list->removeAll(phys);
		if(COL_X==col)
		{
			cleared_x_.remove(phys);
		}
		else if(COL_Y==col)
		{
			cleared_y_.remove(phys);
		}
		else if(COL_X2==col)
		{
			cleared_x2_.remove(phys);
		}
		else if(COL_Y2==col)
		{
			cleared_y2_.remove(phys);
		}
		AddUniquePhys(*list,phys);
		if(COL_X==col || COL_Y==col)
		{
			captureGameCursorRangeSnap(col,phys,item->row());
		}
	}
	else
	{
		list->removeAll(phys);
		if(COL_X==col)
		{
			cleared_x_.insert(phys);
			clearGameCursorRangeSnap(COL_X);
		}
		else if(COL_Y==col)
		{
			cleared_y_.insert(phys);
			clearGameCursorRangeSnap(COL_Y);
		}
		else if(COL_X2==col)
		{
			cleared_x2_.insert(phys);
		}
		else if(COL_Y2==col)
		{
			cleared_y2_.insert(phys);
		}
	}
	if(COL_WATCH==col || COL_CHASE==col)
	{
		syncWatchChaseToCore();
	}
	else if(true==axisCol)
	{
		refreshCheckStates();
	}
}

MouseCoordGameCursorSelection MouseCoordScanWindow::selectedGameCursor(void) const
{
	MouseCoordGameCursorSelection sel;
	if(true==checked_x_.isEmpty() || true==checked_y_.isEmpty())
	{
		return sel;
	}
	sel.physX=checked_x_.front();
	sel.physY=checked_y_.front();
	sel.hasRangeX=snap_x_has_range_;
	sel.minX=snap_x_min_;
	sel.maxX=snap_x_max_;
	sel.hasRangeY=snap_y_has_range_;
	sel.minY=snap_y_min_;
	sel.maxY=snap_y_max_;
	// After Clear min max, snap is empty — use the live table range.
	if(true!=sel.hasRangeX)
	{
		const int rx=rowForPhys(sel.physX);
		if(0<=rx)
		{
			sel.hasRangeX=rangeAtRow(rx,sel.minX,sel.maxX);
		}
	}
	if(true!=sel.hasRangeY)
	{
		const int ry=rowForPhys(sel.physY);
		if(0<=ry)
		{
			sel.hasRangeY=rangeAtRow(ry,sel.minY,sel.maxY);
		}
	}
	// A single observed sample (min==max) is not a clamp range.
	if(true==sel.hasRangeX && sel.minX>=sel.maxX)
	{
		sel.hasRangeX=false;
	}
	if(true==sel.hasRangeY && sel.minY>=sel.maxY)
	{
		sel.hasRangeY=false;
	}
	return sel;
}

MouseCoordGameCursor2Selection MouseCoordScanWindow::selectedGameCursor2(void) const
{
	MouseCoordGameCursor2Selection sel;
	if(true==checked_x2_.isEmpty() || true==checked_y2_.isEmpty())
	{
		return sel;
	}
	sel.physX=checked_x2_.front();
	sel.physY=checked_y2_.front();
	return sel;
}

QList<QPair<unsigned int,unsigned int>> MouseCoordScanWindow::selectedPairs() const
{
	QList<QPair<unsigned int,unsigned int>> out;
	const auto sel=selectedGameCursor();
	if(true==sel.valid())
	{
		out.append(qMakePair(sel.physX,sel.physY));
	}
	const auto sel2=selectedGameCursor2();
	if(true==sel2.valid())
	{
		out.append(qMakePair(sel2.physX,sel2.physY));
	}
	return out;
}

QVariantList MouseCoordScanWindow::keptPhysList() const
{
	QVariantList out;
	QSet<unsigned int> seen;
	auto add=[&](unsigned int p)
	{
		if(0==p || true==seen.contains(p))
		{
			return;
		}
		seen.insert(p);
		out.append(p);
	};
	for(unsigned int p : checked_watch_)
	{
		add(p);
	}
	for(unsigned int p : checked_chase_)
	{
		add(p);
	}
	for(unsigned int p : checked_x_)
	{
		add(p);
	}
	for(unsigned int p : checked_y_)
	{
		add(p);
	}
	for(unsigned int p : checked_x2_)
	{
		add(p);
	}
	for(unsigned int p : checked_y2_)
	{
		add(p);
	}
	return out;
}

QVariantList MouseCoordScanWindow::watchPhysList() const
{
	QVariantList out;
	for(unsigned int p : checked_watch_)
	{
		out.append(p);
	}
	return out;
}

QVariantList MouseCoordScanWindow::chasePhysList() const
{
	QVariantList out;
	for(unsigned int p : checked_chase_)
	{
		out.append(p);
	}
	return out;
}
