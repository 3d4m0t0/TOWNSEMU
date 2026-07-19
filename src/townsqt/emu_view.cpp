#include "emu_view.h"

#include "emu_gl_view.h"
#include "qt_input_queue.h"
#include "qt_to_fskey.h"
#include "shared_rgba_framebuffer.h"
#include "townsqt_drive_access_overlay.h"

#include <QEnterEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QApplication>
#include <QCursor>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

EmuView::EmuView(QWidget *parent) : QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAutoFillBackground(false);

	gl_view_=new EmuGlView(this);
	gl_view_->setFocusPolicy(Qt::NoFocus);
	gl_view_->setAttribute(Qt::WA_TransparentForMouseEvents,true);
	gl_view_->setCursor(Qt::ArrowCursor);
	setCursor(Qt::ArrowCursor);

	auto *layout=new QVBoxLayout(this);
	layout->setContentsMargins(0,0,0,0);
	layout->setSpacing(0);
	layout->addWidget(gl_view_);

	vsync_timer_=new QTimer(this);
	vsync_timer_->setTimerType(Qt::PreciseTimer);
	connect(vsync_timer_,&QTimer::timeout,this,&EmuView::onSoftwareVsyncTick);

	drive_access_hide_timer_=new QTimer(this);
	drive_access_hide_timer_->setSingleShot(true);
	connect(drive_access_hide_timer_,&QTimer::timeout,this,[this](){
		setDriveAccessOverlayVisible(false);
	});
}

void EmuView::attachFramebuffer(SharedRgbaFramebuffer *framebuffer)
{
	framebuffer_=framebuffer;
	gl_view_->attachFramebuffer(framebuffer);
}

void EmuView::attachInputQueue(QtInputQueue *inputQueue)
{
	inputQueue_=inputQueue;
}

void EmuView::setScale(int scale)
{
	scale_=std::max(1,scale);
	recomputeDisplayLayout();
}

void EmuView::setVideoOptions(int base_scale,bool auto_scale,bool maintain_aspect)
{
	scale_=std::max(1,base_scale);
	auto_scale_=auto_scale;
	maintain_aspect_=maintain_aspect;
	recomputeDisplayLayout();
}

void EmuView::setFullscreenVsync(bool enabled)
{
	fullscreen_vsync_=enabled;
	if(nullptr!=gl_view_)
	{
		gl_view_->setSwapIntervalPreference(enabled ? 1 : 0);
	}
}

void EmuView::recomputeDisplayLayout()
{
	scaled_pixmap_=QPixmap();
	scaled_pixmap_serial_=0;
	int effective_scale=scale_;
	if(auto_scale_ && 0<emu_wid_ && 0<emu_hei_ && 0<width() && 0<height())
	{
		effective_scale=std::max(1,std::min(width()/emu_wid_,height()/emu_hei_));
	}
	if(nullptr!=gl_view_)
	{
		gl_view_->setScale(effective_scale);
		gl_view_->setMaintainAspect(maintain_aspect_);
		gl_view_->setStretchToFill(auto_scale_ && !maintain_aspect_);
	}
	updateGeometry();
	update();
	syncInputDisplayLayout();
}

void EmuView::syncInputDisplayLayout()
{
	if(nullptr==inputQueue_)
	{
		return;
	}
	int x=0,y=0,dst_w=0,dst_h=0;
	queryDisplayRect(x,y,dst_w,dst_h);
	inputQueue_->SetDisplayLayout(emu_wid_,emu_hei_,x,y,dst_w,dst_h);
	inputQueue_->SetDevicePixelRatio(devicePixelRatioF());
	if(nullptr!=gl_view_)
	{
		gl_view_->setLogicalDisplayRect(x,y,dst_w,dst_h);
	}
}

void EmuView::setHostCursorBlank(bool blank)
{
	const Qt::CursorShape shape=blank ? Qt::BlankCursor : Qt::ArrowCursor;
	setCursor(shape);
	if(nullptr!=gl_view_)
	{
		gl_view_->setCursor(shape);
	}
}

void EmuView::setMouseDebugCrosshair(bool enabled)
{
	if(mouse_debug_crosshair_==enabled)
	{
		return;
	}
	mouse_debug_crosshair_=enabled;
	if(nullptr!=gl_view_)
	{
		gl_view_->setMouseDebugCrosshair(enabled);
	}
	update();
}

void EmuView::noteViewMousePosition(const QPoint &view_pos)
{
	last_view_mouse_pos_=view_pos;
	has_view_mouse_pos_=true;
}

