/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */

#include "mouse_coord_write_scan.h"
#include "towns.h"
#include "townsdef.h"
#include "discimg.h"
#include "cpputil.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
void ApplyDiscIdentityToProfile(MouseCoordWriteScan::Profile &p,const DiscIdentity &id)
{
	if(true==id.hasContentId)
	{
		p.discVolumeLabel=id.volumeLabel;
		p.discSystemId=id.systemIdentifier;
		p.discContentHash32=id.contentHash32;
	}
	if(true==id.hasFingerprint)
	{
		p.discFingerprintHash32=id.fingerprintHash32;
	}
}

DiscIdentity DiscIdentityForPath(const std::string &discPath,FMTownsCommon *townsPtr)
{
	if(nullptr!=townsPtr)
	{
		const auto &disc=townsPtr->cdrom.state.GetDisc();
		if(true!=disc.fName.empty() &&
		   (discPath.empty() || disc.fName==discPath ||
		    cpputil::GetBaseName(disc.fName)==cpputil::GetBaseName(discPath)))
		{
			return disc.ComputeIdentity();
		}
	}
	if(true==discPath.empty())
	{
		return DiscIdentity();
	}
	DiscImage disc;
	if(DiscImage::ERROR_NOERROR!=disc.Open(discPath))
	{
		return DiscIdentity();
	}
	return disc.ComputeIdentity();
}

bool ProfileMatchesDisc(const MouseCoordWriteScan::Profile &p,
                        const std::string &/*discPath*/,
                        const DiscIdentity &id)
{
	// Profiles are keyed by fp_%08x.ini path. Missing INI fingerprint (legacy) is OK —
	// caller stamps identity after load. Wrong fingerprint rejects the file.
	if(true!=id.hasFingerprint)
	{
		return false;
	}
	if(true!=p.HasFingerprint())
	{
		return true;
	}
	return p.discFingerprintHash32==id.fingerprintHash32;
}
}

void MouseCoordWriteScan::Attach(FMTownsCommon &towns)
{
	townsPtr=&towns;
	memPtr=&towns.mem;
}

void MouseCoordWriteScan::Detach(void)
{
	DisarmMemoryTrace();
	townsPtr=nullptr;
	memPtr=nullptr;
}

void MouseCoordWriteScan::SetEnabled(bool on)
{
	enabled=on;
	if(nullptr!=townsPtr)
	{
		townsPtr->var.mouseCoordWriteScanEnabled=on;
	}
	if(true!=on)
	{
		// Drop the short store-trace window, but keep permanent 追跡 watches so
		// SOURCE hops still work and 監視 rows can refresh outside Calibration.
		if(nullptr!=memPtr && memPtr->storeTraceUser==this)
		{
			memPtr->storeTraceRemaining=0;
			memPtr->storeTraceFn=nullptr;
			memPtr->storeTraceUser=nullptr;
		}
		paused=false;
		huntMotionOpen.store(false,std::memory_order_relaxed);
		if(nullptr!=townsPtr)
		{
			townsPtr->var.mouseCoordCalibrating=false;
		}
		std::lock_guard<std::mutex> lock(mtx);
		SyncChaseWatchLocked();
	}
	else
	{
		std::lock_guard<std::mutex> lock(mtx);
		SyncChaseWatchLocked();
	}
}

void MouseCoordWriteScan::StartCalibration(void)
{
}

void MouseCoordWriteScan::StopCalibration(void)
{
}

bool MouseCoordWriteScan::IsCalibrating(void) const
{
	return false;
}

void MouseCoordWriteScan::ArmMemoryTrace(unsigned int count)
{
	if(nullptr==memPtr)
	{
		return;
	}
	// Never lock mtx here: CaptureGuestWriterTrace / ChaseTraceThunk call this
	// while already holding mtx (non-recursive → deadlock / abort).
	memPtr->storeTraceUser=this;
	memPtr->storeTraceFn=&MouseCoordWriteScan::StoreTraceThunk;
	memPtr->storeTraceRemaining=count;
	++armCount;
}

void MouseCoordWriteScan::DisarmMemoryTrace(void)
{
	if(nullptr==memPtr)
	{
		return;
	}
	if(memPtr->storeTraceUser==this)
	{
		memPtr->storeTraceRemaining=0;
		memPtr->storeTraceFn=nullptr;
		memPtr->storeTraceUser=nullptr;
	}
	if(memPtr->storeChaseUser==this)
	{
		memPtr->storeChaseCount=0;
		memPtr->storeChaseFn=nullptr;
		memPtr->storeChaseUser=nullptr;
	}
}

unsigned int MouseCoordWriteScan::CandidateCount(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return static_cast<unsigned int>(candidates.size());
}

unsigned int MouseCoordWriteScan::TraceRemaining(void) const
{
	if(nullptr==memPtr)
	{
		return 0;
	}
	return memPtr->storeTraceRemaining;
}

void MouseCoordWriteScan::SyncChaseWatchLocked(void)
{
	if(nullptr==memPtr)
	{
		return;
	}
	if(0==chaseCount)
	{
		if(memPtr->storeChaseUser==this)
		{
			memPtr->storeChaseCount=0;
			memPtr->storeChaseFn=nullptr;
			memPtr->storeChaseUser=nullptr;
		}
		return;
	}
	memPtr->storeChaseUser=this;
	memPtr->storeChaseFn=&MouseCoordWriteScan::ChaseTraceThunk;
	memPtr->storeChaseCount=std::min(chaseCount,(unsigned)Memory::STORE_CHASE_MAX);
	for(unsigned int i=0; i<memPtr->storeChaseCount; ++i)
	{
		memPtr->storeChasePhys[i]=chasePhys[i];
	}
}

/* static */ void MouseCoordWriteScan::ChaseTraceThunk(
    void *user,unsigned int physAddr,unsigned int size,unsigned int data)
{
	if(nullptr==user)
	{
		return;
	}
	auto *self=reinterpret_cast<MouseCoordWriteScan *>(user);
	// Permanent 追跡 watches stay armed outside Calibration/SCAN so SOURCE hops
	// and lastValue updates keep working for 監視/追跡 rows.
	if(nullptr==self->townsPtr ||
	   true==self->suppressOwnStore)
	{
		return;
	}
	if(2!=size && 4!=size)
	{
		return;
	}
	unsigned int cs=0,eip=0;
	{
		auto &cpu=self->townsPtr->CPU();
		cs=cpu.state.CS().value;
		eip=cpu.state.EIP;
	}
	const unsigned int masked=data&0xffffu;
	std::lock_guard<std::mutex> lock(self->mtx);
	self->NoteGuestTargetWriteLocked(physAddr,masked,cs,eip);
}

/* static */ bool MouseCoordWriteScan::IsVRAMPhys(unsigned int physAddr)
{
	if(physAddr<SCAN_ADDR_WRAP)
	{
		if(TOWNSADDR_FMR_VRAM_BASE<=physAddr &&
		   physAddr<TOWNSADDR_FMR_VRAM_CVRAM_FONT_END)
		{
			return true;
		}
		if(TOWNSADDR_386SX_VRAM0_BASE<=physAddr &&
		   physAddr<TOWNSADDR_386SX_VRAM1_END)
		{
			return true;
		}
	}
	if(TOWNSADDR_VRAM0_BASE<=physAddr && physAddr<TOWNSADDR_VRAM1_END)
	{
		return true;
	}
	if(TOWNSADDR_VRAM_HIGHRES0_BASE<=physAddr &&
	   physAddr<TOWNSADDR_VRAM_HIGHRES2_END)
	{
		return true;
	}
	if(TOWNSADDR_FMT3631_VRAM<=physAddr && physAddr<0x46800000u)
	{
		return true;
	}
	return false;
}

bool MouseCoordWriteScan::ShouldProcessStore(unsigned int physAddr,unsigned int size) const
{
	if(2!=size && 4!=size)
	{
		return false;
	}
	if(physAddr<SCAN_ADDR_LO || 0x1000000u<=physAddr)
	{
		return false;
	}
	return true!=IsVRAMPhys(physAddr);
}

/* static */ void MouseCoordWriteScan::StoreTraceThunk(
    void *user,unsigned int physAddr,unsigned int size,unsigned int data)
{
	if(nullptr==user || 0==size)
	{
		return;
	}
	auto *self=reinterpret_cast<MouseCoordWriteScan *>(user);
	if(true!=self->ShouldProcessStore(physAddr,size))
	{
		return;
	}
	self->OnStore(physAddr,size,data);
}

int MouseCoordWriteScan::CoordMaxX(void) const
{
	if(nullptr==townsPtr)
	{
		return 1023;
	}
	return std::max(63,townsPtr->var.mouseMaxX);
}

int MouseCoordWriteScan::CoordMaxY(void) const
{
	if(nullptr==townsPtr)
	{
		return 767;
	}
	return std::max(63,townsPtr->var.mouseMaxY);
}

/* static */ bool MouseCoordWriteScan::InCoordRange(unsigned int v,int maxV)
{
	return v<=static_cast<unsigned int>(maxV);
}

/* static */ int MouseCoordWriteScan::ClampInt(int v,int lo,int hi)
{
	if(v<lo)
	{
		return lo;
	}
	if(v>hi)
	{
		return hi;
	}
	return v;
}

bool MouseCoordWriteScan::IsTbiosOrSystemCs(unsigned int cs,unsigned int inInterruptDepth)
{
	if(0!=inInterruptDepth)
	{
		return true;
	}
	if(0x110==cs || 0x110==(cs&0xFFF8))
	{
		return true;
	}
	// Do NOT treat low selectors as system: a 32-bit protected-mode DOS-extender app
	// runs its own code from small GDT selectors like 0x08 / 0x0C, and its
	// world / cursor coordinate stores come from there.  Only reject the null selector
	// and real-mode ROM-BIOS segments; interrupt context is already handled above.
	if(0==cs || cs>=0xF000)
	{
		return true;
	}
	return false;
}

void MouseCoordWriteScan::PushRecentDeltaLocked(int gpDx,int gpDy,int softDx,int softDy)
{
	recentGpDx[recentDeltaHead]=gpDx;
	recentGpDy[recentDeltaHead]=gpDy;
	recentSoftDx[recentDeltaHead]=softDx;
	recentSoftDy[recentDeltaHead]=softDy;
	recentDeltaHead=(recentDeltaHead+1)%DELTA_HISTORY;
	if(recentDeltaCount<DELTA_HISTORY)
	{
		++recentDeltaCount;
	}
	motionFreshFrames=MOTION_FRESH_FRAMES;
	huntMotionOpen.store(true,std::memory_order_relaxed);
}

bool MouseCoordWriteScan::MatchesRecentDeltaLocked(int dVal,bool useGp,bool xAxis) const
{
	if(0==dVal || 0==recentDeltaCount)
	{
		return false;
	}
	const int *hist=
	    (true==useGp) ? (true==xAxis ? recentGpDx : recentGpDy)
	                  : (true==xAxis ? recentSoftDx : recentSoftDy);
	int sum=0;
	for(unsigned int i=0; i<recentDeltaCount; ++i)
	{
		// Walk newest → oldest.
		const unsigned int idx=
		    (recentDeltaHead+DELTA_HISTORY-1-i)%DELTA_HISTORY;
		const int d=hist[idx];
		if(0!=d && dVal==d)
		{
			return true;
		}
		sum+=d;
		if(0!=sum && dVal==sum)
		{
			return true;
		}
	}
	return false;
}

void MouseCoordWriteScan::PollSoftCursorShadow(void)
{
	if(nullptr==townsPtr ||
	   true!=townsPtr->var.mouseCoordWriteScanEnabled ||
	   true==paused)
	{
		return;
	}
	// Decay host-Δ motion window only.  Soft-cursor changes must never open a
	// hunt — MOS-less titles leave soft flat while the real cursor moves.
	std::lock_guard<std::mutex> lock(mtx);
	if(0<motionFreshFrames)
	{
		--motionFreshFrames;
		if(0==motionFreshFrames)
		{
			gpMotionFresh=false;
			softShadowFresh=false;
			huntMotionOpen.store(false,std::memory_order_relaxed);
		}
	}
}

void MouseCoordWriteScan::OnMosBiosAppCall(void)
{
	if(nullptr==townsPtr ||
	   true!=townsPtr->var.mouseCoordWriteScanEnabled ||
	   true==paused)
	{
		return;
	}
	bool deltaFresh=false;
	{
		std::lock_guard<std::mutex> lock(mtx);
		deltaFresh=gpMotionFresh;
	}
	if(true!=deltaFresh)
	{
		return;
	}
	if(TraceRemaining()<(STORE_ARM_COUNT/2))
	{
		ArmMemoryTrace(STORE_ARM_COUNT);
		++biosPathArmCount;
	}
}

void MouseCoordWriteScan::OnSoftCursorAppRead(void)
{
	if(nullptr==townsPtr ||
	   true!=townsPtr->var.mouseCoordWriteScanEnabled ||
	   true==paused)
	{
		return;
	}
	// Soft reads alone are not mouse activity (MOS-less apps).  Only top up
	// the store window while a host/IO Δ is already fresh.
	bool deltaFresh=false;
	{
		std::lock_guard<std::mutex> lock(mtx);
		deltaFresh=gpMotionFresh;
	}
	if(true!=deltaFresh)
	{
		return;
	}
	if(TraceRemaining()<(STORE_ARM_COUNT/2))
	{
		ArmMemoryTrace(STORE_ARM_COUNT);
		++biosPathArmCount;
	}
}

void MouseCoordWriteScan::OnHostMouseMotion(int dx,int dy)
{
	if(nullptr==townsPtr || true==paused)
	{
		return;
	}
	if(0==dx && 0==dy)
	{
		return;
	}
	const bool scanOn=townsPtr->var.mouseCoordWriteScanEnabled;
	{
		std::lock_guard<std::mutex> lock(mtx);
		pendingGpDx=dx;
		pendingGpDy=dy;
		gpMotionFresh=true;
		motionFreshFrames=MOTION_FRESH_FRAMES;
		hostMotionSinceRefresh=true;
		PushRecentDeltaLocked(dx,dy,0,0);
		if(true==scanOn)
		{
			ObserveSoftCursorLocked();
		}
	}
	if(true!=scanOn)
	{
		return;
	}
	enabled=true;
	++ioArmCount;
	ArmMemoryTrace(STORE_ARM_IO_COUNT);
}

void MouseCoordWriteScan::OnMouseIoRead(unsigned int portIndex,bool fromTbios)
{
	(void)fromTbios;
	if(nullptr==townsPtr ||
	   true!=townsPtr->var.mouseCoordWriteScanEnabled ||
	   true==paused)
	{
		return;
	}
	if(1<portIndex)
	{
		return;
	}

	auto &port=townsPtr->gameport.state.ports[portIndex];
	int dx=port.mouseMotion.x();
	int dy=port.mouseMotion.y();
	if(0==dx && 0==dy)
	{
		dx=port.mouseMotionCopy.x();
		dy=port.mouseMotionCopy.y();
	}
	if(0==dx && 0==dy)
	{
		return;
	}
	OnHostMouseMotion(dx,dy);
}

bool MouseCoordWriteScan::ResolveSoftCursorPhys(unsigned int &physX,unsigned int &physY) const
{
	physX=0;
	physY=0;
	unsigned int altX=0,altY=0;
	return CollectSoftCursorPhys(physX,physY,altX,altY);
}

bool MouseCoordWriteScan::CollectSoftCursorPhys(
    unsigned int &physX,unsigned int &physY,
    unsigned int &altX,unsigned int &altY) const
{
	physX=0;
	physY=0;
	altX=0;
	altY=0;
	if(nullptr==townsPtr)
	{
		return false;
	}
	auto &st=townsPtr->state;
	// Same layout as SetMouseCoordinate / GetMouseCoordinate.
	switch(st.tbiosVersion)
	{
	case TBIOS_V31L22A:
		if(0==st.MOS_work_physicalAddr)
		{
			return false;
		}
		physX=st.MOS_work_physicalAddr+0x52;
		physY=physX+2;
		return true;
	case TBIOS_V31L23A:
	case TBIOS_V31L31_90:
		if(0==st.MOS_work_physicalAddr)
		{
			return false;
		}
		physX=st.MOS_work_physicalAddr+0x56;
		physY=physX+2;
		return true;
	case TBIOS_V31L31_91:
		if(0==st.TBIOS_physicalAddr)
		{
			return false;
		}
		physX=st.TBIOS_physicalAddr+0x56C;
		physY=physX+2;
		return true;
	case TBIOS_V31L31_92:
	case TBIOS_V31L31_93:
		if(0==st.TBIOS_physicalAddr)
		{
			return false;
		}
		physX=st.TBIOS_physicalAddr+0x510;
		physY=physX+2;
		return true;
	case TBIOS_V31L35:
		if(0!=st.TBIOS_physicalAddr && 0!=st.TBIOS_mouseInfoOffset)
		{
			physX=st.TBIOS_physicalAddr+st.TBIOS_mouseInfoOffset+0x0C;
			physY=physX+2;
		}
		if(0!=st.MOS_work_physicalAddr)
		{
			if(0==physX)
			{
				physX=st.MOS_work_physicalAddr+0x56;
				physY=physX+2;
			}
			else
			{
				altX=st.MOS_work_physicalAddr+0x56;
				altY=altX+2;
			}
		}
		return 0!=physX;
	default:
		if(0!=st.MOS_work_physicalAddr)
		{
			physX=st.MOS_work_physicalAddr+0x56;
			physY=physX+2;
			return true;
		}
		return false;
	}
}

bool MouseCoordWriteScan::GetSoftCursorSnapshot(unsigned int &physX,unsigned int &physY,int &mx,int &my) const
{
	if(true!=ResolveSoftCursorPhys(physX,physY) || nullptr==townsPtr)
	{
		return false;
	}
	// Soft words sit under MosCoordReadProbe; host Fetch must not re-enter
	// OnSoftCursorAppRead (infinite recursion / SEGV).
	const bool prev=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;
	mx=(int)townsPtr->mem.FetchWord(physX);
	my=(int)townsPtr->mem.FetchWord(physY);
	townsPtr->var.suppressMosCoordReadProbe=prev;
	return true;
}

