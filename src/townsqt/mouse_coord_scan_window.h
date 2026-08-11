#pragma once

#include "debug_text_window.h"

#include <QList>
#include <QPair>
#include <QSet>
#include <QVariantList>
#include <QVector>

struct MouseCoordGameCursorSelection
{
	unsigned int physX=0;
	unsigned int physY=0;
	unsigned int minX=0;
	unsigned int maxX=0;
	unsigned int minY=0;
	unsigned int maxY=0;
	bool hasRangeX=false;
	bool hasRangeY=false;
	bool valid(void) const{return 0!=physX && 0!=physY;}
};

/*! Optional second Game Phys pair from Memory scan X2/Y2 (no separate range). */
struct MouseCoordGameCursor2Selection
{
	unsigned int physX=0;
	unsigned int physY=0;
	bool valid(void) const{return 0!=physX && 0!=physY;}
};

class QCheckBox;
class QPushButton;
class QTableWidget;

/*! Memory scan: word/dword stores that change with captured mouse input. */
class MouseCoordScanWindow : public DebugTextWindow
{
	Q_OBJECT

public:
	explicit MouseCoordScanWindow(QWidget *parent=nullptr);

Q_SIGNALS:
	void clearRequested();
	void clearRangesRequested();
	void scanToggled(bool enabled);
	void captureToggled(bool enabled);
	void scanStopRequested();
	void editProfileRequested();
	void cancelRequested();
	void keepOnlyRequested(const QVariantList &physList);
	void watchPhysChanged(const QVariantList &physList);
	void chasePhysChanged(const QVariantList &physList);

public:
	void setScanChecked(bool on);
	bool scanChecked(void) const;
	void setCaptureChecked(bool on);
	bool captureChecked(void) const;

	void updateCandidates(const QVariantList &cands);
	void refreshCheckStates(void);
	void markFollowedSources(const QVariantList &physList);
	/*! Clear 追跡 on seeds after chase moved to their SOURCE. */
	void clearChaseFlags(const QVariantList &physList);
	void applyProfileChecks(const QVariantList &cands);
	void clearCandidateTable();
	void clearCandidateRanges();
	void resetForDiscChange();
	MouseCoordGameCursorSelection selectedGameCursor(void) const;
	MouseCoordGameCursor2Selection selectedGameCursor2(void) const;
	QList<QPair<unsigned int,unsigned int>> selectedPairs() const;
	QVariantList keptPhysList() const;
	QVariantList watchPhysList() const;
	QVariantList chasePhysList() const;

protected:
	void keyPressEvent(class QKeyEvent *event) override;
	void closeEvent(class QCloseEvent *event) override;
	void showEvent(class QShowEvent *event) override;

private:
	int rowForPhys(unsigned int phys) const;
	unsigned int physAtRow(int row) const;
	bool rangeAtRow(int row,unsigned int &minVal,unsigned int &maxVal) const;
	void captureGameCursorRangeSnap(int col,unsigned int phys,int row);
	void clearGameCursorRangeSnap(int col);
	void onTableItemChanged(class QTableWidgetItem *item);
	void syncWatchChaseToCore(void);
	void syncCaptureChecked(bool on);
	bool isKeptPhys(unsigned int phys) const;
	void fitColumnsToLabels(void);
	class QTableWidgetItem *MakeTextItem(const QString &text,bool numericSort,qulonglong sortKey) const;

	QPushButton *scan_btn_=nullptr;
	QCheckBox *capture_chk_=nullptr;
	QTableWidget *cand_table_=nullptr;
	QVector<unsigned int> checked_watch_;
	QVector<unsigned int> checked_chase_;
	QVector<unsigned int> checked_x_;
	QVector<unsigned int> checked_y_;
	QVector<unsigned int> checked_x2_;
	QVector<unsigned int> checked_y2_;
	QSet<unsigned int> profile_marked_;
	QSet<unsigned int> cleared_x_;
	QSet<unsigned int> cleared_y_;
	QSet<unsigned int> cleared_x2_;
	QSet<unsigned int> cleared_y2_;
	unsigned int snap_x_min_=0;
	unsigned int snap_x_max_=0;
	unsigned int snap_y_min_=0;
	unsigned int snap_y_max_=0;
	bool snap_x_has_range_=false;
	bool snap_y_has_range_=false;
};
