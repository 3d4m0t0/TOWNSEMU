#ifndef TOWNS_MENU_UI_THREAD_IS_INCLUDED
#define TOWNS_MENU_UI_THREAD_IS_INCLUDED

#include <string>

#include "townsargv.h"
#include "townsthread.h"
#include "townscommand.h"

/*! UI thread: menu bar commands and optional debugger console (same role as TownsCUIThread). */
class TownsMenuUIThread : public TownsUIThread
{
public:
	using TownsUIThread::uiLock;
	std::string cmdline;
	TownsCommandInterpreter cmdInterpreter;
	bool uiTerminate=false;

	void Main(TownsThread &vmThread,FMTownsCommon &towns,const TownsARGV &argv,Outside_World &outside_world) override;
	void ExecCommandQueue(TownsThread &vmThread,FMTownsCommon &towns,Outside_World *outside_world,Outside_World::Sound *sound) override;
};

#endif
