/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef CDROM_IS_INCLUDED
#define CDROM_IS_INCLUDED
/* { */

// #include <queue>  std::queue turned out to be useless.
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <deque>
#include <algorithm>
#include "discimg.h"
#include "device.h"
#include "townsdef.h"
#include "outside_world.h"
#include "i486.h"

/*! Disassembly of some game titles suggested that the command can be issued like:
      Push parameter queue -> and then send command, or
      Send command first -> and then push parameter queue.
    My guess is either:
      A the Internal CD drive starts doing something when both are given, or
      B there are some time after a command was written and before the CD controller starts reading parameter queue.
    I first try A and see if it works.
*/


/*! 4-byte Status Code from CD-ROM Drive
My guess based on the BIOS Disassembly:
00H xx  xx xx  Probably No Error
00H 03H xx xx  CDDA is playing
00H 09H xx xx  Drive Not Ready   Towns OS V2.1 L20 CD-ROM BIOS 0421:277C
07H ??         The BIOS is checking, but the meaning is unknown.
xxH 01H xx xx  Probably Parameter Error
21H 01H xx xx  Probably Parameter Error
21H 02H        Probably Parameter Error
21H 0CH        Probably Parameter Error
21H 0FH        Probably Abnormal Termination
21H 05H        Probably Media Error (like bad sector)
21H 06H        Probably Media Error (like bad sector)
21H 07H        Probably Media Error (like bad sector)
21H 03H        Probably Hard Error
21H 04H        Probably Hard Error
21H 09H xx xx  Probably Hard Error
21H 0DH xx xx  Probably Hard Error
21H 08H        Probably Media Changed
    What about drive-not-ready?
00H 08H xx xx  Seems to be also media-changed.  Towns OS V2.1 L20 CD-ROM BIOS 0421:277C

00H 01H xx xx  Probably CDDA Paused?  Reponse to command A0H.  INT 93H AX=55C0H returns AH=22H (already paused)
               if CDC returns 00 01 xx xx on command A0H.


My guess based on the Boot-ROM Disassembly:
04H xxH xx xx   Probably Seek done (After issuing 20H Seek, it waits for 00H No Error, and then 04H)
00H 04H xx xx   CDROM BIOS re-shoots command A0H if CDROM returns this code.       (0b00000100)
00H 08H xx xx   CDROM BIOS re-shoots command A0H if CDROM returns this code.       (0b00001000)
00H 0DH xx xx   CDROM BIOS Checking (2ndByte)&0x0D and wait for it to be non zero. (0b00001101)

Interpretation in the Linux for Towns source towns_cd.c (static void process_event(u_char st))
00H 09H xx xx  Media change?                                                       (0b00001001)
00H xx  xx xx  No error
01H xx  xx xx  Command Accept Error
22H xx  xx xx  Data Ready
06H xx  xx xx  Read Done
07H xx  xx xx  CDDA Play Done
09H xx  xx xx  Door open
10H xx  xx xx  Door close Media not exists
11H xx  xx xx  Stop done
12H xx  xx xx  Pause done
13H xx  xx xx  Resume done
16H MM  xx xx  TOC Read  (MM>>4)==0 Mode 1  (MM>>4)==4 Mode 2  Else Mode 0  What's mode?
17H            TOC Read2
18H            SUBQ Read
19H            SUBQ Read2
20H            SUBQ Read3
21H 05H        Read Audio Track
21H 07H        Drive Not Ready   <- This is probably wrong.  CD-ROM BIOS returns media error for 21 07 xx xx.
21H 08H        Media Changed
21H 0FH        Retry?

MX
24 xx 01 00    CD Door Open
24 xx 02 00    CD Door Locked
*/


class TownsCDROM : public Device
{
private:
	class FMTownsCommon *townsPtr;
	class TownsPIC *PICPtr;
	class TownsDMAC *DMACPtr;

public:
	enum
	{
		PARAM_QUEUE_LEN=8,
		STATE_QUEUE_LEN=4,
		CMDFLAG_STATUS_REQUEST=0x20,
		CMDFLAG_IRQ=0x40,
		CDDA_SAMPLING_RATE=44100,
	};

