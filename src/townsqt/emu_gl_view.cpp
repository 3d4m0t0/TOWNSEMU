#include "emu_gl_view.h"

#include "shared_rgba_framebuffer.h"
#include "townsqt_drive_access_overlay.h"

#include <QOpenGLShader>
#include <QPainter>
#include <QPen>
#include <QVector2D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
constexpr char VERTEX_SHADER_GL[]=
    R"(#version 120
attribute vec2 aVert;
attribute vec2 aTex;
varying vec2 vTex;
uniform vec4 uRect;
uniform vec2 uWin;
void main()
{
	vec2 p=uRect.xy+aVert*uRect.zw;
	vec2 ndc=vec2(p.x/uWin.x*2.0-1.0,1.0-p.y/uWin.y*2.0);
	gl_Position=vec4(ndc,0.0,1.0);
	vTex=aTex;
}
)";

constexpr char FRAGMENT_SHADER_GL[]=
    R"(#version 120
varying vec2 vTex;
uniform sampler2D uTex;
void main()
{
	vec4 c=texture2D(uTex,vTex);
	gl_FragColor=vec4(c.rgb,1.0);
}
)";

constexpr char VERTEX_SHADER_ES[]=
    R"(attribute vec2 aVert;
attribute vec2 aTex;
varying vec2 vTex;
uniform vec4 uRect;
uniform vec2 uWin;
void main()
{
	vec2 p=uRect.xy+aVert*uRect.zw;
	vec2 ndc=vec2(p.x/uWin.x*2.0-1.0,1.0-p.y/uWin.y*2.0);
	gl_Position=vec4(ndc,0.0,1.0);
	vTex=aTex;
}
)";

constexpr char FRAGMENT_SHADER_ES[]=
    R"(precision mediump float;
varying vec2 vTex;
uniform sampler2D uTex;
void main()
{
	vec4 c=texture2D(uTex,vTex);
	gl_FragColor=vec4(c.rgb,1.0);
}
)";

constexpr float QUAD_VERTS[]=
    {
        0.0f,0.0f,0.0f,0.0f,
        1.0f,0.0f,1.0f,0.0f,
        1.0f,1.0f,1.0f,1.0f,
        0.0f,0.0f,0.0f,0.0f,
        1.0f,1.0f,1.0f,1.0f,
        0.0f,1.0f,0.0f,1.0f,
    };
}

EmuGlView::EmuGlView(QWidget *parent) : QOpenGLWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAutoFillBackground(false);
	setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

EmuGlView::~EmuGlView()
{
	if(isValid())
	{
		makeCurrent();
		destroyTexture();
		doneCurrent();
	}
}

void EmuGlView::attachFramebuffer(SharedRgbaFramebuffer *framebuffer)
{
	framebuffer_=framebuffer;
}

void EmuGlView::setScale(int scale)
{
	scale_=std::max(1,scale);
	update();
}

void EmuGlView::setMaintainAspect(bool enabled)
{
	maintain_aspect_=enabled;
	update();
}

void EmuGlView::setStretchToFill(bool enabled)
{
	stretch_to_fill_=enabled;
	update();
}

void EmuGlView::setLogicalDisplayRect(int x,int y,int w,int h)
{
	const int nw=std::max(1,w);
	const int nh=std::max(1,h);
	if(true==have_logical_display_rect_ &&
	   logical_display_x_==x &&
	   logical_display_y_==y &&
	   logical_display_w_==nw &&
	   logical_display_h_==nh)
	{
		return;
	}
	have_logical_display_rect_=true;
	logical_display_x_=x;
	logical_display_y_=y;
	logical_display_w_=nw;
	logical_display_h_=nh;
	update();
}

void EmuGlView::setMouseDebugCrosshair(bool enabled)
{
	if(mouse_debug_crosshair_==enabled)
	{
		return;
	}
	mouse_debug_crosshair_=enabled;
	update();
}

void EmuGlView::setMouseDebugCrosshairEmuPos(int emu_x,int emu_y)
{
	if(mouse_debug_emu_x_==emu_x && mouse_debug_emu_y_==emu_y)
	{
		return;
	}
	mouse_debug_emu_x_=emu_x;
	mouse_debug_emu_y_=emu_y;
	if(true==mouse_debug_crosshair_)
	{
		update();
	}
}

void EmuGlView::setDriveAccessOverlayEnabled(bool enabled)
{
	if(drive_access_overlay_enabled_==enabled)
	{
		return;
	}
	drive_access_overlay_enabled_=enabled;
	if(true!=enabled)
	{
		drive_access_overlay_visible_=false;
	}
	update();
}

