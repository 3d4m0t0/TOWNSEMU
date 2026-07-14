#ifndef QT_COMMAND_THREAD_IS_INCLUDED
#define QT_COMMAND_THREAD_IS_INCLUDED

#include "townsthread.h"
#include "townscommand.h"

/*! Processes outside_world commandQueue from the VM loop (no stdin). */
class QtCommandThread : public TownsUIThread
{
public:
	TownsCommandInterpreter cmdInterpreter;
	bool uiTerminate=false;

	void Main(TownsThread &vmThread,FMTownsCommon &towns,const TownsARGV &argv,Outside_World &outside_world) override;
	void ExecCommandQueue(TownsThread &vmThread,FMTownsCommon &towns,Outside_World *outside_world,Outside_World::Sound *sound) override;

	/*! Push a VM command from any thread (locks uiLock). */
	void EnqueueCommand(Outside_World &outside_world,const std::string &cmd);
};

#endif