	enum
	{
		DELAYED_STATUS_IRQ_TIME=  50000,  // Tentatively  50us (ordering also via completionSIRQSticky)
		/*! Poll interval while async CDDA GetWave is BUSY (long tracks). 50us spam
		    made DRY flicker ready and flooded Exec without completion status. */
		CDDA_PREFETCH_POLL_TIME=1000000,  // 1ms
		DEFAULT_READ_SECTOR_TIME=5000000, // Tentatively   5ms  1X CD-ROM should be 1second/75frames=13.3ms per sector
		DEFAULT_SEEK_TIME=            0,
		NOTIFICATION_TIME=      1000000,  // Tentatively   1ms
		CDDASTOP_TIME=          1000000,  // Tentatively   1ms
		SEEK_TIME=            100000000,  // Tentatively 100ms
		LOSTDATA_TIMEOUT=     100000000,  // Tentatively 100ms. I don't think the CDC had a large FIFO buffer back in 1989. The real time-out should have been much shorter.
		STATUS_CHECKBACK_TIME=  1000000,
		/*! Max DEI+IRR holds before forcing Data Ready (100 x 1ms ≈ LOSTDATA window). */
		STATUS_CHECKBACK_MAX=       100,
		MAX_NUM_SECTORS=         350000,  // Max 700MB, 2KB per sector.

		SECTOR_PER_SEC_1X=           75,

		SECTORREAD_DELAY_ORGEL=200000000,
		/*
		Orgel first revision plays back movie without synchronizing with CD pre-fetch.
		When the CD speed is too fast, pre-fetch function apparently overflows the buffer and crashes.
		CD reading needs to be slowed down.
		What I haven't figure is why faster CPU speed does not compensate for this problem.
		With faster CPU clock, the picture runs way much faster than the voice.
		Nonetheless, slowing down CPU to 12MHz, and using MAX_SEEK_TIME=500ms, it did run without crash.
		There might be a cap in the picture-update rate?  Or, something to do with voice?
		The program may be trying to synchronizing by looking at the PCM playback?
		It does crash if I run it on real FM TOWNS II MX.
		DATAWEST later released a revised version that can be used in faster FM TOWNS models.
		Presumably, the bug-fixed version does not require this delay.
		*/
	};

	// Reference [3] 
	enum
	{
		// Take &0x9F with byte data written to 04C2H
		CDCMD_SEEK=       0x00,
		CDCMD_MODE2READ=  0x01,
		CDCMD_MODE1READ=  0x02,
		CDCMD_RAWREAD=    0x03,
		CDCMD_CDDAPLAY=   0x04,
		CDCMD_TOCREAD=    0x05,
		CDCMD_SUBQREAD=   0x06,
		CDCMD_UNKNOWN1=   0x1F, // NOP and requst status? I guess?

		CDCMD_SETSTATE=   0x80, // Linux for FM TOWNS source label this as "SETSTATE" but isn't it "GETSTATE"?
		CDCMD_CDDASET=    0x81,
		CDCMD_CDDASTOP=   0x84,
		CDCMD_CDDAPAUSE=  0x85,
		CDCMD_UNKNOWN2=   0x86,
		CDCMD_CDDARESUME= 0x87,
		CDCMD_UNKNOWN3=   0x9F, // Used from Windows 95 Internal CD-ROM Driver.
	};

	enum
	{
		CDDA_IDLE,
		CDDA_PLAYING,
		CDDA_PAUSED,
		CDDA_STOPPING,
		CDDA_ENDED,
	};
	enum
	{
		CDDA_POLLING_INTERVAL=1000000000/75, // 1 frame = 1/75 second.
	};

	class AsyncWaveReader
	{
	public:
		enum
		{
			STATE_IDLE,
			STATE_BUSY,
			STATE_DATAREADY,
		};
	private:
		std::thread thr;
		std::mutex stateLock;
		int state=STATE_IDLE;
		std::vector <unsigned char> wave;
		DiscImage *discImg;
		DiscImage::MinSecFrm from,to;
		bool restartRequested=false;
		/*! Abandon in-flight/prefetch result without blocking the VM on GetWave. */
		bool cancelRequested=false;

