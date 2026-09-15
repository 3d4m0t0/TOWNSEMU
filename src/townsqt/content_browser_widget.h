#pragma once

#include <QPixmap>
#include <QWidget>

#include "townsqt_content_library.h"

class QGraphicsOpacityEffect;
class QLabel;
class QParallelAnimationGroup;
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
	/*! Window scale: 1 → 5×2 state grid; ≥2 → 10×1 single row. */
	void setWindowScale(int scale);

Q_SIGNALS:
	/*! stateSlot: -2 = default auto-resume (state0_*), -1 = cold, 0..9 = stateN_*. */
	void launchRequested(unsigned int fingerprint,const QString &cdImagePath,int stateSlot);
	void saveStateRequested(unsigned int fingerprint,int stateSlot);
	void deleteStateRequested(unsigned int fingerprint,int stateSlot);
	void closeRequested(void);

protected:
	bool eventFilter(QObject *watched,QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void rebuildList(bool animateExpand);
	void animateStatesReveal(QWidget *statesHost);
	void scrollExpandedEntryFollow(QWidget *entry);
	void scrollExpandedStatesIntoView(QWidget *statesHost);
	void setSelectedFingerprint(unsigned int fingerprint);
	void setExpandedFingerprint(unsigned int fingerprint);
	void applyEntryRowStyle(QWidget *row,unsigned int fingerprint) const;
	void refreshSelectionStyles(void);
	void showStateHover(const QPixmap &pixmap,int slot,const QString &timeText);
	void hideStateHover(void);
	void syncHoverOverlayGeometry(void);
	void refreshHoverOverlayPixmap(void);
	QSize hoverOverlaySize(void) const;
	QRect hoverOverlayRect(int yBias) const;
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
	int state_grid_cols_=5;
	QPushButton *register_btn_=nullptr;
	QLabel *hover_overlay_=nullptr;
	QScrollArea *scroll_=nullptr;
	QWidget *list_host_=nullptr;
	QVBoxLayout *list_layout_=nullptr;
	QGraphicsOpacityEffect *hover_opacity_=nullptr;
	QPropertyAnimation *hover_fade_=nullptr;
	QPropertyAnimation *hover_move_=nullptr;
	QParallelAnimationGroup *hover_anim_=nullptr;
	QPixmap hover_source_;
	int hover_slot_=0;
	QString hover_time_text_;
};