bool MouseCoordWriteScan::GetSoftCursorRange(
    unsigned int &minX,unsigned int &maxX,
    unsigned int &minY,unsigned int &maxY) const
{
	minX=0;
	maxX=0;
	minY=0;
	maxY=0;
	unsigned int px=0,py=0;
	if(true!=ResolveSoftCursorPhys(px,py))
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(mtx);
	const Candidate *cx=nullptr;
	const Candidate *cy=nullptr;
	for(const auto &c : candidates)
	{
		if(c.physAddr==px && 2==c.size)
		{
			cx=&c;
		}
		if(c.physAddr==py && 2==c.size)
		{
			cy=&c;
		}
	}
	if(nullptr==cx || nullptr==cy ||
	   true!=cx->hasRange || true!=cy->hasRange ||
	   cx->minValue>cx->maxValue || cy->minValue>cy->maxValue)
	{
		return false;
	}
	minX=cx->minValue;
	maxX=cx->maxValue;
	minY=cy->minValue;
	maxY=cy->maxValue;
	return true;
}

bool MouseCoordWriteScan::IsKnownSoftCursorPhysLocked(unsigned int physAddr) const
{
	unsigned int px=0,py=0,ax=0,ay=0;
	if(true!=CollectSoftCursorPhys(px,py,ax,ay))
	{
		return false;
	}
	return physAddr==px || physAddr==py ||
	       (0!=ax && (physAddr==ax || physAddr==ay));
}

void MouseCoordWriteScan::ObserveSoftCursorLocked(void)
{
	unsigned int px=0,py=0,ax=0,ay=0;
	if(true!=CollectSoftCursorPhys(px,py,ax,ay) || nullptr==townsPtr)
	{
		return;
	}

	// Seed only — scoring happens on the following StoreWord into these addrs.
	// Soft cursor is typically updated after this gameport read returns.
	const bool prev=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;
	auto seed=[&](unsigned int phys)
	{
		auto *c=FindOrAddLocked(phys,2,true);
		if(nullptr==c)
		{
			return;
		}
		c->knownSoftCursor=true;
		if(0==c->hits)
		{
			const unsigned int v=townsPtr->mem.FetchWord(phys);
			c->lastValue=v;
			c->NoteValue(v);
		}
		c->cs=0x110;
		c->eip=0;
	};

	seed(px);
	seed(py);
	if(0!=ax)
	{
		seed(ax);
		seed(ay);
	}
	townsPtr->var.suppressMosCoordReadProbe=prev;
}

void MouseCoordWriteScan::ClearCandidates(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	candidates.clear();
	candidatePhysSet.clear();
	candidateIndex.clear();
	chaseNearPrev.clear();
	scanRangeColdBases.clear();
	scanRangeStoreCount=0;
	scanRangeBase=SCAN_ADDR_LO;
	scanRangeEnd=SCAN_ADDR_LO+SCAN_RANGE_SIZE;
	selectedPhysAddr=0;
	selectedSize=0;
	// Profile phys stay visible after a clear.
	if(true==profileLoaded)
	{
		SeedProfileCandidatesLocked();
	}
}

void MouseCoordWriteScan::KeepOnlyCandidates(const std::vector <unsigned int> &keep)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true==keep.empty())
	{
		candidates.clear();
		candidatePhysSet.clear();
		candidateIndex.clear();
		selectedPhysAddr=0;
		selectedSize=0;
		return;
	}
	candidates.erase(
	    std::remove_if(candidates.begin(),candidates.end(),
	        [&](const Candidate &c)
	        {
	            for(unsigned int p : keep)
	            {
	                if(c.physAddr==p)
	                {
	                    return false;
	                }
	            }
	            return true;
	        }),
	    candidates.end());
	bool stillSelected=false;
	for(const auto &c : candidates)
	{
		if(c.physAddr==selectedPhysAddr)
		{
			stillSelected=true;
			break;
		}
	}
	if(true!=stillSelected)
	{
		selectedPhysAddr=0;
		selectedSize=0;
	}
	RebuildCandidateMapsLocked();
}

void MouseCoordWriteScan::SelectCandidate(unsigned int physAddr,unsigned int size)
{
	std::lock_guard<std::mutex> lock(mtx);
	selectedPhysAddr=physAddr;
	selectedSize=size;
}

void MouseCoordWriteScan::RebuildCandidateMapsLocked(void)
{
	candidatePhysSet.clear();
	candidateIndex.clear();
	candidateIndex.reserve(candidates.size());
	for(size_t i=0; i<candidates.size(); ++i)
	{
		const unsigned int phys=candidates[i].physAddr;
		if(0!=phys)
		{
			candidatePhysSet.insert(phys);
			candidateIndex[phys]=i;
		}
	}
}

void MouseCoordWriteScan::NoteCandidatePhysLocked(unsigned int phys)
{
	if(0!=phys)
	{
		candidatePhysSet.insert(phys);
	}
}

void MouseCoordWriteScan::DropCandidatePhysLocked(unsigned int phys)
{
	if(0!=phys)
	{
		candidatePhysSet.erase(phys);
		candidateIndex.erase(phys);
	}
}

MouseCoordWriteScan::Candidate *MouseCoordWriteScan::FindOrAddLocked(
    unsigned int physAddr,unsigned int size,bool allowNew)
{
	if(true==IsVRAMPhys(physAddr))
	{
		return nullptr;
	}
	auto idxIt=candidateIndex.find(physAddr);
	if(idxIt!=candidateIndex.end() && idxIt->second<candidates.size())
	{
		Candidate &c=candidates[idxIt->second];
		if(c.physAddr==physAddr)
		{
			if(c.size==size)
			{
				return &c;
			}
			if(true==c.userWatch || true==c.userChase || true==c.fromProfile)
			{
				return &c;
			}
		}
	}
	Candidate *pinnedSamePhys=nullptr;
	for(auto &c : candidates)
	{
		if(c.physAddr!=physAddr)
		{
			continue;
		}
		if(c.size==size)
		{
			return &c;
		}
		if(true==c.userWatch || true==c.userChase || true==c.fromProfile)
		{
			pinnedSamePhys=&c;
		}
	}
	if(nullptr!=pinnedSamePhys)
	{
		return pinnedSamePhys;
	}
	if(true!=allowNew)
	{
		return nullptr;
	}
	if(MAX_CANDIDATES<=candidates.size())
	{
		auto rank=[](const Candidate &c)->long long
		{
			if(true==c.userWatch || true==c.userChase || true==c.fromProfile)
			{
				return 1000000000LL;
			}
			return static_cast<long long>(c.hits);
		};
		auto worst=std::min_element(candidates.begin(),candidates.end(),
			[&](const Candidate &a,const Candidate &b)
			{
				return rank(a)<rank(b);
			});
		if(worst==candidates.end() ||
		   true==worst->knownSoftCursor || true==worst->userWatch ||
		   true==worst->userChase || true==worst->fromProfile)
		{
			// Fall back: evict the quietest non-pinned cell (even if motionCorr).
			worst=std::min_element(candidates.begin(),candidates.end(),
				[](const Candidate &a,const Candidate &b)
				{
					auto pinned=[](const Candidate &c)
					{
						return true==c.knownSoftCursor || true==c.userWatch ||
						       true==c.userChase || true==c.fromProfile;
					};
					if(pinned(a)!=pinned(b))
					{
						return !pinned(a);
					}
					return a.lastHitSerial<b.lastHitSerial;
				});
			if(worst==candidates.end() ||
			   true==worst->knownSoftCursor || true==worst->userWatch ||
			   true==worst->userChase || true==worst->fromProfile)
			{
				return nullptr;
			}
		}
		DropCandidatePhysLocked(worst->physAddr);
		*worst=Candidate();
		worst->physAddr=physAddr;
		worst->size=size;
		NoteCandidatePhysLocked(physAddr);
		candidateIndex[physAddr]=static_cast<size_t>(worst-candidates.begin());
		return &(*worst);
	}
	candidates.push_back(Candidate());
	candidates.back().physAddr=physAddr;
	candidates.back().size=size;
	NoteCandidatePhysLocked(physAddr);
	candidateIndex[physAddr]=candidates.size()-1;
	return &candidates.back();
}

void MouseCoordWriteScan::OnStore(unsigned int physAddr,unsigned int size,unsigned int data)
{
	if(nullptr==townsPtr ||
	   true!=townsPtr->var.mouseCoordWriteScanEnabled ||
	   true==paused ||
	   true==suppressOwnStore)
	{
		return;
	}
	if(2!=size && 4!=size)
	{
		return;
	}
	if(physAddr<SCAN_ADDR_LO || 0x1000000u<=physAddr)
	{
		return;
	}
	if(true==IsVRAMPhys(physAddr))
	{
		return;
	}

	unsigned int cs=0,eip=0,irqDepth=0;
	{
		auto &cpu=townsPtr->CPU();
		cs=cpu.state.CS().value;
		eip=cpu.state.EIP;
		irqDepth=cpu.state.inInterruptDepth;
	}

	if(true==IsTbiosOrSystemCs(cs,irqDepth))
	{
		unsigned int sx=0,sy=0,ax=0,ay=0;
		if(true!=CollectSoftCursorPhys(sx,sy,ax,ay) ||
		   (physAddr!=sx && physAddr!=sy &&
		    physAddr!=ax && physAddr!=ay))
		{
			return;
		}
	}

	const unsigned int masked=((2==size)?(data&0xffffu):data);

	std::lock_guard<std::mutex> lock(mtx);

	// Host/IO Δ only — soft-cursor activity must not gate candidate pickup.
	if(true!=gpMotionFresh)
	{
		return;
	}

	auto *c=FindOrAddLocked(physAddr,size,true);
	if(nullptr==c)
	{
		return;
	}

	if(true==IsKnownSoftCursorPhysLocked(physAddr))
	{
		c->knownSoftCursor=true;
	}

	NoteCandidateValueLocked(*c,masked,cs,eip);

	if(candidates.size()>=(MAX_CANDIDATES*3u/4u) &&
	   0==(storeEventCount&0x3FFu))
	{
		PruneStaleLocked();
	}
}

void MouseCoordWriteScan::NoteCandidateValueLocked(
    Candidate &c,unsigned int masked,unsigned int cs,unsigned int eip)
{
	const int maxX=CoordMaxX();
	const int maxY=CoordMaxY();

	const unsigned int prevHits=c.hits;
	c.prevValue=c.lastValue;
	c.hasPrev=(0<prevHits);

	++storeEventCount;
	++c.hits;
	c.lastValue=masked;
	c.NoteValue(masked);
	UpdateResRangeFitLocked(c);
	c.lastHitSerial=storeEventCount;
	if(1==c.hits)
	{
		c.firstHitSerial=storeEventCount;
	}
	if(0!=cs || 0!=eip)
	{
		c.cs=cs;
		c.eip=eip;
	}

	if(true==InCoordRange(masked,maxX))
	{
		++c.scoreX;
		++c.inRangeHits;
	}
	if(true==InCoordRange(masked,maxY))
	{
		++c.scoreY;
		++c.inRangeHits;
	}

	if(true==c.hasPrev && true==gpMotionFresh)
	{
		const int dWord=static_cast<int>(static_cast<short>(masked&0xffffu))-
		                static_cast<int>(static_cast<short>(c.prevValue&0xffffu));
		if(0!=dWord &&
		   (dWord==pendingGpDx || dWord==pendingGpDy ||
		    true==MatchesRecentDeltaLocked(dWord,true,true) ||
		    true==MatchesRecentDeltaLocked(dWord,true,false)))
		{
			++c.motionCorrHits;
			c.motionCorr=true;
			c.lastMotionSerial=storeEventCount;
		}
	}
}

void MouseCoordWriteScan::UpdateResRangeFitLocked(Candidate &c) const
{
	c.resRangeFit=false;
	c.resRangeFitX=false;
	c.resRangeFitY=false;
	if(true!=c.hasRange)
	{
		return;
	}
	const int maxX=CoordMaxX();
	const int maxY=CoordMaxY();
	constexpr int slack=8;
	const int sMin=static_cast<int>(static_cast<short>(c.minValue&0xffffu));
	const int sMax=static_cast<int>(static_cast<short>(c.maxValue&0xffffu));
	const bool loOk=(sMin>=-slack && sMin<=slack);
	if(true!=loOk)
	{
		return;
	}
	if(sMax+slack>=maxX)
	{
		c.resRangeFitX=true;
		c.resRangeFit=true;
	}
	else if(sMax+slack>=maxY)
	{
		c.resRangeFitY=true;
		c.resRangeFit=true;
	}
}

void MouseCoordWriteScan::PruneStaleLocked(void)
{
	candidates.erase(
	    std::remove_if(candidates.begin(),candidates.end(),
	        [](const Candidate &c)
	        {
	            if(true==c.userWatch || true==c.userChase || true==c.fromProfile)
	            {
	                return false;
	            }
	            if(0!=c.inRangeHits || 0!=c.motionCorrHits || true==c.motionCorr)
	            {
	                return false;
	            }
	            return c.hits<MIN_PICKUP_HITS;
	        }),
	    candidates.end());
	RebuildCandidateMapsLocked();
}

void MouseCoordWriteScan::ClearCandidateRanges(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	for(auto &c : candidates)
	{
		c.ClearRange();
	}
}

void MouseCoordWriteScan::RefreshWatchedValues(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(nullptr==townsPtr)
	{
		return;
	}
	const bool scanOn=townsPtr->var.mouseCoordWriteScanEnabled;
	// Host/IO Δ only (never soft-cursor change — MOS-less titles leave soft flat).
	const bool motionOk=hostMotionSinceRefresh;
	hostMotionSinceRefresh=false;
	const bool huntOk=gpMotionFresh;
	const int maxX=CoordMaxX();
	const int maxY=CoordMaxY();
	const bool prev=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;
	for(auto &c : candidates)
	{
		const bool tracked=
		    0!=c.inRangeHits || 0!=c.motionCorrHits || true==c.motionCorr ||
		    true==c.knownSoftCursor;
		const bool inList=
		    c.hits>=MIN_PICKUP_HITS || true==tracked ||
		    true==c.userWatch || true==c.userChase || true==c.fromProfile;
		if(true==scanOn)
		{
			if(c.hits<MIN_PICKUP_HITS && true!=tracked &&
			   true!=c.userWatch && true!=c.userChase &&
			   true!=c.fromProfile && true!=c.knownSoftCursor)
			{
				continue;
			}
		}
		else if(true!=inList)
		{
			continue;
		}

		const unsigned int v=townsPtr->mem.FetchWord(c.physAddr)&0xffffu;
		const bool valueChanged=(v!=c.lastValue);
		const bool mouseActive=(true==motionOk || true==huntOk);
		// min..max/value tracked as signed int16.
		c.NoteValue(v);
		UpdateResRangeFitLocked(c);
		if(true==scanOn && true==huntOk && true==valueChanged)
		{
			NoteCandidateValueLocked(c,v,c.cs,c.eip);
		}
		else
		{
			c.lastValue=v;
		}

		// Values that keep changing with no host mouse activity are timers /
		// animation / unrelated RAM — not cursor coordinates.
		if(true==valueChanged && true!=mouseActive)
		{
			++c.idleChangeStreak;
		}
		else if(true==mouseActive)
		{
			c.idleChangeStreak=0;
		}

		// Do NOT delete based on absolute value vs resolution (some titles
		// use coords that exceed screen size).  Drop only when the
		// observed span (max-min) substantially exceeds the screen extent.
		if(true!=scanOn && true==motionOk && true==c.hasRange)
		{
			const int sMin=static_cast<int>(static_cast<short>(c.minValue&0xffffu));
			const int sMax=static_cast<int>(static_cast<short>(c.maxValue&0xffffu));
			const int span=sMax-sMin;
			const int resExtent=std::max(maxX,maxY);
			// "Substantial": more than 2× the larger screen axis.
			const bool spanTooLarge=(span>(resExtent*2));
			if(true==spanTooLarge)
			{
				++c.outOfResStreak;
			}
			else
			{
				c.outOfResStreak=0;
			}
		}
	}
	townsPtr->var.suppressMosCoordReadProbe=prev;
	ProbeChaseNeighborhoodLocked(huntOk);
	PruneIdleChurnLocked();
}

void MouseCoordWriteScan::PruneIdleChurnLocked(void)
{
	candidates.erase(
	    std::remove_if(candidates.begin(),candidates.end(),
	        [](const Candidate &c)
	        {
	            if(true==c.userWatch || true==c.userChase || true==c.fromProfile ||
	               true==c.knownSoftCursor)
	            {
	                return false;
	            }
	            if(c.idleChangeStreak>=IDLE_CHANGE_DROP)
	            {
	                return true;
	            }
	            return c.outOfResStreak>=OUT_OF_RES_DROP;
	        }),
	    candidates.end());
	RebuildCandidateMapsLocked();
}

std::vector <MouseCoordWriteScan::Candidate> MouseCoordWriteScan::GetTopCandidates(unsigned int maxN) const
{
	std::lock_guard<std::mutex> lock(mtx);
	std::vector <Candidate> sorted;
	sorted.reserve(candidates.size());
	for(const auto &c : candidates)
	{
		const bool tracked=
		    0!=c.inRangeHits || 0!=c.motionCorrHits || true==c.motionCorr ||
		    true==c.knownSoftCursor;
		if(c.hits<MIN_PICKUP_HITS && true!=tracked &&
		   true!=c.userWatch && true!=c.userChase && true!=c.fromProfile)
		{
			continue;
		}
		sorted.push_back(c);
	}
	std::sort(sorted.begin(),sorted.end(),
		[](const Candidate &a,const Candidate &b)
		{
			if(a.resRangeFit!=b.resRangeFit)
			{
				return a.resRangeFit>b.resRangeFit;
			}
			if(a.inRangeHits!=b.inRangeHits)
			{
				return a.inRangeHits>b.inRangeHits;
			}
			const int sa=(a.scoreX+a.scoreY)-a.rejectScore;
			const int sb=(b.scoreX+b.scoreY)-b.rejectScore;
			if(sa!=sb)
			{
				return sa>sb;
			}
			if(a.motionCorrHits!=b.motionCorrHits)
			{
				return a.motionCorrHits>b.motionCorrHits;
			}
			if(a.hits!=b.hits)
			{
				return a.hits>b.hits;
			}
			if(a.lastHitSerial!=b.lastHitSerial)
			{
				return a.lastHitSerial>b.lastHitSerial;
			}
			return a.physAddr<b.physAddr;
		});
	if(maxN<sorted.size())
	{
		sorted.resize(maxN);
	}
	return sorted;
}


