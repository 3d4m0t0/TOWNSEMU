/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */

#include "mos_coord_probe.h"
#include "towns.h"

MosCoordReadProbe::MosCoordReadProbe(FMTownsCommon &towns,unsigned int physAddr)
{
	townsPtr=&towns;
	physAddrTop=physAddr&(~(Memory::MEMORY_ACCESS_SLOT_SIZE-1));
	ClearWatch();
}

void MosCoordReadProbe::ClearWatch(void)
{
	for(unsigned int i=0; i<Memory::MEMORY_ACCESS_SLOT_SIZE; ++i)
	{
		watch[i]=0;
	}
}

void MosCoordReadProbe::WatchByte(unsigned int physAddr)
{
	if((physAddr&(~(Memory::MEMORY_ACCESS_SLOT_SIZE-1)))!=physAddrTop)
	{
		return;
	}
	watch[physAddr-physAddrTop]=1;
}

void MosCoordReadProbe::WatchWord(unsigned int physAddr)
{
	WatchByte(physAddr);
	WatchByte(physAddr+1);
}

bool MosCoordReadProbe::IsWatched(unsigned int physAddr) const
{
	if((physAddr&(~(Memory::MEMORY_ACCESS_SLOT_SIZE-1)))!=physAddrTop)
	{
		return false;
	}
	return 0!=watch[physAddr-physAddrTop];
}

void MosCoordReadProbe::NoteRead(unsigned int physAddr) const
{
	if(nullptr==townsPtr || true==townsPtr->var.suppressMosCoordReadProbe)
	{
		return;
	}
	if(true!=IsWatched(physAddr))
	{
		return;
	}
	auto &cpu=townsPtr->CPU();
	const unsigned int csVal=cpu.state.CS().value;

	// Reads made from an interrupt handler are "system": the TBIOS soft cursor (and similar
	// redraw code) reads the MOS coordinate from the VSYNC/timer ISR every frame regardless
	// of the application, whereas an application reads it from its main loop (nesting depth 0).
	// This tells the soft cursor apart from the app without depending on which code segment
	// happens to read first.  Also treat BIOS / kernel / TBIOS selectors as system:
	//   low selectors (<0x100): DOS/BIOS/kernel stubs.  0x110: TBIOS.  >=0xF000: ROM BIOS
	//   area (e.g. 0xFC00).  MOS_code_CS: the Mouse BIOS itself.
	const bool systemRead=
	    0<cpu.state.inInterruptDepth ||
	    csVal<0x100 ||
	    0xF000<=csVal ||
	    0x110==csVal ||
	    0x110==(csVal&0xFFF8) ||
	    (0!=townsPtr->state.MOS_code_CS && csVal==townsPtr->state.MOS_code_CS);

	if(true==systemRead)
	{
		++townsPtr->state.mosCoordTbiosSelfReadCount;
		return;
	}

	++townsPtr->state.mosCoordAppReadCount;

	// Diagnostic: remember which CS selectors are counted as application reads so the
	// usage-probe log can show whether the "app" reads come from the game code or a
	// mis-classified soft-cursor / system module.
	auto &st=townsPtr->state;
	for(unsigned int i=0; i<st.mosCoordAppReadCSCount; ++i)
	{
		if(csVal==st.mosCoordAppReadCS[i])
		{
			++st.mosCoordAppReadCSHits[i];
			return;
		}
	}
	if(st.mosCoordAppReadCSCount<4)
	{
		st.mosCoordAppReadCS[st.mosCoordAppReadCSCount]=csVal;
		st.mosCoordAppReadCSHits[st.mosCoordAppReadCSCount]=1;
		++st.mosCoordAppReadCSCount;
	}
}

/* virtual */ unsigned int MosCoordReadProbe::FetchByte(unsigned int physAddr) const
{
	auto data=memAccessChain->FetchByte(physAddr);
	NoteRead(physAddr);
	return data;
}

/* virtual */ unsigned int MosCoordReadProbe::FetchWord(unsigned int physAddr) const
{
	auto data=memAccessChain->FetchWord(physAddr);
	NoteRead(physAddr);
	NoteRead(physAddr+1);
	return data;
}

/* virtual */ unsigned int MosCoordReadProbe::FetchDword(unsigned int physAddr) const
{
	auto data=memAccessChain->FetchDword(physAddr);
	NoteRead(physAddr);
	NoteRead(physAddr+1);
	NoteRead(physAddr+2);
	NoteRead(physAddr+3);
	return data;
}

/* virtual */ void MosCoordReadProbe::StoreByte(unsigned int physAddr,unsigned char data)
{
	memAccessChain->StoreByte(physAddr,data);
}

