#include "townsqt_wayland_idle_inhibit.h"

#ifdef TOWNSQT_HAS_WAYLAND_IDLE_INHIBIT

#include "idle-inhibit-unstable-v1-client-protocol.h"

#include <QGuiApplication>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <cstring>

namespace
{
struct IdleState
{
	wl_registry_listener registry_listener{};
	zwp_idle_inhibit_manager_v1 *manager=nullptr;
	zwp_idle_inhibitor_v1 *inhibitor=nullptr;
	QWindow *window=nullptr;
	bool probed=false;
	bool available=false;
};

IdleState g;

void RegistryGlobal(void *,wl_registry *registry,uint32_t name,const char *interface,uint32_t)
{
	if(0==std::strcmp(interface,zwp_idle_inhibit_manager_v1_interface.name) && nullptr==g.manager)
	{
		g.manager=static_cast<zwp_idle_inhibit_manager_v1 *>(wl_registry_bind(
		    registry,name,&zwp_idle_inhibit_manager_v1_interface,1));
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

	g.available=nullptr!=g.manager;
	return g.available;
}

void DestroyInhibitor()
{
	if(nullptr!=g.inhibitor)
	{
		zwp_idle_inhibitor_v1_destroy(g.inhibitor);
		g.inhibitor=nullptr;
	}
	g.window=nullptr;
}
}

namespace TownsQtWaylandIdleInhibit
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

void Apply(QWindow *window,bool enable)
{
	if(!enable)
	{
		DestroyInhibitor();
		return;
	}
	if(nullptr==window || !Available())
	{
		DestroyInhibitor();
		return;
	}

	DestroyInhibitor();

	wl_surface *surface=QtWaylandSurface(window);
	if(nullptr==surface || nullptr==g.manager)
	{
		return;
	}

	g.inhibitor=zwp_idle_inhibit_manager_v1_create_inhibitor(g.manager,surface);
	g.window=window;
}

void Shutdown()
{
	DestroyInhibitor();
	if(nullptr!=g.manager)
	{
		zwp_idle_inhibit_manager_v1_destroy(g.manager);
		g.manager=nullptr;
	}
	g.probed=false;
	g.available=false;
}
}

#else

namespace TownsQtWaylandIdleInhibit
{
bool Available()
{
	return false;
}

void Apply(QWindow * /*window*/,bool /*enable*/)
{
}

void Shutdown()
{
}
}

#endif
