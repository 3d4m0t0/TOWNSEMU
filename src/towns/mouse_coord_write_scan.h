/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef MOUSE_COORD_WRITE_SCAN_IS_INCLUDED
#define MOUSE_COORD_WRITE_SCAN_IS_INCLUDED
/* { */

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class FMTownsCommon;
class Memory;

/*! Prep-phase tracer: after mouse activity, record following RAM stores and
    score clamped guest coordinates.  Host mouse is not used.

    Arm sources (apps often never touch gameport IO themselves):
      - gameport IO (app or TBIOS soft-cursor ISR)
      - soft-cursor value change (poll)
      - app MOS BIOS call (AH≠init/end) — BIOS returns position/delta
      - app read of soft-cursor RAM — copy from BIOS-owned words */
class MouseCoordWriteScan
{
public:
	enum
	{
		STORE_ARM_COUNT=96,
		/*! After gameport mouse IO: keep tracing long enough for the app's later
		    screen-draw cursor writes.  8192 kept every store on the hot path and
		    made some titles crawl during Calibration; 1024 is enough for a few
		    frames of app main-loop stores after TBIOS soft update. */
		STORE_ARM_IO_COUNT=96,
		/*! Show row after first in-range or motion-correlated sample. */
		MIN_PICKUP_HITS=1,
		/*! Calibration: one short burst at a time; total coverage over many sweeps. */
		STORE_ARM_CALIB_COUNT=16,
		/*! Soft-cursor / shadow-poll top-up only (not a full IO window). */
		STORE_ARM_SOFT_TOPUP=48,
		/*! Perimeter laps after home-to-TL (TL→TR→BR→BL→TL each lap). */
		AUTO_CALIB_LAPS=3,
		AUTO_CALIB_LEGS=4,
		/*! Fixed travel per edge (covers any Towns resolution). */
		AUTO_CALIB_TRAVEL_X=1024,
		AUTO_CALIB_TRAVEL_Y=768,
		/*! Gameport polls between injected calibration deltas (spread load). */
		AUTO_CALIB_IO_INTERVAL=16,
		/*! Per-axis delta injected each calibration step (gameport motion buffer). */
		AUTO_CALIB_STEP_SIZE=8,
		/*! Address-range scan windows over main RAM (not per-phys exclude). */
		SCAN_RANGE_SIZE=0x40000u,
		SCAN_RANGE_MIN_STORES=8u,
		SCAN_ADDR_LO=0x10000u,
		SCAN_ADDR_WRAP=0x1000000u,
		/*! The drawn cursor (and world-space copies) update in the app's main loop,
		    a few frames after the IO — keep the hunt window open across host mouse
		    samples, not just a fixed store budget. */
		MOTION_FRESH_FRAMES=16,
		/*! Recent gameport / soft deltas, so a late store still matches (and so does
		    a coalesced store that applied several frames of motion at once). */
		DELTA_HISTORY=32,
		MAX_CANDIDATES=768,
		TOP_N_DEFAULT=64,
		/*! storeEventCount gap with no hit → treat as low-frequency / droppable. */
		STALE_HIT_GAP=4096,
		/*! Consecutive samples where (max-min) span ≫ screen → drop. */
		OUT_OF_RES_DROP=8,
		/*! Consecutive value changes with no host mouse activity → drop. */
		IDLE_CHANGE_DROP=8,
		/*! Bytes each side of a 追跡 seed: poll word-aligned neighbors for Δ match. */
		CHASE_NEAR_RADIUS=32,
	};

	/*! CD-keyed absolute-integration profile.
	    Default dir (TownsQt): <configDir>/profiles/fp_XXXXXXXX.ini

	    Host emu-image → live guest screen (from CRTC), then
	    soft target = screen + (offsetX,offsetY).
	    Example: offset=(0,16) → host (0,0) aims soft (0,16).

	    [mouse_coord]
	    pair0_x=0x........
	    pair0_y=0x........
	    pair0_min_x=0
	    pair0_max_x=0
	    pair0_min_y=0
	    pair0_max_y=0
	    offset_x=0
	    offset_y=0
	    scale_x=1
	    scale_y=1
	    verified=1
	    integration_mode=0|1|2|3|4   (differential / MOS / direct-write / game-feedback / auto)
	    invert_x=0
	    invert_y=0
	    cd_size=0
	*/
	enum
	{
		MAX_COORD_PAIRS=4
	};

	/*! How this CD profile drives host→guest mouse (one primary mode).
	    Capture is exclusive with MOS absolute and with app-specific apply. */
	enum IntegrationMode
	{
		/*! Mouse capture: relative gameport deltas. */
		INTEGRATION_DIFFERENTIAL=0,
		/*! Mouse integration (MOS): no MOS → capture; MOS → absolute.
		    No unused detection. Does not use Game Phys / offset / invert. */
		INTEGRATION_MOS=1,
		/*! Mouse integration (App): poke guest RAM cursor words.
		    No MOS → capture; MOS → absolute; bound EXE/EXP start → Game Phys
		    (while MOS lives both may update). Unused detection does not switch. */
		INTEGRATION_DIRECT_WRITE=2,
		/*! Mouse integration (new): read configured Phys, feed gameport deltas.
		    Bound EXE/EXP start applies (same as direct-write). */
		INTEGRATION_GAME_FEEDBACK=3,
		/*! Default: no MOS → capture; MOS → absolute; MOS unused → capture. */
		INTEGRATION_AUTO=4,
	};