std::string MouseCoordWriteScan::Profile::ToIniString(void) const
{
	Profile sync=*this;
	sync.SyncLegacyFlagsFromMode();
	std::ostringstream oss;
	if(true==sync.machine.HasAny())
	{
		oss << "[machine]\n";
		if(true==sync.machine.hasFrequencyMhz)
		{
			oss << "frequency_mhz=" << sync.machine.frequencyMhz << "\n";
		}
		if(true==sync.machine.hasCustomFrequencyMhz)
		{
			oss << "custom_frequency_mhz=" << sync.machine.customFrequencyMhz << "\n";
		}
		if(true==sync.machine.hasFastMode)
		{
			oss << "fast_mode=" << (sync.machine.fastMode ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasMemSizeInMB)
		{
			oss << "mem_size_mb=" << sync.machine.memSizeInMB << "\n";
		}
		if(true==sync.machine.hasGamePort0)
		{
			oss << "gameport0=" << sync.machine.gamePort0 << "\n";
		}
		if(true==sync.machine.hasGamePort1)
		{
			oss << "gameport1=" << sync.machine.gamePort1 << "\n";
		}
		if(true==sync.machine.hasMaxButtonHoldMs0)
		{
			oss << "max_button_hold_ms0=" << sync.machine.maxButtonHoldMs0 << "\n";
		}
		if(true==sync.machine.hasMaxButtonHoldMs1)
		{
			oss << "max_button_hold_ms1=" << sync.machine.maxButtonHoldMs1 << "\n";
		}
		if(true==sync.machine.hasModelGroup)
		{
			oss << "model_group=" << sync.machine.modelGroup << "\n";
		}
		else if(true==sync.machine.hasModelGroupIndex)
		{
			oss << "model_group_index=" << sync.machine.modelGroupIndex << "\n";
		}
		if(true==sync.machine.hasCpu)
		{
			oss << "cpu=" << sync.machine.cpu << "\n";
		}
		if(true==sync.machine.hasCpuHighFidelity)
		{
			oss << "cpu_high_fidelity=" << (sync.machine.cpuHighFidelity ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasPretend386DX)
		{
			oss << "pretend_386dx=" << (sync.machine.pretend386DX ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasUseFPU)
		{
			oss << "use_fpu=" << (sync.machine.useFPU ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasFastScsi)
		{
			oss << "fast_scsi=" << (sync.machine.fastScsi ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasFastFd)
		{
			oss << "fast_fd=" << (sync.machine.fastFd ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasMidiBoard)
		{
			oss << "midi_board=" << (sync.machine.midiBoard ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasHighResCrtc)
		{
			oss << "high_res_crtc=" << (sync.machine.highResCrtc ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasHighResPcm)
		{
			oss << "high_res_pcm=" << (sync.machine.highResPcm ? 1 : 0) << "\n";
		}
		if(true==sync.machine.hasCdSpeed)
		{
			oss << "cd_speed=" << sync.machine.cdSpeed << "\n";
		}
		if(true==sync.machine.hasSpriteTransfer)
		{
			oss << "sprite_transfer=" << sync.machine.spriteTransfer << "\n";
		}
		if(true==sync.machine.hasFdImg[0])
		{
			oss << "fd0=" << sync.machine.fdImg[0] << "\n";
		}
		if(true==sync.machine.hasFdImg[1])
		{
			oss << "fd1=" << sync.machine.fdImg[1] << "\n";
		}
		oss << "\n";
	}
	oss << "[mouse_coord]\n";
	if(0!=sync.physX || 0!=sync.physY)
	{
		// Legacy soft phys — no longer written by the UI; keep for old files only.
		oss << "phys_x=0x" << cpputil::Uitox(sync.physX) << "\n";
		oss << "phys_y=0x" << cpputil::Uitox(sync.physY) << "\n";
	}
	// Always write pair0 so Clear→0 clears stale addresses on disk.
	// Higher pairs are omitted when unset (no phys / no range).
	auto writePair=[&](unsigned int i,bool force)
	{
		const auto &pr=sync.pair[i];
		const bool hasRange=
		    (true==pr.hasRangeX && pr.rangeMinX!=pr.rangeMaxX) ||
		    (true==pr.hasRangeY && pr.rangeMinY!=pr.rangeMaxY);
		if(true!=force && true!=pr.Valid() && true!=hasRange)
		{
			return;
		}
		oss << "pair" << i << "_x=0x" << cpputil::Uitox(pr.physX) << "\n";
		oss << "pair" << i << "_y=0x" << cpputil::Uitox(pr.physY) << "\n";
		oss << "pair" << i << "_bias_x=" << pr.biasX << "\n";
		oss << "pair" << i << "_bias_y=" << pr.biasY << "\n";
		oss << "pair" << i << "_scale_x=" << pr.scaleX << "\n";
		oss << "pair" << i << "_scale_y=" << pr.scaleY << "\n";
		const int minX=(true==pr.hasRangeX && pr.rangeMinX!=pr.rangeMaxX) ? pr.rangeMinX : 0;
		const int maxX=(true==pr.hasRangeX && pr.rangeMinX!=pr.rangeMaxX) ? pr.rangeMaxX : 0;
		const int minY=(true==pr.hasRangeY && pr.rangeMinY!=pr.rangeMaxY) ? pr.rangeMinY : 0;
		const int maxY=(true==pr.hasRangeY && pr.rangeMinY!=pr.rangeMaxY) ? pr.rangeMaxY : 0;
		oss << "pair" << i << "_min_x=" << minX << "\n";
		oss << "pair" << i << "_max_x=" << maxX << "\n";
		oss << "pair" << i << "_min_y=" << minY << "\n";
		oss << "pair" << i << "_max_y=" << maxY << "\n";
	};
	writePair(0,true);
	for(unsigned int i=1; i<MAX_COORD_PAIRS; ++i)
	{
		writePair(i,false);
	}
	oss << "integration_mode=" << sync.integrationMode << "\n";
	// feedback_only / enabled are legacy mirrors of integration_mode — not written.
	oss << "offset_x=" << sync.offsetX << "\n";
	oss << "offset_y=" << sync.offsetY << "\n";
	// Legacy top-level scale mirrors pair0 (DW uses per-pair scale).
	oss << "scale_x=" << sync.pair[0].scaleX << "\n";
	oss << "scale_y=" << sync.pair[0].scaleY << "\n";
	oss << "invert_x=" << (sync.invertX ? 1 : 0) << "\n";
	oss << "invert_y=" << (sync.invertY ? 1 : 0) << "\n";
	oss << "wait_feedback=" << (sync.waitFeedback ? 1 : 0) << "\n";
	oss << "stop_soft_write=" << (sync.stopSoftWrite ? 1 : 0) << "\n";
	oss << "verified=" << (sync.verified ? 1 : 0) << "\n";
	oss << "cd_size=" << sync.cdSize << "\n";
	// cd_basename omitted — disc identity is fingerprint / content hash.
	if(true!=sync.discVolumeLabel.empty())
	{
		oss << "disc_volume_label=" << sync.discVolumeLabel << "\n";
	}
	if(true!=sync.discSystemId.empty())
	{
		oss << "disc_system_id=" << sync.discSystemId << "\n";
	}
	if(0!=sync.discContentHash32)
	{
		oss << "disc_content_hash=0x" << cpputil::Uitox(sync.discContentHash32) << "\n";
	}
	if(0!=sync.discFingerprintHash32)
	{
		oss << "disc_fingerprint_hash=0x" << cpputil::Uitox(sync.discFingerprintHash32) << "\n";
	}
	if(true!=sync.appExecName.empty())
	{
		oss << "app_exec_name=" << sync.appExecName << "\n";
		oss << "app_exec_hash=0x" << cpputil::Uitox(sync.appExecHash32) << "\n";
	}
	return oss.str();
}

bool MouseCoordWriteScan::Profile::FromIniString(const std::string &ini,Profile &out)
{
	Profile p;
	std::istringstream iss(ini);
	std::string line;
	enum { SEC_NONE, SEC_MOUSE, SEC_MACHINE } section=SEC_NONE;
	bool hasOffsetKey=false;
	bool hasIntegrationModeKey=false;
	int legacyMinX=0,legacyMinY=0;
	bool hasLegacyMin=false;
	// Legacy single app/world slots load into pair[] after the parse loop.
	CoordPair legacyApp,legacyWorld;
	bool hasPairScale[MAX_COORD_PAIRS]={false,false,false,false};
	while(std::getline(iss,line))
	{
		while(true!=line.empty() && ('\r'==line.back() || ' '==line.back() || '\t'==line.back()))
		{
			line.pop_back();
		}
		if(line.empty() || '#'==line[0] || ';'==line[0])
		{
			continue;
		}
		if('['==line.front())
		{
			if("[mouse_coord]"==line)
			{
				section=SEC_MOUSE;
			}
			else if("[machine]"==line)
			{
				section=SEC_MACHINE;
			}
			else
			{
				section=SEC_NONE;
			}
			continue;
		}
		if(SEC_NONE==section)
		{
			continue;
		}
		const auto eq=line.find('=');
		if(std::string::npos==eq)
		{
			continue;
		}
		const std::string key=line.substr(0,eq);
		const std::string val=line.substr(eq+1);
		if(SEC_MACHINE==section)
		{
			if("frequency_mhz"==key)
			{
				p.machine.frequencyMhz=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasFrequencyMhz=true;
			}
			else if("custom_frequency_mhz"==key)
			{
				p.machine.customFrequencyMhz=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasCustomFrequencyMhz=true;
			}
			else if("fast_mode"==key)
			{
				p.machine.fastMode=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasFastMode=true;
			}
			else if("mem_size_mb"==key)
			{
				p.machine.memSizeInMB=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasMemSizeInMB=true;
			}
			else if("gameport0"==key)
			{
				p.machine.gamePort0=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
				p.machine.hasGamePort0=true;
			}
			else if("gameport1"==key)
			{
				p.machine.gamePort1=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
				p.machine.hasGamePort1=true;
			}
			else if("max_button_hold_ms0"==key)
			{
				p.machine.maxButtonHoldMs0=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasMaxButtonHoldMs0=true;
			}
			else if("max_button_hold_ms1"==key)
			{
				p.machine.maxButtonHoldMs1=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasMaxButtonHoldMs1=true;
			}
			else if("model_group_index"==key)
			{
				p.machine.modelGroupIndex=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasModelGroupIndex=true;
			}
			else if("model_group"==key)
			{
				p.machine.modelGroup=val;
				p.machine.hasModelGroup=true;
			}
			else if("cpu"==key)
			{
				p.machine.cpu=val;
				p.machine.hasCpu=true;
			}
			else if("cpu_high_fidelity"==key)
			{
				p.machine.cpuHighFidelity=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasCpuHighFidelity=true;
			}
			else if("pretend_386dx"==key)
			{
				p.machine.pretend386DX=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasPretend386DX=true;
			}
			else if("use_fpu"==key)
			{
				p.machine.useFPU=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasUseFPU=true;
			}
			else if("fast_scsi"==key)
			{
				p.machine.fastScsi=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasFastScsi=true;
			}
			else if("fast_fd"==key)
			{
				p.machine.fastFd=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasFastFd=true;
			}
			else if("midi_board"==key)
			{
				p.machine.midiBoard=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasMidiBoard=true;
			}
			else if("high_res_crtc"==key)
			{
				p.machine.highResCrtc=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasHighResCrtc=true;
			}
			else if("high_res_pcm"==key)
			{
				p.machine.highResPcm=(0!=std::strtol(val.c_str(),nullptr,0));
				p.machine.hasHighResPcm=true;
			}
			else if("cd_speed"==key)
			{
				p.machine.cdSpeed=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasCdSpeed=true;
			}
			else if("sprite_transfer"==key)
			{
				p.machine.spriteTransfer=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				p.machine.hasSpriteTransfer=true;
			}
			else if("fd0"==key)
			{
				p.machine.fdImg[0]=val;
				p.machine.hasFdImg[0]=true;
			}
			else if("fd1"==key)
			{
				p.machine.fdImg[1]=val;
				p.machine.hasFdImg[1]=true;
			}
			continue;
		}
		// SEC_MOUSE — existing key parse (same as before)
		if("phys_x"==key)
		{
			p.physX=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("phys_y"==key)
		{
			p.physY=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("app_x"==key)
		{
			legacyApp.physX=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("app_y"==key)
		{
			legacyApp.physY=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("world_x"==key)
		{
			legacyWorld.physX=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("world_y"==key)
		{
			legacyWorld.physY=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
		}
		else if("world_bias_x"==key)
		{
			legacyWorld.biasX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
		}
		else if("world_bias_y"==key)
		{
			legacyWorld.biasY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
		}
		else if(0==key.compare(0,4,"pair") && 6<=key.size())
		{
			const unsigned int idx=(unsigned int)(key[4]-'0');
			if(idx<MAX_COORD_PAIRS)
			{
				const std::string sub=key.substr(5); // "_x" "_y" "_bias_x" "_bias_y"
				if("_x"==sub)
				{
					p.pair[idx].physX=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
				}
				else if("_y"==sub)
				{
					p.pair[idx].physY=static_cast<unsigned int>(std::strtoul(val.c_str(),nullptr,0));
				}
				else if("_bias_x"==sub)
				{
					p.pair[idx].biasX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				}
				else if("_bias_y"==sub)
				{
					p.pair[idx].biasY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
				}
				else if("_scale_x"==sub)
				{
					p.pair[idx].scaleX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					hasPairScale[idx]=true;
				}
				else if("_scale_y"==sub)
				{
					p.pair[idx].scaleY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					hasPairScale[idx]=true;
				}
				else if("_min_x"==sub)
				{
					p.pair[idx].rangeMinX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					p.pair[idx].hasRangeX=true;
				}
				else if("_max_x"==sub)
				{
					p.pair[idx].rangeMaxX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					p.pair[idx].hasRangeX=true;
				}
				else if("_min_y"==sub)
				{
					p.pair[idx].rangeMinY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					p.pair[idx].hasRangeY=true;
				}
				else if("_max_y"==sub)
				{
					p.pair[idx].rangeMaxY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
					p.pair[idx].hasRangeY=true;
				}
			}
		}
		else if("feedback_only"==key)
		{
			p.feedbackOnly=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("enabled"==key)
		{
			p.enabled=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("integration_mode"==key)
		{
			const int mode=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
			if(INTEGRATION_DIFFERENTIAL==mode ||
			   INTEGRATION_MOS==mode ||
			   INTEGRATION_DIRECT_WRITE==mode ||
			   INTEGRATION_GAME_FEEDBACK==mode ||
			   INTEGRATION_AUTO==mode)
			{
				p.integrationMode=mode;
				hasIntegrationModeKey=true;
			}
		}
		else if("offset_x"==key)
		{
			p.offsetX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
			hasOffsetKey=true;
		}
		else if("offset_y"==key)
		{
			p.offsetY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
			hasOffsetKey=true;
		}
		else if("invert_x"==key)
		{
			p.invertX=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("invert_y"==key)
		{
			p.invertY=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("wait_feedback"==key)
		{
			p.waitFeedback=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("stop_soft_write"==key)
		{
			p.stopSoftWrite=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("min_x"==key)
		{
			legacyMinX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
			hasLegacyMin=true;
		}
		else if("min_y"==key)
		{
			legacyMinY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
			hasLegacyMin=true;
		}
		else if("scale_x"==key)
		{
			p.scaleX=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
		}
		else if("scale_y"==key)
		{
			p.scaleY=static_cast<int>(std::strtol(val.c_str(),nullptr,0));
		}
		else if("verified"==key)
		{
			p.verified=(0!=std::strtol(val.c_str(),nullptr,0));
		}
		else if("cd_size"==key)
		{
			p.cdSize=std::strtoull(val.c_str(),nullptr,0);
		}
		else if("cd_basename"==key)
		{
			p.cdBasename=val;
		}
		else if("disc_volume_label"==key)
		{
			p.discVolumeLabel=val;
		}
		else if("disc_system_id"==key)
		{
			p.discSystemId=val;
		}
		else if("disc_content_hash"==key)
		{
			// Uitox writes bare hex (e.g. 86A01CD0); base-0 strtoul stops at 'A'.
			const char *s=val.c_str();
			if(0==strncmp(s,"0x",2) || 0==strncmp(s,"0X",2))
			{
				s+=2;
			}
			p.discContentHash32=static_cast<unsigned int>(std::strtoul(s,nullptr,16));
		}
		else if("disc_fingerprint_hash"==key)
		{
			const char *s=val.c_str();
			if(0==strncmp(s,"0x",2) || 0==strncmp(s,"0X",2))
			{
				s+=2;
			}
			p.discFingerprintHash32=static_cast<unsigned int>(std::strtoul(s,nullptr,16));
		}
		else if("app_exec_name"==key)
		{
			p.appExecName=NormalizeDosExecPath(val);
		}
		else if("app_exec_hash"==key)
		{
			const char *s=val.c_str();
			if(0==strncmp(s,"0x",2) || 0==strncmp(s,"0X",2))
			{
				s+=2;
			}
			p.appExecHash32=static_cast<unsigned int>(std::strtoul(s,nullptr,16));
		}
		// max_x / max_y ignored (screen size comes from CRTC).
	}
	// Machine-only stubs (no phys yet) are valid disc profiles.
	if(0==p.physX && 0==p.physY && true!=p.machine.HasAny() &&
	   0==p.discFingerprintHash32)
	{
		return false;
	}
	// Legacy: soft origin lived in min_* when offset_* was absent.
	if(true!=hasOffsetKey && true==hasLegacyMin)
	{
		p.offsetX=legacyMinX;
		p.offsetY=legacyMinY;
	}
	// Legacy app/world → first free pair slots.
	auto placeLegacy=[&](const CoordPair &src)
	{
		if(true!=src.Valid())
		{
			return;
		}
		for(auto &dst : p.pair)
		{
			if(dst.physX==src.physX && dst.physY==src.physY)
			{
				return;
			}
		}
		for(auto &dst : p.pair)
		{
			if(true!=dst.Valid())
			{
				dst=src;
				return;
			}
		}
	};
	placeLegacy(legacyApp);
	placeLegacy(legacyWorld);
	// Old INI only had top-level scale_*; apply to pair0 when per-pair scale absent.
	if(true!=hasPairScale[0])
	{
		p.pair[0].scaleX=p.scaleX;
		p.pair[0].scaleY=p.scaleY;
	}
	p.scaleX=p.pair[0].scaleX;
	p.scaleY=p.pair[0].scaleY;
	if(true!=hasIntegrationModeKey)
	{
		p.DeriveModeFromLegacyFlags();
	}
	p.SyncLegacyFlagsFromMode();
	out=p;
	return true;
}

void MouseCoordWriteScan::SetProfileDirectory(const std::string &dir)
{
	profileDir=dir;
}

bool MouseCoordWriteScan::DiscProfileLoaded(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return profileLoaded && activeProfile.HasFingerprint();
}

bool MouseCoordWriteScan::ProfileLoaded(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return profileLoaded && activeProfile.HasMouseIntegration();
}

MouseCoordWriteScan::Profile MouseCoordWriteScan::GetActiveProfile(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return activeProfile;
}

void MouseCoordWriteScan::SetActiveProfile(const Profile &p)
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		activeProfile=p;
		activeProfile.SyncLegacyFlagsFromMode();
		profileLoaded=activeProfile.HasFingerprint();
		softLogValid=false;
		if(true==activeProfile.HasPhys() || 0<activeProfile.NumPairs())
		{
			SeedProfileCandidatesLocked();
		}
	}
	if(nullptr!=townsPtr)
	{
		townsPtr->DontControlMouse();
	}
}

void MouseCoordWriteScan::LogSoftCursorIfChanged(int mx,int my)
{
	if(nullptr==townsPtr || true!=townsPtr->var.mouseCoordProfileApply)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(mtx);
	if(true==softLogValid && mx==lastSoftLogX && my==lastSoftLogY)
	{
		return;
	}
	lastSoftLogX=mx;
	lastSoftLogY=my;
	softLogValid=true;
}

bool MouseCoordWriteScan::FindAppShadowPair(Candidate &outX,Candidate &outY) const
{
	if(nullptr==townsPtr)
	{
		return false;
	}

	int screenW=0,screenH=0;
	if(true!=TryGuestScreenSize(screenW,screenH))
	{
		return false;
	}
	const int maxX=screenW-1;
	const int maxY=screenH-1;

	// Reference is always the BIOS soft cursor: the app copy tracks it with drift.
	int softX=0,softY=0;
	unsigned int spx=0,spy=0;
	if(true!=GetSoftCursorSnapshot(spx,spy,softX,softY))
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(mtx);
	const Candidate *bestX=nullptr;
	const Candidate *bestY=nullptr;
	int bestScoreX=-1;
	int bestScoreY=-1;
	int bestXd=0x7fffffff;
	int bestYd=0x7fffffff;
	for(const auto &c : candidates)
	{
		if(true!=c.appShadow || c.size<2 || true==c.knownSoftCursor)
		{
			continue;
		}
		if(0==c.appShadowHits)
		{
			continue;
		}
		// Reject polluted candidates (e.g. observed max 25416).
		if(true==c.hasRange &&
		   (c.maxValue>(unsigned int)(maxX*2+16) || c.maxValue>(unsigned int)(maxY*2+16)))
		{
			continue;
		}
		const int v=(int)(short)(c.lastValue&0xffffu);
		// Drawn cursor can sit ~offset away from soft (often soft+16..32 on Y).
		const int SHADOW_NEAR_MAX=64;
		const int hitScore=static_cast<int>(c.appShadowHits)*4+c.scoreX+c.scoreY;
		if(0<c.scoreX && 0<=v && v<=maxX)
		{
			const int d=std::abs(v-softX);
			if(d<=SHADOW_NEAR_MAX &&
			   (hitScore>bestScoreX || (hitScore==bestScoreX && d<bestXd)))
			{
				bestX=&c;
				bestScoreX=hitScore;
				bestXd=d;
			}
		}
		if(0<c.scoreY && 0<=v && v<=maxY)
		{
			const int d=std::abs(v-softY);
			if(d<=SHADOW_NEAR_MAX &&
			   (hitScore>bestScoreY || (hitScore==bestScoreY && d<bestYd)))
			{
				bestY=&c;
				bestScoreY=hitScore;
				bestYd=d;
			}
		}
	}
	if(nullptr==bestX || nullptr==bestY || bestX==bestY)
	{
		return false;
	}
	outX=*bestX;
	outY=*bestY;
	return true;
}

bool MouseCoordWriteScan::TryReadAppShadowCoords(int &mx,int &my) const
{
	mx=0;
	my=0;
	Candidate cx,cy;
	if(true!=FindAppShadowPair(cx,cy))
	{
		return false;
	}
	mx=(int)(short)cx.lastValue;
	my=(int)(short)cy.lastValue;
	return true;
}

bool MouseCoordWriteScan::TryReadAppShadowPhys(unsigned int &px,unsigned int &py) const
{
	px=0;
	py=0;
	Candidate cx,cy;
	if(true!=FindAppShadowPair(cx,cy))
	{
		return false;
	}
	px=cx.physAddr;
	py=cy.physAddr;
	return true;
}

bool MouseCoordWriteScan::FindScreenDrawPair(Candidate &outX,Candidate &outY) const
{
	if(nullptr==townsPtr)
	{
		return false;
	}
	int screenW=0,screenH=0;
	if(true!=TryGuestScreenSize(screenW,screenH))
	{
		return false;
	}
	const int maxX=screenW-1;
	const int maxY=screenH-1;

	int softX=0,softY=0;
	unsigned int spx=0,spy=0;
	GetSoftCursorSnapshot(spx,spy,softX,softY);

	std::lock_guard<std::mutex> lock(mtx);
	const Candidate *bestX=nullptr;
	const Candidate *bestY=nullptr;
	int bestScoreX=-1;
	int bestScoreY=-1;
	int bestXd=0x7fffffff;
	int bestYd=0x7fffffff;
	for(const auto &c : candidates)
	{
		if(c.size<2 || true==c.knownSoftCursor || true==c.motionPulse)
		{
			continue;
		}
		if(0==c.screenDrawHits && true!=c.screenDraw)
		{
			continue;
		}
		// World / polluted: X beyond viewport.
		if(true==c.hasRange && c.maxValue>(unsigned int)(maxX+16))
		{
			continue;
		}
		const int v=(int)(short)(c.lastValue&0xffffu);
		const int hitScore=
		    static_cast<int>(c.screenDrawHits)*8+
		    static_cast<int>(c.appShadowHits)*2+
		    c.scoreX+c.scoreY;
		const int NEAR_SOFT=80;
		if(0<c.scoreX && 0<=v && v<=maxX)
		{
			const int d=std::abs(v-softX);
			if(d<=NEAR_SOFT &&
			   (hitScore>bestScoreX || (hitScore==bestScoreX && d<bestXd)))
			{
				bestX=&c;
				bestScoreX=hitScore;
				bestXd=d;
			}
		}
		// Prefer Y that has been observed past a typical playfield band (144).
		if(0<c.scoreY && 0<=v && v<=maxY)
		{
			if(true==c.hasRange && c.maxValue<=143u && maxY>160)
			{
				continue; // room-local Y, not full-screen draw
			}
			const int d=std::abs(v-softY);
			if(d<=NEAR_SOFT &&
			   (hitScore>bestScoreY || (hitScore==bestScoreY && d<bestYd)))
			{
				bestY=&c;
				bestScoreY=hitScore;
				bestYd=d;
			}
		}
	}
	if(nullptr==bestX || nullptr==bestY || bestX==bestY)
	{
		return false;
	}
	outX=*bestX;
	outY=*bestY;
	return true;
}

bool MouseCoordWriteScan::TryReadScreenDrawCoords(int &mx,int &my) const
{
	mx=0;
	my=0;
	Candidate cx,cy;
	if(true!=FindScreenDrawPair(cx,cy))
	{
		return false;
	}
	mx=(int)(short)(cx.lastValue&0xffffu);
	my=(int)(short)(cy.lastValue&0xffffu);
	return true;
}

bool MouseCoordWriteScan::TryReadScreenDrawPhys(unsigned int &px,unsigned int &py) const
{
	px=0;
	py=0;
	Candidate cx,cy;
	if(true!=FindScreenDrawPair(cx,cy))
	{
		return false;
	}
	px=cx.physAddr;
	py=cy.physAddr;
	return true;
}

void MouseCoordWriteScan::ClearActiveProfile(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	activeProfile=Profile();
	profileLoaded=false;
	softLogValid=false;
	loggedProfileApply=false;
	if(nullptr!=townsPtr)
	{
		townsPtr->var.mouseCoordProfileApply=false;
	}
}

std::string MouseCoordWriteScan::NormalizeDosExecPath(const std::string &path)
{
	if(true==path.empty())
	{
		return std::string();
	}
	std::string out;
	out.reserve(path.size());
	for(size_t i=0; i<path.size(); ++i)
	{
		unsigned char c=(unsigned char)path[i];
		if(0==c || ' '==c || '\t'==c || '\r'==c || '\n'==c || '"'==c)
		{
			if(true==out.empty())
			{
				continue;
			}
			break;
		}
		if('/'==c)
		{
			c='\\';
		}
		if(c>='a' && c<='z')
		{
			c=(unsigned char)(c-'a'+'A');
		}
		out.push_back((char)c);
	}
	// Strip drive letter prefix (Q:\...).
	if(2<=out.size() && ':'==out[1] &&
	   (('A'<=out[0] && out[0]<='Z') || ('0'<=out[0] && out[0]<='9')))
	{
		out.erase(0,2);
	}
	while(true!=out.empty() && '\\'==out.front())
	{
		out.erase(out.begin());
	}
	// Collapse duplicate backslashes.
	std::string collapsed;
	collapsed.reserve(out.size());
	bool prevSlash=false;
	for(char c : out)
	{
		if('\\'==c)
		{
			if(true==prevSlash)
			{
				continue;
			}
			prevSlash=true;
		}
		else
		{
			prevSlash=false;
		}
		collapsed.push_back(c);
	}
	while(true!=collapsed.empty() && '\\'==collapsed.back())
	{
		collapsed.pop_back();
	}
	return collapsed;
}

std::string MouseCoordWriteScan::DosExecBasename(const std::string &normPath)
{
	size_t start=0;
	for(size_t i=0; i<normPath.size(); ++i)
	{
		if('\\'==normPath[i] || '/'==normPath[i])
		{
			start=i+1;
		}
	}
	return normPath.substr(start);
}

bool MouseCoordWriteScan::IsDosExtenderBasename(const std::string &base)
{
	return "RUN386.EXE"==base ||
	       "RUN386P.EXE"==base ||
	       "RUN386D.EXE"==base ||
	       "DOS4GW.EXE"==base ||
	       "DOS32A.EXE"==base ||
	       "PLDOS386.EXE"==base;
}

bool MouseCoordWriteScan::IsDosExtenderPayloadBasename(const std::string &base)
{
	if(4>base.size())
	{
		return false;
	}
	const std::string ext=base.substr(base.size()-4);
	return ".EXP"==ext || ".REX"==ext;
}

bool MouseCoordWriteScan::IsIgnoredAppExecBasename(const std::string &base)
{
	// System Load/Exec must not become Current EXE.  Matching AH=4CH is handled
	// via appExecSilentNestDepth only when an app identity is already live.
	if(true==base.empty())
	{
		return true;
	}
	if("TBIOS.SYS"==base ||
	   "COMMAND.COM"==base ||
	   "COMMAND.EXE"==base ||
	   "MSCDEX.EXE"==base ||
	   "MSCDEX.COM"==base)
	{
		return true;
	}
	if(4<=base.size() && ".SYS"==base.substr(base.size()-4))
	{
		return true;
	}
	return false;
}

unsigned int MouseCoordWriteScan::MixAppExecFingerprint(
    const std::string &normPath,unsigned int contentHash32)
{
	// Path always participates so GAME\FOO.EXE ≠ FOO.EXE even when content
	// lookup fails or hits the wrong same-basename file.
	unsigned int h=2166136261u;
	for(unsigned char b : normPath)
	{
		h^=b;
		h*=16777619u;
	}
	if(0!=contentHash32)
	{
		h^=contentHash32+(0x9e3779b9u)+(h<<6)+(h>>2);
	}
	// Avoid 0 so "unset / wildcard" stays distinct from a real fingerprint.
	if(0==h)
	{
		h=1u;
	}
	return h;
}

bool MouseCoordWriteScan::AppExecNameMatches(
    const std::string &profileName,const std::string &activeName)
{
	if(true==profileName.empty() || true==activeName.empty())
	{
		return false;
	}
	if(profileName==activeName)
	{
		return true;
	}
	// Legacy basename-only binds: allow basename equality only when the
	// profile name has no directory component.  Callers must still require a
	// non-zero hash match so same-basename launcher/main cannot both hit.
	const bool profileBare=
	    std::string::npos==profileName.find('\\') &&
	    std::string::npos==profileName.find('/');
	if(true==profileBare)
	{
		return DosExecBasename(activeName)==profileName;
	}
	return false;
}

unsigned int MouseCoordWriteScan::HashExecFromMountedDisc(const std::string &normPath) const
{
	if(nullptr==townsPtr || true==normPath.empty())
	{
		return 0;
	}
	const auto &disc=townsPtr->cdrom.state.GetDisc();
	if(DiscImage::FILETYPE_NONE==disc.fileType)
	{
		return 0;
	}
	// Prefer full relative path; basename-only search is ambiguous when the
	// disc has both a launcher and a game EXE with the same leaf name.
	return disc.HashIso9660FilePrefix(normPath,4096u);
}

void MouseCoordWriteScan::LogAppMonitorLine(const std::string &line)
{
	appMonitorLines.push_back(line);
	while(500<appMonitorLines.size())
	{
		appMonitorLines.pop_front();
	}
}

void MouseCoordWriteScan::ClearActiveAppPayloadLocked(const char *reason)
{
	if(true!=activeAppPayloadValid)
	{
		return;
	}
	std::ostringstream oss;
	oss << "[APP] payload end reason=" << (nullptr!=reason ? reason : "?")
	    << " name=" << activeAppPayloadName
	    << " hash=0x" << cpputil::Uitox(activeAppPayloadHash32);
	LogAppMonitorLine(oss.str());
	activeAppPayloadValid=false;
	activeAppPayloadName.clear();
	activeAppPayloadHash32=0;
}

void MouseCoordWriteScan::ResetAppExecSoftPhaseLocked(void)
{
	appExecSoftAliveSeen=false;
	appExecSoftMovedInSession=false;
	appExecSoftTrackedHost=false;
	appExecSoftTrackSampleValid=false;
	appExecSoftStuckHostMotion=0;
	appExecSoftUnresponsive=false;
	appExecSoftPendingHostStuck=0;
	appExecSoftLagSamplesLeft=0;
	appExecLoggedInGame=false;
}

void MouseCoordWriteScan::ResetAppExecPhaseLocked(void)
{
	ResetAppExecSoftPhaseLocked();
	appExecSawMosActive=false;
	appExecMosStartSerial=0;
	appExecMosStartTime=0;
	appExecGameportBaseline=
	    (nullptr!=townsPtr) ? townsPtr->state.gameportMouseReadCount : 0;
}

void MouseCoordWriteScan::ClearActiveAppExecLocked(const char *reason)
{
	ClearActiveAppPayloadLocked(reason);
	if(true!=activeAppExecValid)
	{
		return;
	}
	std::ostringstream oss;
	oss << "[APP] exec end reason=" << (nullptr!=reason ? reason : "?")
	    << " name=" << activeAppExecName
	    << " hash=0x" << cpputil::Uitox(activeAppExecHash32);
	LogAppMonitorLine(oss.str());
	activeAppExecValid=false;
	activeAppExecName.clear();
	activeAppExecHash32=0;
	activeAppExecStartTime=0;
	activeAppExtender=false;
	ResetAppExecPhaseLocked();
}

void MouseCoordWriteScan::PushActiveAppExecParentLocked(void)
{
	if(true!=activeAppExecValid)
	{
		return;
	}
	if(MAX_APP_EXEC_PARENTS<=appExecParents.size())
	{
		// Missed AH=4CH somewhere — drop the oldest parent to keep tracking.
		std::ostringstream oss;
		oss << "[APP] exec nest overflow drop name=" << appExecParents.front().name;
		LogAppMonitorLine(oss.str());
		appExecParents.erase(appExecParents.begin());
	}
	AppExecFrame frame;
	frame.name=activeAppExecName;
	frame.hash32=activeAppExecHash32;
	frame.extender=activeAppExtender;
	frame.payloadValid=activeAppPayloadValid;
	frame.payloadName=activeAppPayloadName;
	frame.payloadHash32=activeAppPayloadHash32;
	appExecParents.push_back(frame);
	std::ostringstream oss;
	oss << "[APP] exec nest push name=" << frame.name
	    << " depth=" << appExecParents.size();
	LogAppMonitorLine(oss.str());
	// Suspend payload with the parent frame; child starts clean.
	if(true==activeAppPayloadValid)
	{
		activeAppPayloadValid=false;
		activeAppPayloadName.clear();
		activeAppPayloadHash32=0;
	}
}

void MouseCoordWriteScan::EndActiveAppExecLocked(const char *reason)
{
	const std::string endedName=activeAppExecName;
	const unsigned int endedHash=activeAppExecHash32;
	ClearActiveAppExecLocked(reason);
	if(true==appExecParents.empty())
	{
		return;
	}
	const AppExecFrame frame=appExecParents.back();
	appExecParents.pop_back();
	activeAppExecValid=true;
	activeAppExecName=frame.name;
	activeAppExecHash32=frame.hash32;
	activeAppExtender=frame.extender;
	activeAppExecStartTime=(nullptr!=townsPtr) ? townsPtr->state.townsTime : 0;
	if(true==frame.payloadValid)
	{
		activeAppPayloadValid=true;
		activeAppPayloadName=frame.payloadName;
		activeAppPayloadHash32=frame.payloadHash32;
	}
	ResetAppExecPhaseLocked();
	std::ostringstream oss;
	oss << "[APP] exec return from=" << endedName
	    << " to=" << activeAppExecName;
	if(true==activeAppPayloadValid)
	{
		oss << " payload=" << activeAppPayloadName;
	}
	oss << " hash=0x" << cpputil::Uitox(activeAppExecHash32)
	    << " (ended hash=0x" << cpputil::Uitox(endedHash) << ")";
	LogAppMonitorLine(oss.str());
}

void MouseCoordWriteScan::GetEffectiveAppExecLocked(
    std::string &name,unsigned int &hash32) const
{
	// Under RUN386 / DOS4GW, the bound identity should be the .EXP payload.
	if(true==activeAppPayloadValid)
	{
		name=activeAppPayloadName;
		hash32=activeAppPayloadHash32;
		return;
	}
	if(true==activeAppExecValid)
	{
		name=activeAppExecName;
		hash32=activeAppExecHash32;
		return;
	}
	name.clear();
	hash32=0;
}

void MouseCoordWriteScan::OnDosExec(unsigned int AX,const std::string &fName)
{
	if(0x4B00!=(AX&0xFF00))
	{
		return;
	}
	const std::string name=NormalizeDosExecPath(fName);
	if(true==name.empty())
	{
		return;
	}
	const std::string base=DosExecBasename(name);
	if(true==IsIgnoredAppExecBasename(base))
	{
		std::lock_guard<std::mutex> lock(mtx);
		// Boot-time TBIOS/COMMAND/MSCDEX: ignore without silent depth.
		// Only nest-count while a guest app is already tracked so a later
		// AH=4CH does not end a DOS-extender payload / RUN386.
		if(true==activeAppExecValid || true==activeAppPayloadValid)
		{
			++appExecSilentNestDepth;
			std::ostringstream oss;
			oss << "[APP] exec ignore name=" << name
			    << " silent=" << appExecSilentNestDepth;
			LogAppMonitorLine(oss.str());
		}
		else
		{
			std::ostringstream oss;
			oss << "[APP] exec ignore name=" << name << " (boot)";
			LogAppMonitorLine(oss.str());
		}
		return;
	}
	// Extender loading another EXE via 4BH is rare; .EXP via 4BH counts as payload.
	if(true==activeAppExtender && true==IsDosExtenderPayloadBasename(base))
	{
		OnDosFileOpen(0x3D00,fName);
		return;
	}
	const unsigned int contentHash=HashExecFromMountedDisc(name);
	const unsigned int hash=MixAppExecFingerprint(name,contentHash);
	std::lock_guard<std::mutex> lock(mtx);
	if(true==activeAppExecValid)
	{
		// Nested Load/Exec — keep parent so child AH=4CH restores it
		// (some payloads briefly run helpers; without a stack Current goes none).
		PushActiveAppExecParentLocked();
	}
	activeAppExecValid=true;
	activeAppExecName=name;
	activeAppExecHash32=hash;
	activeAppExecStartTime=(nullptr!=townsPtr) ? townsPtr->state.townsTime : 0;
	activeAppExtender=IsDosExtenderBasename(base);
	ResetAppExecPhaseLocked();
	std::ostringstream oss;
	oss << "[APP] exec start name=" << name
	    << (true==activeAppExtender ? " extender=1" : "")
	    << " content=0x" << cpputil::Uitox(contentHash)
	    << " hash=0x" << cpputil::Uitox(hash);
	if(true!=appExecParents.empty())
	{
		oss << " nest=" << appExecParents.size();
	}
	LogAppMonitorLine(oss.str());
}

void MouseCoordWriteScan::OnDosFileOpen(unsigned int AX,const std::string &fName)
{
	if(0x3D00!=(AX&0xFF00))
	{
		return;
	}
	const std::string name=NormalizeDosExecPath(fName);
	if(true==name.empty())
	{
		return;
	}
	const std::string base=DosExecBasename(name);
	if(true==IsIgnoredAppExecBasename(base) ||
	   true!=IsDosExtenderPayloadBasename(base))
	{
		return;
	}
	const unsigned int contentHash=HashExecFromMountedDisc(name);
	const unsigned int hash=MixAppExecFingerprint(name,contentHash);
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=activeAppExecValid || true!=activeAppExtender)
	{
		// Payload without a known extender host — still track as the active identity
		// so Bind can capture the payload if 3DH is seen alone.
		if(true==activeAppExecValid)
		{
			ClearActiveAppExecLocked("next_exec");
			appExecParents.clear();
			appExecSilentNestDepth=0;
		}
		activeAppExecValid=true;
		activeAppExecName=name;
		activeAppExecHash32=hash;
		activeAppExecStartTime=(nullptr!=townsPtr) ? townsPtr->state.townsTime : 0;
		activeAppExtender=false;
		ResetAppExecPhaseLocked();
		std::ostringstream oss;
		oss << "[APP] payload-as-exec name=" << name
		    << " content=0x" << cpputil::Uitox(contentHash)
		    << " hash=0x" << cpputil::Uitox(hash);
		LogAppMonitorLine(oss.str());
		return;
	}
	if(true==activeAppPayloadValid && activeAppPayloadName==name)
	{
		return;
	}
	if(true==activeAppPayloadValid)
	{
		ClearActiveAppPayloadLocked("next_payload");
	}
	activeAppPayloadValid=true;
	activeAppPayloadName=name;
	activeAppPayloadHash32=hash;
	activeAppExecStartTime=(nullptr!=townsPtr) ? townsPtr->state.townsTime : 0;
	ResetAppExecPhaseLocked();
	std::ostringstream oss;
	oss << "[APP] payload start name=" << name
	    << " host=" << activeAppExecName
	    << " content=0x" << cpputil::Uitox(contentHash)
	    << " hash=0x" << cpputil::Uitox(hash);
	LogAppMonitorLine(oss.str());
}

void MouseCoordWriteScan::OnDosTerminate(const char *reason)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(0<appExecSilentNestDepth)
	{
		--appExecSilentNestDepth;
		std::ostringstream oss;
		oss << "[APP] exec ignore end reason="
		    << (nullptr!=reason ? reason : "?")
		    << " silent=" << appExecSilentNestDepth;
		LogAppMonitorLine(oss.str());
		return;
	}
	// Nested Load/Exec: child exit restores parent (do not drop to none while
	// the real game EXE/EXP is still on the stack).
	EndActiveAppExecLocked(nullptr!=reason ? reason : "terminate");
}

bool MouseCoordWriteScan::AppExecMatched(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=profileLoaded || true!=activeProfile.HasAppExecBind())
	{
		return false;
	}
	auto hashOk=[&](unsigned int effHash)->bool
	{
		if(0==activeProfile.appExecHash32)
		{
			const bool profileBare=
			    std::string::npos==activeProfile.appExecName.find('\\') &&
			    std::string::npos==activeProfile.appExecName.find('/');
			return true!=profileBare;
		}
		return effHash==activeProfile.appExecHash32;
	};

	std::string effName;
	unsigned int effHash=0;
	GetEffectiveAppExecLocked(effName,effHash);
	if(true!=effName.empty() &&
	   true==AppExecNameMatches(activeProfile.appExecName,effName) &&
	   true==hashOk(effHash))
	{
		return true;
	}
	// Legacy bind to RUN386.EXE while a .EXP payload is the effective identity:
	// still arm apply.  New binds should capture the .EXP via Bind.
	if(true==activeAppPayloadValid &&
	   true==activeAppExecValid &&
	   true==activeAppExtender &&
	   true==AppExecNameMatches(activeProfile.appExecName,activeAppExecName) &&
	   true==hashOk(activeAppExecHash32))
	{
		return true;
	}
	return false;
}

void MouseCoordWriteScan::GetActiveAppExec(std::string &name,unsigned int &hash32) const
{
	std::lock_guard<std::mutex> lock(mtx);
	GetEffectiveAppExecLocked(name,hash32);
}

bool MouseCoordWriteScan::BindActiveAppExecToProfile(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::string effName;
	unsigned int effHash=0;
	GetEffectiveAppExecLocked(effName,effHash);
	if(true!=profileLoaded || true==effName.empty())
	{
		return false;
	}
	activeProfile.appExecName=effName;
	activeProfile.appExecHash32=effHash;
	std::ostringstream oss;
	oss << "[APP] bind name=" << activeProfile.appExecName
	    << " hash=0x" << cpputil::Uitox(activeProfile.appExecHash32);
	LogAppMonitorLine(oss.str());
	return true;
}

bool MouseCoordWriteScan::SoftCursorDead(void) const
{
	unsigned int spx=0,spy=0;
	int softX=0,softY=0;
	if(true!=GetSoftCursorSnapshot(spx,spy,softX,softY))
	{
		return true;
	}
	return 0==softX && 0==softY;
}

bool MouseCoordWriteScan::SoftCursorInactiveForApp(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	// Only host-motion vs soft-freeze.  Soft 0,0 alone is NOT unused —
	// MOS-launcher soft may appear after load; deciding early steals MOS.
	if(true!=appExecSoftUnresponsive)
	{
		return false;
	}
	// Soft never appeared (still 0,0 during load) → keep waiting for MOS soft UI.
	if(true==appExecSoftTrackedHost || true==appExecSoftAliveSeen)
	{
		return true;
	}
	unsigned int spx=0,spy=0;
	int softX=0,softY=0;
	// Non-zero frozen soft (UW) counts as alive-seen for unused.
	return true==GetSoftCursorSnapshot(spx,spy,softX,softY) &&
	       (0!=softX || 0!=softY);
}

bool MouseCoordWriteScan::AppExecSoftTrackedHost(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return true==appExecSoftTrackedHost;
}

void MouseCoordWriteScan::NoteAppExecMosActive(unsigned int mosStartSerial)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=activeAppExecValid && true!=activeAppPayloadValid)
	{
		return;
	}
	// New AH=00 under this identity: drop TOS leftover soft / pre-MOS stuck.
	if(true!=appExecSawMosActive || appExecMosStartSerial!=mosStartSerial)
	{
		ResetAppExecSoftPhaseLocked();
		appExecMosStartSerial=mosStartSerial;
		appExecMosStartTime=(nullptr!=townsPtr) ? townsPtr->state.townsTime : 0;
		appExecGameportBaseline=
		    (nullptr!=townsPtr) ? townsPtr->state.gameportMouseReadCount : 0;
		std::ostringstream oss;
		oss << "[APP] mos session serial=" << mosStartSerial;
		LogAppMonitorLine(oss.str());
	}
	appExecSawMosActive=true;
}

bool MouseCoordWriteScan::AppExecReadyForProfileApply(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true==appExecLoggedInGame)
	{
		return true;
	}
	if(true!=profileLoaded || true!=activeProfile.HasAppExecBind())
	{
		return false;
	}
	if(true!=activeAppExecValid && true!=activeAppPayloadValid)
	{
		return false;
	}

	auto hashOk=[&](unsigned int effHash)->bool
	{
		if(0==activeProfile.appExecHash32)
		{
			const bool profileBare=
			    std::string::npos==activeProfile.appExecName.find('\\') &&
			    std::string::npos==activeProfile.appExecName.find('/');
			return true!=profileBare;
		}
		return effHash==activeProfile.appExecHash32;
	};

	std::string effName;
	unsigned int effHash=0;
	GetEffectiveAppExecLocked(effName,effHash);
	bool matched=
	    true!=effName.empty() &&
	    true==AppExecNameMatches(activeProfile.appExecName,effName) &&
	    true==hashOk(effHash);
	if(true!=matched &&
	   true==activeAppPayloadValid &&
	   true==activeAppExecValid &&
	   true==activeAppExtender &&
	   true==AppExecNameMatches(activeProfile.appExecName,activeAppExecName) &&
	   true==hashOk(activeAppExecHash32))
	{
		matched=true;
	}
	if(true!=matched)
	{
		return false;
	}

	// File open / 4BH is not "in-game" yet — wait out loader/init.
	if(nullptr==townsPtr)
	{
		return false;
	}
	const unsigned long long now=townsPtr->state.townsTime;
	const unsigned long long started=activeAppExecStartTime;
	if(0==started || now<started ||
	   (now-started)<APP_EXEC_APPLY_GRACE_NS)
	{
		return false;
	}

	// 本編 = bound EXE/EXP has been running past loader grace.
	auto *self=const_cast<MouseCoordWriteScan *>(this);
	self->appExecLoggedInGame=true;
	std::ostringstream oss;
	oss << "[APP] in-game phase via exp_start"
	    << " name=" << effName
	    << " hash=0x" << cpputil::Uitox(effHash)
	    << " grace_ns=" << APP_EXEC_APPLY_GRACE_NS;
	self->LogAppMonitorLine(oss.str());
	return true;
}

void MouseCoordWriteScan::NoteAppExecSoftTrackingSample(
    int hostX,int hostY,bool softAlive,int softX,int softY)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=activeAppExecValid && true!=activeAppPayloadValid)
	{
		return;
	}
	if(true==softAlive)
	{
		appExecSoftAliveSeen=true;
	}
	enum
	{
		HOST_MOVE_MIN=2,
		SOFT_MOVE_MIN=1,
		STUCK_HOST_MOTION=120,
		/*! Real warps only — normal fast mouse motion between polls is often 80–120px. */
		HOST_TELEPORT_MIN=256,
		HOST_TELEPORT_FROM_ORIGIN=96,
		SOFT_TELEPORT_MIN=64,
		SOFT_LAG_SAMPLES=4,
	};
	auto markTracked=[&](const char *tag,int hdx,int hdy,int sdx,int sdy)
	{
		const bool first=(true!=appExecSoftTrackedHost);
		appExecSoftTrackedHost=true;
		appExecSoftMovedInSession=true;
		appExecSoftStuckHostMotion=0;
		appExecSoftPendingHostStuck=0;
		appExecSoftLagSamplesLeft=0;
		appExecSoftUnresponsive=false;
		if(true==first && nullptr!=townsPtr)
		{
			appExecGameportBaseline=townsPtr->state.gameportMouseReadCount;
			std::ostringstream oss;
			oss << "[APP] soft tracked host (" << tag << ")"
			    << " host=" << hostX << "," << hostY
			    << " soft=" << softX << "," << softY
			    << " dHost=" << hdx << "," << hdy
			    << " dSoft=" << sdx << "," << sdy;
			LogAppMonitorLine(oss.str());
		}
	};
	auto noteUnresponsiveIfNeeded=[&](void)
	{
		if(STUCK_HOST_MOTION<=appExecSoftStuckHostMotion &&
		   true!=appExecSoftUnresponsive)
		{
			appExecSoftUnresponsive=true;
			std::ostringstream oss;
			oss << "[APP] soft unused latch"
			    << " stuckHost=" << appExecSoftStuckHostMotion
			    << "/" << STUCK_HOST_MOTION
			    << " host=" << hostX << "," << hostY
			    << " soft=" << softX << "," << softY
			    << " tracked=" << (true==appExecSoftTrackedHost ? 1 : 0)
			    << " softMoved=" << (true==appExecSoftMovedInSession ? 1 : 0)
			    << " aliveSeen=" << (true==appExecSoftAliveSeen ? 1 : 0);
			LogAppMonitorLine(oss.str());
		}
	};
	auto commitPendingStuck=[&](void)
	{
		if(0==appExecSoftPendingHostStuck)
		{
			return;
		}
		appExecSoftStuckHostMotion+=appExecSoftPendingHostStuck;
		appExecSoftPendingHostStuck=0;
		noteUnresponsiveIfNeeded();
	};

	if(true!=appExecSoftTrackSampleValid)
	{
		appExecSoftTrackSampleValid=true;
		appExecSoftTrackHostX=hostX;
		appExecSoftTrackHostY=hostY;
		appExecSoftTrackSoftX=softX;
		appExecSoftTrackSoftY=softY;
		return;
	}
	const int hdx=hostX-appExecSoftTrackHostX;
	const int hdy=hostY-appExecSoftTrackHostY;
	const unsigned int hostDist=
	    (unsigned int)(0<=hdx?hdx:-hdx)+(unsigned int)(0<=hdy?hdy:-hdy);
	const int sdx=softX-appExecSoftTrackSoftX;
	const int sdy=softY-appExecSoftTrackSoftY;
	const unsigned int softDist=
	    (unsigned int)(0<=sdx?sdx:-sdx)+(unsigned int)(0<=sdy?sdy:-sdy);

	const bool prevHostNearOrigin=
	    8>=appExecSoftTrackHostX && 8>=appExecSoftTrackHostY;
	const bool hostTeleport=
	    (HOST_TELEPORT_MIN<=hostDist) ||
	    (true==prevHostNearOrigin && HOST_TELEPORT_FROM_ORIGIN<=hostDist);

	// Soft teleport (MOS recenter) — not host tracking.
	if(SOFT_TELEPORT_MIN<=softDist && HOST_MOVE_MIN>hostDist)
	{
		std::ostringstream oss;
		oss << "[APP] soft teleport ignore"
		    << " dSoft=" << sdx << "," << sdy
		    << " soft=" << softX << "," << softY
		    << " host=" << hostX << "," << hostY;
		LogAppMonitorLine(oss.str());
		appExecSoftTrackSoftX=softX;
		appExecSoftTrackSoftY=softY;
		return;
	}

	// Host teleport — rebaseline; soft catch-up here can prove tracking.
	if(true==hostTeleport)
	{
		std::ostringstream oss;
		oss << "[APP] host teleport ignore"
		    << " dHost=" << hdx << "," << hdy
		    << " host=" << hostX << "," << hostY
		    << " soft=" << softX << "," << softY
		    << " dSoft=" << sdx << "," << sdy;
		LogAppMonitorLine(oss.str());
		appExecSoftTrackHostX=hostX;
		appExecSoftTrackHostY=hostY;
		appExecSoftTrackSoftX=softX;
		appExecSoftTrackSoftY=softY;
		appExecSoftPendingHostStuck=0;
		appExecSoftLagSamplesLeft=0;
		if(SOFT_MOVE_MIN<=softDist && SOFT_TELEPORT_MIN>softDist)
		{
			markTracked("teleport+soft",hdx,hdy,sdx,sdy);
		}
		return;
	}

	// Soft caught up (lag grace or concurrent with host).
	if(SOFT_MOVE_MIN<=softDist && SOFT_TELEPORT_MIN>softDist)
	{
		appExecSoftTrackSoftX=softX;
		appExecSoftTrackSoftY=softY;
		if(HOST_MOVE_MIN<=hostDist)
		{
			appExecSoftTrackHostX=hostX;
			appExecSoftTrackHostY=hostY;
		}
		markTracked(
		    (HOST_MOVE_MIN<=hostDist) ? "host+soft" : "lag-catchup",
		    hdx,hdy,sdx,sdy);
		return;
	}

	if(HOST_MOVE_MIN<=hostDist)
	{
		appExecSoftTrackHostX=hostX;
		appExecSoftTrackHostY=hostY;
		// Soft still — wait a few samples for MOS lag before counting unused.
		if(true!=appExecSoftAliveSeen && true!=appExecSoftTrackedHost &&
		   0==softX && 0==softY)
		{
			appExecSoftStuckHostMotion=0;
			appExecSoftPendingHostStuck=0;
			appExecSoftLagSamplesLeft=0;
			return;
		}
		if(true==appExecSoftTrackedHost)
		{
			// Launcher already proven: do not keep resetting lag while the user moves —
			// that prevented in-game unused forever.  Soft catch-up still clears via markTracked.
			appExecSoftStuckHostMotion+=hostDist;
			appExecSoftPendingHostStuck=0;
			appExecSoftLagSamplesLeft=0;
			noteUnresponsiveIfNeeded();
			return;
		}
		appExecSoftPendingHostStuck+=hostDist;
		if(0==appExecSoftLagSamplesLeft)
		{
			appExecSoftLagSamplesLeft=SOFT_LAG_SAMPLES;
		}
		return;
	}

	// No host motion: tick lag grace (pre-tracked only).
	if(0<appExecSoftLagSamplesLeft)
	{
		--appExecSoftLagSamplesLeft;
		if(0==appExecSoftLagSamplesLeft)
		{
			commitPendingStuck();
		}
	}
}

void MouseCoordWriteScan::NoteMouseProfileApply(bool on)
{
	std::lock_guard<std::mutex> lock(mtx);
	if(on==loggedProfileApply)
	{
		return;
	}
	loggedProfileApply=on;
	LogAppMonitorLine(on ? "[APP] mouse apply ON" : "[APP] mouse apply OFF");
}

std::vector <std::string> MouseCoordWriteScan::TakeMonitorLines(void)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::vector <std::string> out(appMonitorLines.begin(),appMonitorLines.end());
	appMonitorLines.clear();
	return out;
}

bool MouseCoordWriteScan::ProfileKeepsAbsolute(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=profileLoaded ||
	   true!=activeProfile.verified ||
	   true==activeProfile.WantsDifferential())
	{
		return false;
	}
	// App-specific absolute is owned by mouseCoordProfileApply (Outside_World:
	// bound EXE armed + MOS unused / sticky).  Do not keep-abs here.
	if(true==activeProfile.WantsDirectWrite() ||
	   true==activeProfile.WantsGameFeedback())
	{
		return false;
	}
	if(true==activeProfile.WantsMosIntegration())
	{
		// MOS absolute: system soft cursor while Mouse BIOS soft phys is resolvable.
		if(nullptr==townsPtr || true!=townsPtr->state.mouseBIOSActive)
		{
			return false;
		}
		unsigned int sx=0,sy=0;
		return ResolveSoftCursorPhys(sx,sy);
	}
	return false;
}

bool MouseCoordWriteScan::WantsProfileAbsolute(bool standardDesktopCrtc) const
{
	// TOS / TMENU desktop: never poke game-cursor profile words.
	if(true==standardDesktopCrtc)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(mtx);
	if(true!=profileLoaded ||
	   true!=activeProfile.verified ||
	   true==activeProfile.WantsDifferential() ||
	   true==activeProfile.WantsMosIntegration())
	{
		// Capture / MOS: not mouseCoordProfileApply (MOS uses system soft path).
		return false;
	}
	return true==activeProfile.HasDirectWriteTarget() ||
	       true==activeProfile.HasGameFeedbackTarget();
}

int MouseCoordWriteScan::MapAxisToRange(int host,int minV,int maxV,int /*hostSpan*/)
{
	if(maxV<minV)
	{
		std::swap(minV,maxV);
	}
	return ClampInt(host,minV,maxV);
}

bool MouseCoordWriteScan::TryGuestScreenSize(int &wid,int &hei) const
{
	if(nullptr==townsPtr)
	{
		return false;
	}
	unsigned int page=0;
	if(true!=townsPtr->crtc.InSinglePageMode())
	{
		page=townsPtr->state.mouseDisplayPage;
	}
	const auto render=townsPtr->crtc.GetRenderSize();
	const auto zoom=townsPtr->crtc.GetPageZoom2X((unsigned char)page);
	wid=std::max(2,render.x());
	hei=std::max(2,render.y());
	// Soft-cursor units are usually half of 2x CRTC render (e.g. 320 for 640).
	if(zoom.x()>=2)
	{
		wid=std::max(2,wid*2/zoom.x());
		if(wid==render.x())
		{
			wid=std::max(2,render.x()/2);
		}
	}
	if(zoom.y()>=2)
	{
		hei=std::max(2,hei*2/zoom.y());
		if(hei==render.y())
		{
			hei=std::max(2,render.y()/2);
		}
	}
	return true;
}

void MouseCoordWriteScan::MapHostToProfileCoords(int &mx,int &my) const
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		// Soft phys is legacy/optional.  App-specific profiles often have Game Phys
		// pairs only — still need host→guest screen mapping.
		if(nullptr==townsPtr || true!=profileLoaded)
		{
			return;
		}
		if(true!=activeProfile.HasPhys() && 0==activeProfile.NumPairs())
		{
			return;
		}
	}

	int screenW=0,screenH=0;
	if(true!=TryGuestScreenSize(screenW,screenH))
	{
		return;
	}
	const int screenMaxX=std::max(1,screenW-1);
	const int screenMaxY=std::max(1,screenH-1);

	// Profile mapping is deliberately only a render-to-guest linear ratio.
	// 640x480 -> 320x240 is exactly host/2.  Do not apply CRTC origin, zoom,
	// page span, profile offset, or guest cursor state here.
	const auto render=townsPtr->crtc.GetRenderSize();
	const int renderW=std::max(1,render.x());
	const int renderH=std::max(1,render.y());
	int sx=mx*screenW/renderW;
	int sy=my*screenH/renderH;
	sx=ClampInt(sx,0,screenMaxX);
	sy=ClampInt(sy,0,screenMaxY);

	mx=ClampInt(sx,0,screenMaxX);
	my=ClampInt(sy,0,screenMaxY);
}

bool MouseCoordWriteScan::ReadProfileCoords(int &mx,int &my) const
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		// Soft phys is optional.  Direct-write / game-feedback read Game Phys pairs.
		if(true!=profileLoaded || nullptr==townsPtr)
		{
			return false;
		}
		if(true!=activeProfile.HasPhys() && 0==activeProfile.NumPairs())
		{
			return false;
		}
		p=activeProfile;
	}
	// The app keeps its own cursor from BIOS deltas; when it is known, it — not the
	// BIOS soft word — is what the on-screen cursor follows, so integrate against it.
	const bool prev=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;

	int screenW=0,screenH=0;
	const bool haveScreen=TryGuestScreenSize(screenW,screenH);
	const int slack=64;
	// Words can hold junk before the app runs or after it relocates; feeding that
	// back would produce a runaway delta, so fall back to the BIOS soft cursor.
	auto usable=[&](int x,int y)->bool
	{
		return true!=haveScreen ||
		       (-slack<=x && x<screenW+slack && -slack<=y && y<screenH+slack);
	};

	for(const auto &pr : p.pair)
	{
		if(true!=pr.Valid())
		{
			continue;
		}
		// stored = screenTarget + bias, so screen = stored − bias.
		const int ax=(int)(short)townsPtr->mem.FetchWord(pr.physX)-pr.biasX;
		const int ay=(int)(short)townsPtr->mem.FetchWord(pr.physY)-pr.biasY;
		if(true==usable(ax,ay))
		{
			mx=ax;
			my=ay;
			townsPtr->var.suppressMosCoordReadProbe=prev;
			return true;
		}
	}
	// Junk / relocating Game Phys: integrate against live BIOS soft cursor instead.
	// Without this, GetMouseCoordinate fails → ControlMouse never reaches
	// WriteAppCursorCoords, and DW store-guard also blocks the guest from
	// initializing Phys → stuck at a large negative forever.
	{
		unsigned int softPhysX=0,softPhysY=0;
		int softX=0,softY=0;
		if(true==GetSoftCursorSnapshot(softPhysX,softPhysY,softX,softY))
		{
			mx=(int)(short)softX;
			my=(int)(short)softY;
			townsPtr->var.suppressMosCoordReadProbe=prev;
			return true;
		}
	}
	if(true==p.HasPhys())
	{
		mx=(int)(short)townsPtr->mem.FetchWord(p.physX);
		my=(int)(short)townsPtr->mem.FetchWord(p.physY);
		townsPtr->var.suppressMosCoordReadProbe=prev;
		return true;
	}
	townsPtr->var.suppressMosCoordReadProbe=prev;
	return false;
}

bool MouseCoordWriteScan::WriteProfileCoords(int mx,int my)
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasPhys() || nullptr==townsPtr)
		{
			return false;
		}
		p=activeProfile;
	}
	unsigned int px=p.physX;
	unsigned int py=p.physY;
	// Prefer live soft-cursor words (MOS_work can relocate; profile phys is the match key).
	unsigned int liveX=0,liveY=0;
	if(true==ResolveSoftCursorPhys(liveX,liveY))
	{
		px=liveX;
		py=liveY;
	}
	if(1!=p.scaleX && 0!=p.scaleX)
	{
		mx=mx*p.scaleX;
	}
	if(1!=p.scaleY && 0!=p.scaleY)
	{
		my=my*p.scaleY;
	}
	// Never poke out-of-range values into soft (false shadows produced 25416).
	int screenW=0,screenH=0;
	if(true==TryGuestScreenSize(screenW,screenH))
	{
		mx=ClampInt(mx,0,screenW-1);
		my=ClampInt(my,0,screenH-1);
	}
	townsPtr->mem.StoreWord(px,(unsigned int)mx);
	townsPtr->mem.StoreWord(py,(unsigned int)my);
	return true;
}

