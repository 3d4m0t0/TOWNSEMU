#include "content_browser_widget.h"

#include "townsqt_disc_statesave.h"
#include "townsqt_paths.h"

#include "mouse_coord_write_scan.h"

#include <algorithm>

#include <QApplication>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

namespace
{
/*! Entry / state thumbs use 640:480 (4:3). */
constexpr int kThumbW=64;
constexpr int kThumbH=48;
/*! State-grid base sizes at ~16px default font (100% DE); scaled by fontMetrics. */
constexpr int kStateThumbBaseW=104;
constexpr int kStateThumbBaseH=78;
constexpr int kStateLabelFontBasePx=9;
constexpr int kStateGridHSpacingBase=8;
constexpr int kStateGridVSpacingBase=8;
constexpr int kStateGridLeftPadBase=28;
constexpr int kStateUiRefFontH=16;
constexpr int kStateGridMaxCols=10;
constexpr int kEntryHMargins=8; /*! entryLay left+right */
constexpr int kHoverOverlayFadeMs=280;
constexpr int kHoverDismissMovePx=4; /*! cursor travel before fade-out */
constexpr int kExpandAnimMs=220;
constexpr qreal kHoverOverlayOpacity=1.0;
constexpr char kPropFingerprint[]="townsqtFp";
constexpr char kPropEntryRow[]="townsqtEntryRow";
constexpr char kPropHoverPixmap[]="townsqtHoverPm";
constexpr char kPropHoverSlot[]="townsqtHoverSlot";
constexpr char kPropHoverTime[]="townsqtHoverTime";
constexpr char kPropLaunchFp[]="townsqtLaunchFp";
constexpr char kPropLaunchPath[]="townsqtLaunchPath";
constexpr char kPropLaunchSlot[]="townsqtLaunchSlot";
constexpr char kPropStateExists[]="townsqtStateExists";

QPixmap ComposeThumb43(const QString &path,int w,int h)
{
	QPixmap canvas(w,h);
	if(path.isEmpty())
	{
		canvas.fill(Qt::darkGray);
		return canvas;
	}
	QPixmap pm;
	if(true!=pm.load(path))
	{
		canvas.fill(Qt::darkGray);
		return canvas;
	}
	canvas.fill(Qt::black);
	const QPixmap scaled=pm.scaled(w,h,Qt::KeepAspectRatio,Qt::SmoothTransformation);
	QPainter p(&canvas);
	p.setRenderHint(QPainter::SmoothPixmapTransform,true);
	p.drawPixmap((w-scaled.width())/2,(h-scaled.height())/2,scaled);
	p.end();
	return canvas;
}

/*! White fill with 1px black outline (offset draw; works at small pixel sizes). */
void DrawOutlinedText(QPainter &p,const QRect &r,const QString &text,int align)
{
	const int flags=align|Qt::AlignVCenter;
	p.setPen(Qt::black);
	for(int dy=-1; dy<=1; ++dy)
	{
		for(int dx=-1; dx<=1; ++dx)
		{
			if(0==dx && 0==dy)
			{
				continue;
			}
			p.drawText(r.translated(dx,dy),flags,text);
		}
	}
	p.setPen(Qt::white);
	p.drawText(r,flags,text);
}

QString StateSlotCaption(int slot)
{
	if(0==slot)
	{
		return QStringLiteral("AUTOSAVE");
	}
	return QStringLiteral("#%1").arg(slot);
}

void PaintStateLabels(QPainter &p,int w,int h,int slot,const QString &timeText,int fontPx)
{
	QFont font=p.font();
	font.setPixelSize(qMax(8,fontPx));
	p.setFont(font);
	const QFontMetrics fm(font);
	const int padX=qMax(2,fontPx/3);
	const int padY=qMax(1,fontPx/6);
	const int inset=qMax(2,fontPx/4);

	const QString slotText=StateSlotCaption(slot);
	const QSize slotSz=fm.size(0,slotText);
	QRect slotR(inset,inset,slotSz.width()+2*padX,slotSz.height()+2*padY);
	DrawOutlinedText(p,slotR,slotText,Qt::AlignCenter);

	const QSize timeSz=fm.size(0,timeText);
	QRect timeR(
	    0,
	    h-inset-(timeSz.height()+2*padY),
	    timeSz.width()+2*padX,
	    timeSz.height()+2*padY);
	timeR.moveLeft(qMax(inset,(w-timeR.width())/2));
	if(timeR.right()>w-inset-1)
	{
		timeR.moveRight(w-inset-1);
	}
	DrawOutlinedText(p,timeR,timeText,Qt::AlignCenter);
}

QPixmap ComposeStateThumb(const QString &path,int w,int h,int slot,const QString &timeText,int fontPx)
{
	QPixmap canvas=ComposeThumb43(path,w,h);
	QPainter p(&canvas);
	p.setRenderHint(QPainter::TextAntialiasing,true);
	PaintStateLabels(p,w,h,slot,timeText,fontPx);
	p.end();
	return canvas;
}

QPixmap LoadHoverSource(const QString &path)
{
	QPixmap pm;
	if(path.isEmpty() || true!=pm.load(path))
	{
		return {};
	}
	return pm;
}

QString FormatStateTime(const QString &statePath)
{
	const QFileInfo fi(statePath);
	if(true!=fi.exists())
	{
		return QStringLiteral("—");
	}
	return fi.lastModified().toString(QStringLiteral("yy-MM-dd HH:mm"));
}

QString MouseIntegrationLabel(const MouseCoordWriteScan::Profile &p,const QObject *trCtx)
{
	using Mode=MouseCoordWriteScan;
	if(true!=p.enabled || Mode::INTEGRATION_DIFFERENTIAL==p.integrationMode)
	{
		return trCtx->tr("None");
	}
	if(Mode::INTEGRATION_DIRECT_WRITE==p.integrationMode ||
	   Mode::INTEGRATION_GAME_FEEDBACK==p.integrationMode)
	{
		return trCtx->tr("App");
	}
	return trCtx->tr("MOS");
}

QString FormatDriveList(const MouseCoordWriteScan::MachineSettings &m)
{
	QStringList parts;
	if(true==m.hasFdImg[0] && true!=m.fdImg[0].empty())
	{
		parts << QStringLiteral("FD0");
	}
	if(true==m.hasFdImg[1] && true!=m.fdImg[1].empty())
	{
		parts << QStringLiteral("FD1");
	}
	for(int hd=0; hd<MouseCoordWriteScan::MachineSettings::kHddImgCount; ++hd)
	{
		if(true==m.hasHddImg[hd] && true!=m.hddImg[hd].empty())
		{
			parts << QStringLiteral("HD%1").arg(hd);
		}
	}
	return parts.isEmpty() ? QStringLiteral("—") : parts.join(QStringLiteral(" "));
}

QString FormatProfileSummary(unsigned int fingerprint,const QObject *trCtx)
{
	const QString profilePath=TownsQtDiscStateSave::ProfilePathForFingerprint(fingerprint);
	const QString profileName=QFileInfo(profilePath).fileName();
	QFile file(profilePath);
	if(true!=file.open(QIODevice::ReadOnly))
	{
		return trCtx->tr("%1 · (unreadable)").arg(profileName);
	}
	MouseCoordWriteScan::Profile profile;
	if(true!=MouseCoordWriteScan::Profile::FromIniString(file.readAll().toStdString(),profile))
	{
		return trCtx->tr("%1 · (unreadable)").arg(profileName);
	}
	const QString ram=profile.machine.hasMemSizeInMB ?
	    trCtx->tr("%1 MB").arg(profile.machine.memSizeInMB) :
	    QStringLiteral("—");
	const QString drives=FormatDriveList(profile.machine);
	const QString mouse=MouseIntegrationLabel(profile,trCtx);
	return trCtx->tr("%1 · RAM %2 · %3 · Mouse: %4")
	    .arg(profileName,ram,drives,mouse);
}
}