QPoint EmuView::hostCursorInView() const
{
	QWidget *top=window();
	if(nullptr==top)
	{
		return mapFromGlobal(QCursor::pos());
	}

	// Qt 6 on Wayland reports QCursor::pos() in top-level window coordinates, not global.
	if(QGuiApplication::platformName()==QLatin1String("wayland"))
	{
		return mapFrom(top,QCursor::pos());
	}

	const QScreen *scr=top->screen();
	const QPoint global=(nullptr!=scr) ? QCursor::pos(scr) : QCursor::pos();
	return mapFromGlobal(global);
}

QPoint EmuView::hostMouseEmuCoords() const
{
	QPoint view_pos;
	if(has_view_mouse_pos_)
	{
		view_pos=last_view_mouse_pos_;
	}
	else
	{
		view_pos=hostCursorInView();
	}
	return mapToEmu(view_pos);
}

void EmuView::setDriveAccessOverlayEnabled(bool enabled)
{
	if(drive_access_overlay_enabled_==enabled)
	{
		return;
	}
	drive_access_overlay_enabled_=enabled;
	if(true!=enabled)
	{
		if(nullptr!=drive_access_hide_timer_)
		{
			drive_access_hide_timer_->stop();
		}
		setDriveAccessOverlayVisible(false);
	}
	if(nullptr!=gl_view_)
	{
		gl_view_->setDriveAccessOverlayEnabled(enabled);
	}
}

void EmuView::setDriveAccessOverlayVisible(bool visible)
{
	if(drive_access_overlay_visible_==visible)
	{
		return;
	}
	drive_access_overlay_visible_=visible;
	update();
	if(nullptr!=gl_view_)
	{
		gl_view_->setDriveAccessOverlayVisible(visible);
	}
}

void EmuView::updateDriveAccessIndicators(const Outside_World::StatusBarInfo &info,
                                          const DriveAccessPresence &presence)
{
	if(true==drive_access_valid_ &&
	   true==DriveAccessStatusEqual(drive_access_,info) &&
	   true==DriveAccessPresenceEqual(drive_access_presence_,presence))
	{
		return;
	}
	drive_access_=info;
	drive_access_presence_=presence;
	drive_access_valid_=true;
	if(nullptr!=gl_view_)
	{
		gl_view_->setDriveAccessIndicators(info,presence);
	}
	if(true==drive_access_overlay_enabled_ && 0<presence.IconCount())
	{
		setDriveAccessOverlayVisible(true);
		if(nullptr!=drive_access_hide_timer_)
		{
			drive_access_hide_timer_->start(kDriveAccessOverlayHideMs);
		}
	}
	else if(0>=presence.IconCount())
	{
		setDriveAccessOverlayVisible(false);
	}
	update();
	if(nullptr!=gl_view_)
	{
		gl_view_->update();
	}
}

void EmuView::paintDriveAccessOverlay(QPainter &painter)
{
	if(true!=drive_access_overlay_enabled_ ||
	   true!=drive_access_overlay_visible_ ||
	   true!=drive_access_valid_ ||
	   0>=drive_access_presence_.IconCount())
	{
		return;
	}
	constexpr int kMargin=2;
	DrawDriveAccessOverlay(
	    painter,
	    DriveAccessOverlayOriginX(width(),drive_access_presence_),
	    height()-kDriveAccessIconSize-kMargin,
	    drive_access_,
	    drive_access_presence_);
}

void EmuView::pollMousePosition()
{
	if(nullptr==inputQueue_ || !isVisible())
	{
		return;
	}
	inputQueue_->SetViewSize(width(),height());
	syncInputDisplayLayout();
	QPoint view_pos;
	if(underMouse() && has_view_mouse_pos_)
	{
		view_pos=last_view_mouse_pos_;
	}
	else
	{
		view_pos=hostCursorInView();
		noteViewMousePosition(view_pos);
	}
	const Qt::MouseButtons buttons=QApplication::mouseButtons();
	const auto emu_pos=mapToEmu(view_pos);
	if(true==mouse_debug_crosshair_ && nullptr!=gl_view_)
	{
		gl_view_->setMouseDebugCrosshairEmuPos(emu_pos.x(),emu_pos.y());
	}
	bool lb=(buttons & Qt::LeftButton)!=0;
	bool mb=(buttons & Qt::MiddleButton)!=0;
	bool rb=(buttons & Qt::RightButton)!=0;
	// The live button poll bypasses the discrete press/release suppression, so re-apply it here:
	// while capture is released no button reaches the guest (the host cursor drives the UI), and a
	// button whose press only resumed capture stays masked until it is physically released — so
	// the click that starts capture never registers as an in-game click.
	if(true==mouse_capture_released_)
	{
		lb=false;
		mb=false;
		rb=false;
	}
	else
	{
		if(0!=(suppressed_guest_buttons_&QtInputQueue::MOUSE_BTN_LEFT))   { lb=false; }
		if(0!=(suppressed_guest_buttons_&QtInputQueue::MOUSE_BTN_RIGHT))  { rb=false; }
		if(0!=(suppressed_guest_buttons_&QtInputQueue::MOUSE_BTN_MIDDLE)) { mb=false; }
	}
	inputQueue_->PollMouseState(
	    lb,
	    mb,
	    rb,
	    view_pos.x(),
	    view_pos.y(),
	    emu_pos.x(),
	    emu_pos.y());
}

