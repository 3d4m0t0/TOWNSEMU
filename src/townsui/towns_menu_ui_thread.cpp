#include "towns_menu_ui_thread.h"

#include <chrono>
#include <iostream>
#include <thread>

void TownsMenuUIThread::Main(TownsThread &townsThread,FMTownsCommon &towns,const TownsARGV &argv,Outside_World &outside_world)
{
	for(auto &ftfr : argv.toSend)
	{
		towns.var.ftfr.AddHostToVM(ftfr.hostFName,ftfr.vmFName);
	}

	if(true==argv.interactive)
	{
		while(true!=uiTerminate)
		{
			std::string line;
			std::cout << "TownsUI>";
			std::getline(std::cin,line);

			uiLock.lock();
			this->cmdline=line;
			if(true==this->vmTerminated)
			{
				uiTerminate=true;
			}
			uiLock.unlock();

			bool commandDone=false;
			while(true!=commandDone && true!=uiTerminate)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				uiLock.lock();
				if(""==this->cmdline)
				{
					commandDone=true;
				}
				if(true==this->vmTerminated)
				{
					uiTerminate=true;
				}
				uiLock.unlock();
			}
		}
	}
}

void TownsMenuUIThread::ExecCommandQueue(TownsThread &townsThread,FMTownsCommon &towns,Outside_World *outside_world,Outside_World::Sound *sound)
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
		if(""!=this->cmdline)
		{
			auto cmd=cmdInterpreter.Interpret(this->cmdline);
			cmdInterpreter.Execute(townsThread,towns,outside_world,sound,cmd);
			if(TownsCommandInterpreter::CMD_QUIT==cmd.primaryCmd)
			{
				uiTerminate=true;
			}
			this->cmdline="";
		}
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