ContentBrowserWidget::ContentBrowserWidget(QWidget *parent)
	: QWidget(parent)
{
	auto *root=new QVBoxLayout(this);
	root->setContentsMargins(8,8,8,8);
	root->setSpacing(8);

	auto *toolbar=new QHBoxLayout();
	auto *title=new QLabel(tr("Content browser"),this);
	toolbar->addWidget(title);
	toolbar->addStretch(1);
	register_btn_=new QPushButton(tr("Register new"),this);
	connect(register_btn_,&QPushButton::clicked,this,&ContentBrowserWidget::onRegisterClicked);
	toolbar->addWidget(register_btn_);
	auto *closeBtn=new QPushButton(tr("Close"),this);
	connect(closeBtn,&QPushButton::clicked,this,[this](){
		Q_EMIT closeRequested();
	});
	toolbar->addWidget(closeBtn);
	root->addLayout(toolbar);

	scroll_=new QScrollArea(this);
	scroll_->setWidgetResizable(true);
	scroll_->setFrameShape(QFrame::NoFrame);
	scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list_host_=new QWidget(scroll_);
	list_host_->setMinimumWidth(0);
	list_layout_=new QVBoxLayout(list_host_);
	list_layout_->setContentsMargins(0,0,0,0);
	list_layout_->setSpacing(6);
	list_layout_->addStretch(1);
	scroll_->setWidget(list_host_);
	root->addWidget(scroll_,1);

	/*! In-app overlay (child of top-level window). Wayland ignores
	    WindowStaysOnTopHint on separate xdg_toplevel; a child surface always
	    stacks above siblings. True desktop-wide overlay needs wlr-layer-shell. */
	hover_overlay_=new QLabel(this);
	hover_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
	hover_overlay_->setAttribute(Qt::WA_TranslucentBackground);
	hover_overlay_->setAlignment(Qt::AlignCenter);
	hover_overlay_->setStyleSheet(QStringLiteral("background: transparent;"));
	hover_opacity_=new QGraphicsOpacityEffect(hover_overlay_);
	hover_opacity_->setOpacity(0.0);
	hover_overlay_->setGraphicsEffect(hover_opacity_);
	hover_overlay_->hide();

	hover_fade_=new QPropertyAnimation(hover_opacity_,QByteArrayLiteral("opacity"),this);
	hover_fade_->setDuration(kHoverOverlayFadeMs);
	hover_fade_->setEasingCurve(QEasingCurve::InOutQuad);
	connect(hover_fade_,&QPropertyAnimation::finished,this,[this](){
		if(true!=hover_fading_out_)
		{
			return;
		}
		hover_fading_out_=false;
		hideStateHover();
	});

	updateRegisterButton();
	reload();
}

ContentBrowserWidget::~ContentBrowserWidget()
{
	if(nullptr!=qApp)
	{
		qApp->removeEventFilter(this);
	}
	hideStateHover();
	/*! Parent may be the top-level window; detach before our destruction order differs. */
	if(nullptr!=hover_overlay_)
	{
		hover_overlay_->hide();
		hover_overlay_->setParent(nullptr);
		delete hover_overlay_;
	}
	hover_overlay_=nullptr;
	hover_opacity_=nullptr;
	hover_fade_=nullptr;
}

void ContentBrowserWidget::setRegisterCdPath(const QString &cdPath)
{
	register_cd_path_=cdPath;
	updateRegisterButton();
}

void ContentBrowserWidget::updateRegisterButton(void)
{
	if(nullptr==register_btn_)
	{
		return;
	}
	const bool canRegister=
	    !register_cd_path_.isEmpty() &&
	    QFileInfo::exists(register_cd_path_) &&
	    TownsQtDiscStateSave::ProfileExistsForDiscPath(register_cd_path_);
	register_btn_->setEnabled(canRegister);
	register_btn_->setToolTip(
	    canRegister ?
	        tr("Register the current CD in the content library.") :
	        tr("Requires a mounted CD with a disc profile (fp_XXXXXXXX.ini)."));
}

void ContentBrowserWidget::setActiveProfileFingerprint(unsigned int fingerprint)
{
	active_profile_fingerprint_=fingerprint;
	updateRegisterButton();
}

void ContentBrowserWidget::setWindowScale(int scale)
{
	scale=qMax(1,scale);
	const bool scaleChanged=(window_scale_!=scale);
	window_scale_=scale;
	if(true==scaleChanged && nullptr!=hover_overlay_ && true==hover_overlay_->isVisible())
	{
		hideStateHover();
	}
	if(0!=expanded_fingerprint_ && true==scaleChanged)
	{
		rebuildList(false);
	}
}

