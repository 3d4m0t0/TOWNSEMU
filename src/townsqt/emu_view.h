#pragma once

#include "outside_world.h"
#include "townsqt_drive_access_overlay.h"

#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <cstdint>

class EmuGlView;
class QEnterEvent;
class QPainter;
class QtInputQueue;
class SharedRgbaFramebuffer;

class EmuView : public QWidget
{
	Q_OBJECT

public:
	explicit EmuView(QWidget *parent=nullptr);

	void attachFramebuffer(SharedRgbaFramebuffer *framebuffer);
	void attachInputQueue(QtInputQueue *inputQueue);
	void setScale(int scale);
	void setVideoOptions(int base_scale,bool auto_scale,bool maintain_aspect);
	void setFullscreenVsync(bool enabled);
	void setHostCursorBlank(bool blank);
	void setMouseDebugCrosshair(bool enabled);

	/*! While mouse capture is released (auto-forced/differential "click to start capture"
	    state), the click that resumes capture must not be delivered to the guest — it is a UI
	    gesture, not an in-game click.  MainWindow keeps this in sync with the runtime state. */
	void setMouseCaptureReleased(bool released);

	/*! Poll host cursor over the view (GUI thread, each input interval). */
	void pollMousePosition();

	/*! Map the host cursor into this view's coordinate system. */
	QPoint hostCursorInView() const;

	/*! Host mouse position in emulator image coordinates (0..emu_w-1). */
	QPoint hostMouseEmuCoords() const;

	void updateDriveAccessIndicators(const Outside_World::StatusBarInfo &info,
	                                 const DriveAccessPresence &presence);
	void setDriveAccessOverlayEnabled(bool enabled);

	/*! True when view_pos lies inside the scaled emulator picture (not letterbox). */
	bool isPointOnEmuPicture(const QPoint &view_pos) const;

Q_SIGNALS:
	void emuPictureClicked();

public Q_SLOTS:
	void refreshFrame();

protected:
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	QSize sizeHint() const override;

private:
	QPoint mapToEmu(const QPoint &pos) const;
	void noteViewMousePosition(const QPoint &view_pos);
	void syncInputDisplayLayout();
	void refreshFrameSoftware();
	void decideRenderBackend();
	void startSoftwareVsync();
	void onSoftwareVsyncTick();
	void recomputeDisplayLayout();
	void queryDisplayRect(int &x,int &y,int &dst_w,int &dst_h) const;
	QPoint mapFromEmu(int emu_x,int emu_y) const;
	void paintDriveAccessOverlay(QPainter &painter);
	void paintMouseDebugCrosshair(QPainter &painter);
	void setDriveAccessOverlayVisible(bool visible);

	SharedRgbaFramebuffer *framebuffer_=nullptr;
	QtInputQueue *inputQueue_=nullptr;
	EmuGlView *gl_view_=nullptr;
	QTimer *vsync_timer_=nullptr;
	QTimer *drive_access_hide_timer_=nullptr;
	QImage image_;
	QPixmap scaled_pixmap_;
	int scale_=1;
	bool auto_scale_=false;
	bool maintain_aspect_=true;
	bool fullscreen_vsync_=true;
	uint64_t last_serial_=0;
	uint64_t scaled_pixmap_serial_=0;
	int emu_wid_=640;
	int emu_hei_=480;
	bool gl_mode_decided_=false;
	bool use_gl_=false;
	bool software_present_pending_=false;
	bool has_view_mouse_pos_=false;
	QPoint last_view_mouse_pos_;
	bool mouse_debug_crosshair_=false;
	Outside_World::StatusBarInfo drive_access_{};
	DriveAccessPresence drive_access_presence_{};
	bool drive_access_valid_=false;
	bool drive_access_overlay_enabled_=true;
	bool drive_access_overlay_visible_=false;
	bool mouse_capture_released_=false;
	int suppressed_guest_buttons_=0;
};