	/*! One game-owned screen-coordinate word pair (X,Y).  Titles often keep several
	    copies of the same logical-screen cursor (display copy, logic copy, ...); all
	    configured pairs are poked and store-guarded in direct-write mode.
	    stored = screenTarget + bias (bias normally 0; use for room/world variants). */
	struct CoordPair
	{
		unsigned int physX=0;
		unsigned int physY=0;
		int biasX=0;
		int biasY=0;
		/*! Multiplier applied to the mapped write value (0 = skip axis write multiply). */
		int scaleX=1;
		int scaleY=1;
		/*! Clamp direct-write target on each axis (from scan min..max). */
		int rangeMinX=0;
		int rangeMaxX=0;
		int rangeMinY=0;
		int rangeMaxY=0;
		bool hasRangeX=false;
		bool hasRangeY=false;
		bool Valid(void) const{return 0!=physX && 0!=physY;}
	};

	struct MachineSettings
	{
		bool hasFrequencyMhz=false;
		int frequencyMhz=33;
		bool hasCustomFrequencyMhz=false;
		int customFrequencyMhz=33;
		bool hasFastMode=false;
		bool fastMode=true;
		bool hasMemSizeInMB=false;
		int memSizeInMB=4;
		bool hasGamePort0=false;
		unsigned int gamePort0=0;
		bool hasGamePort1=false;
		unsigned int gamePort1=0;
		bool hasMaxButtonHoldMs0=false;
		int maxButtonHoldMs0=0;
		bool hasMaxButtonHoldMs1=false;
		int maxButtonHoldMs1=0;
		/*! Legacy keys; kept for older fp_*.ini, no longer written by TownsQt. */
		bool hasModelGroupIndex=false;
		int modelGroupIndex=0;
		bool hasModelGroup=false;
		std::string modelGroup;
		bool hasCpu=false;
		std::string cpu;
		bool hasCpuHighFidelity=false;
		bool cpuHighFidelity=false;
		bool hasPretend386DX=false;
		bool pretend386DX=false;
		bool hasUseFPU=false;
		bool useFPU=false;
		bool hasFastScsi=false;
		bool fastScsi=false;
		bool hasFastFd=false;
		bool fastFd=false;
		bool hasMidiBoard=false;
		bool midiBoard=false;
		bool hasHighResCrtc=false;
		bool highResCrtc=true;
		bool hasHighResPcm=false;
		bool highResPcm=true;
		bool hasCdSpeed=false;
		int cdSpeed=0;
		bool hasSpriteTransfer=false;
		int spriteTransfer=0;
		/*! FD0/FD1 image paths (empty string = explicitly unmounted). */
		bool hasFdImg[2]={false,false};
		std::string fdImg[2];

		bool HasAny(void) const
		{
			return true==hasFrequencyMhz ||
			       true==hasCustomFrequencyMhz ||
			       true==hasFastMode ||
			       true==hasMemSizeInMB ||
			       true==hasGamePort0 ||
			       true==hasGamePort1 ||
			       true==hasMaxButtonHoldMs0 ||
			       true==hasMaxButtonHoldMs1 ||
			       true==hasModelGroupIndex ||
			       true==hasModelGroup ||
			       true==hasCpu ||
			       true==hasCpuHighFidelity ||
			       true==hasPretend386DX ||
			       true==hasUseFPU ||
			       true==hasFastScsi ||
			       true==hasFastFd ||
			       true==hasMidiBoard ||
			       true==hasHighResCrtc ||
			       true==hasHighResPcm ||
			       true==hasCdSpeed ||
			       true==hasSpriteTransfer ||
			       true==hasFdImg[0] ||
			       true==hasFdImg[1];
		}
	};

	struct Profile
	{
		/*! Legacy optional soft phys (unused at runtime; MOS uses live soft).
		    Kept for reading old INI files. */
		unsigned int physX=0;
		unsigned int physY=0;
		/*! Game cursor word pairs (see CoordPair).  Legacy INI app_x/app_y and
		    world_x/world_y (+world_bias) load into pair slots. */
		CoordPair pair[MAX_COORD_PAIRS];
		/*! Primary integration choice (see IntegrationMode). */
		int integrationMode=INTEGRATION_DIRECT_WRITE;
		/*! Legacy INI mirrors of integrationMode (still read; no longer written).
	    enabled=false → capture; feedbackOnly=true → MOS or game-feedback. */
		bool feedbackOnly=false;
		bool enabled=true;
		/*! Added to guest-screen coords (App poke / new-mode Δ).  Unused for MOS. */
		int offsetX=0;
		int offsetY=0;
		/*! Legacy mirrors of pair[0].scaleX/Y (still written for old readers). */
		int scaleX=1;
		int scaleY=1;
		/*! Flip guest-axis sign in new-mode Δ (and App poke equilibrium).
		    Unused for MOS system integration. */
		bool invertX=false;
		bool invertY=false;
		/*! New-mode only: wait for Phys/port ACK before the next gameport packet. */
		bool waitFeedback=true;
		/*! App-specific: while MOS is alive, skip immediate soft-cursor writes
		    (Phys / gameport still follow settings). */
		bool stopSoftWrite=false;
		bool verified=false;
		unsigned long long cdSize=0;
		/*! Optional display name from old INI; no longer written (fingerprint keys the file). */
		std::string cdBasename;
		/*! ISO9660 content id (metadata; profile files use fingerprint only). */
		std::string discVolumeLabel;
		std::string discSystemId;
		unsigned int discContentHash32=0;
		/*! Hashed root+track fingerprint — profile file key (fp_%08x.ini). */
		unsigned int discFingerprintHash32=0;
		/*! Guest EXE path (uppercase; may include dirs, e.g. GAME\MAIN.EXE)
		    that gates app-specific mouse apply.  Empty = unbound. */
		std::string appExecName;
		/*! Fingerprint: path FNV mixed with optional CD content hash.
		    0 only when unset; new binds always write a non-zero value. */
		unsigned int appExecHash32=0;
		/*! Optional per-disc machine / peripheral overrides ([machine] section). */
		MachineSettings machine;

