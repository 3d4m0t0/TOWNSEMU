#include "townsqt_argv_from_settings.h"

#include "townsargv.h"
#include "townsparam.h"
#include "townsqt_paths.h"
#include "townsqt_settings.h"
#include "i486.h"

namespace
{
void ApplyStrikeCommanderMemOverride(TownsARGV &argv)
{
	if(TOWNS_APPSPECIFIC_STRIKECOMMANDER==argv.appSpecificSetting &&
	   argv.memSizeInMB<8)
	{
		argv.memSizeInMB=8;
	}
}
}

namespace TownsQtArgvFromSettings
{
void ApplyMachineFromSettings(TownsARGV &argv)
{
	argv.townsType=TownsQtSettings::townsType();
	argv.freq=static_cast<unsigned int>(TownsQtSettings::cpuFrequencyMhz());
	argv.cdSpeed=static_cast<unsigned int>(TownsQtSettings::cdSpeed());
	argv.memSizeInMB=static_cast<unsigned int>(TownsQtSettings::memSizeInMB());
	argv.CPUFidelityLevel=TownsQtSettings::cpuHighFidelity() ?
	    i486DXCommon::HIGH_FIDELITY :
	    i486DXCommon::MID_FIDELITY;
	argv.pretend386DX=TownsQtSettings::pretend386DX();
	argv.useFPU=TownsQtSettings::useFPU();
	argv.fastSCSI=TownsQtSettings::fastScsi();
	argv.nMidiCards=TownsQtSettings::midiBoard() ? 1 : 0;
	argv.fmVol=TownsQtSettings::fmChipVolume();
	argv.pcmVol=TownsQtSettings::pcmChipVolume();
	argv.alwaysBootToFASTMode=TownsQtSettings::cpuFastModeEnabled();
	argv.damperWireLine=TownsQtSettings::damperWireLine();
	argv.spriteTransferMode=static_cast<unsigned int>(TownsQtSettings::spriteTransferMode());
	argv.scanLineEffectIn15KHz=TownsQtSettings::scanLineEffectIn15KHz();
}

void ApplySessionSettings(TownsARGV &argv)
{
	argv.noWait=false;
	argv.noWaitStandby=false;
	argv.catchUpRealTime=false;
	argv.autoScaling=TownsQtSettings::autoScaling();
	argv.maintainAspect=TownsQtSettings::maintainAspect();
	argv.gamePort[0]=TownsQtSettings::gamePort(0);
	argv.gamePort[1]=TownsQtSettings::gamePort(1);
	for(int port=0; port<TownsStartParameters::NUM_GAMEPORTS; ++port)
	{
		for(int button=0; button<2; ++button)
		{
			argv.maxButtonHoldTime[port][button]=
			    static_cast<long long int>(TownsQtSettings::maxButtonHoldTimeMs(port,button))*1000000LL;
		}
	}
	argv.mouseIntegrationSpeed=static_cast<unsigned int>(TownsQtSettings::mouseIntegrationSpeed());
	argv.considerVRAMOffsetInMouseIntegration=TownsQtSettings::considerVRAMOffsetInMouseIntegration();
	argv.differentialMouseIntegration=TownsQtSettings::differentialMouseIntegration();
	argv.mouseMinX=TownsQtSettings::mouseMinX();
	argv.mouseMinY=TownsQtSettings::mouseMinY();
	argv.mouseMaxX=TownsQtSettings::mouseMaxX();
	argv.mouseMaxY=TownsQtSettings::mouseMaxY();
	argv.appSpecificSetting=TownsQtSettings::appSpecificSetting();
	ApplyStrikeCommanderMemOverride(argv);
	if(0==argv.scaling || 100==argv.scaling)
	{
		argv.scaling=static_cast<unsigned int>(TownsQtSettings::displayScale()*100);
	}
}

void ApplyDefaultCmosPath(TownsARGV &argv)
{
	if(false==argv.autoSaveCMOS || !argv.CMOSFName.empty())
	{
		return;
	}
	argv.CMOSFName=TownsQtPaths::cmosFilePath().toStdString();
}

void Apply(TownsARGV &argv)
{
	ApplyMachineFromSettings(argv);
	ApplySessionSettings(argv);
	ApplyDefaultCmosPath(argv);
}
}