	public:
		AsyncWaveReader();
		~AsyncWaveReader();
		unsigned int GetState(void);
		void Start(DiscImage *discImg,DiscImage::MinSecFrm from,DiscImage::MinSecFrm to);
		std::vector <unsigned char> &GetWave(void);
		/*! Drop ready data immediately; if BUSY, finish GetWave then go IDLE (no DATAREADY). */
		void RequestCancel(void);

	private:
		void ThreadFunc(void);
	};
	AsyncWaveReader waveReader;

	class State
	{
	public:
		// Removed std::string imgFileName, which was redundant with DiscImage.

		bool SIRQ; // 4C0H bit 7
		bool DEI;  // 4C0H bit 6 (DMA End Flag)
		bool STSF; // 4C0H bit 5 (Software Transfer End)
		bool DTSF; // 4C0H bit 4 (DMA Transfer in progress)
		bool DRY;  // 4C0H bit 0 (Ready to receive command)

		bool enableSIRQ;
		bool enableDEI;

		bool cmdReceived;
		unsigned char cmd;
		int nParamQueue;
		unsigned char paramQueue[8];
		std::vector <unsigned char> statusQueue;

		unsigned int readingSectorHSG,endSectorHSG,headPositionHSG;

		unsigned int readSectorTime=DEFAULT_READ_SECTOR_TIME;
		unsigned int maxSeekTime=DEFAULT_SEEK_TIME;
		bool DMATransfer,CPUTransfer; // Both are not supposed to be 1, but I/O can set it that way.
		unsigned int CPUTransferPointer=0;
		bool WaitForDTSSTS;

		bool discChanged;

		bool lidClosed,lidLocked;


		// Fractal Engine Demo does the following:
		// (1) Issue command 62H (MODE1READ+SIRQ Request+Status Request),
		// (2) Clear SIRQ and DEI, and then
		// (3) Wait for SIRQ.
		// It is a near coding error. (1) and (2) must happen in the reverse order.
		// Real hardware got away with it because CDC completion lagged after (2).
		// Emulation still uses a short delayedSIRQ timer, but Compatible-mode i386DX
		// clocks can advance townsTime so fast that completion SIRQ lands before (2).
		// completionSIRQSticky makes that race timing-independent: SMIC cannot drop an
		// unread completion IRQ (re-assert while status remains); status drain ends it.
		bool delayedSIRQ=false;
		/*! Unread command-completion SIRQ: survive premature SMIC (Fractal order). */
		bool completionSIRQSticky=false;
		bool smicWhileCompletionSticky=false;
		/*! DEI+IRR Data-Ready checkbacks; escape to SetStatusDataReady after a bound. */
		unsigned int dataReadyCheckbacks=0;
		// Snapshot at ExecuteCDROMCommand: live paramQueue is freed for the next
		// command while this one is still pending (avoids MODE params corrupting PLAY).
		unsigned char delayedCmd=0;
		unsigned char delayedParam[8]={0,0,0,0,0,0,0,0};

		// ---- Guest CDC (silent; drives SubQ / ah / GETSTATE only) ----
		// Two-layer model: guest = real no-cache CDC; host = PCM + bridge.
		// BIOS disassembly: A0H after natural CDDA end returns
		//   00 00 00 00 07 00 00 00
		// RAYXANBER: PLAYING → STOPPING → ENDED so SubQ reaches commanded end.
		unsigned int CDDAState=CDDA_IDLE;
		long long int nextCDDAPollingTime=0;
		DiscImage::MinSecFrm CDDAStartTime,CDDAEndTime;
		bool CDDARepeat=false;
		/*! Guest playhead: absolute disc HSG at guestPlayAnchorTownsTime (PLAYING). */
		unsigned int guestPlayAnchorHSG=0;
		unsigned long long guestPlayAnchorTownsTime=0;
		/*! Guest-visible disc HSG frozen at PAUSE (SubQ). */
		unsigned int CDDAPauseDiscHSG=0;
		bool CDDAPauseDiscValid=false;