		bool HasAppExecBind(void) const{return true!=appExecName.empty();}

		bool HasPhys(void) const{return 0!=physX && 0!=physY;}
		bool HasContentId(void) const{return 0!=discContentHash32;}
		bool HasFingerprint(void) const{return 0!=discFingerprintHash32;}
		/*! File is a usable disc profile (machine and/or mouse). */
		bool HasDiscContent(void) const
		{
			return true==HasPhys() || true==machine.HasAny() || true==HasFingerprint();
		}
		unsigned int NumPairs(void) const
		{
			unsigned int n=0;
			for(auto &pr : pair)
			{
				if(true==pr.Valid())
				{
					++n;
				}
			}
			return n;
		}
		bool WantsDifferential(void) const
		{
			return INTEGRATION_DIFFERENTIAL==integrationMode;
		}
		bool WantsMosIntegration(void) const
		{
			return INTEGRATION_MOS==integrationMode;
		}
		bool WantsDirectWrite(void) const
		{
			return INTEGRATION_DIRECT_WRITE==integrationMode;
		}
		bool WantsGameFeedback(void) const
		{
			return INTEGRATION_GAME_FEEDBACK==integrationMode;
		}
		/*! Follow the same MOS/capture heuristics as when no mouse profile exists. */
		bool WantsAuto(void) const
		{
			return INTEGRATION_AUTO==integrationMode;
		}
		/*! Direct-write (App): game Phys pair(s) to poke. */
		bool HasDirectWriteTarget(void) const
		{
			return true==WantsDirectWrite() && 0<NumPairs();
		}
		/*! New-mode: game Phys pair(s) for feedback Δ. */
		bool HasGameFeedbackTarget(void) const
		{
			return true==WantsGameFeedback() && 0<NumPairs();
		}
		/*! Any mouse-integration settings worth treating as a mouse profile. */
		bool HasMouseIntegration(void) const
		{
			return true==WantsAuto() ||
			       true==WantsMosIntegration() ||
			       true==WantsDifferential() ||
			       true==WantsDirectWrite() ||
			       true==WantsGameFeedback() ||
			       true==HasPhys();
		}
		/*! Keep enabled/feedbackOnly in sync with integrationMode (INI + old UI). */
		void SyncLegacyFlagsFromMode(void)
		{
			enabled=(INTEGRATION_DIFFERENTIAL!=integrationMode);
			// Legacy feedback_only meant "IO deltas, not RAM poke" (MOS or new-mode).
			feedbackOnly=
			    (INTEGRATION_MOS==integrationMode) ||
			    (INTEGRATION_GAME_FEEDBACK==integrationMode);
		}
		/*! After loading legacy-only INI keys, derive integrationMode. */
		void DeriveModeFromLegacyFlags(void)
		{
			if(true!=enabled)
			{
				integrationMode=INTEGRATION_DIFFERENTIAL;
			}
			else if(true==feedbackOnly)
			{
				integrationMode=INTEGRATION_MOS;
			}
			else
			{
				integrationMode=INTEGRATION_DIRECT_WRITE;
			}
		}

		std::string ToIniString(void) const;
		static bool FromIniString(const std::string &ini,Profile &out);
	};

	struct Candidate
	{
		unsigned int physAddr=0;
		unsigned int size=2; // 1, 2 or 4
		unsigned int lastValue=0;
		unsigned int prevValue=0;
		bool hasPrev=false;
		/*! Observed word range as signed int16 (stored in minValue/maxValue bits). */
		unsigned int minValue=0;
		unsigned int maxValue=0;
		bool hasRange=false; // true once at least one value observed
		int scoreX=0;
		int scoreY=0;
		int rejectScore=0; // accumulator / out-of-range penalties
		unsigned int hits=0;
		unsigned int inRangeHits=0;
		unsigned int cs=0;
		unsigned int eip=0;
		bool knownSoftCursor=false; // MOS/TBIOS soft-cursor word
		bool appShadow=false; // app CS wrote value matching soft cursor (private copy?)
		bool motionPulse=false; // small-range word that tracks mouse Δ (not abs coords)
		bool screenDraw=false; // full CRTC-screen abs (not world / playfield-only)
		bool motionCorr=false; // value Δ matched a recent mouse Δ (range-agnostic)
		/*! Seeded from the active profile (soft / pair words) so they show up
		    even before the store-trace catches them. */
		bool fromProfile=false;
		bool profileAxisX=false;
		bool profileAxisY=false;
		/*! User 監視: keep in list and always refresh RAM value. */
		bool userWatch=false;
		/*! Value sits in guest screen resolution (0..W-1 / 0..H-1) — priority candidate. */
		bool resRangeFit=false;
		bool resRangeFitX=false;
		bool resRangeFitY=false;
		/*! User 追跡: chase guest-writer SOURCE on every store. */
		bool userChase=false;
		unsigned int appShadowHits=0;
		unsigned int screenDrawHits=0;
		unsigned int motionCorrHits=0;
		/*! storeEventCount at last motion-correlated update (for stale pruning). */
		unsigned int lastMotionSerial=0;
		/*! storeEventCount at last observed store (for low-frequency drop). */
		unsigned int lastHitSerial=0;
		/*! storeEventCount when this phys first entered the candidate list. */
		unsigned int firstHitSerial=0;
		/*! Non-SCAN: consecutive host-Δ samples where (max-min) greatly exceeds resolution. */
		unsigned int outOfResStreak=0;
		/*! Consecutive refresh samples where value changed with no host mouse activity. */
		unsigned int idleChangeStreak=0;

