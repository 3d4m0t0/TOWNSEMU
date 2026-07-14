#pragma once

class QWindow;

namespace TownsQtWaylandIdleInhibit
{
bool Available();
void Apply(QWindow *window,bool enable);
void Shutdown();
}
