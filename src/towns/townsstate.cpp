#include <fstream>
#include <cstring>
#include <vector>

#include "towns.h"

#include <zlib.h>

// Disk-image (Disc-image) search rule:
// (1) Hard-disk image is not auto-mounted.
// (2) Try using the filename stored in the state file as is.
// (3) Try state path+relative path
// (4) Try image search path+file name
// (5) Try state path+file name
// (6) If floppy-disk image, use image stored in the state file.

namespace
{
/*! Compressed on-disk state: "TSTC" + u32le version + u32le rawLen + u32le cmpLen + zlib.
    Legacy uncompressed .TState still loads. */
constexpr char kStateCompressMagic[4]={'T','S','T','C'};
constexpr uint32_t kStateCompressVersion=1;
constexpr int kStateCompressLevel=6;

bool WriteUint32LE(std::ostream &os,uint32_t v)
{
	const unsigned char b[4]={
	    static_cast<unsigned char>(v&0xffu),
	    static_cast<unsigned char>((v>>8)&0xffu),
	    static_cast<unsigned char>((v>>16)&0xffu),
	    static_cast<unsigned char>((v>>24)&0xffu)};
	os.write(reinterpret_cast<const char *>(b),4);
	return os.good();
}

bool ReadUint32LE(std::istream &is,uint32_t &v)
{
	unsigned char b[4];
	is.read(reinterpret_cast<char *>(b),4);
	if(true!=is.good())
	{
		return false;
	}
	v=static_cast<uint32_t>(b[0])|
	  (static_cast<uint32_t>(b[1])<<8)|
	  (static_cast<uint32_t>(b[2])<<16)|
	  (static_cast<uint32_t>(b[3])<<24);
	return true;
}

bool CompressStateBlob(const std::vector <uint8_t> &raw,std::vector <uint8_t> &out)
{
	if(true==raw.empty())
	{
		return false;
	}
	const uLong bound=compressBound(static_cast<uLong>(raw.size()));
	out.resize(static_cast<size_t>(bound));
	uLongf destLen=bound;
	const int rc=compress2(
	    out.data(),
	    &destLen,
	    raw.data(),
	    static_cast<uLong>(raw.size()),
	    kStateCompressLevel);
	if(Z_OK!=rc)
	{
		out.clear();
		return false;
	}
	out.resize(static_cast<size_t>(destLen));
	return true;
}

bool DecompressStateBlob(
    const std::vector <uint8_t> &cmp,uint32_t rawLen,std::vector <uint8_t> &out)
{
	if(true==cmp.empty() || 0==rawLen)
	{
		return false;
	}
	out.resize(rawLen);
	uLongf destLen=rawLen;
	const int rc=uncompress(
	    out.data(),
	    &destLen,
	    cmp.data(),
	    static_cast<uLong>(cmp.size()));
	if(Z_OK!=rc || destLen!=rawLen)
	{
		out.clear();
		return false;
	}
	return true;
}
}

bool FMTownsCommon::SaveState(std::string fName,bool compress) const
{
	if(true!=compress)
	{
		std::ofstream ofp(fName,std::ios::binary);
		if(true!=ofp.is_open())
		{
			return false;
		}
		for(auto *devPtr : DevicesToSaveState())
		{
			auto dat=devPtr->Serialize(fName);
			uint32_t len=(uint32_t)dat.size();
			ofp.write((char *)&len,4);
			ofp.write((char *)dat.data(),len);
		}
		ofp.flush();
		return ofp.good();
	}

	const std::vector <uint8_t> raw=SaveStateMem();
	if(true==raw.empty())
	{
		return false;
	}
	std::vector <uint8_t> compressed;
	if(true!=CompressStateBlob(raw,compressed))
	{
		return false;
	}
	std::ofstream ofp(fName,std::ios::binary);
	if(true!=ofp.is_open())
	{
		return false;
	}
	ofp.write(kStateCompressMagic,4);
	if(true!=WriteUint32LE(ofp,kStateCompressVersion) ||
	   true!=WriteUint32LE(ofp,static_cast<uint32_t>(raw.size())) ||
	   true!=WriteUint32LE(ofp,static_cast<uint32_t>(compressed.size())))
	{
		return false;
	}
	ofp.write(reinterpret_cast<const char *>(compressed.data()),
	          static_cast<std::streamsize>(compressed.size()));
	ofp.flush();
	return ofp.good();
}