bool MouseCoordWriteScan::WriteAppCursorCoords(int mx,int my)
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasDirectWriteTarget() || nullptr==townsPtr)
		{
			lastDirectWriteOk=false;
			return false;
		}
		p=activeProfile;
	}

	int screenW=0,screenH=0;
	int screenX=mx,screenY=my;
	if(true==TryGuestScreenSize(screenW,screenH))
	{
		screenX=ClampInt(mx,0,screenW-1);
		screenY=ClampInt(my,0,screenH-1);
	}

	// Soft profile physX/Y is legacy ID only (TownsQt leaves them 0).
	// Game Phys pairs may intentionally be the live MOS soft words — poke them.
	SyncAppStoreGuard(true);

	bool wrote=false;
	suppressOwnStore=true;
	townsPtr->mem.storeGuardAllow=true;

	// Per-pair mapped target; scale multiplies the value written to that pair.
	for(const auto &pr : p.pair)
	{
		if(true!=pr.Valid())
		{
			continue;
		}
		int writeX=screenX+pr.biasX;
		int writeY=screenY+pr.biasY;
		if(true==pr.hasRangeX && pr.rangeMinX!=pr.rangeMaxX)
		{
			writeX=MapAxisToRange(writeX,pr.rangeMinX,pr.rangeMaxX,0);
		}
		else if(0<screenW)
		{
			writeX=ClampInt(writeX,0,screenW-1);
		}
		if(true==pr.hasRangeY && pr.rangeMinY!=pr.rangeMaxY)
		{
			writeY=MapAxisToRange(writeY,pr.rangeMinY,pr.rangeMaxY,0);
		}
		else if(0<screenH)
		{
			writeY=ClampInt(writeY,0,screenH-1);
		}
		if(0!=pr.scaleX && 1!=pr.scaleX)
		{
			writeX*=pr.scaleX;
		}
		if(0!=pr.scaleY && 1!=pr.scaleY)
		{
			writeY*=pr.scaleY;
		}
		townsPtr->mem.StoreWord(pr.physX,(unsigned int)writeX);
		townsPtr->mem.StoreWord(pr.physY,(unsigned int)writeY);
		wrote=true;
	}
	townsPtr->mem.storeGuardAllow=false;
	suppressOwnStore=false;

	{
		std::lock_guard<std::mutex> lock(mtx);
		lastDirectTargetX=screenX;
		lastDirectTargetY=screenY;
		lastDirectWriteOk=wrote;
		if(true==wrote)
		{
			++directWriteCount;
		}
	}

	if(true==wrote)
	{
		LogSoftCursorIfChanged(screenX,screenY);
	}
	return wrote;
}