/* virtual */ void MosCoordReadProbe::StoreWord(unsigned int physAddr,unsigned int data)
{
	memAccessChain->StoreWord(physAddr,data);
}

/* virtual */ void MosCoordReadProbe::StoreDword(unsigned int physAddr,unsigned int data)
{
	memAccessChain->StoreDword(physAddr,data);
}

/* virtual */ MemoryAccess::ConstMemoryWindow MosCoordReadProbe::GetConstMemoryWindow(unsigned int physAddr) const
{
	return memAccessChain->GetConstMemoryWindow(physAddr);
}

/* virtual */ MemoryAccess::MemoryWindow MosCoordReadProbe::GetMemoryWindow(unsigned int physAddr)
{
	return memAccessChain->GetMemoryWindow(physAddr);
}

namespace
{
MosCoordReadProbe *EnsureProbeSlot(FMTownsCommon &towns,std::unique_ptr<MosCoordReadProbe> &slot,unsigned int physAddr)
{
	const unsigned int top=physAddr&(~(Memory::MEMORY_ACCESS_SLOT_SIZE-1));
	if(nullptr!=slot && slot->physAddrTop==top)
	{
		return slot.get();
	}
	if(nullptr!=slot)
	{
		if(nullptr!=slot->memAccessChain)
		{
			towns.mem.SetAccessObject(slot->memAccessChain,slot->physAddrTop);
		}
		slot.reset();
	}
	auto *cur=towns.mem.GetAccessObject(physAddr);
	slot.reset(new MosCoordReadProbe(towns,physAddr));
	slot->memAccessChain=cur;
	towns.mem.SetAccessObject(slot.get(),physAddr);
	return slot.get();
}
}

void FMTownsCommon::StopMosCoordUsageProbe(void)
{
	var.mosUsageLearnSystemCS=false;
	auto uninstall=[&](std::unique_ptr<MosCoordReadProbe> &slot)
	{
		if(nullptr==slot)
		{
			return;
		}
		if(nullptr!=slot->memAccessChain)
		{
			mem.SetAccessObject(slot->memAccessChain,slot->physAddrTop);
		}
		slot.reset();
	};
	uninstall(mosCoordProbeA);
	uninstall(mosCoordProbeB);
}

void FMTownsCommon::StartMosCoordUsageProbe(void)
{
	StopMosCoordUsageProbe();
	state.mosCoordAppReadCount=0;
	state.mosCoordTbiosSelfReadCount=0;
	state.mosBIOSAppCallCount=0;
	state.gameportMouseReadCount=0;
	// Keep learned desktop system CS across re-probes in the same MOS session.

	auto watchWord=[&](unsigned int physAddr)
	{
		if(0==physAddr)
		{
			return;
		}
		const unsigned int top=physAddr&(~(Memory::MEMORY_ACCESS_SLOT_SIZE-1));
		std::unique_ptr<MosCoordReadProbe> *slot=&mosCoordProbeA;
		if(nullptr!=mosCoordProbeA && mosCoordProbeA->physAddrTop!=top)
		{
			slot=&mosCoordProbeB;
		}
		auto *probe=EnsureProbeSlot(*this,*slot,physAddr);
		if(nullptr!=probe)
		{
			probe->WatchWord(physAddr);
		}
	};

	if(0!=state.MOS_work_physicalAddr)
	{
		// Cover both known MOS layouts (V22 uses +0x52, later use +0x56).
		watchWord(state.MOS_work_physicalAddr+0x52);
		watchWord(state.MOS_work_physicalAddr+0x54);
		watchWord(state.MOS_work_physicalAddr+0x56);
		watchWord(state.MOS_work_physicalAddr+0x58);
	}
	if(0!=state.TBIOS_physicalAddr)
	{
		if(0!=state.TBIOS_mouseInfoOffset)
		{
			watchWord(state.TBIOS_physicalAddr+state.TBIOS_mouseInfoOffset+0x0C);
			watchWord(state.TBIOS_physicalAddr+state.TBIOS_mouseInfoOffset+0x0E);
		}
		watchWord(state.TBIOS_physicalAddr+0x510);
		watchWord(state.TBIOS_physicalAddr+0x512);
		watchWord(state.TBIOS_physicalAddr+0x56C);
		watchWord(state.TBIOS_physicalAddr+0x56E);
	}
}

unsigned int FMTownsCommon::GetGameportMouseMotionPacketCount(void) const
{
	unsigned int n=0;
	for(auto &p : gameport.state.ports)
	{
		if(TownsGamePort::MOUSE==p.device)
		{
			n+=p.mouseMotionPacketCount;
		}
	}
	return n;
}

void FMTownsCommon::ResetGameportMouseMotionPacketCount(void)
{
	for(auto &p : gameport.state.ports)
	{
		p.mouseMotionPacketCount=0;
	}
}