ContentBrowserWidget::StateUiMetrics ContentBrowserWidget::stateUiMetrics(void) const
{
	/*! DE UI scale from font height so text scaling and fractional DE scale track together. */
	const qreal s=qMax(
	    1.0,
	    static_cast<qreal>(fontMetrics().height())/static_cast<qreal>(kStateUiRefFontH));
	StateUiMetrics m;
	m.thumbW=qMax(1,qRound(static_cast<qreal>(kStateThumbBaseW)*s));
	m.thumbH=qMax(1,qRound(static_cast<qreal>(kStateThumbBaseH)*s));
	m.hSpacing=qMax(0,qRound(static_cast<qreal>(kStateGridHSpacingBase)*s));
	m.vSpacing=qMax(0,qRound(static_cast<qreal>(kStateGridVSpacingBase)*s));
	m.leftPad=qMax(0,qRound(static_cast<qreal>(kStateGridLeftPadBase)*s));
	m.fontPx=qMax(8,qRound(static_cast<qreal>(kStateLabelFontBasePx)*s));
	return m;
}

int ContentBrowserWidget::stateGridColumns(const StateUiMetrics &metrics) const
{
	int avail=0;
	if(nullptr!=scroll_ && nullptr!=scroll_->viewport())
	{
		avail=scroll_->viewport()->width();
		if(nullptr!=scroll_->verticalScrollBar())
		{
			avail-=scroll_->verticalScrollBar()->sizeHint().width();
		}
	}
	if(avail<=0 && nullptr!=list_host_)
	{
		avail=list_host_->width();
	}
	const int usable=avail-kEntryHMargins-metrics.leftPad;
	if(usable<metrics.thumbW)
	{
		return 1;
	}
	const int step=metrics.thumbW+metrics.hSpacing;
	return qBound(1,(usable+metrics.hSpacing)/step,kStateGridMaxCols);
}

void ContentBrowserWidget::reload(void)
{
	rebuildList(false);
}

void ContentBrowserWidget::refreshStateSlot(unsigned int fingerprint,int slot)
{
	if(nullptr==list_host_ || 0==fingerprint || slot<0 || 9<slot)
	{
		return;
	}
	hideStateHover();
	const StateUiMetrics metrics=stateUiMetrics();
	const QString statePath=TownsQtDiscStateSave::StateSlotPath(slot,fingerprint);
	const bool exists=!statePath.isEmpty() && QFileInfo::exists(statePath);
	const QString imgPath=exists ?
	    TownsQtDiscStateSave::StateSlotImagePath(slot,fingerprint) : QString();
	const QString timeText=FormatStateTime(exists ? statePath : QString());
	const QPixmap thumbPm=ComposeStateThumb(
	    imgPath,metrics.thumbW,metrics.thumbH,slot,timeText,metrics.fontPx);
	const QPixmap hoverPm=LoadHoverSource(imgPath);

	for(QWidget *w : list_host_->findChildren<QWidget*>())
	{
		if(w->property(kPropLaunchFp).toUInt()!=fingerprint ||
		   w->property(kPropLaunchSlot).toInt()!=slot)
		{
			continue;
		}
		QLabel *thumb=qobject_cast<QLabel *>(w);
		if(nullptr==thumb)
		{
			thumb=w->findChild<QLabel*>();
		}
		if(nullptr==thumb)
		{
			continue;
		}
		thumb->setFixedSize(metrics.thumbW,metrics.thumbH);
		thumb->setPixmap(thumbPm);
		w->setFixedSize(metrics.thumbW,metrics.thumbH);
		w->setProperty(kPropStateExists,exists);
		if(true!=hoverPm.isNull())
		{
			w->setProperty(kPropHoverPixmap,QVariant::fromValue(hoverPm));
			w->setProperty(kPropHoverSlot,slot);
			w->setProperty(kPropHoverTime,timeText);
			w->setAttribute(Qt::WA_Hover,true);
		}
		else
		{
			w->setProperty(kPropHoverPixmap,QVariant());
			w->setProperty(kPropHoverSlot,QVariant());
			w->setProperty(kPropHoverTime,QVariant());
		}
	}
}

void ContentBrowserWidget::applyEntryRowStyle(QWidget *row,unsigned int fingerprint) const
{
	if(nullptr==row)
	{
		return;
	}
	if(fingerprint==selected_fingerprint_ && 0!=fingerprint)
	{
		const QColor sel=row->palette().color(QPalette::Highlight);
		row->setStyleSheet(
		    QStringLiteral(
		        "QFrame#townsqtEntryRow {"
		        " background-color: rgba(%1,%2,%3,70);"
		        " border-radius: 4px;"
		        "}")
		        .arg(sel.red())
		        .arg(sel.green())
		        .arg(sel.blue()));
	}
	else
	{
		row->setStyleSheet(
		    QStringLiteral(
		        "QFrame#townsqtEntryRow {"
		        " background-color: transparent;"
		        " border-radius: 4px;"
		        "}"));
	}
}

void ContentBrowserWidget::refreshSelectionStyles(void)
{
	if(nullptr==list_host_)
	{
		return;
	}
	const QList<QFrame *> rows=list_host_->findChildren<QFrame *>(QString(),Qt::FindDirectChildrenOnly);
	for(QFrame *row : rows)
	{
		if(true!=row->property(kPropEntryRow).toBool())
		{
			continue;
		}
		applyEntryRowStyle(row,row->property(kPropFingerprint).toUInt());
	}
}

void ContentBrowserWidget::setSelectedFingerprint(unsigned int fingerprint)
{
	if(selected_fingerprint_==fingerprint)
	{
		return;
	}
	selected_fingerprint_=fingerprint;
	refreshSelectionStyles();
}

void ContentBrowserWidget::setExpandedFingerprint(unsigned int fingerprint)
{
	if(expanded_fingerprint_==fingerprint)
	{
		expanded_fingerprint_=0;
		rebuildList(false);
		return;
	}
	expanded_fingerprint_=fingerprint;
	selected_fingerprint_=fingerprint;
	rebuildList(true);
}