bool FMTownsCommon::LoadState(std::string fName)
{
	std::ifstream ifp(fName,std::ios::binary);
	if(true!=ifp.is_open())
	{
		return false;
	}

	char magic[4]={};
	ifp.read(magic,4);
	if(4==ifp.gcount() && 0==std::memcmp(magic,kStateCompressMagic,4))
	{
		uint32_t version=0,rawLen=0,cmpLen=0;
		if(true!=ReadUint32LE(ifp,version) ||
		   true!=ReadUint32LE(ifp,rawLen) ||
		   true!=ReadUint32LE(ifp,cmpLen) ||
		   kStateCompressVersion!=version ||
		   0==rawLen ||
		   0==cmpLen)
		{
			return false;
		}
		std::vector <uint8_t> compressed(cmpLen);
		ifp.read(reinterpret_cast<char *>(compressed.data()),
		         static_cast<std::streamsize>(cmpLen));
		if(static_cast<std::streamsize>(cmpLen)!=ifp.gcount())
		{
			return false;
		}
		std::vector <uint8_t> raw;
		if(true!=DecompressStateBlob(compressed,rawLen,raw))
		{
			return false;
		}
		return LoadStateMem(raw);
	}

	// Legacy uncompressed .TState
	ifp.clear();
	ifp.seekg(0,std::ios::beg);

	highResPCM.state.enabled=false; // If not read must be made by an old version, keep it disabled.
	midi.Stop();
	midi.EnableCards(0); // If no data, leave all disabled.

	rex3586.DisconnectAll();
	rex3586.state.enabled=false; // If not read, disable it.

	while(true!=ifp.eof())
	{
		uint32_t len=0;
		ifp.read((char *)&len,4);
		if(0==len || true!=ifp.good())
		{
			break;
		}

		std::vector <unsigned char> data;
		data.resize(len);
		ifp.read((char *)data.data(),len);

		bool successful=false;
		for(auto *devPtr : DevicesToLoadState())
		{
			if(true==devPtr->Deserialize(data,fName))
			{
				successful=true;
				break;
			}
		}

		if(true!=successful)
		{
			return false;
		}
	}
	LoadStatePostProcess();
	return true;
}

std::vector <uint8_t> FMTownsCommon::SaveStateMem(void) const
{
	std::vector <uint8_t> state;

	for(auto devPtr : DevicesToSaveState())
	{
		auto dat=devPtr->Serialize("MEM");
		uint32_t len=(uint32_t)dat.size();
		PushUint32(state,len);
		state.insert(state.end(),dat.begin(),dat.end());
	}

	return state;
}
bool FMTownsCommon::LoadStateMem(const std::vector <uint8_t> &state)
{
	fmt3631.state.enabled=false; // If not read must be made by an old version, keep it disabled.
	highResPCM.state.enabled=false; // If not read must be made by an old version, keep it disabled.
	midi.Stop();
	midi.EnableCards(0); // If no data, leave all disabled.

	rex3586.DisconnectAll();
	rex3586.state.enabled=false; // If not read, disable it.

	for(size_t ptr=0; ptr+4<=state.size(); )
	{
		const uint8_t *data=state.data()+ptr;
		uint32_t len=ReadUint32(data); // This increments the pointer.
		if(0==len)
		{
			break;
		}

		ptr+=4;
		auto left=state.size()-ptr;
		if(left<len)
		{
			std::cout << "Memory-Saved State is too short." << std::endl;
			return false;
		}

		std::vector <uint8_t> DATA;
		DATA.insert(DATA.end(),data,data+len);
		ptr+=len;

		bool successful=false;
		for(auto devPtr : DevicesToLoadState())
		{
			if(true==devPtr->Deserialize(DATA,"MEM"))
			{
				successful=true;
				break;
			}
		}

		if(true!=successful)
		{
			return false;
		}
	}
	LoadStatePostProcess();
	return true;
}

