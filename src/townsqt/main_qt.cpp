#include <cstdio>
#include <string>

#include <QApplication>
#include <QByteArray>
#include <QIcon>
#include <QSurfaceFormat>

#include "main_window.h"
#include "osinit.h"
#include "townsargv.h"
#include "townsdef.h"
#include "townsparam.h"
#include "i486.h"
#include "townsqt_argv_from_settings.h"
#include "townsqt_i18n.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"
#include "townsqt_settings.h"
#include "townsqt_version.h"

#if defined(__linux__)
#include "linux/midi_fluidsynth_host.h"
#endif

namespace
{
bool ArgvHasExplicitWaitFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-NOWAIT"==arg || "-NOWAITBOOT"==arg || "-YESWAIT"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitDamperWireFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-DAMPERWIRELINE"==arg || "-NODAMPERWIRELINE"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitFreqFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-FREQ"==arg && i+1<argc)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitTownsTypeFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-TOWNSTYPE"==arg && i+1<argc)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitMemSizeFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-MEMSIZE"==arg && i+1<argc)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitFidelityFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-HIGHFIDELITY"==arg || "-HIGHFIDELITYCPU"==arg ||
		   "-DEFAULTFIDELITY"==arg || "-MIDFIDELITY"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitPretend386Flag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-PRETEND386DX"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitFpuFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-USEFPU"==arg || "-DONTUSEFPU"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitSpriteTransferFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-SPRITETRANSFERHALF"==arg ||
		   "-SPRITETRANSFERUNLIMITED"==arg ||
		   "-SPRITETRANSFERAUTO"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitScanLineFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-SCANLINE15K"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitMidiFlag(int argc,char *argv[])
{
	for(int i=1; i+1<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-MIDI"==arg)
		{
			return true;
		}
	}
	return false;
}

bool ArgvHasExplicitFastScsiFlag(int argc,char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		std::string arg=argv[i];
		for(auto &c : arg)
		{
			if('a'<=c && c<='z')
			{
				c=static_cast<char>(c+'A'-'a');
			}
		}
		if("-FASTSCSI"==arg || "-SLOWSCSI"==arg || "-NORMALSCSI"==arg)
		{
			return true;
		}
	}
	return false;
}

void TownsQtConfigureOpenGL()
{
	QSurfaceFormat fmt;
	fmt.setDepthBufferSize(0);
	fmt.setStencilBufferSize(0);
	fmt.setAlphaBufferSize(0);
	fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
	fmt.setSwapInterval(0);

	const QByteArray platform=qgetenv("QT_QPA_PLATFORM");
	if(platform=="xcb" || platform=="offscreen")
	{
		fmt.setRenderableType(QSurfaceFormat::OpenGL);
		fmt.setVersion(2,1);
		fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	}
	else if(platform=="wayland" || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
	{
		fmt.setRenderableType(QSurfaceFormat::OpenGLES);
		fmt.setVersion(2,0);
	}
	else
	{
		fmt.setRenderableType(QSurfaceFormat::OpenGL);
		fmt.setVersion(2,1);
	}

	QSurfaceFormat::setDefaultFormat(fmt);
}
}