void ContentBrowserWidget::animateStatesReveal(QWidget *statesHost)
{
	if(nullptr==statesHost)
	{
		return;
	}
	QWidget *entry=statesHost->parentWidget();
	if(nullptr==entry)
	{
		entry=statesHost;
	}
	statesHost->setMaximumHeight(0);
	statesHost->show();
	statesHost->setMaximumHeight(QWIDGETSIZE_MAX);
	const int targetH=qMax(1,statesHost->sizeHint().height());
	statesHost->setMaximumHeight(0);

	auto *anim=new QPropertyAnimation(statesHost,QByteArrayLiteral("maximumHeight"),statesHost);
	anim->setDuration(kExpandAnimMs);
	anim->setStartValue(0);
	anim->setEndValue(targetH);
	anim->setEasingCurve(QEasingCurve::OutCubic);
	/*! Follow the growing entry so the full state grid stays in view (smooth with the reveal). */
	connect(anim,&QPropertyAnimation::valueChanged,this,[this,entry](const QVariant &){
		scrollExpandedEntryFollow(entry);
	});
	connect(anim,&QPropertyAnimation::finished,this,[this,statesHost,entry](){
		statesHost->setMaximumHeight(QWIDGETSIZE_MAX);
		scrollExpandedEntryFollow(entry);
	});
	anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ContentBrowserWidget::scrollExpandedEntryFollow(QWidget *entry)
{
	if(nullptr==scroll_ || nullptr==entry)
	{
		return;
	}
	QWidget *viewport=scroll_->viewport();
	QScrollBar *bar=scroll_->verticalScrollBar();
	if(nullptr==viewport || nullptr==bar)
	{
		return;
	}
	constexpr int margin=8;
	const QRect entryInView(entry->mapTo(viewport,QPoint(0,0)),entry->size());
	const int viewTop=margin;
	const int viewBottom=viewport->height()-margin;
	if(entryInView.top()>=viewTop && entryInView.bottom()<=viewBottom)
	{
		return;
	}
	int delta=0;
	if(entryInView.bottom()>viewBottom)
	{
		delta=entryInView.bottom()-viewBottom;
	}
	else if(entryInView.top()<viewTop)
	{
		delta=entryInView.top()-viewTop;
	}
	if(0!=delta)
	{
		bar->setValue(bar->value()+delta);
	}
}

void ContentBrowserWidget::scrollExpandedStatesIntoView(QWidget *statesHost)
{
	if(nullptr==statesHost)
	{
		return;
	}
	QWidget *entry=statesHost->parentWidget();
	scrollExpandedEntryFollow(nullptr!=entry ? entry : statesHost);
}

QRect ContentBrowserWidget::vmDisplayRectInHost(void) const
{
	/*! Content browser shares the central-stack slot with EmuView — same draw area. */
	QWidget *host=nullptr!=window() ? window() : const_cast<ContentBrowserWidget *>(this);
	return QRect(mapTo(host,QPoint(0,0)),size());
}

QRect ContentBrowserWidget::hoverOverlayRect(void) const
{
	return vmDisplayRectInHost();
}

void ContentBrowserWidget::syncHoverOverlayGeometry(void)
{
	if(nullptr==hover_overlay_ || true!=hover_overlay_->isVisible())
	{
		return;
	}
	if(nullptr!=hover_fade_ && QAbstractAnimation::Running==hover_fade_->state())
	{
		return;
	}
	refreshHoverOverlayPixmap();
	QWidget *host=window();
	if(nullptr!=host && hover_overlay_->parentWidget()!=host)
	{
		hover_overlay_->setParent(host);
	}
	hover_overlay_->setGeometry(hoverOverlayRect());
	hover_overlay_->raise();
}

void ContentBrowserWidget::refreshHoverOverlayPixmap(void)
{
	if(nullptr==hover_overlay_ || true==hover_source_.isNull())
	{
		return;
	}
	const QSize box=hoverOverlayRect().size();
	if(box.width()<=0 || box.height()<=0)
	{
		return;
	}
	/*! Fill VM draw area; letterbox/pillarbox with black when the shot is smaller. */
	QPixmap canvas(box);
	canvas.fill(Qt::black);
	const QPixmap scaled=
	    hover_source_.scaled(box,Qt::KeepAspectRatio,Qt::SmoothTransformation);
	QPainter p(&canvas);
	p.setRenderHint(QPainter::SmoothPixmapTransform,true);
	p.setRenderHint(QPainter::TextAntialiasing,true);
	if(true!=scaled.isNull())
	{
		p.drawPixmap((box.width()-scaled.width())/2,(box.height()-scaled.height())/2,scaled);
	}
	const int fontPx=qMax(8,(state_font_px_*box.width())/qMax(1,state_thumb_w_*2));
	PaintStateLabels(p,box.width(),box.height(),hover_slot_,hover_time_text_,fontPx);
	p.end();
	hover_overlay_->setPixmap(canvas);
	hover_overlay_->resize(box);
}

void ContentBrowserWidget::showStateHover(const QPixmap &pixmap,int slot,const QString &timeText)
{
	if(nullptr==hover_overlay_ || nullptr==hover_opacity_ || nullptr==hover_fade_ ||
	   true==pixmap.isNull())
	{
		hideStateHover();
		return;
	}
	/*! Already up: leave as-is so the first click of a double-click does not re-fade. */
	if(true==hover_block_show_until_press_ ||
	   true==hover_pinned_for_launch_ ||
	   true==hover_fading_out_)
	{
		return;
	}
	if(true==hover_overlay_->isVisible())
	{
		return;
	}
	hover_fading_out_=false;
	hover_pinned_for_launch_=false;
	hover_source_=pixmap;
	hover_slot_=slot;
	hover_time_text_=timeText;
	hover_fade_->stop();

	QWidget *host=window();
	if(nullptr==host)
	{
		host=this;
	}
	if(hover_overlay_->parentWidget()!=host)
	{
		hover_overlay_->setParent(host);
	}

	refreshHoverOverlayPixmap();
	hover_overlay_->setGeometry(hoverOverlayRect());
	hover_opacity_->setOpacity(0.0);
	hover_overlay_->show();
	hover_overlay_->raise();
	qApp->removeEventFilter(this);
	qApp->installEventFilter(this);
	hover_show_global_pos_=QCursor::pos();
	hover_arm_move_hide_=true;

	hover_fade_->setDuration(kHoverOverlayFadeMs);
	hover_fade_->setStartValue(0.0);
	hover_fade_->setEndValue(kHoverOverlayOpacity);
	hover_fade_->start();
}

void ContentBrowserWidget::pinStateOverlayForLaunch(void)
{
	if(nullptr==hover_overlay_ || nullptr==hover_opacity_)
	{
		return;
	}
	hover_arm_move_hide_=false;
	hover_fading_out_=false;
	hover_pinned_for_launch_=true;
	hover_block_show_until_press_=true;
	/*! Interrupt fade-in/out and snap to full opacity for launch confirm. */
	if(nullptr!=hover_fade_)
	{
		hover_fade_->stop();
		hover_fade_->setStartValue(kHoverOverlayOpacity);
		hover_fade_->setEndValue(kHoverOverlayOpacity);
	}
	hover_opacity_->setOpacity(kHoverOverlayOpacity);
	if(true==hover_source_.isNull())
	{
		return;
	}
	QWidget *host=window();
	if(nullptr==host)
	{
		host=this;
	}
	if(hover_overlay_->parentWidget()!=host)
	{
		hover_overlay_->setParent(host);
	}
	refreshHoverOverlayPixmap();
	hover_overlay_->setGeometry(hoverOverlayRect());
	hover_overlay_->show();
	hover_overlay_->raise();
	qApp->removeEventFilter(this);
	qApp->installEventFilter(this);
}

void ContentBrowserWidget::notifyVmRunning(void)
{
	if(true!=hover_pinned_for_launch_ &&
	   (nullptr==hover_overlay_ || true!=hover_overlay_->isVisible()))
	{
		return;
	}
	hover_pinned_for_launch_=false;
	fadeOutStateHover();
}

void ContentBrowserWidget::fadeOutStateHover(void)
{
	if(true==hover_pinned_for_launch_)
	{
		return;
	}
	if(nullptr==hover_overlay_ || true!=hover_overlay_->isVisible() ||
	   nullptr==hover_fade_ || nullptr==hover_opacity_)
	{
		return;
	}
	if(true==hover_fading_out_)
	{
		return;
	}
	hover_arm_move_hide_=false;
	hover_fading_out_=true;
	hover_fade_->stop();
	hover_fade_->setDuration(kHoverOverlayFadeMs);
	hover_fade_->setStartValue(hover_opacity_->opacity());
	hover_fade_->setEndValue(0.0);
	hover_fade_->start();
}

void ContentBrowserWidget::hideStateHover(void)
{
	hover_arm_move_hide_=false;
	hover_fading_out_=false;
	hover_pinned_for_launch_=false;
	/*! Keep hover_block_show_until_press_: fade-out must not unlock the
	    trailing double-click release that would re-show the overlay. */
	if(nullptr!=qApp)
	{
		qApp->removeEventFilter(this);
	}
	if(nullptr!=hover_fade_)
	{
		hover_fade_->stop();
	}
	if(nullptr!=hover_opacity_)
	{
		hover_opacity_->setOpacity(0.0);
	}
	hover_source_=QPixmap();
	hover_slot_=0;
	hover_time_text_.clear();
	if(nullptr!=hover_overlay_)
	{
		hover_overlay_->hide();
		hover_overlay_->clear();
	}
}

void ContentBrowserWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	syncHoverOverlayGeometry();
	if(0!=expanded_fingerprint_)
	{
		const StateUiMetrics metrics=stateUiMetrics();
		const int cols=stateGridColumns(metrics);
		if(cols!=state_grid_cols_ ||
		   metrics.thumbW!=state_thumb_w_ ||
		   metrics.thumbH!=state_thumb_h_ ||
		   metrics.hSpacing!=state_h_spacing_ ||
		   metrics.fontPx!=state_font_px_)
		{
			rebuildList(false);
		}
	}
}