		// ---- Host audio (PCM mix; never feeds StatusSecondByte / SubQ) ----
		std::vector <unsigned char> CDDAWave;
		unsigned int CDDAPlayPointer=0;
		DiscImage::MinSecFrm CDDAWaveBaseTime;
		/*! TOC audio track (1-based) for host wave base; 0 if unknown. */
		unsigned int CDDAWaveBaseTrack=0;
		bool CDDAAudioOutput=false;
		uint64_t CDDAHostSamplesMixed=0;
		uint64_t CDDACacheStopAfterHostSamples=0; // 0=inactive
		bool CDDACacheHostStoppedByGrace=false;
		bool CDDACacheBridgingDataRead=false;
		/*! Host: PAUSE awaiting quick MODE to keep mix (standalone PAUSE → discard). */
		bool CDDACacheAwaitModeAfterPause=false;
		/*! Host: guest state when MODE burst started (bridge context). */
		unsigned int CDDAStateBeforeDataRead=CDDA_IDLE;
		/*! Host: short fixed-LBA MODE poll → keep mix. */
		bool CDDAFixedLbaPolling=false;
		enum { MODE_POLL_RING=16 };
		unsigned int modePollHsg[MODE_POLL_RING]={0};
		unsigned int modePollSectors[MODE_POLL_RING]={0};
		unsigned int modePollRingUsed=0;
		unsigned int modePollRingNext=0;

		// MODE1/2/RAW sector transfer in progress (protect schedule from GETSTATE).
		bool dataTransferActive=false;
		unsigned char dataTransferCmd=0;
		/*! MODE armed but waiting for CDDA prefetch to release the disc (ON or OFF). */
		bool CDDAPrefetchWaitForMode=false;
		uint64_t modeDeferredSeekTime=0;
		/*! Soft retries when a MODE sector read returns empty (disc contention). */
		unsigned int modeSectorEmptyRetries=0;

	private:
		DiscImage *imgPtr;
	public:
		State();
		~State();

		const DiscImage &GetDisc(void) const;
		DiscImage &GetDisc(void);

		void ClearStatusQueue(void);
		void PushStatusQueue(unsigned char d0,unsigned char d1,unsigned char d2,unsigned char d3);

		void Reset(void);
		void ResetMPU(void);
	};

	class Variables
	{
	public:
		// For debugging purposes
		unsigned char lastParam[8]={0,0,0,0,0,0,0,0};
		// For debugging purposes
		i486DXCommon::FarPointer lastCmdIssuedAt;
		i486DXCommon::FarPointer lastParamWrittenAt;
		unsigned int lastCmdAX=0; // CPU AX when CDC command I/O was written
		/*! Most recent INT 93H before CDC traffic (caller CS:EIP of INT, AX=BIOS function). */
		unsigned int lastInt93AX=0;
		i486DXCommon::FarPointer lastInt93From;

		bool virtuallyRemoved=false; // If true, ready signal (b0 of I/O 4C0H) is kept low.

		bool CDEleVolUpdate=false;
		bool CDDAmute=false;

		bool debugBreakOnCommandWrite=false;
		bool debugMonitorCommandWrite=false;
		bool debugBreakOnDEI=false;
		bool debugBreakOnDataReady=false;

		unsigned int sectorReadTimeDelay=0;

		// Host-layer switch: bridge/continue/grace through MODE/PAUSE.
		// OFF → host follows guest strictly (no bridge; still outputs while guest PLAYING).
		bool cddaCacheDuringDataRead=true;
		unsigned int cddaCachePostReadGraceSec=1;

		// If debugBreakOnCommandWrite==true and 0xffff!=debugBreakOnSpecificCommand,
		// it breaks the VM only if a specific command is sent.
		// debugBreakOnSpecificCommand is ignored if debugBreakOnCommandWrite!=true.
		unsigned int debugBreakOnSpecificCommand=0xffff;

		std::vector <uint8_t> sectorCacheForCPUTransfer;
	};

	State state;
	Variables var;

	/*! Audio TOC spans rebuilt on disc load (start/end HSG, 1-based track). */
	struct AudioTrackSpan
	{
		unsigned int track=0;
		unsigned int startHSG=0;
		unsigned int endHSG=0;
	};
	std::vector <AudioTrackSpan> audioTrackTable;

	std::vector <std::string> searchPaths;

	const char *DeviceName(void) const override {return "CDROM";}

	TownsCDROM(class FMTownsCommon *townsPtr,class TownsPIC *PICPtr,class TownsDMAC *DMACPtr);
	~TownsCDROM();
	void WaitUntilAsyncWaveReaderFinished(void);