int main(int argc,char *argv[])
{
	if(sizeof(void *)<8)
	{
		fprintf(stderr,"Tsugaru_QT: requires a 64-bit CPU.\n");
		return 1;
	}

	OSInit();

#if defined(__linux__)
	MidiFluidSynthHost::SetMixInApplicationAudio(true);
#endif

	TownsQtConfigureOpenGL();

	QApplication app(argc,argv);
	app.setApplicationName(QStringLiteral("TownsQt"));
	app.setApplicationVersion(QStringLiteral(TOWNSQT_VERSION));
	app.setOrganizationName(QStringLiteral("TOWNSEMU"));
	TownsQtInstallTranslators(app);
	{
		QIcon icon;
		icon.addFile(QStringLiteral(":/icons/tsugaru_16.png"));
		icon.addFile(QStringLiteral(":/icons/tsugaru_32.png"));
		icon.addFile(QStringLiteral(":/icons/tsugaru_48.png"));
		icon.addFile(QStringLiteral(":/icons/tsugaru_128.png"));
		icon.addFile(QStringLiteral(":/icons/tsugaru.png"));
		icon.addFile(QStringLiteral(":/icons/tsugaru_512.png"));
		app.setWindowIcon(icon);
	}

	if(true!=TownsQtPaths::ensureLayout())
	{
		fprintf(stderr,"Tsugaru_QT: failed to create config directory.\n");
		return 1;
	}

	TownsARGV townsArgv;
	if(true!=townsArgv.AnalyzeCommandParameter(argc,argv))
	{
		return 1;
	}

	if(townsArgv.ROMPath.empty())
	{
		townsArgv.ROMPath=TownsQtPaths::romsDir().toStdString();
		std::printf("Tsugaru_QT: Default ROM directory: %s\n",townsArgv.ROMPath.c_str());
	}

	townsArgv.interactive=false;
	if(true!=ArgvHasExplicitTownsTypeFlag(argc,argv))
	{
		townsArgv.townsType=TownsQtSettings::townsType();
		const int model_idx=TownsQtSettings::modelGroupIndex();
		const QString rom_dir=QString::fromStdString(townsArgv.ROMPath);
		const TownsQtSysRomProfile sys_rom_profile=
		    TownsQtRomAvailability::ClassifySysRom(rom_dir);
		const bool marty_ex_rom=TownsQtRomAvailability::MartyExRomPresent(rom_dir);
		if(!TownsQtRomAvailability::ModelGroupAllowedForSysRom(
		       model_idx,
		       sys_rom_profile,
		       marty_ex_rom))
		{
			const int preferred=
			    TownsQtRomAvailability::PreferredModelGroupForSysRom(sys_rom_profile);
			TownsQtSettings::setModelGroupIndex(preferred);
			townsArgv.townsType=TownsQtSettings::townsType();
			std::fprintf(
			    stderr,
			    "Tsugaru_QT: Saved model is incompatible with SYS ROM; using default model.\n");
		}
	}
	else
	{
		TownsQtSettings::setTownsType(townsArgv.townsType);
	}
	if(true!=ArgvHasExplicitFreqFlag(argc,argv))
	{
		townsArgv.freq=static_cast<unsigned int>(TownsQtSettings::cpuFrequencyMhz());
	}
	else
	{
		TownsQtSettings::setCpuFrequencyMhz(static_cast<int>(townsArgv.freq));
	}

	if(true!=ArgvHasExplicitMemSizeFlag(argc,argv))
	{
		townsArgv.memSizeInMB=static_cast<unsigned int>(TownsQtSettings::memSizeInMB());
	}
	else
	{
		TownsQtSettings::setMemSizeInMB(static_cast<int>(townsArgv.memSizeInMB));
	}

	if(true!=ArgvHasExplicitFidelityFlag(argc,argv))
	{
		townsArgv.CPUFidelityLevel=TownsQtSettings::cpuHighFidelity() ?
		    i486DXCommon::HIGH_FIDELITY :
		    i486DXCommon::MID_FIDELITY;
	}
	else
	{
		TownsQtSettings::setCpuHighFidelity(i486DXCommon::HIGH_FIDELITY==townsArgv.CPUFidelityLevel);
	}

	if(true!=ArgvHasExplicitPretend386Flag(argc,argv))
	{
		townsArgv.pretend386DX=TownsQtSettings::pretend386DX();
	}
	else
	{
		TownsQtSettings::setPretend386DX(townsArgv.pretend386DX);
	}

	if(true!=ArgvHasExplicitFpuFlag(argc,argv))
	{
		townsArgv.useFPU=TownsQtSettings::useFPU();
	}
	else
	{
		TownsQtSettings::setUseFPU(townsArgv.useFPU);
	}

	// Real-time sync for stable audio tempo (equivalent to -YESWAIT).
	if(true!=ArgvHasExplicitWaitFlag(argc,argv))
	{
		townsArgv.noWait=false;
		townsArgv.noWaitStandby=false;
	}
	// Avoid deficit catch-up bursts that wobble emulated VSYNC rate.
	townsArgv.catchUpRealTime=false;

	if(true!=ArgvHasExplicitDamperWireFlag(argc,argv))
	{
		townsArgv.damperWireLine=TownsQtSettings::damperWireLine();
	}
	else
	{
		TownsQtSettings::setDamperWireLine(townsArgv.damperWireLine);
	}

	if(true!=ArgvHasExplicitSpriteTransferFlag(argc,argv))
	{
		townsArgv.spriteTransferMode=static_cast<unsigned int>(TownsQtSettings::spriteTransferMode());
	}
	else
	{
		TownsQtSettings::setSpriteTransferMode(static_cast<int>(townsArgv.spriteTransferMode));
	}

	if(true!=ArgvHasExplicitScanLineFlag(argc,argv))
	{
		townsArgv.scanLineEffectIn15KHz=TownsQtSettings::scanLineEffectIn15KHz();
	}
	else
	{
		TownsQtSettings::setScanLineEffectIn15KHz(townsArgv.scanLineEffectIn15KHz);
	}

	if(true!=ArgvHasExplicitMidiFlag(argc,argv))
	{
		townsArgv.nMidiCards=TownsQtSettings::midiBoard() ? 1 : 0;
	}

	if(true!=ArgvHasExplicitFastScsiFlag(argc,argv))
	{
		townsArgv.fastSCSI=TownsQtSettings::fastScsi();
	}
	else
	{
		TownsQtSettings::setFastScsi(townsArgv.fastSCSI);
	}

	townsArgv.fmVol=TownsQtSettings::fmChipVolume();
	townsArgv.pcmVol=TownsQtSettings::pcmChipVolume();
	townsArgv.alwaysBootToFASTMode=TownsQtSettings::cpuFastModeEnabled();

	TownsQtArgvFromSettings::ApplySessionSettings(townsArgv);
	TownsQtArgvFromSettings::ApplyHardDiskFromSettings(townsArgv);
	TownsQtArgvFromSettings::ApplyDefaultCmosPath(townsArgv);
	if(!townsArgv.CMOSFName.empty())
	{
		std::printf("Tsugaru_QT: CMOS file: %s\n",townsArgv.CMOSFName.c_str());
	}

	MainWindow window(townsArgv,TownsQtSettings::displayScale());
	window.show();

	return app.exec();
}