		void ClearRange(void)
		{
			minValue=0;
			maxValue=0;
			hasRange=false;
		}
		void NoteValue(unsigned int v)
		{
			// min/max/value are signed int16 coordinate words.
			const int s=static_cast<int>(static_cast<short>(v&0xffffu));
			if(true!=hasRange)
			{
				minValue=(unsigned int)(unsigned short)s;
				maxValue=(unsigned int)(unsigned short)s;
				hasRange=true;
				return;
			}
			const int sMin=static_cast<int>(static_cast<short>(minValue&0xffffu));
			const int sMax=static_cast<int>(static_cast<short>(maxValue&0xffffu));
			if(s<sMin)
			{
				minValue=(unsigned int)(unsigned short)s;
			}
			if(s>sMax)
			{
				maxValue=(unsigned int)(unsigned short)s;
			}
		}
	};

	bool enabled=false;
	bool paused=false;
	bool calibrating=false;
	bool autoCalibratingByIo=false;
	int autoCalibRemainingDx=0;
	int autoCalibRemainingDy=0;
	unsigned int autoCalibLegIndex=0;
	unsigned int autoCalibLapRepeat=0;
	unsigned int autoCalibIoCooldown=0;
	/*! False until the initial (-1024,-768) home-to-TL leg finishes. */
	bool autoCalibHomed=false;

	void Attach(FMTownsCommon &towns);
	void Detach(void);
	void SetEnabled(bool on);

	/*! Calibration: scan ON + force differential (no direct-write) until Stop. */
	void StartCalibration(void);
	void StopCalibration(void);
	bool IsCalibrating(void) const;

	/*! portIndex: 0 = 0x4D0, 1 = 0x4D2. fromTbios: CS was TBIOS (soft cursor owner). */
	void OnMouseIoRead(unsigned int portIndex,bool fromTbios);
	/*! App called MOS BIOS (AH≠00/01).  Tops up store-trace only while a
	    host/IO Δ is already fresh (soft alone is not mouse activity). */
	void OnMosBiosAppCall(void);
	/*! App read a MOS/TBIOS soft-cursor word.  Tops up store-trace only while
	    a host/IO Δ is already fresh (MOS-less titles never move soft). */
	void OnSoftCursorAppRead(void);
	/*! Host/IO mouse Δ.  Always records motion for value-based prune;
	    store-trace arming only while scan is on. */
	void OnHostMouseMotion(int dx,int dy);
	void ClearCandidates(void);
	/*! Reset min..max on every candidate (keep candidates). */
	void ClearCandidateRanges(void);
	/*! Drop every candidate whose phys is not in keep (empty → clear all). */
	void KeepOnlyCandidates(const std::vector <unsigned int> &keep);

	std::vector <Candidate> GetTopCandidates(unsigned int maxN=TOP_N_DEFAULT) const;
	/*! Refresh lastValue for list rows; during SCAN also bump hits when RAM
	    changes while a host/IO Δ is fresh (store trace alone misses most
	    game-cursor updates). */
	void RefreshWatchedValues(void);

	void SelectCandidate(unsigned int physAddr,unsigned int size);
	unsigned int SelectedPhysAddr(void) const{return selectedPhysAddr;}
	unsigned int SelectedSize(void) const{return selectedSize;}

	/*! Watch phys for the next guest store, disassemble the writer, and promote
	    its SOURCE phys into the candidate list (copy-hop).
	    Returns the SOURCE phys if already known from the last writer capture,
	    otherwise 0 (SOURCE appears after the next guest write to phys). */
	unsigned int ChaseSourceOf(unsigned int phys);
	/*! Phys addresses promoted by the last ChaseSourceOf / writer hop (UI 追跡). */
	void TakeFollowedSources(unsigned int out[8],unsigned int &count);
	/*! Chase seeds whose SOURCE was found — UI should clear 追跡 on these. */
	void TakeClearedChase(unsigned int out[8],unsigned int &count);

	unsigned int ArmCount(void) const{return armCount;}
	unsigned int IoArmCount(void) const{return ioArmCount;}
	unsigned int TbiosIoArmCount(void) const{return tbiosIoArmCount;}
	unsigned int StoreEventCount(void) const{return storeEventCount;}
	unsigned int TraceRemaining(void) const;
	unsigned int CandidateCount(void) const;
	int PendingGpDx(void) const{return pendingGpDx;}
	int PendingGpDy(void) const{return pendingGpDy;}

	/*! Live TBIOS/MOS soft-cursor snapshot for the debug UI. */
	bool GetSoftCursorSnapshot(unsigned int &physX,unsigned int &physY,int &mx,int &my) const;
	/*! Observed min/max for soft-cursor words (calibration). Returns false if not tracked yet. */
	bool GetSoftCursorRange(
	    unsigned int &minX,unsigned int &maxX,
	    unsigned int &minY,unsigned int &maxY) const;

	void SetProfileDirectory(const std::string &dir);
	const std::string &ProfileDirectory(void) const{return profileDir;}