	void PowerOn(void) override;
	void Reset(void) override;

	inline void UpdateCDDAState(long long int townsTime)
	{
		if(state.nextCDDAPollingTime<townsTime)
		{
			UpdateCDDAStateInternal(townsTime);
		}
	}
	static inline bool StatusRequestBit(unsigned char cmd)
	{
		// Status request bit is supposed to be bit 5 of the command according to [2] pp.225.

		// Fractal Engine Demo issues command 00H (Seek without STATUS REQUEST Bit and IRQ Bit),
		// and still expects to get SRQ and IRQ.  This 00H may not be intended, and may happen to be working.

		// Shadow of the Beast issues command 02H (MODE1READ), again without STATUS REQUEST.
		// This is intended.  I am positive.  And then it waits for the Data Ready status (22H 0 0 0).
		// At this time, I am leaning toward Status Request bit of the CDC command has no effect.
		// Or, "Command Status" and other status may be different, and of course such information is
		// undocumented, and I need to make a guess.

		// However, totally disregarding this flag and always return status will prevent ChaseHQ from starting.
		// ChaseHQ issues command C1H (CDDASET) and expects no status is reported.

		// At the end of day, the reaction to this STATUS REQUEST bit may need to be different for different command.

		return (0!=(cmd&CMDFLAG_STATUS_REQUEST)); // This interpretation of [2] prevents Shadow of the Beast from starting.
	}
private:
	void UpdateCDDAStateInternal(long long int townsTime);
	void RebuildAudioTrackTable(void);

	// Guest CDC (silent)
	unsigned int GuestAbsHSG(long long int townsTime) const;
	void GuestStartPlay(DiscImage::MinSecFrm msfBegin,DiscImage::MinSecFrm msfEnd,bool repeat,long long int townsTime);
	void GuestPause(long long int townsTime);
	void GuestResume(long long int townsTime);
	void GuestStopToEnded(void);
	void GuestStopToIdle(void);

	// Host audio / bridge
	/*! TOC track (1-based) for HSG: audioTrackTable, else DiscImage::GetTrackFromMSF. */
	unsigned int CacheAudioTrackFromHSG(unsigned int hsg) const;
	unsigned int CacheAudioTrackFromMSF(DiscImage::MinSecFrm msf) const;
	/*! Host: MODE/bridge/poll context for mid-continue (not await-MODE alone). */
	bool HostHasModeContinueContext(void) const;
	/*! Host: reuse wave across PLAY with mid ptr. Never gates guest CDC.
	    No-MODE: asymmetric begin vs guest SubQ (ahead coarse / behind tight).
	    MODE context: near host playhead / pause / in-span earlier begin (ptr kept). */
	bool HostCanContinuePlay(DiscImage::MinSecFrm msfBegin,DiscImage::MinSecFrm msfEnd) const;
	bool CacheWaveRemaining(void) const;
	bool CacheHostMixAfterDataRead(void);
	void CacheClearStopDeadline(void);
	void CacheArmStopIfNoPlay(void);
	void CacheArmAwaitModeAfterPause(void);
	void CacheOnStopDeadlineReached(void);
	void CacheOnDataReadStarted(unsigned int numSectors);
	void CacheOnDataReadFinished(void);
	bool ModeIsShortPollRead(unsigned int numSectors) const;
	void ModePollClear(void);
	bool ModePollNoteShortRead(unsigned int hsg0,unsigned int numSectors);
	/*! Host: keep PCM mix through this MODE (guest already follows CDC stop). */
	bool ModePollShouldKeepHostMix(unsigned int hsg0,unsigned int hsg1,unsigned int numSectors,unsigned int guestBefore);
	void PushGetStateStatus(void);
	bool TryHandleGetStateWithoutStealingSchedule(unsigned char newCmdByte,unsigned char keepCmd);
	void LogMonitorLine(const std::string &line);
	/*! Monitor: one line per CDC command phase with → disposition (how it was handled). */
	void LogCmdEvent(const char phase[],const std::string &disposition);
	std::string FormatStatusQueueTail(unsigned maxPairs=2) const;