void MouseCoordWriteScan::SyncAppStoreGuard(bool on)
{
	if(nullptr==townsPtr || nullptr==memPtr)
	{
		return;
	}
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded)
		{
			on=false;
		}
		else
		{
			p=activeProfile;
		}
	}

	// Guard every Game Phys pair word we poke (including MOS soft when used as Phys).
	// Guest stores into those words are blocked so the app copy cannot fight us.
	if(true!=on || true!=p.HasDirectWriteTarget())
	{
		memPtr->storeGuardActive=false;
		memPtr->storeGuardCount=0;
		return;
	}

	unsigned int n=0;
	auto add=[&](unsigned int phys)
	{
		if(0==phys || Memory::STORE_GUARD_MAX<=n)
		{
			return;
		}
		for(unsigned int i=0; i<n; ++i)
		{
			if(memPtr->storeGuardPhys[i]==phys)
			{
				return;
			}
		}
		memPtr->storeGuardPhys[n++]=phys;
	};
	for(const auto &pr : p.pair)
	{
		if(true==pr.Valid())
		{
			add(pr.physX);
			add(pr.physY);
		}
	}
	memPtr->storeGuardCount=n;
	memPtr->storeGuardActive=(0!=n);
}

unsigned int MouseCoordWriteScan::AppStoreGuardBlockCount(void) const
{
	if(nullptr==memPtr)
	{
		return 0;
	}
	return memPtr->storeGuardBlockCount;
}

