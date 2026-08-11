#ifndef QT_OUTSIDE_WORLD_IS_INCLUDED
#define QT_OUTSIDE_WORLD_IS_INCLUDED

#include "fssimplewindow_connection.h"
#include "qt_input_queue.h"
#include "shared_rgba_framebuffer.h"

class QtOutsideWorld : public FsSimpleWindowConnection
{
public:
	QtOutsideWorld(QtInputQueue *inputQueue,SharedRgbaFramebuffer *framebuffer);

	Outside_World::WindowInterface *CreateWindowInterface(void) const override;
	Sound *CreateSound(void) const override;
	void DeleteSound(Sound *) const override;

	QtInputQueue *InputQueue() const { return inputQueue_; }
	SharedRgbaFramebuffer *Framebuffer() const { return framebuffer_; }

private:
	QtInputQueue *inputQueue_=nullptr;
	SharedRgbaFramebuffer *framebuffer_=nullptr;

	class QtWindowConnection : public FsSimpleWindowConnection::WindowConnection
	{
	public:
		QtWindowConnection(QtOutsideWorld *owner,QtInputQueue *inputQueue,SharedRgbaFramebuffer *framebuffer);

		void Start(void) override;
		void Stop(void) override;
		void Interval(void) override;
		void Render(bool swapBuffers) override;
		void UpdateImage(TownsRender::ImageCopy &img) override;

	private:
		QtOutsideWorld *owner_=nullptr;
		QtInputQueue *inputQueue_=nullptr;
		SharedRgbaFramebuffer *framebuffer_=nullptr;
		bool diff_mouse_tracking_ready_=false;
		bool prev_differential_path_=false;
		bool prev_relative_ptr_=false;
		bool prev_feeding_=true;
	};
};

#endif