void EmuView::queryDisplayRect(int &x,int &y,int &dst_w,int &dst_h) const
{
	int effective_scale=scale_;
	if(auto_scale_ && 0<emu_wid_ && 0<emu_hei_ && 0<width() && 0<height())
	{
		effective_scale=std::max(1,std::min(width()/emu_wid_,height()/emu_hei_));
	}

	if(auto_scale_ && !maintain_aspect_)
	{
		x=0;
		y=0;
		dst_w=width();
		dst_h=height();
		return;
	}

	dst_w=emu_wid_*effective_scale;
	dst_h=emu_hei_*effective_scale;
	x=(width()-dst_w)/2;
	y=(height()-dst_h)/2;
}

QSize EmuView::sizeHint() const
{
	return QSize(emu_wid_*scale_,emu_hei_*scale_);
}

void EmuView::decideRenderBackend()
{
	if(gl_mode_decided_)
	{
		return;
	}
	gl_mode_decided_=true;

	if(gl_view_->isValid())
	{
		use_gl_=true;
		vsync_timer_->stop();
		std::fprintf(stderr,"Tsugaru_QT: OpenGL display backend enabled.\n");
	}
	else
	{
		use_gl_=false;
		gl_view_->hide();
		startSoftwareVsync();
		std::fprintf(stderr,"Tsugaru_QT: OpenGL unavailable, using software rendering.\n");
	}
}

void EmuView::startSoftwareVsync()
{
	double hz=60.0;
	if(nullptr!=window() && nullptr!=window()->screen())
	{
		hz=window()->screen()->refreshRate();
	}
	if(hz<=0.0)
	{
		hz=60.0;
	}
	const int interval_ms=std::max(1,static_cast<int>(std::lround(1000.0/hz)));
	vsync_timer_->setInterval(interval_ms);
	vsync_timer_->start();
	std::fprintf(stderr,"Tsugaru_QT: software display VSync ~%.1f Hz (%d ms).\n",hz,interval_ms);
}

void EmuView::onSoftwareVsyncTick()
{
	if(!software_present_pending_)
	{
		return;
	}
	software_present_pending_=false;
	update();
}

void EmuView::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	decideRenderBackend();
}

void EmuView::refreshFrame()
{
	if(!gl_mode_decided_)
	{
		decideRenderBackend();
	}

	if(use_gl_)
	{
		gl_view_->refreshFrame();
		if(nullptr!=framebuffer_)
		{
			const unsigned char *rgba=nullptr;
			unsigned int wid=0,hei=0;
			uint64_t serial=0;
			if(true==framebuffer_->Acquire(&rgba,&wid,&hei,&serial) && serial!=last_serial_)
			{
				const bool size_changed=(static_cast<int>(wid)!=emu_wid_ || static_cast<int>(hei)!=emu_hei_);
				emu_wid_=static_cast<int>(wid);
				emu_hei_=static_cast<int>(hei);
				last_serial_=serial;
				if(size_changed)
				{
					updateGeometry();
					recomputeDisplayLayout();
				}
			}
		}
		if(nullptr!=inputQueue_)
		{
			inputQueue_->SetViewSize(width(),height());
			syncInputDisplayLayout();
		}
		return;
	}

	refreshFrameSoftware();
}

