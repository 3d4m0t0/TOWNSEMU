#include "qt_command_thread.h"

#include "townsargv.h"

#include <chrono>
#include <mutex>
#include <thread>

void QtCommandThread::EnqueueCommand(Outside_World &outside_world,const std::string &cmd)
{
	std::lock_guard<std::mutex> lock(uiLock);
	outside_world.commandQueue.push(cmd);
}

void QtCommandThread::Main(TownsThread &townsThread,FMTownsCommon &towns,const TownsARGV &argv,Outside_World &outside_world)
{
	for(auto &ftfr : argv.toSend)
	{
		towns.var.ftfr.AddHostToVM(ftfr.hostFName,ftfr.vmFName);
	}

	while(true!=uiTerminate)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		uiLock.lock();
		if(true==vmTerminated)
		{
			uiTerminate=true;
		}
		uiLock.unlock();
	}
}

void QtCommandThread::ExecCommandQueue(TownsThread &townsThread,FMTownsCommon &towns,Outside_World *outside_world,Outside_World::Sound *sound)
{
	if(true==cmdInterpreter.waitVM)
	{
		const unsigned int vmState=townsThread.GetRunMode();
		if(TownsThread::RUNMODE_PAUSE==vmState)
		{
			cmdInterpreter.waitVM=false;
		}
		else if(TownsThread::RUNMODE_EXIT==vmState)
		{
			uiTerminate=true;
		}
	}
	else
	{
		while(true!=outside_world->commandQueue.empty())
		{
			auto cmd=cmdInterpreter.Interpret(outside_world->commandQueue.front());
			cmdInterpreter.Execute(townsThread,towns,outside_world,sound,cmd);
			if(TownsCommandInterpreter::CMD_QUIT==cmd.primaryCmd)
			{
				uiTerminate=true;
			}
			outside_world->commandQueue.pop();
		}
	}
}
