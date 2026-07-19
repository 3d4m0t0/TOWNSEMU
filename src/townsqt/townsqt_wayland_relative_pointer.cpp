#include "townsqt_wayland_relative_pointer.h"

#include "qt_input_queue.h"

#ifdef TOWNSQT_HAS_WAYLAND_RELATIVE_POINTER

#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"

#include <QGuiApplication>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <cstring>

namespace
{
struct RelativeState
{
	wl_registry_listener registry_listener{};
	zwp_relative_pointer_manager_v1 *relative_manager=nullptr;
	zwp_pointer_constraints_v1 *constraints=nullptr;
	zwp_relative_pointer_v1 *relative=nullptr;
	zwp_locked_pointer_v1 *locked=nullptr;
	QWindow *window=nullptr;
	QtInputQueue *queue=nullptr;
	bool probed=false;
	bool available=false;
	bool locked_ok=false;
};

RelativeState g;

zwp_relative_pointer_v1_listener g_relative_listener{};
zwp_locked_pointer_v1_listener g_locked_listener{};

void RegistryGlobal(void *,wl_registry *registry,uint32_t name,const char *interface,uint32_t)
{
	if(0==std::strcmp(interface,zwp_relative_pointer_manager_v1_interface.name) &&
	   nullptr==g.relative_manager)
	{
		g.relative_manager=static_cast<zwp_relative_pointer_manager_v1 *>(
		    wl_registry_bind(registry,name,&zwp_relative_pointer_manager_v1_interface,1));
	}
	else if(0==std::strcmp(interface,zwp_pointer_constraints_v1_interface.name) &&
	        nullptr==g.constraints)
	{
		g.constraints=static_cast<zwp_pointer_constraints_v1 *>(
		    wl_registry_bind(registry,name,&zwp_pointer_constraints_v1_interface,1));
	}
}

void RegistryGlobalRemove(void *,wl_registry *,uint32_t)
{
}

wl_display *QtWaylandDisplay()
{
	QGuiApplication *app=qGuiApp;
	if(nullptr==app || QGuiApplication::platformName()!=QLatin1String("wayland"))
	{
		return nullptr;
	}
	if(auto *wayland_app=app->nativeInterface<QNativeInterface::QWaylandApplication>())
	{
		return wayland_app->display();
	}
	return nullptr;
}

wl_pointer *QtWaylandPointer()
{
	QGuiApplication *app=qGuiApp;
	if(nullptr==app)
	{
		return nullptr;
	}
	if(auto *wayland_app=app->nativeInterface<QNativeInterface::QWaylandApplication>())
	{
		return wayland_app->pointer();
	}
	return nullptr;
}

wl_surface *QtWaylandSurface(QWindow *window)
{
	if(nullptr==window)
	{
		return nullptr;
	}
	const WId handle=window->winId();
	if(!handle)
	{
		return nullptr;
	}
	return reinterpret_cast<wl_surface *>(handle);
}

bool EnsureProbed()
{
	if(g.probed)
	{
		return g.available;
	}
	g.probed=true;

	wl_display *display=QtWaylandDisplay();
	if(nullptr==display)
	{
		return false;
	}

	wl_registry *registry=wl_display_get_registry(display);
	if(nullptr==registry)
	{
		return false;
	}

	g.registry_listener.global=RegistryGlobal;
	g.registry_listener.global_remove=RegistryGlobalRemove;
	wl_registry_add_listener(registry,&g.registry_listener,nullptr);
	wl_display_roundtrip(display);
	wl_registry_destroy(registry);

	g.available=(nullptr!=g.relative_manager && nullptr!=g.constraints);
	return g.available;
}

void RelativeMotion(
    void *,
    zwp_relative_pointer_v1 *,
    uint32_t /*utime_hi*/,
    uint32_t /*utime_lo*/,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t /*dx_unaccel*/,
    wl_fixed_t /*dy_unaccel*/)
{
	if(nullptr==g.queue)
	{
		return;
	}
	// Use compositor-accelerated dx/dy so motion matches absolute (cursor) integration.
	const double rdx=wl_fixed_to_double(dx);
	const double rdy=wl_fixed_to_double(dy);
	if(0.0==rdx && 0.0==rdy)
	{
		return;
	}
	g.queue->AddRelativeMotion(rdx,rdy);
}

void LockedPointerLocked(void *,zwp_locked_pointer_v1 *)
{
	g.locked_ok=true;
	if(nullptr!=g.queue)
	{
		g.queue->SetRelativePointerActive(true);
	}
}

void LockedPointerUnlocked(void *,zwp_locked_pointer_v1 *)
{
	g.locked_ok=false;
	if(nullptr!=g.queue)
	{
		g.queue->SetRelativePointerActive(false);
	}
}

void DestroySessionObjects()
{
	if(nullptr!=g.queue)
	{
		g.queue->SetRelativePointerActive(false);
		g.queue->ClearRelativeMotion();
	}
	if(nullptr!=g.locked)
	{
		zwp_locked_pointer_v1_destroy(g.locked);
		g.locked=nullptr;
	}
	if(nullptr!=g.relative)
	{
		zwp_relative_pointer_v1_destroy(g.relative);
		g.relative=nullptr;
	}
	g.window=nullptr;
	g.queue=nullptr;
	g.locked_ok=false;
}
}