	/*! Build verified profile from soft-cursor phys + soft origin as offset
	    (e.g. soft minY=16 → offsetY=16). Screen size is taken live from CRTC. */
	bool CaptureSoftCursorProfile(void);
	bool SaveActiveProfile(void) const;
	/*! Clear phys/pairs/offsets in the CD's profile file (file kept).
	    Unset values are written as 0; CD basename/size are preserved. */
	bool ResetProfileSettings(void);
	bool TryLoadForDisc(const std::string &discPath);
	void ClearActiveProfile(void);
	/*! Create a new fp_*.ini for the mounted disc from machine defaults (no mouse phys yet). */
	bool CreateProfileForCurrentDisc(const MachineSettings &machineDefaults);
	/*! Delete the on-disk fp_*.ini for the mounted disc and clear the active profile. */
	bool DeleteProfileForCurrentDisc(void);
	/*! Update [machine] on the active disc profile and save (preserves mouse section). */
	bool ApplyAndSaveMachineSettings(const MachineSettings &machine);
	/*! Merge CPU clock fields into the active disc profile [machine] and save
	    (preserves other machine keys and the mouse section). */
	bool MergeAndSaveMachineClock(bool fastMode,int frequencyMhz,int customFrequencyMhz);
	/*! Update FD0/FD1 mount paths on the active disc profile and save. */
	bool ApplyAndSaveFdMounts(const std::string &fd0,const std::string &fd1);
	/*! True when a disc profile file is loaded (machine and/or mouse). */
	bool DiscProfileLoaded(void) const;
	/*! Basename of the on-disk profile (e.g. "fp_........ini"), or empty if new/unsaved. */
	std::string ActiveProfileFileName(void) const;

	/*! True when mouse-integration settings are loaded (MOS / capture / pairs). */
	bool ProfileLoaded(void) const;
	Profile GetActiveProfile(void) const;
	void SetActiveProfile(const Profile &p);
	/*! While profile apply is active, print soft X/Y to stdout when they change. */
	void LogSoftCursorIfChanged(int mx,int my);

	/*! True while a verified CD profile wants absolute.
	    Direct-write keeps absolute even if Mouse BIOS is down; MOS mode needs BIOS. */
	bool ProfileKeepsAbsolute(void) const;
	/*! Apply profile absolute off TOS/TMENU desktop when a verified direct-write
	    profile is loaded.  With Mouse BIOS alive, soft-cursor phys must match.
	    With Mouse BIOS down (typical in-game), apply from the saved profile.
	    App→TMENU CRTC heuristic clears apply briefly; Current EXE stays until
	    INT 21H AH=4CH (same EXP can host MOS menus and in-game cursors). */
	bool WantsProfileAbsolute(bool standardDesktopCrtc) const;

	/*! INT 21H AH=4BH Load/Exec — track active guest EXE / DOS extender. */
	void OnDosExec(unsigned int AX,const std::string &fName);
	/*! INT 21H AH=3DH Open — capture .EXP/.REX payload under a DOS extender. */
	void OnDosFileOpen(unsigned int AX,const std::string &fName);
	/*! INT 21H AH=4CH or next top-level exec — end current EXE (nested 4BH
	    restores the parent; extender payloads must survive helper exits). */
	void OnDosTerminate(const char *reason);
	/*! True when app-specific profile bind matches the effective guest identity
	    (payload .EXP when a DOS extender is running, else the EXE). */
	bool AppExecMatched(void) const;
	/*! Live identity for UI / Bind (prefers extender payload when present). */
	void GetActiveAppExec(std::string &name,unsigned int &hash32) const;
	/*! Copy effective identity into the loaded profile's app_exec_* (does not save). */
	bool BindActiveAppExecToProfile(void);
	/*! Soft cursor absent or stuck at 0,0. */
	bool SoftCursorDead(void) const;
	/*! Soft frozen after it was alive/non-zero (host moved, soft did not).
	    Soft still 0,0 during load is NOT inactive (MOS launcher). */
	bool SoftCursorInactiveForApp(void) const;
	/*! Sample host↔soft motion while MOS soft integration is live. */
	void NoteAppExecSoftTrackingSample(int hostX,int hostY,bool softAlive,int softX,int softY);
	/*! True once soft has clearly tracked host motion (MOS launcher UI). */
	bool AppExecSoftTrackedHost(void) const;
	/*! Note Mouse BIOS AH active while the bound identity runs.
	    Pass mouseBIOSStartSerial so each AH=00 re-arms soft phase (drop TOS leftovers). */
	void NoteAppExecMosActive(unsigned int mosStartSerial);
	/*! Bound EXE/EXP is running (本編 = EXP/EXE start only).
	    No soft-freeze / gameport / mos_absent heuristics.
	    Latched until EXE end / desktop / clear. */
	bool AppExecReadyForProfileApply(void) const;
	/*! Log rising/falling edge of mouseCoordProfileApply. */
	void NoteMouseProfileApply(bool on);
	/*! Drain [APP] monitor lines (merged into CD monitor take). */
	std::vector <std::string> TakeMonitorLines(void);

	/*! Map emulator-image host coords → guest soft space using only a linear
	    render-size ratio (e.g. /2 for 640→320). */
	void MapHostToProfileCoords(int &mx,int &my) const;

	/*! Guest logical screen size in soft-cursor units (width/height). */
	bool TryGuestScreenSize(int &wid,int &hei) const;
	bool ReadProfileCoords(int &mx,int &my) const;
	bool WriteProfileCoords(int mx,int my);
	/*! Write mapped host coordinates to profile game Phys pair(s) only.
	    Soft (prof_px/py) is title ID / MOS reference — not written here.
	    Returns false when no pair is configured. */
	bool WriteAppCursorCoords(int mx,int my);
	unsigned int DirectWriteCount(void) const;
	bool LastDirectWriteOk(void) const;
	void GetLastDirectWrite(int &targetX,int &targetY) const;
	/*! Install/remove Memory store-guards on profile pair words.
	    on=true while direct-write profile apply is active. */
	void SyncAppStoreGuard(bool on);
	unsigned int AppStoreGuardBlockCount(void) const;