unsigned int MouseCoordWriteScan::DirectWriteCount(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return directWriteCount;
}

bool MouseCoordWriteScan::LastDirectWriteOk(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return lastDirectWriteOk;
}

void MouseCoordWriteScan::AddChasePhysLocked(unsigned int phys)
{
	if(0==phys)
	{
		return;
	}
	for(unsigned int i=0; i<chaseCount; ++i)
	{
		if(chasePhys[i]==phys)
		{
			SyncChaseWatchLocked();
			return;
		}
	}
	if(MAX_CHASE<=chaseCount)
	{
		// Drop oldest.
		for(unsigned int i=1; i<MAX_CHASE; ++i)
		{
			chasePhys[i-1]=chasePhys[i];
		}
		chasePhys[MAX_CHASE-1]=phys;
	}
	else
	{
		chasePhys[chaseCount++]=phys;
	}
	SyncChaseWatchLocked();
}

void MouseCoordWriteScan::RemoveChasePhysLocked(unsigned int phys)
{
	if(0==phys || 0==chaseCount)
	{
		return;
	}
	unsigned int dst=0;
	for(unsigned int i=0; i<chaseCount; ++i)
	{
		if(chasePhys[i]==phys)
		{
			continue;
		}
		chasePhys[dst++]=chasePhys[i];
	}
	if(dst==chaseCount)
	{
		return;
	}
	chaseCount=dst;
	for(auto &c : candidates)
	{
		if(c.physAddr==phys)
		{
			c.userChase=false;
		}
	}
	SyncChaseWatchLocked();
}

void MouseCoordWriteScan::NoteClearedChaseLocked(unsigned int phys)
{
	if(0==phys)
	{
		return;
	}
	for(unsigned int i=0; i<clearedChaseCount; ++i)
	{
		if(clearedChasePhys[i]==phys)
		{
			return;
		}
	}
	if(MAX_CLEARED_CHASE<=clearedChaseCount)
	{
		for(unsigned int i=1; i<MAX_CLEARED_CHASE; ++i)
		{
			clearedChasePhys[i-1]=clearedChasePhys[i];
		}
		clearedChasePhys[MAX_CLEARED_CHASE-1]=phys;
	}
	else
	{
		clearedChasePhys[clearedChaseCount++]=phys;
	}
}

void MouseCoordWriteScan::EnsureChaseCandidateLocked(unsigned int phys)
{
	if(0==phys || nullptr==townsPtr)
	{
		return;
	}
	auto *c=FindOrAddLocked(phys,2,true);
	if(nullptr==c)
	{
		return;
	}
	const bool prev=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;
	const unsigned int v=townsPtr->mem.FetchWord(phys)&0xffffu;
	townsPtr->var.suppressMosCoordReadProbe=prev;
	c->lastValue=v;
	c->NoteValue(v);
	c->motionCorr=true;
	c->userWatch=true;
	c->lastMotionSerial=storeEventCount;
	if(0==c->hits)
	{
		++c->hits;
	}
}

void MouseCoordWriteScan::PromoteSourceCandidateLocked(unsigned int phys)
{
	if(0==phys || nullptr==townsPtr)
	{
		return;
	}
	EnsureChaseCandidateLocked(phys);

	for(unsigned int i=0; i<followedSrcCount; ++i)
	{
		if(followedSrcPhys[i]==phys)
		{
			return;
		}
	}
	if(MAX_FOLLOWED<=followedSrcCount)
	{
		for(unsigned int i=1; i<MAX_FOLLOWED; ++i)
		{
			followedSrcPhys[i-1]=followedSrcPhys[i];
		}
		followedSrcPhys[MAX_FOLLOWED-1]=phys;
	}
	else
	{
		followedSrcPhys[followedSrcCount++]=phys;
	}
}