bool ContentBrowserWidget::eventFilter(QObject *watched,QEvent *event)
{
	/*! Dismiss preview on cursor move or wheel after click-to-show. */
	if(true==hover_arm_move_hide_ &&
	   true!=hover_pinned_for_launch_ &&
	   nullptr!=hover_overlay_ &&
	   true==hover_overlay_->isVisible())
	{
		if(QEvent::Wheel==event->type())
		{
			fadeOutStateHover();
		}
		else if(QEvent::MouseMove==event->type())
		{
			const QPoint delta=QCursor::pos()-hover_show_global_pos_;
			if(delta.manhattanLength()>=kHoverDismissMovePx)
			{
				fadeOutStateHover();
			}
		}
	}

	if(nullptr!=watched && watched->property(kPropFingerprint).isValid()
	   && true!=watched->property(kPropLaunchFp).isValid())
	{
		if(QEvent::MouseButtonDblClick==event->type())
		{
			auto *me=static_cast<QMouseEvent *>(event);
			if(Qt::LeftButton==me->button())
			{
				const unsigned int fp=watched->property(kPropFingerprint).toUInt();
				setExpandedFingerprint(fp);
				return true;
			}
		}
		if(QEvent::MouseButtonRelease==event->type())
		{
			auto *me=static_cast<QMouseEvent *>(event);
			if(Qt::LeftButton==me->button())
			{
				setSelectedFingerprint(watched->property(kPropFingerprint).toUInt());
				return true;
			}
		}
	}

	if(nullptr!=watched && watched->property(kPropLaunchFp).isValid())
	{
		if(QEvent::MouseButtonPress==event->type())
		{
			auto *me=static_cast<QMouseEvent *>(event);
			if(Qt::LeftButton==me->button())
			{
				hover_block_show_until_press_=false;
			}
		}
		if(QEvent::MouseButtonDblClick==event->type())
		{
			auto *me=static_cast<QMouseEvent *>(event);
			if(Qt::LeftButton==me->button())
			{
				if(true!=watched->property(kPropStateExists).toBool())
				{
					return true;
				}
				const QVariant pmVar=watched->property(kPropHoverPixmap);
				if(true==pmVar.isValid())
				{
					const QPixmap pm=pmVar.value<QPixmap>();
					if(true!=pm.isNull())
					{
						hover_source_=pm;
						hover_slot_=watched->property(kPropHoverSlot).toInt();
						hover_time_text_=watched->property(kPropHoverTime).toString();
					}
				}
				/*! Snap through any in-progress fade-in, then keep pinned until VM runs. */
				pinStateOverlayForLaunch();
				onLaunch(
				    watched->property(kPropLaunchFp).toUInt(),
				    watched->property(kPropLaunchPath).toString(),
				    watched->property(kPropLaunchSlot).toInt());
				return true;
			}
		}
		if(QEvent::MouseButtonRelease==event->type())
		{
			auto *me=static_cast<QMouseEvent *>(event);
			if(Qt::LeftButton==me->button())
			{
				/*! Trailing release of a double-click must not re-show after fade-out. */
				if(true==hover_block_show_until_press_ ||
				   true==hover_pinned_for_launch_ ||
				   true==hover_fading_out_)
				{
					return true;
				}
				const QVariant pmVar=watched->property(kPropHoverPixmap);
				if(true==pmVar.isValid())
				{
					const QPixmap pm=pmVar.value<QPixmap>();
					if(true!=pm.isNull())
					{
						showStateHover(
						    pm,
						    watched->property(kPropHoverSlot).toInt(),
						    watched->property(kPropHoverTime).toString());
					}
				}
				return true;
			}
		}
	}

	return QWidget::eventFilter(watched,event);
}