void EmuGlView::setDriveAccessOverlayVisible(bool visible)
{
	if(drive_access_overlay_visible_==visible)
	{
		return;
	}
	drive_access_overlay_visible_=visible;
	update();
}

void EmuGlView::setDriveAccessIndicators(const Outside_World::StatusBarInfo &info,
                                         const DriveAccessPresence &presence)
{
	drive_access_=info;
	drive_access_presence_=presence;
	drive_access_valid_=true;
}

void EmuGlView::setSwapIntervalPreference(int interval)
{
	if(nullptr==context())
	{
		return;
	}
	QSurfaceFormat fmt=format();
	if(fmt.swapInterval()==interval)
	{
		return;
	}
	fmt.setSwapInterval(interval);
	setFormat(fmt);
}

bool EmuGlView::buildShaderProgram()
{
	const bool gles=context()->isOpenGLES();
	const char *const vs=gles ? VERTEX_SHADER_ES : VERTEX_SHADER_GL;
	const char *const fs=gles ? FRAGMENT_SHADER_ES : FRAGMENT_SHADER_GL;

	program_.removeAllShaders();
	if(!program_.addShaderFromSourceCode(QOpenGLShader::Vertex,vs))
	{
		std::fprintf(stderr,"Tsugaru_QT: GL vertex shader: %s\n",program_.log().toUtf8().constData());
		return false;
	}
	if(!program_.addShaderFromSourceCode(QOpenGLShader::Fragment,fs))
	{
		std::fprintf(stderr,"Tsugaru_QT: GL fragment shader: %s\n",program_.log().toUtf8().constData());
		return false;
	}
	program_.bindAttributeLocation("aVert",0);
	program_.bindAttributeLocation("aTex",1);
	if(!program_.link())
	{
		std::fprintf(stderr,"Tsugaru_QT: GL program link: %s\n",program_.log().toUtf8().constData());
		return false;
	}
	return true;
}

void EmuGlView::initializeGL()
{
	initializeOpenGLFunctions();
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	if(nullptr!=context())
	{
		const int interval=context()->format().swapInterval();
		if(0<interval)
		{
			std::fprintf(stderr,"Tsugaru_QT: display VSync enabled (swap interval %d).\n",interval);
		}
		else
		{
			std::fprintf(stderr,"Tsugaru_QT: display VSync requested but swap interval is 0 (compositor may still vsync).\n");
		}
	}

	if(true!=buildShaderProgram())
	{
		return;
	}

	vbo_.create();
	vbo_.bind();
	vbo_.allocate(QUAD_VERTS,static_cast<int>(sizeof(QUAD_VERTS)));
	vbo_.release();
}

void EmuGlView::destroyTexture()
{
	if(0!=tex_id_)
	{
		glDeleteTextures(1,&tex_id_);
		tex_id_=0;
	}
	tex_w_=0;
	tex_h_=0;
}

void EmuGlView::uploadTexture(const unsigned char *rgba,unsigned int wid,unsigned int hei)
{
	if(nullptr==rgba || 0==wid || 0==hei)
	{
		return;
	}

	if(0==tex_id_)
	{
		glGenTextures(1,&tex_id_);
	}

	glBindTexture(GL_TEXTURE_2D,tex_id_);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

	if(wid!=tex_w_ || hei!=tex_h_)
	{
		glTexImage2D(
		    GL_TEXTURE_2D,
		    0,
		    GL_RGBA,
		    static_cast<GLsizei>(wid),
		    static_cast<GLsizei>(hei),
		    0,
		    GL_RGBA,
		    GL_UNSIGNED_BYTE,
		    rgba);
		tex_w_=wid;
		tex_h_=hei;
	}
	else
	{
		glTexSubImage2D(
		    GL_TEXTURE_2D,
		    0,
		    0,
		    0,
		    static_cast<GLsizei>(wid),
		    static_cast<GLsizei>(hei),
		    GL_RGBA,
		    GL_UNSIGNED_BYTE,
		    rgba);
	}
}

void EmuGlView::refreshFrame()
{
	if(nullptr==framebuffer_)
	{
		return;
	}

	unsigned int wid=0,hei=0;
	uint64_t serial=0;
	if(true!=framebuffer_->PeekLatest(&wid,&hei,&serial))
	{
		return;
	}
	if(serial==last_serial_ && !frame_dirty_)
	{
		return;
	}
	if(true!=framebuffer_->CopyLatest(&staging_rgba_,&wid,&hei,&serial))
	{
		return;
	}
	if(serial==last_serial_ && !frame_dirty_)
	{
		return;
	}

	emu_wid_=static_cast<int>(wid);
	emu_hei_=static_cast<int>(hei);
	pending_w_=wid;
	pending_h_=hei;
	pending_serial_=serial;
	frame_dirty_=true;
	update();
}