	/*! Guest store into one of the profile target words (soft or a pair word).
	    Tells whether the engine owns the word, and which code re-writes it. */
	struct TargetWrite
	{
		unsigned int physAddr=0;
		unsigned int count=0;
		unsigned int lastValue=0;
		unsigned int cs=0;
		unsigned int eip=0;
	};
	enum
	{
		TARGET_SOFT_X,TARGET_SOFT_Y,
		TARGET_PAIR_BASE,
		NUM_TARGET_WRITE=TARGET_PAIR_BASE+MAX_COORD_PAIRS*2
	};
	TargetWrite GetGuestTargetWrite(unsigned int which) const;

	/*! Snapshot of the guest code that last wrote an app/world profile word.
	    Captured at store time so CS:EIP is the write instruction itself. */
	struct GuestWriterTrace
	{
		bool valid=false;
		unsigned int which=0; // TARGET_PAIR_BASE+n etc.
		unsigned int physAddr=0;
		unsigned int value=0;
		unsigned int cs=0;
		unsigned int eip=0;
		unsigned int eax=0,ebx=0,ecx=0,edx=0;
		unsigned int esi=0,edi=0,ebp=0,esp=0;
		unsigned int ds=0,es=0,ss=0,fs=0,gs=0;
		unsigned int dsBase=0,esBase=0,ssBase=0;
		/*! phys − moffs from the store insn.  Live DS.base can be a different
		    selector (flat 32-bit apps may show a high linear DS while data
		    lives at a lower base). */
		unsigned int impliedDsBase=0;
		bool hasImpliedDsBase=false;
		unsigned int storeMoffs=0;
		bool hasStoreMoffs=false;
		/*! Immediate source of AX for this store: preceding MOV AX,[moffs]. */
		unsigned int srcMoffs=0;
		unsigned int srcPhys=0;
		int srcValue=0;
		bool hasSrc=false;
		/*! Disassembly window around the store EIP ('>' marks the store). */
		std::string disasm;
		/*! Human-readable copy-block summary (dest←src pairs near the store). */
		std::string copySummary;
		/*! Words around the store target (phys-8 .. phys+14). */
		unsigned int nearWords[12]={};
		/*! True when this capture was from an auto-chased SOURCE phys, not profile. */
		bool fromChase=false;
	};
	GuestWriterTrace GetGuestWriterTrace(void) const;
	/*! Phys addresses auto-chased from the last SOURCE (for the next hop). */
	void GetChasePhys(unsigned int out[8],unsigned int &count) const;
	/*! Sync permanent 追跡 watches from the UI (replaces chase list). */
	void SetUserChasePhys(const std::vector <unsigned int> &phys);
	/*! Mark 監視 phys so they stay in the candidate list and refresh. */
	void SetUserWatchPhys(const std::vector <unsigned int> &phys);

	/*! Best app-private cursor shadow pair (shad=Y), if scored. */
	bool TryReadAppShadowCoords(int &mx,int &my) const;
	/*! Physical addresses of the same best shadow pair (for profile app_x / app_y). */
	bool TryReadAppShadowPhys(unsigned int &px,unsigned int &py) const;
	/*! Best full-screen draw-cursor pair (CRTC range, not world/room-only). */
	bool TryReadScreenDrawCoords(int &mx,int &my) const;
	bool TryReadScreenDrawPhys(unsigned int &px,unsigned int &py) const;

	/*! While scan is on: decay the host/IO Δ motion window each sample.
	    Soft-cursor changes never open a hunt (MOS-less titles). */
	void PollSoftCursorShadow(void);
	unsigned int ShadowArmCount(void) const{return shadowArmCount;}
	unsigned int BiosPathArmCount(void) const{return biosPathArmCount;}

private:
	FMTownsCommon *townsPtr=nullptr;
	Memory *memPtr=nullptr;

	/*! Host mouse Δ since last RefreshWatchedValues (value-based prune). */
	bool hostMotionSinceRefresh=false;

	int pendingGpDx=0;
	int pendingGpDy=0;
	bool gpMotionFresh=false;
	/*! Host mouse samples left before the motion window closes (cursor-lag margin). */
	unsigned int motionFreshFrames=0;

	int recentGpDx[DELTA_HISTORY]={0};
	int recentGpDy[DELTA_HISTORY]={0};
	int recentSoftDx[DELTA_HISTORY]={0};
	int recentSoftDy[DELTA_HISTORY]={0};
	unsigned int recentDeltaCount=0;
	unsigned int recentDeltaHead=0;

	int pendingSoftX=0;
	int pendingSoftY=0;
	int pendingSoftDx=0;
	int pendingSoftDy=0;
	bool softShadowFresh=false;
	bool softSampleValid=false;
	int lastSoftSampleX=0;
	int lastSoftSampleY=0;

	/*! Latest mouse-sized Δ written to a motion-pulse cell.
	    Used to find absolute cursor words updated via add, not mov-from-soft. */
	int pendingAppDelta=0;
	bool appDeltaFresh=false;
	int lastBiosArmSoftX=0;
	int lastBiosArmSoftY=0;
	bool lastBiosArmValid=false;