std::vector <const Device *> FMTownsCommon::DevicesToSaveState(void) const
{
	std::vector <const Device *> allDevices;
	auto &cpu=CPU();
	allDevices.push_back(this);
	allDevices.push_back(&cpu);
	allDevices.push_back(&pic);
	allDevices.push_back(&dmac);
	allDevices.push_back(&physMem);
	allDevices.push_back(&crtc);
	allDevices.push_back(&fmt3631);
	allDevices.push_back(&sprite);
	allDevices.push_back(&fdc);
	allDevices.push_back(&scsi);
	allDevices.push_back(&cdrom);
	allDevices.push_back(&rtc);
	allDevices.push_back(&sound);
	allDevices.push_back(&gameport);
	allDevices.push_back(&timer);
	allDevices.push_back(&keyboard);
	allDevices.push_back(&serialport);
	if(true==highResPCM.state.enabled)  // Don't save High-Res PCM unless enabled so that older version can load state.
	{
		allDevices.push_back(&highResPCM);
	}
	allDevices.push_back(&midi);
	allDevices.push_back(&rex3586);
	// allDevices.push_back(&vndrv);
	return allDevices;
}
std::vector <Device *> FMTownsCommon::DevicesToLoadState(void)
{
	std::vector <Device *> allDevices;
	auto &cpu=CPU();
	allDevices.push_back(this);
	allDevices.push_back(&cpu);
	allDevices.push_back(&pic);
	allDevices.push_back(&dmac);
	allDevices.push_back(&physMem);
	allDevices.push_back(&crtc);
	allDevices.push_back(&fmt3631);
	allDevices.push_back(&sprite);
	allDevices.push_back(&fdc);
	allDevices.push_back(&scsi);
	allDevices.push_back(&cdrom);
	allDevices.push_back(&rtc);
	allDevices.push_back(&sound);
	allDevices.push_back(&gameport);
	allDevices.push_back(&timer);
	allDevices.push_back(&keyboard);
	allDevices.push_back(&serialport);
	allDevices.push_back(&highResPCM);
	allDevices.push_back(&midi);
	allDevices.push_back(&rex3586);
	// allDevices.push_back(&vndrv);
	return allDevices;
}
void FMTownsCommon::LoadStatePostProcess(void)
{
	// I was first running a loop for unscheduling all devices,
	// and then a loop for re-scheduling devices that has non-null scheduleTime
	// only to realize that UnscheduleDeviceCallBack was nullifying the scheduleTime.

	for(auto devPtr : DevicesToLoadState())
	{
		if(TIME_NO_SCHEDULE!=devPtr->commonState.scheduleTime)
		{
			ScheduleDeviceCallBack(*devPtr,devPtr->commonState.scheduleTime);
		}
		else
		{
			UnscheduleDeviceCallBack(*devPtr);
		}
	}

	// Drop previous-session host CDDA wave + bridge/grace flags, then rebuild from
	// the restored guest position if PLAYING/PAUSED. Must not call DiscardCDDAWaveCache
	// here: that would wipe play pointer / base time restored by SpecificDeserialize.
	cdrom.DiscardHostCDDACacheForStateLoad();
	cdrom.ResumeCDDAAfterRestore();
	scsi.ResumeCDDAAfterRestore();

	// Save states do not include host-side app-exec tracking; re-sync profile + apply.
	if(true==var.useDiscProfiles)
	{
		const std::string &disc=cdrom.state.GetDisc().fName;
		if(true!=disc.empty())
		{
			mouseCoordWriteScan.TryLoadForDisc(disc);
			mouseCoordWriteScan.SyncAfterStateLoad();
		}
	}

	// Save states may have been taken without a MIDI board; honor machine config.
	midi.EnableCards(var.configuredMidiCards);

	// Force a capture soon, and publish townsTime so host present queues are not
	// gated on a pre-load observer clock (TownsQt PresentOneDueFrame vsync_index).
	state.nextRenderingTime=state.townsTime;
	PublishObserverTownsTime();

	crtc.RepairCorruptDisplayState();

	AdjustMachineSpeedForMemoryWait();

	var.justLoadedState=true;
}

