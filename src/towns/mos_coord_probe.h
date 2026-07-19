/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef MOS_COORD_PROBE_IS_INCLUDED
#define MOS_COORD_PROBE_IS_INCLUDED
/* { */

#include "ramrom.h"

class FMTownsCommon;

/*! Counts guest reads of MOS / TBIOS mouse-coordinate words.
    Host-side Fetch/Store must set FMTownsCommon::var.suppressMosCoordReadProbe. */
class MosCoordReadProbe : public MemoryAccess
{
public:
	FMTownsCommon *townsPtr=nullptr;
	unsigned int physAddrTop=0;
	unsigned char watch[Memory::MEMORY_ACCESS_SLOT_SIZE]={};

	MosCoordReadProbe(FMTownsCommon &towns,unsigned int physAddr);

	void ClearWatch(void);
	void WatchByte(unsigned int physAddr);
	void WatchWord(unsigned int physAddr);

	bool IsWatched(unsigned int physAddr) const;

	virtual unsigned int FetchByte(unsigned int physAddr) const override;
	virtual unsigned int FetchWord(unsigned int physAddr) const override;
	virtual unsigned int FetchDword(unsigned int physAddr) const override;
	virtual void StoreByte(unsigned int physAddr,unsigned char data) override;
	virtual void StoreWord(unsigned int physAddr,unsigned int data) override;
	virtual void StoreDword(unsigned int physAddr,unsigned int data) override;

	virtual ConstMemoryWindow GetConstMemoryWindow(unsigned int physAddr) const override;
	virtual MemoryWindow GetMemoryWindow(unsigned int physAddr) override;

	void NoteRead(unsigned int physAddr) const;
};

/* } */
#endif