void ContentBrowserWidget::rebuildList(bool animateExpand)
{
	int preservedScroll=-1;
	if(true!=animateExpand && nullptr!=scroll_ && nullptr!=scroll_->verticalScrollBar())
	{
		preservedScroll=scroll_->verticalScrollBar()->value();
	}

	hideStateHover();
	while(QLayoutItem *item=list_layout_->takeAt(0))
	{
		if(nullptr!=item->widget())
		{
			item->widget()->deleteLater();
		}
		delete item;
	}

	const auto doc=TownsQtContentLibrary::LoadDocument();
	QVector<TownsQtContentLibrary::Entry> entries=doc.entries;
	std::sort(entries.begin(),entries.end(),
	    [](const TownsQtContentLibrary::Entry &a,const TownsQtContentLibrary::Entry &b){
		const int c=QString::localeAwareCompare(a.displayName,b.displayName);
		if(0!=c)
		{
			return c<0;
		}
		return a.fingerprint<b.fingerprint;
	});
	if(entries.isEmpty())
	{
		expanded_fingerprint_=0;
		selected_fingerprint_=0;
		auto *empty=new QLabel(
		    tr("No library entries. Mount a CD with a disc profile, then Register new."),
		    list_host_);
		empty->setWordWrap(true);
		empty->setAttribute(Qt::WA_TranslucentBackground);
		list_layout_->addWidget(empty);
		list_layout_->addStretch(1);
		return;
	}

	const StateUiMetrics metrics=stateUiMetrics();
	state_grid_cols_=stateGridColumns(metrics);
	state_thumb_w_=metrics.thumbW;
	state_thumb_h_=metrics.thumbH;
	state_h_spacing_=metrics.hSpacing;
	state_v_spacing_=metrics.vSpacing;
	state_left_pad_=metrics.leftPad;
	state_font_px_=metrics.fontPx;

	bool expandedStillPresent=false;
	bool selectedStillPresent=false;
	for(const TownsQtContentLibrary::Entry &e : entries)
	{
		if(e.fingerprint==expanded_fingerprint_)
		{
			expandedStillPresent=true;
		}
		if(e.fingerprint==selected_fingerprint_)
		{
			selectedStillPresent=true;
		}
	}
	if(true!=expandedStillPresent)
	{
		expanded_fingerprint_=0;
		animateExpand=false;
	}
	if(true!=selectedStillPresent)
	{
		selected_fingerprint_=0;
	}

	/*! Pinned top row: last auto-resume (state0_*) recorded in the library. */
	{
		const unsigned int lastFp=doc.lastAutosaveFingerprint;
		const int lastIdx=(0!=lastFp) ? TownsQtContentLibrary::IndexOfFingerprint(entries,lastFp) : -1;
		if(0<=lastIdx)
		{
			const TownsQtContentLibrary::Entry &last=entries[lastIdx];
			const QString statePath=TownsQtDiscStateSave::StateSlotPath(0,last.fingerprint);
			if(!statePath.isEmpty() && QFileInfo::exists(statePath))
			{
				const QString imgPath=TownsQtDiscStateSave::StateSlotImagePath(0,last.fingerprint);
				const QString timeText=FormatStateTime(statePath);

				auto *pin=new QFrame(list_host_);
				pin->setObjectName(QStringLiteral("townsqtLastAutosaveRow"));
				pin->setFrameShape(QFrame::NoFrame);
				pin->setAutoFillBackground(false);
				pin->setCursor(Qt::PointingHandCursor);
				/*! Launch from row; hover overlay only on the thumb below. */
				pin->setProperty(kPropLaunchFp,last.fingerprint);
				pin->setProperty(kPropLaunchPath,last.cdImagePath);
				pin->setProperty(kPropLaunchSlot,0);
				pin->setProperty(kPropStateExists,true);
				pin->installEventFilter(this);
				auto *pinLay=new QHBoxLayout(pin);
				pinLay->setContentsMargins(4,6,4,6);
				pinLay->setSpacing(8);

				auto *thumb=new QLabel(pin);
				thumb->setFixedSize(metrics.thumbW,metrics.thumbH);
				thumb->setPixmap(ComposeStateThumb(
				    imgPath,metrics.thumbW,metrics.thumbH,0,timeText,metrics.fontPx));
				thumb->setAlignment(Qt::AlignCenter);
				thumb->setCursor(Qt::PointingHandCursor);
				thumb->setToolTip(tr("Double-click to resume"));
				const QPixmap hoverPm=LoadHoverSource(imgPath);
				if(true!=hoverPm.isNull())
				{
					thumb->setProperty(kPropHoverPixmap,QVariant::fromValue(hoverPm));
					thumb->setProperty(kPropHoverSlot,0);
					thumb->setProperty(kPropHoverTime,timeText);
					thumb->setAttribute(Qt::WA_Hover,true);
				}
				thumb->setProperty(kPropLaunchFp,last.fingerprint);
				thumb->setProperty(kPropLaunchPath,last.cdImagePath);
				thumb->setProperty(kPropLaunchSlot,0);
				thumb->setProperty(kPropStateExists,true);
				thumb->installEventFilter(this);
				pinLay->addWidget(thumb,0,Qt::AlignTop);

				auto *textCol=new QVBoxLayout();
				textCol->setContentsMargins(0,0,0,0);
				textCol->setSpacing(2);
				auto *title=new QLabel(tr("Last autosave"),pin);
				title->setAttribute(Qt::WA_TransparentForMouseEvents);
				{
					QFont f=title->font();
					f.setBold(true);
					title->setFont(f);
				}
				textCol->addWidget(title);
				auto *nameLbl=new QLabel(last.displayName,pin);
				nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
				textCol->addWidget(nameLbl);
				auto *metaLbl=new QLabel(
				    tr("AUTOSAVE · %1 — double-click to resume").arg(timeText),pin);
				metaLbl->setWordWrap(true);
				metaLbl->setStyleSheet(
				    QStringLiteral("color: gray; font-size: 11px; background: transparent;"));
				metaLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
				textCol->addWidget(metaLbl);
				textCol->addStretch(1);
				pinLay->addLayout(textCol,1);

				list_layout_->addWidget(pin);
			}
		}
	}

	for(const TownsQtContentLibrary::Entry &e : entries)
	{
		auto *entry=new QFrame(list_host_);
		entry->setObjectName(QStringLiteral("townsqtEntryRow"));
		entry->setFrameShape(QFrame::NoFrame);
		entry->setAutoFillBackground(false);
		entry->setProperty(kPropEntryRow,true);
		entry->setProperty(kPropFingerprint,e.fingerprint);
		applyEntryRowStyle(entry,e.fingerprint);
		auto *entryLay=new QVBoxLayout(entry);
		entryLay->setContentsMargins(4,4,4,4);
		entryLay->setSpacing(4);

		auto *header=new QWidget(entry);
		header->setAutoFillBackground(false);
		header->setAttribute(Qt::WA_TranslucentBackground);
		header->setCursor(Qt::PointingHandCursor);
		header->setProperty(kPropFingerprint,e.fingerprint);
		header->setProperty(kPropLaunchPath,e.cdImagePath);
		header->installEventFilter(this);
		auto *headLay=new QHBoxLayout(header);
		headLay->setContentsMargins(0,0,0,0);
		headLay->setSpacing(8);

		auto *menu=new QMenu(header);
		menu->addAction(tr("Cold start"),this,[this,fp=e.fingerprint,path=e.cdImagePath](){
			onLaunch(fp,path,-1);
		});
		menu->addSeparator();
		menu->addAction(tr("Change icon…"),this,[this,fp=e.fingerprint](){
			onChangeIcon(fp);
		});
		menu->addAction(tr("Rename"),this,[this,fp=e.fingerprint,name=e.displayName](){
			onRenameEntry(fp,name);
		});
		menu->addSeparator();
		menu->addAction(tr("Remove"),this,[this,fp=e.fingerprint](){
			onRemoveEntry(fp);
		});

		auto *iconLbl=new QLabel(header);
		iconLbl->setPixmap(ComposeThumb43(
		    TownsQtContentLibrary::AbsoluteIconPath(e),kThumbW,kThumbH));
		iconLbl->setFixedSize(kThumbW,kThumbH);
		iconLbl->setAlignment(Qt::AlignCenter);
		iconLbl->setCursor(Qt::PointingHandCursor);
		iconLbl->setToolTip(
		    tr("Click: select\nDouble-click: expand / collapse states\nRight-click: entry menu"));
		iconLbl->setProperty(kPropFingerprint,e.fingerprint);
		iconLbl->setProperty(kPropLaunchPath,e.cdImagePath);
		iconLbl->installEventFilter(this);
		iconLbl->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(iconLbl,&QLabel::customContextMenuRequested,this,[menu,iconLbl](const QPoint &pos){
			menu->exec(iconLbl->mapToGlobal(pos));
		});
		headLay->addWidget(iconLbl,0,Qt::AlignTop);

		auto *textCol=new QVBoxLayout();
		textCol->setContentsMargins(0,0,0,0);
		textCol->setSpacing(2);
		auto *nameLbl=new QLabel(e.displayName,header);
		nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
		textCol->addWidget(nameLbl);
		auto *metaLbl=new QLabel(FormatProfileSummary(e.fingerprint,this),header);
		metaLbl->setWordWrap(true);
		metaLbl->setStyleSheet(QStringLiteral("color: gray; font-size: 11px; background: transparent;"));
		metaLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
		textCol->addWidget(metaLbl);
		textCol->addStretch(1);
		headLay->addLayout(textCol,1);

		entryLay->addWidget(header);

		if(e.fingerprint==expanded_fingerprint_)
		{
			auto *statesHost=new QWidget(entry);
			statesHost->setAutoFillBackground(false);
			statesHost->setAttribute(Qt::WA_TranslucentBackground);
			statesHost->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
			statesHost->setMaximumWidth(qMax(1,scroll_->viewport()->width()-kEntryHMargins));
			auto *grid=new QGridLayout(statesHost);
			grid->setContentsMargins(metrics.leftPad,4,0,4);
			grid->setHorizontalSpacing(metrics.hSpacing);
			grid->setVerticalSpacing(metrics.vSpacing);
			grid->setAlignment(Qt::AlignLeft|Qt::AlignTop);

			const bool canSave=
			    0!=active_profile_fingerprint_ && active_profile_fingerprint_==e.fingerprint;

			auto makeStateCell=[&](int slot)->QWidget *{
				auto *cell=new QWidget(statesHost);
				cell->setAutoFillBackground(false);
				cell->setAttribute(Qt::WA_TranslucentBackground);
				cell->setCursor(Qt::PointingHandCursor);
				cell->setFixedSize(metrics.thumbW,metrics.thumbH);
				cell->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
				auto *lay=new QVBoxLayout(cell);
				lay->setContentsMargins(0,0,0,0);
				lay->setSpacing(0);

				const QString statePath=TownsQtDiscStateSave::StateSlotPath(slot,e.fingerprint);
				const bool exists=!statePath.isEmpty() && QFileInfo::exists(statePath);
				const QString imgPath=exists ?
				    TownsQtDiscStateSave::StateSlotImagePath(slot,e.fingerprint) : QString();
				const QString timeText=FormatStateTime(exists ? statePath : QString());

				auto *thumb=new QLabel(cell);
				thumb->setFixedSize(metrics.thumbW,metrics.thumbH);
				thumb->setPixmap(ComposeStateThumb(
				    imgPath,metrics.thumbW,metrics.thumbH,slot,timeText,metrics.fontPx));
				thumb->setAlignment(Qt::AlignCenter);
				thumb->setAttribute(Qt::WA_TransparentForMouseEvents);
				lay->addWidget(thumb);

				cell->setProperty(kPropLaunchFp,e.fingerprint);
				cell->setProperty(kPropLaunchPath,e.cdImagePath);
				cell->setProperty(kPropLaunchSlot,slot);
				cell->setProperty(kPropStateExists,exists);
				cell->setAttribute(Qt::WA_Hover,true);
				cell->installEventFilter(this);

				const QPixmap hoverPm=LoadHoverSource(imgPath);
				if(true!=hoverPm.isNull())
				{
					cell->setProperty(kPropHoverPixmap,QVariant::fromValue(hoverPm));
					cell->setProperty(kPropHoverSlot,slot);
					cell->setProperty(kPropHoverTime,timeText);
				}

				/*! Slot 0 (AUTOSAVE): double-click load only — no Save/Delete menu. */
				if(0!=slot)
				{
					cell->setContextMenuPolicy(Qt::CustomContextMenu);
					connect(cell,&QWidget::customContextMenuRequested,this,
					    [this,cell,fp=e.fingerprint,slot](const QPoint &pos){
						hideStateHover();
						const bool exists=cell->property(kPropStateExists).toBool();
						const bool canSave=
						    0!=active_profile_fingerprint_ &&
						    active_profile_fingerprint_==fp;
						QMenu menu(cell);
						QAction *saveAct=menu.addAction(tr("Save"));
						saveAct->setEnabled(canSave);
						QAction *delAct=menu.addAction(tr("Delete"));
						delAct->setEnabled(exists);
						QAction *chosen=menu.exec(cell->mapToGlobal(pos));
						if(chosen==saveAct)
						{
							Q_EMIT saveStateRequested(fp,slot);
						}
						else if(chosen==delAct)
						{
							Q_EMIT deleteStateRequested(fp,slot);
						}
					});
				}
				return cell;
			};

			for(int slot=0; slot<=9; ++slot)
			{
				grid->addWidget(
				    makeStateCell(slot),
				    slot/state_grid_cols_,
				    slot%state_grid_cols_);
			}
			for(int col=0; col<state_grid_cols_; ++col)
			{
				grid->setColumnMinimumWidth(col,metrics.thumbW);
				grid->setColumnStretch(col,0);
			}
			/*! Absorb leftover width so the grid does not force horizontal scroll. */
			grid->setColumnStretch(state_grid_cols_,1);

			entryLay->addWidget(statesHost,0,Qt::AlignLeft);
			if(true==animateExpand)
			{
				animateStatesReveal(statesHost);
			}
		}

		list_layout_->addWidget(entry);
	}
	list_layout_->addStretch(1);
	syncHoverOverlayGeometry();

	/*! Save/delete/reload: keep scroll; only fresh expand may scroll into view. */
	if(0<=preservedScroll)
	{
		QTimer::singleShot(0,this,[this,preservedScroll](){
			if(nullptr==scroll_ || nullptr==scroll_->verticalScrollBar())
			{
				return;
			}
			scroll_->verticalScrollBar()->setValue(preservedScroll);
		});
	}
}