void MouseCoordWriteScan::ProbeChaseNeighborhoodLocked(bool huntOk)
{
	if(0==chaseCount || nullptr==townsPtr)
	{
		return;
	}

	std::unordered_set <unsigned int> liveNear;
	liveNear.reserve(chaseCount*(CHASE_NEAR_RADIUS+1u));

	for(unsigned int i=0; i<chaseCount; ++i)
	{
		const unsigned int seed=chasePhys[i];
		if(0==seed)
		{
			continue;
		}
		for(int off=-(int)CHASE_NEAR_RADIUS; off<=(int)CHASE_NEAR_RADIUS; off+=2)
		{
			if(0==off)
			{
				continue;
			}
			const int signedPhys=(int)seed+off;
			if(0>=signedPhys)
			{
				continue;
			}
			const unsigned int p=(unsigned int)signedPhys;
			if(true==IsVRAMPhys(p))
			{
				continue;
			}
			liveNear.insert(p);

			const unsigned int v=townsPtr->mem.FetchWord(p)&0xffffu;
			auto prevIt=chaseNearPrev.find(p);
			if(prevIt==chaseNearPrev.end())
			{
				chaseNearPrev[p]=v;
				continue;
			}
			const unsigned int prev=prevIt->second;
			prevIt->second=v;
			if(true!=huntOk || v==prev)
			{
				continue;
			}
			const int dWord=
			    static_cast<int>(static_cast<short>(v&0xffffu))-
			    static_cast<int>(static_cast<short>(prev&0xffffu));
			if(0==dWord)
			{
				continue;
			}
			const bool matched=
			    dWord==pendingGpDx || dWord==pendingGpDy ||
			    true==MatchesRecentDeltaLocked(dWord,true,true) ||
			    true==MatchesRecentDeltaLocked(dWord,true,false);
			if(true!=matched)
			{
				continue;
			}

			auto *c=FindOrAddLocked(p,2,true);
			if(nullptr==c)
			{
				continue;
			}
			// Avoid re-scoring the 追跡 seed itself (already refreshed above).
			if(true==c->userChase)
			{
				continue;
			}
			if(0==c->hits)
			{
				c->lastValue=prev;
				c->NoteValue(prev);
				++c->hits;
				c->firstHitSerial=storeEventCount;
				c->lastHitSerial=storeEventCount;
			}
			c->lastValue=prev;
			NoteCandidateValueLocked(*c,v,c->cs,c->eip);
			c->userWatch=true;
			c->motionCorr=true;
			c->lastMotionSerial=storeEventCount;
		}
	}

	for(auto it=chaseNearPrev.begin(); it!=chaseNearPrev.end(); )
	{
		if(liveNear.end()==liveNear.find(it->first))
		{
			it=chaseNearPrev.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void MouseCoordWriteScan::SeedProfileCandidatesLocked(void)
{
	if(true!=profileLoaded || nullptr==townsPtr)
	{
		return;
	}
	if(true!=activeProfile.HasPhys() && 0==activeProfile.NumPairs())
	{
		return;
	}

	// TownsQt Game Phys may be MOS soft-cursor words; seed every configured pair.
	// Legacy profile soft physX/Y (usually 0) is not a pair and is not written.
	auto seed=[&](unsigned int phys,bool axisX,bool axisY)
	{
		if(0==phys)
		{
			return;
		}
		auto *c=FindOrAddLocked(phys,2,true);
		if(nullptr==c)
		{
			return;
		}
		const bool prev=townsPtr->var.suppressMosCoordReadProbe;
		townsPtr->var.suppressMosCoordReadProbe=true;
		const unsigned int v=townsPtr->mem.FetchWord(phys)&0xffffu;
		townsPtr->var.suppressMosCoordReadProbe=prev;
		c->lastValue=v;
		c->NoteValue(v);
		c->fromProfile=true;
		c->motionCorr=true;
		c->lastMotionSerial=storeEventCount;
		if(true==IsKnownSoftCursorPhysLocked(phys))
		{
			c->knownSoftCursor=true;
		}
		if(true==axisX)
		{
			c->profileAxisX=true;
		}
		if(true==axisY)
		{
			c->profileAxisY=true;
		}
		if(0==c->hits)
		{
			++c->hits;
		}
	};
	for(const auto &pr : activeProfile.pair)
	{
		if(true!=pr.Valid())
		{
			continue;
		}
		seed(pr.physX,true,false);
		seed(pr.physY,false,true);
	}
}

unsigned int MouseCoordWriteScan::ChaseSourceOf(unsigned int phys)
{
	if(0==phys)
	{
		return 0;
	}
	unsigned int immediate=0;
	{
		std::lock_guard<std::mutex> lock(mtx);
		AddChasePhysLocked(phys);
		// Keep the watched word itself visible — do not treat it as a new SOURCE.
		EnsureChaseCandidateLocked(phys);
		if(true==guestWriterTrace.valid &&
		   guestWriterTrace.physAddr==phys &&
		   true==guestWriterTrace.hasSrc &&
		   0!=guestWriterTrace.srcPhys &&
		   guestWriterTrace.srcPhys!=phys)
		{
			PromoteSourceCandidateLocked(guestWriterTrace.srcPhys);
			AddChasePhysLocked(guestWriterTrace.srcPhys);
			RemoveChasePhysLocked(phys);
			NoteClearedChaseLocked(phys);
			immediate=guestWriterTrace.srcPhys;
		}
	}
	if(nullptr!=memPtr)
	{
		ArmMemoryTrace(STORE_ARM_IO_COUNT);
	}
	return immediate;
}

void MouseCoordWriteScan::TakeFollowedSources(unsigned int out[8],unsigned int &count)
{
	std::lock_guard<std::mutex> lock(mtx);
	count=followedSrcCount;
	for(unsigned int i=0; i<followedSrcCount && i<8; ++i)
	{
		out[i]=followedSrcPhys[i];
	}
	followedSrcCount=0;
}

void MouseCoordWriteScan::TakeClearedChase(unsigned int out[8],unsigned int &count)
{
	std::lock_guard<std::mutex> lock(mtx);
	count=clearedChaseCount;
	for(unsigned int i=0; i<clearedChaseCount && i<8; ++i)
	{
		out[i]=clearedChasePhys[i];
	}
	clearedChaseCount=0;
}

void MouseCoordWriteScan::GetChasePhys(unsigned int out[8],unsigned int &count) const
{
	std::lock_guard<std::mutex> lock(mtx);
	count=chaseCount;
	for(unsigned int i=0; i<chaseCount && i<8; ++i)
	{
		out[i]=chasePhys[i];
	}
}

void MouseCoordWriteScan::SetUserChasePhys(const std::vector <unsigned int> &phys)
{
	std::lock_guard<std::mutex> lock(mtx);
	chaseCount=0;
	chaseNearPrev.clear();
	for(auto &c : candidates)
	{
		c.userChase=false;
	}
	for(unsigned int p : phys)
	{
		if(0==p)
		{
			continue;
		}
		AddChasePhysLocked(p);
		auto *c=FindOrAddLocked(p,2,true);
		if(nullptr!=c)
		{
			c->userChase=true;
			c->motionCorr=true;
			if(nullptr!=townsPtr)
			{
				const bool prev=townsPtr->var.suppressMosCoordReadProbe;
				townsPtr->var.suppressMosCoordReadProbe=true;
				c->lastValue=townsPtr->mem.FetchWord(p)&0xffffu;
				townsPtr->var.suppressMosCoordReadProbe=prev;
				c->NoteValue(c->lastValue);
			}
		}
	}
	SyncChaseWatchLocked();
	if(nullptr!=memPtr && 0<chaseCount)
	{
		ArmMemoryTrace(STORE_ARM_IO_COUNT);
	}
}

void MouseCoordWriteScan::SetUserWatchPhys(const std::vector <unsigned int> &phys)
{
	std::lock_guard<std::mutex> lock(mtx);
	for(auto &c : candidates)
	{
		c.userWatch=false;
	}
	for(unsigned int p : phys)
	{
		if(0==p)
		{
			continue;
		}
		auto *c=FindOrAddLocked(p,2,true);
		if(nullptr==c)
		{
			continue;
		}
		c->userWatch=true;
		c->motionCorr=true;
		if(nullptr!=townsPtr)
		{
			const bool prev=townsPtr->var.suppressMosCoordReadProbe;
			townsPtr->var.suppressMosCoordReadProbe=true;
			c->lastValue=townsPtr->mem.FetchWord(p)&0xffffu;
			townsPtr->var.suppressMosCoordReadProbe=prev;
			c->NoteValue(c->lastValue);
		}
	}
}

void MouseCoordWriteScan::NoteGuestTargetWriteLocked(
    unsigned int physAddr,unsigned int value,unsigned int cs,unsigned int eip)
{
	if(true!=profileLoaded && 0==chaseCount)
	{
		return;
	}
	unsigned int addr[NUM_TARGET_WRITE]={};
	addr[TARGET_SOFT_X]=activeProfile.physX;
	addr[TARGET_SOFT_Y]=activeProfile.physY;
	for(unsigned int i=0; i<MAX_COORD_PAIRS; ++i)
	{
		addr[TARGET_PAIR_BASE+i*2u]=activeProfile.pair[i].physX;
		addr[TARGET_PAIR_BASE+i*2u+1u]=activeProfile.pair[i].physY;
	}
	for(unsigned int i=0; i<NUM_TARGET_WRITE; ++i)
	{
		if(true==profileLoaded && 0!=addr[i] && physAddr==addr[i])
		{
			auto &w=guestTargetWrite[i];
			w.physAddr=physAddr;
			w.lastValue=value;
			w.cs=cs;
			w.eip=eip;
			++w.count;

			// Soft is BIOS-owned noise.  Pair writers are the game engine —
			// capture their instruction stream so we can see what feeds them.
			if(TARGET_PAIR_BASE<=i)
			{
				const bool eipChanged=
				    true!=guestWriterTrace.valid ||
				    guestWriterTrace.cs!=cs ||
				    guestWriterTrace.eip!=eip ||
				    guestWriterTrace.which!=i ||
				    true==guestWriterTrace.fromChase;
				if(true==eipChanged || 0==(w.count&0x3fu))
				{
					CaptureGuestWriterTrace(i,physAddr,value,cs,eip,false);
				}
				else
				{
					guestWriterTrace.value=value;
					guestWriterTrace.physAddr=physAddr;
				}
			}
			return;
		}
	}

	// Auto-chase: SOURCE phys from a previous copy hop.
	for(unsigned int i=0; i<chaseCount; ++i)
	{
		if(chasePhys[i]==physAddr)
		{
			const bool eipChanged=
			    true!=guestWriterTrace.valid ||
			    guestWriterTrace.cs!=cs ||
			    guestWriterTrace.eip!=eip ||
			    guestWriterTrace.physAddr!=physAddr;
			if(true==eipChanged)
			{
				CaptureGuestWriterTrace(TARGET_PAIR_BASE,physAddr,value,cs,eip,true);
			}
			else
			{
				guestWriterTrace.value=value;
			}
			// Surface chased words in the candidate table so the picker can
			// select them (they are usually missed by the short store trace).
			auto *c=FindOrAddLocked(physAddr,2,true);
			if(nullptr!=c)
			{
				c->userChase=true;
				c->lastValue=value;
				c->NoteValue(value);
				c->cs=cs;
				c->eip=eip;
				++c->hits;
				c->lastHitSerial=storeEventCount;
				c->motionCorr=true; // protect from prune/evict
				c->lastMotionSerial=storeEventCount;
			}
			return;
		}
	}
}

void MouseCoordWriteScan::CaptureGuestWriterTrace(
    unsigned int which,unsigned int physAddr,unsigned int value,
    unsigned int cs,unsigned int eip,bool fromChase)
{
	if(nullptr==townsPtr)
	{
		return;
	}
	auto &cpu=townsPtr->CPU();
	GuestWriterTrace t;
	t.valid=true;
	t.which=which;
	t.physAddr=physAddr;
	t.value=value;
	t.cs=cs;
	t.eip=eip;
	t.fromChase=fromChase;
	t.eax=cpu.GetEAX();
	t.ebx=cpu.GetEBX();
	t.ecx=cpu.GetECX();
	t.edx=cpu.GetEDX();
	t.esi=cpu.GetESI();
	t.edi=cpu.GetEDI();
	t.ebp=cpu.GetEBP();
	t.esp=cpu.GetESP();
	t.ds=cpu.state.DS().value;
	t.es=cpu.state.ES().value;
	t.ss=cpu.state.SS().value;
	t.fs=cpu.state.FS().value;
	t.gs=cpu.state.GS().value;
	t.dsBase=cpu.state.DS().baseLinearAddr;
	t.esBase=cpu.state.ES().baseLinearAddr;
	t.ssBase=cpu.state.SS().baseLinearAddr;

	const bool prevSuppress=townsPtr->var.suppressMosCoordReadProbe;
	townsPtr->var.suppressMosCoordReadProbe=true;
	for(unsigned int i=0; i<12; ++i)
	{
		const unsigned int p=(physAddr>=8u) ? (physAddr-8u+i*2u) : (i*2u);
		t.nearWords[i]=townsPtr->mem.FetchWord(p)&0xffffu;
	}

	// Disassemble a window around the store.  Prefer the live CS (same selector)
	// so GDT base/limit already match the writer's code segment.
	i486DXCommon::SegmentRegister codeSeg=cpu.state.CS();
	if(codeSeg.value!=cs)
	{
		cpu.DebugLoadSegmentRegister(codeSeg,cs,townsPtr->mem,cpu.state.mode);
	}

	struct Line
	{
		unsigned int off=0;
		unsigned int op=0;
		unsigned int moffs=0;
		bool hasMoffs=false;
		std::string text;
	};
	std::vector <Line> lines;
	const unsigned int start=(eip>0x30u) ? (eip-0x30u) : 0u;
	unsigned int off=start;
	MemoryAccess::ConstMemoryWindow emptyWin;
	const auto &symTable=townsPtr->debugger.GetSymTable();
	const auto &ioTable=townsPtr->debugger.GetIOTable();
	while(lines.size()<28 && off<eip+0x40u)
	{
		i486DXCommon::InstructionAndOperand instOp;
		Line L;
		L.off=off;
		try
		{
			cpu.DebugFetchInstruction(emptyWin,instOp,codeSeg,off,townsPtr->mem);
			L.op=instOp.inst.opCode&0xffu;
			// MOV AX/EAX,[moffs] = A1, MOV [moffs],AX/EAX = A3 (66 = operand-size).
			if(0xA1==L.op || 0xA3==L.op)
			{
				L.moffs=instOp.inst.EvalUimm32();
				L.hasMoffs=true;
			}
			L.text=cpu.Disassemble(
			    instOp.inst,instOp.op1,instOp.op2,
			    codeSeg,off,townsPtr->mem,symTable,ioTable);
		}
		catch(...)
		{
			L.text="(fetch/disasm failed)";
			lines.push_back(L);
			break;
		}
		lines.push_back(L);
		if(0==instOp.inst.numBytes)
		{
			break;
		}
		off+=instOp.inst.numBytes;
		if(off>eip+0x30u && off>eip && lines.size()>8)
		{
			break;
		}
	}

	// Resolve the data-segment base from the store that hit our profile phys.
	// Live DS.base is often a different selector while the moffs addresses are
	// relative to the app data segment base.
	for(const auto &L : lines)
	{
		if(L.off==eip && true==L.hasMoffs && 0xA3==L.op)
		{
			t.storeMoffs=L.moffs;
			t.hasStoreMoffs=true;
			if(physAddr>=L.moffs)
			{
				t.impliedDsBase=physAddr-L.moffs;
				t.hasImpliedDsBase=true;
			}
			break;
		}
	}
	// Chased linear phys (no A3 moffs decode): flat DS or protected-mode app.
	if(true==fromChase && true!=t.hasImpliedDsBase)
	{
		t.impliedDsBase=0;
		t.hasImpliedDsBase=true;
		t.storeMoffs=physAddr;
		t.hasStoreMoffs=true;
	}

	// Walk backward from the store for the last MOV AX,[moffs] — that is the
	// absolute source feeding this profile word.
	if(true==t.hasImpliedDsBase)
	{
		for(int i=(int)lines.size()-1; 0<=i; --i)
		{
			const auto &L=lines[(size_t)i];
			if(L.off>eip)
			{
				continue;
			}
			if(L.off==eip)
			{
				continue;
			}
			if(true==L.hasMoffs && 0xA1==L.op)
			{
				t.srcMoffs=L.moffs;
				t.srcPhys=t.impliedDsBase+L.moffs;
				t.srcValue=(int)(short)(townsPtr->mem.FetchWord(t.srcPhys)&0xffffu);
				t.hasSrc=true;
				break;
			}
			// Stop if something else clobbers AX.
			if(0xA1!=L.op && 0xB8!=(L.op&0xf8) && 0xB0!=(L.op&0xf8))
			{
				// keep scanning; simple heuristic only trusts immediate A1 loads
			}
		}

		std::ostringstream sum;
		sum << std::hex;
		unsigned int pairs=0;
		bool promotedFromChase=false;
		for(size_t i=1; i<lines.size() && pairs<8; ++i)
		{
			const auto &load=lines[i-1];
			const auto &store=lines[i];
			if(true==load.hasMoffs && 0xA1==load.op &&
			   true==store.hasMoffs && 0xA3==store.op)
			{
				const unsigned int dp=t.impliedDsBase+store.moffs;
				const unsigned int sp=t.impliedDsBase+load.moffs;
				const int sv=(int)(short)(townsPtr->mem.FetchWord(sp)&0xffffu);
				const int dv=(int)(short)(townsPtr->mem.FetchWord(dp)&0xffffu);
				sum << "  [" << store.moffs << "]phys=" << dp
				    << "=" << dv << "  <-  [" << load.moffs << "]phys=" << sp
				    << "=" << sv;
				if(store.off==eip)
				{
					sum << "  << this store";
				}
				sum << "\n";
				++pairs;
				if(true==fromChase)
				{
					if(0!=sp && sp!=physAddr)
					{
						PromoteSourceCandidateLocked(sp);
						AddChasePhysLocked(sp);
						promotedFromChase=true;
					}
				}
				else
				{
					AddChasePhysLocked(sp);
					PromoteSourceCandidateLocked(sp);
				}
			}
		}
		t.copySummary=sum.str();
		if(true==t.hasSrc)
		{
			if(true==fromChase)
			{
				if(0!=t.srcPhys && t.srcPhys!=physAddr)
				{
					PromoteSourceCandidateLocked(t.srcPhys);
					AddChasePhysLocked(t.srcPhys);
					promotedFromChase=true;
				}
			}
			else
			{
				AddChasePhysLocked(t.srcPhys);
				PromoteSourceCandidateLocked(t.srcPhys);
			}
		}
		// Move 追跡 from seed → SOURCE; UI clears the seed checkbox.
		if(true==fromChase && true==promotedFromChase)
		{
			RemoveChasePhysLocked(physAddr);
			NoteClearedChaseLocked(physAddr);
		}
	}
	townsPtr->var.suppressMosCoordReadProbe=prevSuppress;

	std::ostringstream oss;
	for(const auto &L : lines)
	{
		oss << ((L.off==eip) ? '>' : ' ') << ' ' << L.text << '\n';
	}
	t.disasm=oss.str();

	// Keep a deeper chase disasm on screen while the profile target is still
	// being hit by a later copy hop every frame.
	if(true!=fromChase && true==guestWriterTrace.fromChase && 0<chaseCount)
	{
		if(nullptr!=memPtr)
		{
			ArmMemoryTrace(STORE_ARM_IO_COUNT);
		}
		return;
	}
	guestWriterTrace=std::move(t);
	if(0<chaseCount && nullptr!=memPtr)
	{
		ArmMemoryTrace(STORE_ARM_IO_COUNT);
	}
}

MouseCoordWriteScan::GuestWriterTrace MouseCoordWriteScan::GetGuestWriterTrace(void) const
{
	std::lock_guard<std::mutex> lock(mtx);
	return guestWriterTrace;
}

MouseCoordWriteScan::TargetWrite MouseCoordWriteScan::GetGuestTargetWrite(unsigned int which) const
{
	std::lock_guard<std::mutex> lock(mtx);
	if(NUM_TARGET_WRITE<=which)
	{
		return TargetWrite();
	}
	return guestTargetWrite[which];
}

void MouseCoordWriteScan::GetLastDirectWrite(int &targetX,int &targetY) const
{
	std::lock_guard<std::mutex> lock(mtx);
	targetX=lastDirectTargetX;
	targetY=lastDirectTargetY;
}

bool MouseCoordWriteScan::CaptureSoftCursorProfile(void)
{
	if(nullptr==townsPtr)
	{
		return false;
	}
	unsigned int px=0,py=0;
	if(true!=ResolveSoftCursorPhys(px,py))
	{
		return false;
	}

	Profile p;
	p.physX=px;
	p.physY=py;
	p.verified=true;
	// Initial mode: Mouse BIOS alive → mouse integration, else mouse capture.
	p.integrationMode=
	    (true==townsPtr->state.mouseBIOSActive)
	    ? INTEGRATION_MOS : INTEGRATION_DIFFERENTIAL;
	p.SyncLegacyFlagsFromMode();
	p.scaleX=1;
	p.scaleY=1;
	p.offsetX=0;
	p.offsetY=0;

	{
		std::lock_guard<std::mutex> lock(mtx);
		const Candidate *cx=nullptr;
		const Candidate *cy=nullptr;
		for(const auto &c : candidates)
		{
			if(c.physAddr==px && 2==c.size)
			{
				cx=&c;
			}
			if(c.physAddr==py && 2==c.size)
			{
				cy=&c;
			}
		}
		// Soft min is the CS origin offset (e.g. Y=16).
		if(nullptr!=cx && true==cx->hasRange)
		{
			p.offsetX=(int)cx->minValue;
		}
		if(nullptr!=cy && true==cy->hasRange)
		{
			p.offsetY=(int)cy->minValue;
		}
	}

	const std::string discPath=townsPtr->cdrom.state.GetDisc().fName;
	if(true!=discPath.empty())
	{
		p.cdBasename=cpputil::GetBaseName(discPath);
		const auto sz=cpputil::FileSize(discPath);
		if(0<sz)
		{
			p.cdSize=(unsigned long long)sz;
		}
		ApplyDiscIdentityToProfile(p,DiscIdentityForPath(discPath,townsPtr));
	}

	SetActiveProfile(p);
	return true;
}

std::string MouseCoordWriteScan::ProfilePathForFingerprint(unsigned int fingerprintHash32) const
{
	if(true==profileDir.empty() || 0==fingerprintHash32)
	{
		return std::string();
	}
	char hex[16];
	snprintf(hex,sizeof(hex),"fp_%08x",fingerprintHash32);
	std::filesystem::path dir(profileDir);
	return (dir/hex).replace_extension(".ini").string();
}

std::string MouseCoordWriteScan::ProfilePathForContentHash(unsigned int contentHash32) const
{
	if(true==profileDir.empty() || 0==contentHash32)
	{
		return std::string();
	}
	char hex[16];
	snprintf(hex,sizeof(hex),"content_%08x",contentHash32);
	std::filesystem::path dir(profileDir);
	return (dir/hex).replace_extension(".ini").string();
}

std::string MouseCoordWriteScan::ProfilePathForBasename(const std::string &basename) const
{
	if(true==profileDir.empty() || true==basename.empty())
	{
		return std::string();
	}
	std::filesystem::path dir(profileDir);
	return (dir/basename).replace_extension(".ini").string();
}

std::string MouseCoordWriteScan::ActiveProfileFileName(void) const
{
	unsigned int fingerprintHash=0;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded)
		{
			return std::string();
		}
		fingerprintHash=activeProfile.discFingerprintHash32;
	}
	if(0==fingerprintHash)
	{
		return std::string();
	}
	const std::string path=ProfilePathForFingerprint(fingerprintHash);
	if(true==path.empty())
	{
		return std::string();
	}
	return std::filesystem::path(path).filename().string();
}

bool MouseCoordWriteScan::WriteProfileFile(const Profile &p) const
{
	// Allow writing a cleared stub (phys unset) so ResetProfileSettings can
	// blank the CD's ini without deleting the file.
	if(true==profileDir.empty() || 0==p.discFingerprintHash32)
	{
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories(profileDir,ec);
	const std::string path=ProfilePathForFingerprint(p.discFingerprintHash32);
	if(true==path.empty())
	{
		return false;
	}
	Profile out=p;
	if(true==out.cdBasename.empty() && nullptr!=townsPtr)
	{
		out.cdBasename=cpputil::GetBaseName(townsPtr->cdrom.state.GetDisc().fName);
	}
	std::ofstream ofs(path);
	if(!ofs)
	{
		return false;
	}
	ofs << out.ToIniString();
	return true==ofs.good();
}

bool MouseCoordWriteScan::ReadProfileFile(const std::string &path,Profile &out) const
{
	std::ifstream ifs(path);
	if(!ifs)
	{
		return false;
	}
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return Profile::FromIniString(oss.str(),out);
}

bool MouseCoordWriteScan::SaveActiveProfile(void) const
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasFingerprint())
		{
			return false;
		}
		p=activeProfile;
	}
	return WriteProfileFile(p);
}