	unsigned int selectedPhysAddr=0;
	unsigned int selectedSize=0;

	unsigned int armCount=0;
	unsigned int ioArmCount=0;
	unsigned int tbiosIoArmCount=0;
	unsigned int shadowArmCount=0;
	unsigned int biosPathArmCount=0;
	unsigned int storeEventCount=0;

	std::string profileDir;
	Profile activeProfile;
	bool profileLoaded=false;
	bool softLogValid=false;
	int lastSoftLogX=0;
	int lastSoftLogY=0;

	/*! Last INT 21H AH=4BH guest EXE (normalized path + fingerprint). */
	bool activeAppExecValid=false;
	std::string activeAppExecName;
	unsigned int activeAppExecHash32=0;
	unsigned long long activeAppExecStartTime=0;
	/*! DOS extender host (RUN386 etc.); payload is the .EXP it opens. */
	bool activeAppExtender=false;
	bool activeAppPayloadValid=false;
	std::string activeAppPayloadName;
	unsigned int activeAppPayloadHash32=0;
	/*! Suspended parents for nested INT 21H AH=4BH (child AH=4CH restores). */
	enum
	{
		MAX_APP_EXEC_PARENTS=8,
		/*! Delay app-specific apply after EXE/EXP identity start.
		    RUN386 AH=3DH fires before relocation/init; poking Phys / store-guard
		    that early can freeze the guest (capture-then-apply later is fine). */
		APP_EXEC_APPLY_GRACE_NS=1500000000ull, // 1.5s
	};
	struct AppExecFrame
	{
		std::string name;
		unsigned int hash32=0;
		bool extender=false;
		bool payloadValid=false;
		std::string payloadName;
		unsigned int payloadHash32=0;
	};
	std::vector <AppExecFrame> appExecParents;
	/*! AH=4BH of ignored system names (TBIOS.SYS …): count only, keep Current EXE. */
	unsigned int appExecSilentNestDepth=0;
	/*! Soft-alive seen while this identity runs (launcher MOS UI). */
	bool appExecSoftAliveSeen=false;
	/*! Soft position changed at least once in this MOS session (any Δ). */
	bool appExecSoftMovedInSession=false;
	/*! Soft position changed in response to host motion (real MOS soft UI). */
	bool appExecSoftTrackedHost=false;
	bool appExecSoftTrackSampleValid=false;
	int appExecSoftTrackHostX=0;
	int appExecSoftTrackHostY=0;
	int appExecSoftTrackSoftX=0;
	int appExecSoftTrackSoftY=0;
	unsigned int appExecSoftStuckHostMotion=0;
	bool appExecSoftUnresponsive=false;
	/*! Host motion awaiting soft catch-up (MOS soft UI lags a frame or two). */
	unsigned int appExecSoftPendingHostStuck=0;
	unsigned int appExecSoftLagSamplesLeft=0;
	/*! Mouse BIOS was active at least once under this EXE/EXP identity. */
	bool appExecSawMosActive=false;
	unsigned int appExecMosStartSerial=0;
	unsigned long long appExecMosStartTime=0;
	/*! gameportMouseReadCount at identity start (app main-loop polls only). */
	unsigned int appExecGameportBaseline=0;
	bool appExecLoggedInGame=false;
	bool loggedProfileApply=false;
	std::deque <std::string> appMonitorLines;
	void LogAppMonitorLine(const std::string &line);
	void ResetAppExecPhaseLocked(void);
	void ResetAppExecSoftPhaseLocked(void);
	/*! Normalize INT 21H path to uppercase relative form (keep dirs, strip drive). */
	static std::string NormalizeDosExecPath(const std::string &path);
	static std::string DosExecBasename(const std::string &normPath);
	static bool IsDosExtenderBasename(const std::string &base);
	static bool IsDosExtenderPayloadBasename(const std::string &base);
	/*! Towns OS / DOS helpers that must not become Current EXE (e.g. TBIOS.SYS). */
	static bool IsIgnoredAppExecBasename(const std::string &base);
	static unsigned int MixAppExecFingerprint(
	    const std::string &normPath,unsigned int contentHash32);
	static bool AppExecNameMatches(
	    const std::string &profileName,const std::string &activeName);
	unsigned int HashExecFromMountedDisc(const std::string &normPath) const;
	void ClearActiveAppPayloadLocked(const char *reason);
	void ClearActiveAppExecLocked(const char *reason);
	/*! Push current EXE (+ payload) so a nested 4BH can run; AH=4CH pops. */
	void PushActiveAppExecParentLocked(void);
	/*! End current EXE; restore parent if any (nested Load/Exec). */
	void EndActiveAppExecLocked(const char *reason);
	void GetEffectiveAppExecLocked(std::string &name,unsigned int &hash32) const;
	/*! Prevent host-originated direct app-coordinate writes from becoming scan candidates. */
	bool suppressOwnStore=false;
	/*! Last direct-write attempt (for the scan monitor). */
	bool lastDirectWriteOk=false;
	unsigned int directWriteCount=0;
	int lastDirectTargetX=0;
	int lastDirectTargetY=0;
	TargetWrite guestTargetWrite[NUM_TARGET_WRITE];
	GuestWriterTrace guestWriterTrace;
	enum { MAX_CHASE=8, MAX_FOLLOWED=8, MAX_CLEARED_CHASE=8 };
	unsigned int chasePhys[MAX_CHASE]={};
	unsigned int chaseCount=0;
	unsigned int followedSrcPhys[MAX_FOLLOWED]={};
	unsigned int followedSrcCount=0;
	unsigned int clearedChasePhys[MAX_CLEARED_CHASE]={};
	unsigned int clearedChaseCount=0;
	/*! Baseline word values for 追跡 neighborhood probe (not yet candidates). */
	std::unordered_map <unsigned int,unsigned int> chaseNearPrev;
	/*! Record a guest store if physAddr is a profile target word (call with mtx held). */
	void NoteGuestTargetWriteLocked(
	    unsigned int physAddr,unsigned int value,unsigned int cs,unsigned int eip);
	/*! Capture regs + disasm for an app/world writer (call without needing mtx for CPU). */
	void CaptureGuestWriterTrace(
	    unsigned int which,unsigned int physAddr,unsigned int value,
	    unsigned int cs,unsigned int eip,bool fromChase=false);
	void AddChasePhysLocked(unsigned int phys);
	void RemoveChasePhysLocked(unsigned int phys);
	void NoteClearedChaseLocked(unsigned int phys);
	/*! Keep phys visible/protected without reporting it as a new SOURCE. */
	void EnsureChaseCandidateLocked(unsigned int phys);
	/*! Seed phys as a protected candidate and queue it for the UI follow list. */
	void PromoteSourceCandidateLocked(unsigned int phys);
	/*! Around each 追跡 seed: promote word phys whose Δ matches host/IO motion. */
	void ProbeChaseNeighborhoodLocked(bool huntOk);
	/*! Drop non-pinned candidates whose signed value stays above screen
	    resolution across mouse-Δ samples (mtx held; non-SCAN only). */
	void PruneIdleChurnLocked(void);
	/*! Ensure active-profile soft/pair words are in the candidate list. */
	void SeedProfileCandidatesLocked(void);