void EmuView::refreshFrameSoftware()
{
	if(nullptr==framebuffer_)
	{
		return;
	}

	const unsigned char *rgba=nullptr;
	unsigned int wid=0,hei=0;
	uint64_t serial=0;
	if(true!=framebuffer_->Acquire(&rgba,&wid,&hei,&serial))
	{
		return;
	}
	if(serial==last_serial_)
	{
		return;
	}

	const bool size_changed=(static_cast<int>(wid)!=emu_wid_ || static_cast<int>(hei)!=emu_hei_);
	emu_wid_=static_cast<int>(wid);
	emu_hei_=static_cast<int>(hei);
	last_serial_=serial;

	image_=QImage(wid,hei,QImage::Format_RGBA8888);
	if(image_.bytesPerLine()==static_cast<int>(wid*4))
	{
		std::memcpy(image_.bits(),rgba,static_cast<size_t>(wid)*hei*4);
		for(int i=3; i<image_.sizeInBytes(); i+=4)
		{
			image_.bits()[i]=255;
		}
	}
	else
	{
		for(unsigned int y=0; y<hei; ++y)
		{
			std::memcpy(
			    image_.scanLine(static_cast<int>(y)),
			    rgba+static_cast<size_t>(y)*wid*4,
			    static_cast<size_t>(wid)*4);
		}
	}
	for(int i=3; i<image_.sizeInBytes(); i+=4)
	{
		image_.bits()[i]=255;
	}

	if(size_changed)
	{
		updateGeometry();
		recomputeDisplayLayout();
	}

	if(nullptr!=inputQueue_)
	{
		inputQueue_->SetViewSize(width(),height());
	}
	syncInputDisplayLayout();

	software_present_pending_=true;
}

void EmuView::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	has_view_mouse_pos_=false;
	recomputeDisplayLayout();
	if(nullptr!=inputQueue_)
	{
		inputQueue_->SetViewSize(width(),height());
	}
	syncInputDisplayLayout();
}

void EmuView::paintEvent(QPaintEvent * /*event*/)
{
	if(use_gl_)
	{
		return;
	}

	QPainter painter(this);
	painter.fillRect(rect(),Qt::black);

	if(image_.isNull() || emu_wid_<=0 || emu_hei_<=0)
	{
		return;
	}

	int x=0,y=0,dst_w=0,dst_h=0;
	queryDisplayRect(x,y,dst_w,dst_h);
	if(dst_w<=0 || dst_h<=0)
	{
		return;
	}

	if(dst_w!=emu_wid_ || dst_h!=emu_hei_)
	{
		const uint64_t cache_key=(static_cast<uint64_t>(dst_w)<<32)|static_cast<uint64_t>(dst_h);
		if(cache_key!=scaled_pixmap_serial_)
		{
			scaled_pixmap_=QPixmap::fromImage(
			    image_.scaled(dst_w,dst_h,Qt::IgnoreAspectRatio,Qt::FastTransformation));
			scaled_pixmap_serial_=cache_key;
		}
		painter.drawPixmap(x,y,scaled_pixmap_);
	}
	else
	{
		painter.drawImage(x,y,image_);
	}
	paintDriveAccessOverlay(painter);
	paintMouseDebugCrosshair(painter);
}

QPoint EmuView::mapToEmu(const QPoint &pos) const
{
	int x0=0,y0=0,dst_w=0,dst_h=0;
	queryDisplayRect(x0,y0,dst_w,dst_h);
	if(dst_w<=0 || dst_h<=0)
	{
		return QPoint(0,0);
	}
	int mx=(pos.x()-x0)*emu_wid_/dst_w;
	int my=(pos.y()-y0)*emu_hei_/dst_h;
	mx=std::clamp(mx,0,emu_wid_-1);
	my=std::clamp(my,0,emu_hei_-1);
	return QPoint(mx,my);
}

QPoint EmuView::mapFromEmu(int emu_x,int emu_y) const
{
	int x0=0,y0=0,dst_w=0,dst_h=0;
	queryDisplayRect(x0,y0,dst_w,dst_h);
	if(dst_w<=0 || dst_h<=0 || emu_wid_<=0 || emu_hei_<=0)
	{
		return QPoint(0,0);
	}
	return QPoint(
	    x0+emu_x*dst_w/emu_wid_,
	    y0+emu_y*dst_h/emu_hei_);
}

bool EmuView::isPointOnEmuPicture(const QPoint &view_pos) const
{
	int x=0,y=0,dst_w=0,dst_h=0;
	queryDisplayRect(x,y,dst_w,dst_h);
	if(dst_w<=0 || dst_h<=0)
	{
		return false;
	}
	return QRect(x,y,dst_w,dst_h).contains(view_pos);
}

