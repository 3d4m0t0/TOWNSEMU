#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "towns.h"
#include "townsthread.h"
#include "townsargv.h"
#include "osinit.h"
#include "menubar_connection.h"
#include "towns_menu_ui_thread.h"

#ifdef _WIN32
#include <timeapi.h>
#else
static void timeBeginPeriod(int) {}
static void timeEndPeriod(int) {}
#endif

template <class CPUCLASS>
static int Run(FMTownsTemplate <CPUCLASS> &towns,const TownsARGV &argv,Outside_World &outside_world,Outside_World::Sound &sound,Outside_World::WindowInterface &window)
{
	TownsThread townsThread;

	if(true==argv.debugger)
	{
		towns.EnableDebugger();
	}
	else
	{
		towns.DisableDebugger();
	}

	if(true==argv.autoStart)
	{
		townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
		std::cout << "[TownsUI] VM run mode: RUN" << std::endl;
	}
	else
	{
		std::cout << "[TownsUI] VM run mode: PAUSE (press RUN in menu bar)" << std::endl;
	}

	TownsMenuUIThread menuUiThread;

	std::thread uiThread(&TownsMenuUIThread::Run,&menuUiThread,&townsThread,&towns,&argv,&outside_world);

	std::thread vmThread([&]{
		townsThread.VMStart(&towns,&outside_world,&menuUiThread);
		townsThread.VMMainLoop(&towns,&outside_world,&sound,&window,&menuUiThread);
		townsThread.VMEnd(&towns,&outside_world,&menuUiThread);
	});

	auto t0=std::chrono::high_resolution_clock::now();
	window.ClearVMClosedFlag();
	while(true!=window.CheckVMClosed())
	{
		window.Interval();
		auto t=std::chrono::high_resolution_clock::now();
		auto dt=t-t0;
		if(50<=std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() || true==window.winThr.newImageRendered)
		{
			window.Render(true);
			t0=t;
			window.winThr.newImageRendered=false;
		}
		else
		{
			timeBeginPeriod(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
			timeEndPeriod(1);
		}
	}

	vmThread.join();

#ifndef _WIN32
	fclose(stdin);
#endif

	uiThread.join();

	return towns.var.returnCode;
}

int main(int ac,char *av[])
{
	if(sizeof(void *)<8)
	{
		std::cout << "TownsUI requires a 64-bit CPU.\n";
		return 1;
	}

	OSInit();

	TownsARGV argv;
	if(true!=argv.AnalyzeCommandParameter(ac,av))
	{
		return 1;
	}

	// Menu bar is the primary UI; do not block the UI thread on stdin (Tsugaru_CUI default).
	argv.interactive=false;

	// Tsugaru default is 40MHz; 80386DX 16MHz software is more stable at 16MHz.
	if(40==argv.freq)
	{
		argv.freq=16;
	}

	std::cout << "[TownsUI] CPU frequency: " << argv.freq << " MHz" << std::endl;
	std::cout << "[TownsUI] VM auto-start: " << (argv.autoStart ? "yes (RUN)" : "no (-PAUSE; use menu RUN)") << std::endl;
	if(""!=argv.cdImgFName)
	{
		std::cout << "[TownsUI] CD image argument: " << argv.cdImgFName << std::endl;
	}
	else if(""==argv.fdImgFName[0] && ""==argv.fdImgFName[1])
	{
		std::cout << "[TownsUI] WARNING: No -CD or -FD0/-FD1 image. "
		          << "CD/FD boot software will hang at BIOS or \"loading\" screen." << std::endl;
		std::cout << "[TownsUI] Example: ./TownsUI ./ -CD game.cue -FREQ 16" << std::endl;
	}
	std::cout << "[TownsUI] Use menu bar: RUN / PAUSE / POFF / QUIT" << std::endl;

	auto *outside_world=new MenuBarFsSimpleWindowConnection;
	auto *sound=outside_world->CreateSound();
	auto *window=outside_world->CreateWindowInterface();

	if(i486DXCommon::HIGH_FIDELITY==argv.CPUFidelityLevel)
	{
		static FMTownsTemplate <i486DXHighFidelity> towns;
		if(true!=FMTownsCommon::Setup(towns,outside_world,window,argv))
		{
			return 1;
		}
		window->Start();
		return Run(towns,argv,*outside_world,*sound,*window);
	}

	static FMTownsTemplate <i486DXDefaultFidelity> towns;
	if(true!=FMTownsCommon::Setup(towns,outside_world,window,argv))
	{
		return 1;
	}
	window->Start();
	return Run(towns,argv,*outside_world,*sound,*window);
}