	std::mutex monitorMutex;
	std::deque <std::string> monitorLines;

public:
	inline bool CDDAIsPlaying(void) const
	{
		return (CDDA_PLAYING==state.CDDAState || CDDA_STOPPING==state.CDDAState);
	}

	bool CDDAAudioShouldOutput(void) const;
	void DiscardCDDAWaveCache(void);
	/*! After SpecificDeserialize, before ResumeCDDAAfterRestore: drop the previous
	    session's host wave and bridge/grace flags. Leaves play pointer / base time /
	    CDDAAudioOutput / CDDAState from the state file intact for Resume.
	    Resume then cues host ptr to GuestAbsHSG within the rebuilt wave. */
	void DiscardHostCDDACacheForStateLoad(void);
	/*! App→TOS/TMENU or top-level AH=4CH: discard host CDDA wave so MODE cannot
	    resurrect BGM without a new PLAY install. Guest CDDAState / pause SubQ stay. */
	void DiscardCacheOnAppExit(const char *reason);
	std::vector <std::string> TakeMonitorLines(void);

	inline bool DiscLoadedAndLidClosed(void) const
	{
		return (0!=state.GetDisc().GetNumTracks() && true==state.lidClosed);
	}

	bool CanOpenCloseFromCommand(void) const;

	void IOWriteByte(unsigned int ioport,unsigned int data) override;
	unsigned int IOReadByte(unsigned int ioport) override;

	std::vector <std::string> GetStatusText(void) const;

	/*! Loads a disc-image file.  It can be .CUE or .ISO format file.
	    The return value is an error code of DiscImage class.
	    Use DiscImage::ErrorCodeToText(return_value) to get the text if
	    the return_value is not DiscImage::ERROR_NOERROR.
	    It also sets discChanged flag.
	*/
	unsigned int LoadDiscImage(const std::string &fName);

	/*!
	*/
	void Eject(void);

	/*!
	*/
	void OpenLid(void);

	/*!
	*/
	void CloseLid(void);

	/*! 
	*/
	void ExecuteCDROMCommand(void);

	void PrepareCDDAPlay(void);

	void DelayedCommandExecution(unsigned long long int townsTime);

	void BeginReadSector(DiscImage::MinSecFrm from,DiscImage::MinSecFrm to);

	void BreakOnCommandCheck(const char phase[]);

	/*! Call-back from FMTownsCommon class.
	*/
	void RunScheduledTask(unsigned long long int townsTime);
private:
	/*! Push MODE accept (00 00) and schedule the first sector after optional prefetch wait. */
	void StartModeSectorTransfer(uint64_t seekTime);

	void SetStatusDriveNotReadyOrDiscChangedOrNoError(void);
	bool SetStatusDriveNotReadyOrDiscChanged(void);
	void SetStatusNoError(void);
	void SetStatusDriveNotReady(void);
	void SetStatusDiscChanged(void);
	void SetStatusReadDone(void);
	void SetStatusHardError(void);
	void SetStatusParameterError(void);
	void SetStatusQueueForTOC(void);
	void SetStatusDataReady(void);
	void PushStatusCDDAStopDone(void);
	void SetStatusSubQRead(void);
	void PushStatusCDDAPlayEnded(void);

	unsigned char StatusSecondByte(void) const;

	bool StatusQueueHasMediaChangedOnly(void) const;

public:
	/*! As it says.  This function can now also be called from the command module.
	*/
	void StopCDDA(void);

private:
	/* Turn on IRR flag if status queue is not empty.
	*/
	void SetSIRQ_IRR(void);
	/*! Raise SIRQ and arm completion sticky when status bytes are unread. */
	void RaiseSIRQFlag(void);

	uint32_t SerializeVersion(void) const override;
	void SpecificSerialize(std::vector <unsigned char> &data,std::string stateFName) const override;
	bool SpecificDeserialize(const unsigned char *&data,std::string stateFName,uint32_t version) override;

public:
	// Will be called from FMTownsCommon::LoadState
	void ResumeCDDAAfterRestore(void);


public:
	bool IsCDDAPlaying(void) const;
	void AddWaveForNumSamples(unsigned char waveBuf[],unsigned int numSamples,int outSamplingRate);
};


/* } */
#endif