void ContentBrowserWidget::onRegisterClicked(void)
{
	QString cdPath=register_cd_path_;
	if(cdPath.isEmpty() || true!=QFileInfo::exists(cdPath))
	{
		return;
	}
	const QString canonical=QFileInfo(cdPath).canonicalFilePath();
	if(!canonical.isEmpty())
	{
		cdPath=canonical;
	}
	const unsigned int fp=TownsQtDiscStateSave::FingerprintForDiscPath(cdPath);
	if(0==fp)
	{
		QMessageBox::warning(this,tr("Register new"),tr("Could not compute a disc fingerprint for this image."));
		return;
	}
	if(true!=TownsQtDiscStateSave::ProfileExistsForFingerprint(fp))
	{
		updateRegisterButton();
		return;
	}
	QString err;
	const QString name=TownsQtContentLibrary::DefaultDisplayName(cdPath,fp);
	if(true!=TownsQtContentLibrary::AddOrUpdateEntry(fp,cdPath,name,QString(),&err))
	{
		QMessageBox::warning(this,tr("Register new"),tr("Failed to register:\n%1").arg(err));
		return;
	}
	register_cd_path_=cdPath;
	reload();
}

void ContentBrowserWidget::onChangeIcon(unsigned int fingerprint)
{
	const QString iconSrc=QFileDialog::getOpenFileName(
	    this,
	    tr("Select icon image"),
	    TownsQtPaths::imageDir(),
	    tr("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)"));
	if(iconSrc.isEmpty())
	{
		return;
	}
	QString err;
	if(true!=TownsQtContentLibrary::SetIcon(fingerprint,iconSrc,&err))
	{
		QMessageBox::warning(this,tr("Change icon"),tr("Failed:\n%1").arg(err));
		return;
	}
	reload();
}

