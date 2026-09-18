#pragma once

class QWindow;
class QtInputQueue;

/*! Wayland relative-pointer + locked-pointer.
    Used for differential mouse capture. */
namespace TownsQtWaylandRelativePointer
{
bool Available();
/*! Begin relative motion into queue; locks pointer to window's wl_surface. */
bool Start(QWindow *window,QtInputQueue *queue);
void Stop();
bool Active();
void Shutdown();
}