void EmuGlView::drawTexturedQuad(int x,int y,int dst_w,int dst_h,int vp_w,int vp_h)
{
	if(!program_.isLinked())
	{
		return;
	}

	program_.bind();
	program_.setUniformValue("uTex",0);
	program_.setUniformValue("uWin",QVector2D(static_cast<float>(vp_w),static_cast<float>(vp_h)));
	program_.setUniformValue("uRect",QVector4D(static_cast<float>(x),static_cast<float>(y),static_cast<float>(dst_w),static_cast<float>(dst_h)));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,tex_id_);

	vbo_.bind();
	program_.enableAttributeArray(0);
	program_.enableAttributeArray(1);
	program_.setAttributeBuffer(0,GL_FLOAT,0,2,4*sizeof(float));
	program_.setAttributeBuffer(1,GL_FLOAT,2*sizeof(float),2,4*sizeof(float));
	glDrawArrays(GL_TRIANGLES,0,6);
	program_.disableAttributeArray(0);
	program_.disableAttributeArray(1);
	vbo_.release();
	program_.release();
}

void EmuGlView::paintGL()
{
	const qreal dpr=devicePixelRatioF();
	const int vp_w=std::max(1,static_cast<int>(std::lround(width()*dpr)));
	const int vp_h=std::max(1,static_cast<int>(std::lround(height()*dpr)));
	glViewport(0,0,vp_w,vp_h);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	if(frame_dirty_ && !staging_rgba_.empty() && 0<pending_w_ && 0<pending_h_)
	{
		uploadTexture(staging_rgba_.data(),pending_w_,pending_h_);
		frame_dirty_=false;
		last_serial_=pending_serial_;
	}

	if(0==tex_id_ || emu_wid_<=0 || emu_hei_<=0)
	{
		return;
	}

	int x=0,y=0,dst_w=0,dst_h=0;
	if(have_logical_display_rect_)
	{
		x=static_cast<int>(std::lround(logical_display_x_*dpr));
		y=static_cast<int>(std::lround(logical_display_y_*dpr));
		dst_w=std::max(1,static_cast<int>(std::lround(logical_display_w_*dpr)));
		dst_h=std::max(1,static_cast<int>(std::lround(logical_display_h_*dpr)));
	}
	else if(stretch_to_fill_)
	{
		dst_w=vp_w;
		dst_h=vp_h;
	}
	else
	{
		dst_w=std::max(1,static_cast<int>(std::lround(emu_wid_*scale_*dpr)));
		dst_h=std::max(1,static_cast<int>(std::lround(emu_hei_*scale_*dpr)));
		x=(vp_w-dst_w)/2;
		y=(vp_h-dst_h)/2;
	}
	drawTexturedQuad(x,y,dst_w,dst_h,vp_w,vp_h);

	const bool want_overlay=
	    (true==drive_access_overlay_enabled_ &&
	     true==drive_access_overlay_visible_ &&
	     true==drive_access_valid_ &&
	     0<drive_access_presence_.IconCount()) ||
	    true==mouse_debug_crosshair_;
	if(want_overlay)
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::SmoothPixmapTransform,false);
		if(true==drive_access_overlay_enabled_ &&
		   true==drive_access_overlay_visible_ &&
		   true==drive_access_valid_ &&
		   0<drive_access_presence_.IconCount())
		{
			constexpr int kMargin=2;
			DrawDriveAccessOverlay(
			    painter,
			    DriveAccessOverlayOriginX(width(),drive_access_presence_),
			    height()-kDriveAccessIconSize-kMargin,
			    drive_access_,
			    drive_access_presence_);
		}
		if(true==mouse_debug_crosshair_ && have_logical_display_rect_ && 0<emu_wid_ && 0<emu_hei_)
		{
			const int vx=logical_display_x_+mouse_debug_emu_x_*logical_display_w_/emu_wid_;
			const int vy=logical_display_y_+mouse_debug_emu_y_*logical_display_h_/emu_hei_;
			painter.setPen(QPen(QColor(0,255,0),1));
			painter.drawLine(vx-6,vy,vx+6,vy);
			painter.drawLine(vx,vy-6,vx,vy+6);
		}
		painter.end();
	}
}