/* virtual */ uint32_t FMTownsCommon::SerializeVersion(void) const
{
	// Version 1 added app-specific settings for Daikoukaijidai
	// Version 2 added DOSLOLSEG, DOSLOLOFF, DOSVER
	// Version 3 added fastModeFreq, mainRAMWait,VRAMWait
	return 3;
}

/* virtual */ void FMTownsCommon::SpecificSerialize(std::vector <unsigned char> &data,std::string stateFName) const
{
	PushInt64(data,state.townsTime);
	PushInt64(data,state.nextRenderingTime);
	PushInt64(data,state.nextDevicePollingTime);
	PushInt64(data,state.townsTime); // Now dummy.  Used to be cpuTime, but no longer used.
	PushInt64(data,state.timeDeficit);
	PushBool(data,state.noWait);
	PushBool(data,state.pretend386DX);
	PushInt64(data,state.nextFastDevicePollingTime);
	PushInt64(data,state.nextSecondInTownsTime);
	PushInt64(data,state.clockBalance);
	PushInt64(data,state.currentFreq);
	PushUint32(data,state.resetReason);
	PushUint32(data,state.serialROMBitCount);
	PushUint32(data,state.lastSerialROMCommand);

	for(int i=0; i<2; ++i)
	{
		for(int j=0; j<4; ++j)
		{
			PushBool(data,state.eleVol[i][j].EN);
			PushBool(data,state.eleVol[i][j].C32);
			PushBool(data,state.eleVol[i][j].C0);
			PushUint32(data,state.eleVol[i][j].vol);
		}
	}

	PushUint32(data,state.eleVolChLatch[0]);
	PushUint32(data,state.eleVolChLatch[1]);

	PushUint32(data,state.tbiosVersion);
	PushBool(data,state.mouseBIOSActive);
	PushInt32(data,state.mouseDisplayPage);
	PushUint32(data,state.TBIOS_physicalAddr);
	PushUint32(data,state.TBIOS_mouseInfoOffset);
	PushUint32(data,state.MOS_work_linearAddr);
	PushUint32(data,state.MOS_work_physicalAddr);
	PushUint32(data,state.MOS_pulsePerPixelH);
	PushUint32(data,state.MOS_pulsePerPixelV);
	PushInt32(data,state.mouseIntegrationSpeed);

	PushUint16(data,state.DOSSEG);
	// Version 2 and later >>
	PushUint16(data,state.DOSVER);
	PushUint16(data,state.DOSLOLOFF);
	PushUint16(data,state.DOSLOLSEG);
	// Version 2 and later <<

	PushUint32(data,state.appSpecificSetting);
	PushUint32(data,state.appSpecific_MousePtrX);
	PushUint32(data,state.appSpecific_MousePtrY);
	PushUint32(data,state.appSpecific_StickPosXPtr);
	PushUint32(data,state.appSpecific_StickPosYPtr);
	PushUint32(data,state.appSpecific_ThrottlePtr);
	PushUint32(data,state.appSpecific_RudderPtr);
	PushUint32(data,state.appSpecific_WC2_EventQueueBaseAddr);  // DS:03CCH
	PushUint32(data,state.appSpecific_WC_setSpeedPtr);
	PushUint32(data,state.appSpecific_WC_maxSpeedPtr);
	PushBool(data,state.appSpecific_HoldMouseIntegration);

	// Version 1 and later
	PushUint32(data,state.appSpecific_Daikoukai_YNDialogXAddr);
	PushUint32(data,state.appSpecific_Daikoukai_YNDialogYAddr);
	PushUint32(data,state.appSpecific_Daikoukai_DentakuDialogXAddr);
	PushUint32(data,state.appSpecific_Daikoukai_DentakuDialogYAddr);

	// Version 3 and later
	PushUint32(data,state.fastModeFreq);
	PushUint32(data,state.mainRAMWait);
	PushUint32(data,state.VRAMWait);
}
/* virtual */ bool FMTownsCommon::SpecificDeserialize(const unsigned char *&data,std::string stateFName,uint32_t version)
{
	state.townsTime=ReadInt64(data);
	state.nextRenderingTime=ReadInt64(data);
	state.nextDevicePollingTime=ReadInt64(data);
	ReadInt64(data);
	state.timeDeficit=ReadInt64(data);
	state.noWait=ReadBool(data);
	state.pretend386DX=ReadBool(data);
	state.nextFastDevicePollingTime=ReadInt64(data);
	state.nextSecondInTownsTime=ReadInt64(data);
	state.clockBalance=ReadInt64(data);
	state.currentFreq=ReadInt64(data);
	state.resetReason=ReadUint32(data);
	state.serialROMBitCount=ReadUint32(data);
	state.lastSerialROMCommand=ReadUint32(data);

	for(int i=0; i<2; ++i)
	{
		for(int j=0; j<4; ++j)
		{
			state.eleVol[i][j].EN=ReadBool(data);
			state.eleVol[i][j].C32=ReadBool(data);
			state.eleVol[i][j].C0=ReadBool(data);
			state.eleVol[i][j].vol=ReadUint32(data);
		}
	}

	state.eleVolChLatch[0]=ReadUint32(data);
	state.eleVolChLatch[1]=ReadUint32(data);

	state.tbiosVersion=ReadUint32(data);
	state.mouseBIOSActive=ReadBool(data);
	state.mouseDisplayPage=ReadInt32(data);
	state.TBIOS_physicalAddr=ReadUint32(data);
	state.TBIOS_mouseInfoOffset=ReadUint32(data);
	state.MOS_work_linearAddr=ReadUint32(data);
	state.MOS_work_physicalAddr=ReadUint32(data);
	state.MOS_pulsePerPixelH=ReadUint32(data);
	state.MOS_pulsePerPixelV=ReadUint32(data);
	state.mouseIntegrationSpeed=ReadInt32(data);

	state.DOSSEG=ReadUint16(data);
	if(2<=version)
	{
		state.DOSVER=ReadUint16(data);
		state.DOSLOLOFF=ReadUint16(data);
		state.DOSLOLSEG=ReadUint16(data);
	}

	// If the user chose an app-specific setting on start, it shouldn't override it.
	// For example, if start Dungeon Master without app-specific setting, and then later want to
	// turn it on, the start-up setting should have priority.
	if(TOWNS_APPSPECIFIC_NONE==state.appSpecificSetting)
	{
		state.appSpecificSetting=ReadUint32(data);
	}
	else
	{
		ReadUint32(data); // Dummy read
	}
	state.appSpecific_MousePtrX=ReadUint32(data);
	state.appSpecific_MousePtrY=ReadUint32(data);
	state.appSpecific_StickPosXPtr=ReadUint32(data);
	state.appSpecific_StickPosYPtr=ReadUint32(data);
	state.appSpecific_ThrottlePtr=ReadUint32(data);
	state.appSpecific_RudderPtr=ReadUint32(data);
	state.appSpecific_WC2_EventQueueBaseAddr=ReadUint32(data);  // DS:03CCH
	state.appSpecific_WC_setSpeedPtr=ReadUint32(data);
	state.appSpecific_WC_maxSpeedPtr=ReadUint32(data);
	state.appSpecific_HoldMouseIntegration=ReadBool(data);

	if(1<=version)
	{
		state.appSpecific_Daikoukai_YNDialogXAddr=ReadUint32(data);
		state.appSpecific_Daikoukai_YNDialogYAddr=ReadUint32(data);
		state.appSpecific_Daikoukai_DentakuDialogXAddr=ReadUint32(data);
		state.appSpecific_Daikoukai_DentakuDialogYAddr=ReadUint32(data);
	}
	else
	{
		state.appSpecific_Daikoukai_YNDialogXAddr=0;
		state.appSpecific_Daikoukai_YNDialogYAddr=0;
		state.appSpecific_Daikoukai_DentakuDialogXAddr=0;
		state.appSpecific_Daikoukai_DentakuDialogYAddr=0;
	}

	if(3<=version)
	{
	// Version 3 and later
		state.fastModeFreq=ReadUint32(data);
		state.mainRAMWait =ReadUint32(data);
		state.VRAMWait    =ReadUint32(data);
	}
	else
	{
		state.fastModeFreq=state.currentFreq;
		state.mainRAMWait=0;
		state.VRAMWait=0;
	}

	VMBase::ClearAbortFlag();

	lastAutoQSSCheckTime=0;

	return true;
}
