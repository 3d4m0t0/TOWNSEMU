#pragma once

#include <QPixmap>
#include <QHash>
#include <QPointer>
#include <QShowEvent>
#include <QWidget>

#include "townsqt_content_library.h"

class QGraphicsOpacityEffect;
class QLabel;
class QPropertyAnimation;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/*! Content-browser list: click selects; double-click expands state grid. */
class ContentBrowserWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ContentBrowserWidget(QWidget *parent=nullptr);
	~ContentBrowserWidget() override;

	void reload(void);
	/*! Update one state thumb in place (no list rebuild / no scroll jump). */
	void refreshStateSlot(unsigned int fingerprint,int slot);
	/*! CD path used when registering a new entry (cached before VM stop). */
	void setRegisterCdPath(const QString &cdPath);
	/*! Running VM disc-profile fingerprint; Save enabled only when it matches an entry. */
	void setActiveProfileFingerprint(unsigned int fingerprint);
	/*! Host window scale (content width); state grid reflows by DE-scaled size + wrap. */
	void setWindowScale(int scale);
	/*! Pin preview for launch; fade-in runs to completion, then pending load may flush. */
	void pinStateOverlayForLaunch(void);
	/*! Keep the pinned preview above the emu view after the stack switches. */
	void raiseStateOverlay(void);
	/*! VM is advancing — fade out a launch-pinned (or lingering) state preview. */
	void notifyVmRunning(void);
	/*! Immediately hide the state preview (e.g. closing the browser without launch). */
	void hideStateHover(void);

Q_SIGNALS:
	/*! stateSlot: -2 = default auto-resume (state0_*), -1 = cold, 0..9 = stateN_*. */
	void launchRequested(unsigned int fingerprint,const QString &cdImagePath,int stateSlot);
	void saveStateRequested(unsigned int fingerprint,int stateSlot);
	void deleteStateRequested(unsigned int fingerprint,int stateSlot);
	void closeRequested(void);

protected:
	bool eventFilter(QObject *watched,QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	void rebuildList(bool animateExpand);
	/*! Rebuild expanded state grid when viewport/host width or DE metrics changed. */
	void reflowStateGridIfNeeded(void);
	struct StateUiMetrics
	{
		int thumbW=104;
		int thumbH=78;
		int hSpacing=8;
		int vSpacing=8;
		int leftPad=28;
		int fontPx=9;
	};
	/*! Thumb / gap / label sizes follow DE font scale. */
	StateUiMetrics stateUiMetrics(void) const;
	/*! How many state thumbs fit on one row in the current viewport. */
	int stateGridColumns(const StateUiMetrics &metrics) const;
	void animateStatesReveal(QWidget *statesHost);
	void scrollExpandedEntryFollow(QWidget *entry);
	void scrollExpandedStatesIntoView(QWidget *statesHost);
	void setSelectedFingerprint(unsigned int fingerprint);
	void setExpandedFingerprint(unsigned int fingerprint);
	void applyEntryRowStyle(QWidget *row,unsigned int fingerprint) const;
	void refreshSelectionStyles(void);
	void showStateHover(QWidget *thumb,const QPixmap &pixmap,int slot,const QString &timeText);
	void fadeOutStateHover(void);
	void flushPendingLaunch(void);
	void syncHoverOverlayGeometry(void);
	void refreshHoverOverlayPixmap(void);
	/*! Same slot as EmuView in the central stack — VM draw area in host coords. */
	QRect vmDisplayRectInHost(void) const;
	QRect hoverOverlayRect(void) const;
	/*! Decode state PNG once per (fp,slot); reuse until path/mtime changes or forceReload. */
	QPixmap stateImageSource(
	    unsigned int fingerprint,
	    int slot,
	    const QString &imgPath,
	    bool forceReload=false);
	void clearStateImageCache(void);
	void invalidateStateImageCache(unsigned int fingerprint);
	void onRegisterClicked(void);
	void updateRegisterButton(void);
	void onChangeIcon(unsigned int fingerprint);
	void onRenameEntry(unsigned int fingerprint,const QString &currentName);
	void onRemoveEntry(unsigned int fingerprint);
	void onLaunch(unsigned int fingerprint,const QString &cdImagePath,int stateSlot);

	QString register_cd_path_;
	unsigned int selected_fingerprint_=0;
	unsigned int expanded_fingerprint_=0;
	unsigned int active_profile_fingerprint_=0;
	int window_scale_=1;
	int state_grid_cols_=1;
	int state_thumb_w_=104;
	int state_thumb_h_=78;
	int state_h_spacing_=8;
	int state_v_spacing_=8;
	int state_left_pad_=28;
	int state_font_px_=9;
	QPushButton *register_btn_=nullptr;
	QLabel *hover_overlay_=nullptr;
	QScrollArea *scroll_=nullptr;
	QWidget *list_host_=nullptr;
	QVBoxLayout *list_layout_=nullptr;
	QGraphicsOpacityEffect *hover_opacity_=nullptr;
	QPropertyAnimation *hover_fade_=nullptr;
	QPixmap hover_source_;
	int hover_slot_=0;
	QString hover_time_text_;
	bool hover_fading_out_=false;
	bool hover_pinned_for_launch_=false;
	/*! After dblclick launch, ignore the trailing release (and any show) until next press. */
	bool hover_block_show_until_press_=false;
	/*! Thumbnail that opened the preview; Leave fades it out. */
	QPointer<QWidget> hover_thumb_;
	/*! Double-click launch deferred until fade-in finishes. */
	bool hover_launch_pending_=false;
	unsigned int pending_launch_fp_=0;
	QString pending_launch_path_;
	int pending_launch_slot_=-1;
	struct StateImageCacheEntry
	{
		QString path;
		qint64 mtimeMs=0;
		QPixmap source;
	};
	QHash<quint64,StateImageCacheEntry> state_image_cache_;
};