namespace TownsQtWaylandRelativePointer
{
bool Available()
{
	if(nullptr==QGuiApplication::instance())
	{
		return false;
	}
	if(QGuiApplication::platformName()!=QLatin1String("wayland"))
	{
		return false;
	}
	return EnsureProbed();
}

bool Start(QWindow *window,QtInputQueue *queue)
{
	Stop();
	if(nullptr==window || nullptr==queue || !Available())
	{
		return false;
	}

	wl_pointer *pointer=QtWaylandPointer();
	wl_surface *surface=QtWaylandSurface(window);
	if(nullptr==pointer || nullptr==surface ||
	   nullptr==g.relative_manager || nullptr==g.constraints)
	{
		return false;
	}

	g_relative_listener.relative_motion=RelativeMotion;
	g.relative=zwp_relative_pointer_manager_v1_get_relative_pointer(
	    g.relative_manager,pointer);
	if(nullptr==g.relative)
	{
		return false;
	}
	zwp_relative_pointer_v1_add_listener(g.relative,&g_relative_listener,nullptr);

	g_locked_listener.locked=LockedPointerLocked;
	g_locked_listener.unlocked=LockedPointerUnlocked;
	g.locked=zwp_pointer_constraints_v1_lock_pointer(
	    g.constraints,
	    surface,
	    pointer,
	    nullptr,
	    ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	if(nullptr==g.locked)
	{
		zwp_relative_pointer_v1_destroy(g.relative);
		g.relative=nullptr;
		return false;
	}
	zwp_locked_pointer_v1_add_listener(g.locked,&g_locked_listener,nullptr);

	g.window=window;
	g.queue=queue;
	queue->ClearRelativeMotion();
	// Active once locked; relative events may already arrive before the locked event.
	queue->SetRelativePointerActive(true);
	return true;
}

void Stop()
{
	DestroySessionObjects();
}

bool Active()
{
	return nullptr!=g.relative && nullptr!=g.queue && g.queue->RelativePointerActive();
}

void Shutdown()
{
	Stop();
	if(nullptr!=g.relative_manager)
	{
		zwp_relative_pointer_manager_v1_destroy(g.relative_manager);
		g.relative_manager=nullptr;
	}
	if(nullptr!=g.constraints)
	{
		zwp_pointer_constraints_v1_destroy(g.constraints);
		g.constraints=nullptr;
	}
	g.probed=false;
	g.available=false;
}
}

#else

namespace TownsQtWaylandRelativePointer
{
bool Available()
{
	return false;
}

bool Start(QWindow *,QtInputQueue *)
{
	return false;
}

void Stop()
{
}

bool Active()
{
	return false;
}

void Shutdown()
{
}
}

#endif