	mutable std::mutex mtx;
	std::vector <Candidate> candidates;
	/*! Current focused scan window; cold windows are skipped on later passes. */
	unsigned int scanRangeBase=SCAN_ADDR_LO;
	unsigned int scanRangeEnd=SCAN_ADDR_LO+SCAN_RANGE_SIZE;
	unsigned int scanRangeStoreCount=0;
	std::unordered_set <unsigned int> scanRangeColdBases;

	std::unordered_set <unsigned int> candidatePhysSet;
	std::unordered_map <unsigned int,size_t> candidateIndex;
	/*! True while a mouse/soft motion hunt window is open (lock-free hot-path gate). */
	std::atomic <bool> huntMotionOpen{false};
	void RebuildCandidateMapsLocked(void);
	void NoteCandidatePhysLocked(unsigned int phys);
	void DropCandidatePhysLocked(unsigned int phys);

	void OnStore(unsigned int physAddr,unsigned int size,unsigned int data);
	/*! Score one observed value change (store trace or motion RAM poll). */
	void NoteCandidateValueLocked(
	    Candidate &c,unsigned int masked,unsigned int cs,unsigned int eip);
	void UpdateResRangeFitLocked(Candidate &c) const;
	/*! Drop frequently-written but never-tracking cells (call with mtx held). */
	void PruneStaleLocked(void);
	static void StoreTraceThunk(void *user,unsigned int physAddr,unsigned int size,unsigned int data);
	static void ChaseTraceThunk(void *user,unsigned int physAddr,unsigned int size,unsigned int data);
	void SyncChaseWatchLocked(void);

	void ArmMemoryTrace(unsigned int count);
	void DisarmMemoryTrace(void);
	static bool IsVRAMPhys(unsigned int physAddr);
	bool ShouldProcessStore(unsigned int physAddr,unsigned int size) const;
	Candidate *FindOrAddLocked(unsigned int physAddr,unsigned int size,bool allowNew=true);

	int CoordMaxX(void) const;
	int CoordMaxY(void) const;
	static bool InCoordRange(unsigned int v,int maxV);
	static int ClampInt(int v,int lo,int hi);
	static int MapAxisToRange(int host,int minV,int maxV,int hostSpan);

	bool ResolveSoftCursorPhys(unsigned int &physX,unsigned int &physY) const;
	/*! Primary soft-cursor words; for V31L35 also MOS_work alt pair when both exist. */
	bool CollectSoftCursorPhys(
	    unsigned int &physX,unsigned int &physY,
	    unsigned int &altX,unsigned int &altY) const;
	void ObserveSoftCursorLocked(void);
	bool IsKnownSoftCursorPhysLocked(unsigned int physAddr) const;
	/*! Record a host/IO motion sample and open the frame window (call with mtx held). */
	void PushRecentDeltaLocked(int gpDx,int gpDy,int softDx,int softDy);
	/*! True when dVal equals one recent host/IO delta, or the sum of the newest few
	    (app applied several frames of motion in one store). */
	bool MatchesRecentDeltaLocked(int dVal,bool useGp,bool xAxis) const;
	/*! Copies of the best-scored app-private X/Y candidates near the soft cursor. */
	bool FindAppShadowPair(Candidate &cx,Candidate &cy) const;
	/*! Screen-space draw cursor: full CRTC range, tracks gameport Δ after IO. */
	bool FindScreenDrawPair(Candidate &cx,Candidate &cy) const;
	static bool IsTbiosOrSystemCs(unsigned int cs,unsigned int inInterruptDepth);

	std::string ProfilePathForFingerprint(unsigned int fingerprintHash32) const;
	std::string ProfilePathForContentHash(unsigned int contentHash32) const;
	std::string ProfilePathForBasename(const std::string &basename) const;
	bool WriteProfileFile(const Profile &p) const;
	bool ReadProfileFile(const std::string &path,Profile &out) const;
};

/* } */
#endif