bool MouseCoordWriteScan::ApplyAndSaveMachineSettings(const MachineSettings &machine)
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasFingerprint())
		{
			return false;
		}
		activeProfile.machine=machine;
		p=activeProfile;
	}
	return WriteProfileFile(p);
}

bool MouseCoordWriteScan::MergeAndSaveMachineClock(
    bool fastMode,int frequencyMhz,int customFrequencyMhz)
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasFingerprint())
		{
			return false;
		}
		activeProfile.machine.hasFastMode=true;
		activeProfile.machine.fastMode=fastMode;
		activeProfile.machine.hasFrequencyMhz=true;
		activeProfile.machine.frequencyMhz=frequencyMhz;
		if(33<=customFrequencyMhz && customFrequencyMhz<=60)
		{
			activeProfile.machine.hasCustomFrequencyMhz=true;
			activeProfile.machine.customFrequencyMhz=customFrequencyMhz;
		}
		p=activeProfile;
	}
	return WriteProfileFile(p);
}

bool MouseCoordWriteScan::ApplyAndSaveFdMounts(const std::string &fd0,const std::string &fd1)
{
	Profile p;
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(true!=profileLoaded || true!=activeProfile.HasFingerprint())
		{
			return false;
		}
		activeProfile.machine.hasFdImg[0]=true;
		activeProfile.machine.fdImg[0]=fd0;
		activeProfile.machine.hasFdImg[1]=true;
		activeProfile.machine.fdImg[1]=fd1;
		p=activeProfile;
	}
	return WriteProfileFile(p);
}

bool MouseCoordWriteScan::CreateProfileForCurrentDisc(const MachineSettings &machineDefaults)
{
	if(nullptr==townsPtr || true==profileDir.empty())
	{
		return false;
	}
	const std::string discPath=townsPtr->cdrom.state.GetDisc().fName;
	if(true==discPath.empty())
	{
		return false;
	}
	const DiscIdentity discId=DiscIdentityForPath(discPath,townsPtr);
	if(true!=discId.hasFingerprint)
	{
		return false;
	}

	Profile p;
	const auto sz=cpputil::FileSize(discPath);
	if(0<sz)
	{
		p.cdSize=(unsigned long long)sz;
	}
	ApplyDiscIdentityToProfile(p,discId);
	p.machine=machineDefaults;
	p.verified=true;
	p.integrationMode=INTEGRATION_AUTO;
	p.SyncLegacyFlagsFromMode();
	p.offsetX=0;
	p.offsetY=0;
	p.scaleX=1;
	p.scaleY=1;
	p.invertX=false;
	p.invertY=false;
	p.waitFeedback=true;
	p.stopSoftWrite=false;
	// Primary Game Phys slot — zeros until Scan/Update; ranges match editor defaults.
	p.pair[0].physX=0;
	p.pair[0].physY=0;
	p.pair[0].biasX=0;
	p.pair[0].biasY=0;
	p.pair[0].hasRangeX=true;
	p.pair[0].rangeMinX=0;
	p.pair[0].rangeMaxX=0;
	p.pair[0].hasRangeY=true;
	p.pair[0].rangeMinY=0;
	p.pair[0].rangeMaxY=0;

	// Preserve mouse section if an incomplete file already exists.
	{
		Profile existing;
		const std::string fpPath=ProfilePathForFingerprint(discId.fingerprintHash32);
		if(true!=fpPath.empty() && true==ReadProfileFile(fpPath,existing))
		{
			if(true==existing.HasPhys() || 0<existing.NumPairs() ||
			   true==existing.pair[0].hasRangeX || true==existing.pair[0].hasRangeY)
			{
				p.physX=existing.physX;
				p.physY=existing.physY;
				for(unsigned int i=0; i<MAX_COORD_PAIRS; ++i)
				{
					p.pair[i]=existing.pair[i];
				}
				// Ensure pair0 always has range keys after create/migrate.
				if(true!=p.pair[0].hasRangeX)
				{
					p.pair[0].hasRangeX=true;
					p.pair[0].rangeMinX=0;
					p.pair[0].rangeMaxX=0;
				}
				if(true!=p.pair[0].hasRangeY)
				{
					p.pair[0].hasRangeY=true;
					p.pair[0].rangeMinY=0;
					p.pair[0].rangeMaxY=0;
				}
				p.integrationMode=existing.integrationMode;
				p.offsetX=existing.offsetX;
				p.offsetY=existing.offsetY;
				p.scaleX=existing.scaleX;
				p.scaleY=existing.scaleY;
				p.invertX=existing.invertX;
				p.invertY=existing.invertY;
				p.waitFeedback=existing.waitFeedback;
				p.stopSoftWrite=existing.stopSoftWrite;
				p.verified=existing.verified;
				p.SyncLegacyFlagsFromMode();
			}
			if(true==existing.machine.HasAny() && true!=machineDefaults.HasAny())
			{
				p.machine=existing.machine;
			}
		}
	}

	if(true!=WriteProfileFile(p))
	{
		return false;
	}
	SetActiveProfile(p);
	return true;
}

bool MouseCoordWriteScan::DeleteProfileForCurrentDisc(void)
{
	if(nullptr==townsPtr || true==profileDir.empty())
	{
		return false;
	}
	const std::string discPath=townsPtr->cdrom.state.GetDisc().fName;
	if(true==discPath.empty())
	{
		return false;
	}
	const DiscIdentity discId=DiscIdentityForPath(discPath,townsPtr);
	if(true!=discId.hasFingerprint)
	{
		return false;
	}
	const std::string path=ProfilePathForFingerprint(discId.fingerprintHash32);
	if(true==path.empty())
	{
		return false;
	}
	std::error_code ec;
	const bool existed=std::filesystem::exists(path,ec);
	if(true==existed)
	{
		if(true!=std::filesystem::remove(path,ec))
		{
			return false;
		}
	}
	ClearActiveProfile();
	SyncAppStoreGuard(false);
	return true;
}

bool MouseCoordWriteScan::ResetProfileSettings(void)
{
	Profile stub;
	{
		std::lock_guard<std::mutex> lock(mtx);
		stub.cdBasename=activeProfile.cdBasename;
		stub.cdSize=activeProfile.cdSize;
		stub.discVolumeLabel=activeProfile.discVolumeLabel;
		stub.discSystemId=activeProfile.discSystemId;
		stub.discContentHash32=activeProfile.discContentHash32;
		stub.discFingerprintHash32=activeProfile.discFingerprintHash32;
	}
	if(nullptr!=townsPtr)
	{
		const std::string discPath=townsPtr->cdrom.state.GetDisc().fName;
		if(true!=discPath.empty())
		{
			if(true==stub.cdBasename.empty())
			{
				stub.cdBasename=cpputil::GetBaseName(discPath);
			}
			const auto sz=cpputil::FileSize(discPath);
			if(0<sz)
			{
				stub.cdSize=(unsigned long long)sz;
			}
			ApplyDiscIdentityToProfile(stub,DiscIdentityForPath(discPath,townsPtr));
		}
	}
	if(0==stub.discFingerprintHash32)
	{
		return false;
	}
	// Unset = 0 / empty; keep only CD identity so the file stays as a stub.
	stub.physX=0;
	stub.physY=0;
	for(auto &pr : stub.pair)
	{
		pr=CoordPair();
	}
	stub.feedbackOnly=false;
	stub.enabled=true;
	stub.integrationMode=INTEGRATION_AUTO;
	stub.SyncLegacyFlagsFromMode();
	stub.offsetX=0;
	stub.offsetY=0;
	stub.scaleX=1;
	stub.scaleY=1;
	stub.invertX=false;
	stub.invertY=false;
	stub.waitFeedback=true;
	stub.stopSoftWrite=false;
	stub.verified=false;

	if(true!=WriteProfileFile(stub))
	{
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(mtx);
		activeProfile=Profile();
		profileLoaded=false;
		softLogValid=false;
		for(auto &c : candidates)
		{
			if(true==c.fromProfile)
			{
				c.fromProfile=false;
				c.profileAxisX=false;
				c.profileAxisY=false;
			}
		}
		for(auto &w : guestTargetWrite)
		{
			w=TargetWrite();
		}
	}
	if(nullptr!=townsPtr)
	{
		townsPtr->var.mouseCoordProfileApply=false;
	}
	SyncAppStoreGuard(false);
	return true;
}

bool MouseCoordWriteScan::TryLoadForDisc(const std::string &discPath)
{
	if(true==discPath.empty() || true==profileDir.empty())
	{
		ClearActiveProfile();
		return false;
	}
	const std::string base=cpputil::GetBaseName(discPath);
	const DiscIdentity discId=DiscIdentityForPath(discPath,townsPtr);
	if(true!=discId.hasFingerprint)
	{
		ClearActiveProfile();
		return false;
	}

	Profile p;
	const std::string fpPath=ProfilePathForFingerprint(discId.fingerprintHash32);
	if(true==fpPath.empty())
	{
		ClearActiveProfile();
		return false;
	}

	auto tryPath=[&](const std::string &path,bool requireFpMatch)->bool
	{
		if(true==path.empty())
		{
			return false;
		}
		Profile loaded;
		if(true!=ReadProfileFile(path,loaded))
		{
			return false;
		}
		// Accept machine-only stubs or mouse profiles (verified optional for load;
		// Features "use disc profiles" gates apply at the Qt layer).
		if(true!=loaded.HasPhys() && true!=loaded.machine.HasAny() &&
		   true!=loaded.HasFingerprint())
		{
			return false;
		}
		if(true==requireFpMatch &&
		   true!=ProfileMatchesDisc(loaded,discPath,discId))
		{
			// Machine-only / new stub may lack fingerprint in INI; stamp later.
			if(true==loaded.HasFingerprint())
			{
				return false;
			}
		}
		p=loaded;
		return true;
	};

	bool fromLegacy=false;
	if(true!=tryPath(fpPath,true))
	{
		// Migrate pre-fingerprint layouts still sitting in the profiles dir.
		bool got=false;
		if(true==discId.hasContentId)
		{
			got=tryPath(ProfilePathForContentHash(discId.contentHash32),false);
		}
		if(true!=got)
		{
			got=tryPath(ProfilePathForBasename(base),false);
		}
		if(true!=got)
		{
			ClearActiveProfile();
			return false;
		}
		fromLegacy=true;
	}
	if(true==p.cdBasename.empty())
	{
		p.cdBasename=base;
	}
	ApplyDiscIdentityToProfile(p,discId);
	SetActiveProfile(p);
	if(true==fromLegacy)
	{
		// Rewrite under fp_%08x.ini so the next mount hits the canonical path.
		(void)WriteProfileFile(p);
	}
	return true;
}
