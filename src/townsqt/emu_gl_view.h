#pragma once

#include "outside_world.h"
#include "townsqt_drive_access_overlay.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

#include <cstdint>
#include <vector>

class SharedRgbaFramebuffer;

/*! OpenGL texture presenter; requires a compatible QSurfaceFormat (see main_qt.cpp). */
class EmuGlView : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

public:
	explicit EmuGlView(QWidget *parent=nullptr);
	~EmuGlView() override;

	void attachFramebuffer(SharedRgbaFramebuffer *framebuffer);
	void setScale(int scale);
	void setMaintainAspect(bool enabled);
	void setStretchToFill(bool enabled);
	void setSwapIntervalPreference(int interval);
	void setDriveAccessIndicators(const Outside_World::StatusBarInfo &info,
	                              const DriveAccessPresence &presence);
	void setDriveAccessOverlayEnabled(bool enabled);
	void setDriveAccessOverlayVisible(bool visible);

public Q_SLOTS:
	void refreshFrame();

protected:
	void initializeGL() override;
	void paintGL() override;

private:
	void uploadTexture(const unsigned char *rgba,unsigned int wid,unsigned int hei);
	void destroyTexture();
	void drawTexturedQuad(int x,int y,int dst_w,int dst_h,int vp_w,int vp_h);
	bool buildShaderProgram();

	SharedRgbaFramebuffer *framebuffer_=nullptr;
	QOpenGLShaderProgram program_;
	QOpenGLBuffer vbo_;
	std::vector<unsigned char> staging_rgba_;
	GLuint tex_id_=0;
	unsigned int tex_w_=0;
	unsigned int tex_h_=0;
	unsigned int pending_w_=0;
	unsigned int pending_h_=0;
	uint64_t pending_serial_=0;
	bool frame_dirty_=false;
	int scale_=1;
	bool maintain_aspect_=true;
	bool stretch_to_fill_=false;
	uint64_t last_serial_=0;
	int emu_wid_=640;
	int emu_hei_=480;
	bool drive_access_valid_=false;
	bool drive_access_overlay_enabled_=true;
	bool drive_access_overlay_visible_=false;
	Outside_World::StatusBarInfo drive_access_{};
	DriveAccessPresence drive_access_presence_{};
};