void EmuView::paintMouseDebugCrosshair(QPainter &painter)
{
	if(!mouse_debug_crosshair_ || emu_wid_<=0 || emu_hei_<=0)
	{
		return;
	}
	const QPoint emu=hostMouseEmuCoords();
	const QPoint view=mapFromEmu(emu.x(),emu.y());
	painter.setPen(QPen(QColor(0,255,0),1));
	painter.drawLine(view.x()-6,view.y(),view.x()+6,view.y());
	painter.drawLine(view.x(),view.y()-6,view.x(),view.y()+6);
}

void EmuView::keyPressEvent(QKeyEvent *event)
{
	if(nullptr==inputQueue_)
	{
		QWidget::keyPressEvent(event);
		return;
	}
	const int fsKey=QtKeyToFsKey(event->key(),static_cast<unsigned int>(event->modifiers()));
	if(FSKEY_NULL!=fsKey)
	{
		inputQueue_->KeyDown(fsKey);
	}
	const QString text=event->text();
	for(const QChar &ch : text)
	{
		if(!ch.isNull() && ch.unicode()>=0x20)
		{
			inputQueue_->CharInput(static_cast<unsigned int>(ch.unicode()));
		}
	}
	event->accept();
}

void EmuView::keyReleaseEvent(QKeyEvent *event)
{
	if(nullptr==inputQueue_)
	{
		QWidget::keyReleaseEvent(event);
		return;
	}
	const int fsKey=QtKeyToFsKey(event->key(),static_cast<unsigned int>(event->modifiers()));
	if(FSKEY_NULL!=fsKey)
	{
		inputQueue_->KeyUp(fsKey);
	}
	event->accept();
}

void EmuView::enterEvent(QEnterEvent *event)
{
	QWidget::enterEvent(event);
	noteViewMousePosition(event->position().toPoint());
}

void EmuView::mousePressEvent(QMouseEvent *event)
{
	const QPoint view_pos=event->pos();
	if(nullptr!=inputQueue_)
	{
		noteViewMousePosition(view_pos);
		const auto emu_pos=mapToEmu(view_pos);
		int btn=0;
		if(Qt::LeftButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_LEFT;
		}
		else if(Qt::RightButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_RIGHT;
		}
		else if(Qt::MiddleButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_MIDDLE;
		}
		if(0!=btn)
		{
			// A click while capture is released only resumes capture (below); don't pass it to
			// the guest, or it would register as a stray in-game click.  Remember the button so
			// its release is swallowed too.
			const bool resumeClick=
			    true==mouse_capture_released_ && true==isPointOnEmuPicture(view_pos);
			if(true==resumeClick)
			{
				suppressed_guest_buttons_|=btn;
			}
			else
			{
				inputQueue_->MousePress(btn,view_pos.x(),view_pos.y(),emu_pos.x(),emu_pos.y());
			}
		}
	}
	if(isPointOnEmuPicture(view_pos))
	{
		Q_EMIT emuPictureClicked();
	}
	setFocus();
	event->accept();
}

void EmuView::mouseReleaseEvent(QMouseEvent *event)
{
	if(nullptr!=inputQueue_)
	{
		const QPoint view_pos=event->pos();
		noteViewMousePosition(view_pos);
		const auto emu_pos=mapToEmu(view_pos);
		int btn=0;
		if(Qt::LeftButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_LEFT;
		}
		else if(Qt::RightButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_RIGHT;
		}
		else if(Qt::MiddleButton==event->button())
		{
			btn=QtInputQueue::MOUSE_BTN_MIDDLE;
		}
		if(0!=btn)
		{
			if(0!=(suppressed_guest_buttons_&btn))
			{
				// Matching release of a click that only resumed capture — swallow it too.
				suppressed_guest_buttons_&=~btn;
			}
			else
			{
				inputQueue_->MouseRelease(btn,view_pos.x(),view_pos.y(),emu_pos.x(),emu_pos.y());
			}
		}
	}
	event->accept();
}

void EmuView::setMouseCaptureReleased(bool released)
{
	// Clear stale swallow bits when (re-)entering the released state, not when leaving it: the
	// release of the resuming click usually arrives after the runtime has already flipped the
	// state back to captured, and that release still needs to be swallowed.
	if(true==released && true!=mouse_capture_released_)
	{
		suppressed_guest_buttons_=0;
	}
	mouse_capture_released_=released;
}

void EmuView::mouseMoveEvent(QMouseEvent *event)
{
	const QPoint view_pos=event->pos();
	noteViewMousePosition(view_pos);
	if(nullptr!=inputQueue_)
	{
		const auto emu_pos=mapToEmu(view_pos);
		inputQueue_->MouseMove(view_pos.x(),view_pos.y(),emu_pos.x(),emu_pos.y());
	}
	event->accept();
}