void ContentBrowserWidget::onRenameEntry(unsigned int fingerprint,const QString &currentName)
{
	bool ok=false;
	const QString name=QInputDialog::getText(
	    this,
	    tr("Rename"),
	    tr("Display name:"),
	    QLineEdit::Normal,
	    currentName,
	    &ok);
	if(true!=ok)
	{
		return;
	}
	QString err;
	if(true!=TownsQtContentLibrary::SetDisplayName(fingerprint,name,&err))
	{
		QMessageBox::warning(this,tr("Rename"),tr("Failed:\n%1").arg(err));
		return;
	}
	reload();
}

void ContentBrowserWidget::onRemoveEntry(unsigned int fingerprint)
{
	const auto reply=QMessageBox::question(
	    this,
	    tr("Remove"),
	    tr("Remove this entry from the content library?\n"
	       "(Disc profile and state saves are kept.)"));
	if(QMessageBox::Yes!=reply)
	{
		return;
	}
	QString err;
	if(true!=TownsQtContentLibrary::RemoveEntry(fingerprint,&err))
	{
		QMessageBox::warning(this,tr("Remove"),tr("Failed:\n%1").arg(err));
		return;
	}
	if(expanded_fingerprint_==fingerprint)
	{
		expanded_fingerprint_=0;
	}
	reload();
}

void ContentBrowserWidget::onLaunch(unsigned int fingerprint,const QString &cdImagePath,int stateSlot)
{
	QString path=cdImagePath;
	unsigned int liveFp=TownsQtDiscStateSave::FingerprintForDiscPath(path);
	if(path.isEmpty() || true!=QFileInfo::exists(path) || liveFp!=fingerprint)
	{
		path=QFileDialog::getOpenFileName(
		    this,
		    tr("Locate CD image"),
		    QFileInfo(cdImagePath).absolutePath(),
		    tr("CD images (*.cue *.iso *.mds *.ccd *.bin);;All files (*)"));
		if(path.isEmpty())
		{
			return;
		}
		const QString canonical=QFileInfo(path).canonicalFilePath();
		if(!canonical.isEmpty())
		{
			path=canonical;
		}
		liveFp=TownsQtDiscStateSave::FingerprintForDiscPath(path);
		if(liveFp!=fingerprint)
		{
			QMessageBox::warning(
			    this,
			    tr("Launch"),
			    tr("Selected image fingerprint does not match this library entry."));
			return;
		}
		TownsQtContentLibrary::UpdateCdImagePath(fingerprint,path,nullptr);
	}
	TownsQtContentLibrary::TouchLastLaunched(fingerprint);
	Q_EMIT launchRequested(fingerprint,path,stateSlot);
}
