#pragma once

class QWindow;
class QtInputQueue;

/*! Wayland relative-pointer + locked-pointer for differential mouse integration.
    Falls back to cursor-warp deltas when unavailable (X11 / missing protocols). */
namespace TownsQtWaylandRelativePointer
{
bool Available();
/*! Begin relative motion into queue; locks pointer to window's wl_surface. */
bool Start(QWindow *window,QtInputQueue *queue);
void Stop();
bool Active();
void Shutdown();
}
