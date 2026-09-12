/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef OUTSIDE_WORLD_IS_INCLUDED
#define OUTSIDE_WORLD_IS_INCLUDED
/* { */

#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <mutex>

#include "render.h"
#include "discimg.h"
#include "rf5c68.h"
#include "ym2612.h"
#include "townsdef.h"
#include "townsparam.h"
#include "crtc.h"

class Outside_World
{
public:
	class VirtualKey
	{
	public:
		unsigned int townsKey=0;
		int physicalId=-1;
		unsigned int button=0;
	};

	// Mouse will be automatically identified by towns.gameport.
	// Only game-pad emulation takes effect.
	unsigned int gamePort[TOWNS_NUM_GAMEPORTS];

	// Pause mouse integration until mouse cursor is moved.
	// Strike Commander controls the view direction with mouse, and press F1 to reset.
	// The program moves the mouse coordinate to the center of the window when F1 is pressed.
	// However, with mouse integration turned on, the mouse coordinate moves back to wherever
	// host mouse cursor is located, and the view changes again.  To prevent it,
	// Mouse Integration should be paused until mouse is moved.
	// To more generalize, mouse integration is turned on when mouse moves, and pauses
	// when the mouse cursor in the VM is stationary at the host's mouse coordinate for
	// several steps, it pauses again.
	enum
	{
		MOUSE_STATIONARY_COUNT=4
	};
	bool mouseIntegrationActive=false;
	int lastMx,lastMy,mouseStationaryCount=MOUSE_STATIONARY_COUNT;
	/*! User preference while Mouse BIOS (TBIOS/MOS) is active. Absolute/snap when false.
	    Legacy middle-button toggle; Default mode no longer flips this for priority. */
	bool differentialMouseIntegration=false;
	/*! When MOS is active but unused (Default mode only), automatically switch to
	    mouse capture.  Disabled by redesign: Default keeps MOS while BIOS is alive. */
	bool autoDifferentialOnMosUnused=false;
	/*! Runtime path actually used for ProcessMouse vs ProcessMouseDifferential. */
	bool effectiveDifferentialMouseIntegration=false;
	/*! False after middle-button release with MOS down; middle button resumes (not click). */
	bool mouseFeedingEnabled_=true;
	/*! After abs↔diff / apply / capture path change: next ProcessMouse* forces buttons up
	    so a stale host lastKnownMouse cannot re-stick the guest gameport buttons. */
	bool mouseButtonsForceRelease_=false;
	/*! Middle-button released capture while differential (host cursor shown). */
	bool mouseCaptureReleased_=false;
	/*! Middle-button temporary override: force mouse capture while MOS/app absolute
	    would otherwise apply.  Middle again clears this and restores absolute. */
	bool middleForceCapture_=false;
	/*! GUI/core failsafe: force host cursor visible (e.g. VM appear hung). */
	bool mouseFailsafeShowHostCursor_=false;
	/*! TownsQt mouse-integration debug overlay; OFF skips UpdateMouseIntegrationDebug work. */
	bool mouseIntegrationDebugEnabled_=false;
	/*! Saw MOS AH=00 at least once (debug / legacy). */
	bool mouseBIOSEverActive_=false;
	/*! No MOS → differential-only (unless app-specific hold). Cleared while MOS active. */
	bool mouseBIOSStoppedForcedDiff_=false;
	/*! Auto-forced differential begins with capture released; set once per force episode
	    so the user starts capture with the middle button (not grabbed automatically). */
	bool forcedDiffReleaseApplied_=false;
	/*! MOS still active, but guest never reads MOS/TBIOS coords and only uses gameport. */
	bool mosUnusedForcedDiff_=false;
	/*! Observing MOS usage after AH=00 (force differential while probing). */
	bool mosUsageObserving_=false;
	/*! After concluding "MOS is read" (MOS-driven UI), watch for reads to stop. */
	bool mosUsageMonitorAfterUse_=false;
	/*! After desktop concluded "MOS is read", re-check once non-desktop CRTC appears. */
	bool mosUsageAwaitExoticReprobe_=false;
	long long int mosUsageObserveStartTownsTime_=0;
	unsigned int mosUsageObserveSerial_=0;
	unsigned int mosUsageIdleReadBaseline_=0;
	unsigned int mosUsageIdlePacketBaseline_=0;
	/*! Throttle for the post-launcher monitor diagnostic log. */
	long long int mosUsageMonitorLogTownsTime_=0;
	/*! Host mouse motion (|dx|+|dy|, emu image px) accumulated since the last MOS coord
	    read, while monitoring after use.  Distinguishes "app reads MOS when the mouse
	    moves" (launcher → absolute) from "app ignores MOS on motion" (game → differential).
	    Idle time alone cannot tell them apart: both stop reading MOS when the mouse rests. */
	unsigned int mosUsageMotionSinceRead_=0;
	int mosUsagePrevHostX_=0,mosUsagePrevHostY_=0;
	bool mosUsageHostPosValid_=false;
	/*! Count of "forced differential → MOS reads resumed → reverted" oscillations in the
	    current MOS session.  A MOS-driven UI reads MOS again as soon as
	    differential feeds the gameport, so it reverts every time; a gameport-only title never
	    resumes MOS reads.  After a couple of reverts we latch to absolute (below). */
	unsigned int mosUsageRevertCount_=0;
	/*! Latched "this MOS session is MOS-driven, keep absolute" until the next AH=00. */
	bool mosUsageLatchAbsolute_=false;
	/*! Capture-first UI helper (unused by HEAD MOS probe; kept for status compatibility). */
	bool mosUsageCaptureFirstActive_=false;
	/*! Previous IsStandardDesktopCrtc sample (legacy). */
	bool mosUsagePrevStandardDesktop_=true;
	bool mosUsagePrevStandardDesktopInited_=false;
	/*! MOS used/unused is only meaningful while Mouse Integration (MOS) drives soft
	    (used → values change; unused → values stay fixed).  App apply freezes MOS
	    either way — probe skips while mouseCoordProfileApply.
	    After AH=00: settle, then host motion vs soft motion:
	      soft tracks host → latch absolute;
	      soft frozen → mosUnusedForcedDiff_;
	      absolute latch + soft inactive → demote. */
	bool mosUsageLearnPhase_=false;
	unsigned int mosUsageAppReadBaseline_=0;
	unsigned int mosUsageBiosCallBaseline_=0;
	unsigned int mosUsageSysReadBaseline_=0;
	unsigned int mosUsageGameportBaseline_=0;
	int mosTrackPrevHostX_=0,mosTrackPrevHostY_=0;
	bool mosTrackSampleValid_=false;
	unsigned int mosTrackHostMotion_=0;
	int mosTrackSoftX_=0,mosTrackSoftY_=0;
	bool mosTrackSoftValid_=false;
	bool prevSpriteSpen_=false;
	/*! Set when a non-desktop CRTC mode is seen; cleared after desktop restore. */
	bool spriteOffsetSeenInExoticMode_=false;
	/*! Last seen Mouse BIOS AH=00 serial (TOS/desktop re-init detection). */
	unsigned int lastMouseBIOSStartSerial_=0;
	bool mouseBIOSStartSerialInited_=false;
	/*! Debug Snap: saw Director-like CRTC / ran post-Director absolute-integration pause. */
	bool mouseDesktopSnapshotValid_=false;
	bool mouseDesktopSnapApplied_=false;
	/*! Pause absolute mouse integration after return to TOS (soft cursor recenters). */
	int mouseInfoRepairFrames_=0;
	/*! TownsQt: drive mouse motion with image-space deltas when differential integration is off. */
	bool qtImageDeltaMouseMotion=false;
	/*! Test mode: set guest mouse coordinate equal to host in one step (memory write). */
	bool snapMouseIntegration=true;
	/*! Gradual integration frames before snap engages (layout / BIOS settle). */
	int snapMouseWarmupRemaining=0;
	int snapMouseWarmupFrames=30;
	void ResetSnapMouseWarmup(void);

	// Wing Commander and Strike Commander series can be configured to use mouse as joystick.
	bool mouseByFlightstickAvailable=false;
	bool cyberStickAssignment=false;
	bool mouseByFlightstickEnabled=false;
	int mouseByFlightstickPhysicalId=-1;  // Physical joystick ID.
	int mouseByFlightstickRecalibrateButton=-1; // Recalibrate button
	int mouseByFlightstickCenterX,mouseByFlightstickCenterY;
	float mouseByFlightstickZeroZoneX=0.0F,mouseByFlightstickZeroZoneY=0.0F;
	float mouseByFlightstickScaleX,mouseByFlightstickScaleY;
	float lastJoystickPos[2]={0.0F,0.0F};
	int lastMousePosForSwitchBackToNormalMode[2]={0,0};

	int wingCommander1ThrottleState=0;

	// For Wing Commander, Strike Commander, Fujitsu Air Warrior V2 Throttle Integration.
	int throttlePhysicalId=-1;
	int throttleAxis=2;  // Typically flight-stick's throttle axis is the 3rd axis (#2 axis).
	uint64_t lastThrottleMoveTime=0;
	uint64_t nextThrottleUpdateTime=0;


	/*! Virtual Keys.
	*/
	std::vector <VirtualKey> virtualKeys;

	/*!
	*/
	std::queue <std::string> commandQueue;

	/*! Cache of game-pad indices that needs to be updated in polling.
	*/
	std::vector <unsigned int> gamePadsNeedUpdate;
	bool gameDevsNeedUpdateCached=false;

	/*! Show or hide mouse cursor.  Sent to the WindowThread in the Communicate.
	    Then the window thread will look at it.
	*/
	bool showMouseCursor=true;


	inline float ApplyZeroZone(float rawInput,float zeroZone)
	{
		if(rawInput<-zeroZone)
		{
			return rawInput+zeroZone;
		}
		else if(zeroZone<=rawInput)
		{
			return rawInput-zeroZone;
		}
		else
		{
			return 0.0;
		}
	}


	unsigned int keyboardMode=TOWNS_KEYBOARD_MODE_DIRECT;

	enum
	{
		KEYBOARD_LAYOUT_US,
		KEYBOARD_LAYOUT_JP,
	};

	enum
	{
		LOWER_RIGHT_NONE,
		LOWER_RIGHT_PAUSE,
		LOWER_RIGHT_MENU,
	};
	enum
	{
		STATUS_WID=640,
		STATUS_HEI=16
	};
	class StatusBarInfo
	{
	public:
		bool cdAccessLamp=false;
		bool fdAccessLamp[4]={false,false,false,false};
		bool scsiAccessLamp[6]={false,false,false,false,false,false};
		bool strikeCommanderSpecial=false;

		bool rocketRangerSpecial=false;
		int rocketRangerTiming=0,rocketRangerSpeed=0,rocketRangerNecessarySpeed=0;
		unsigned char rocketRangerPosition=0;
	};
	StatusBarInfo statusBarInfo;
	unsigned int dx=0,dy=0;  // Screen (0,0) will be window (dx,dy)
	unsigned int scalingX=100; // In Percent
	unsigned int scalingY=100; // In Percent
	unsigned int displayW=0,displayH=0; // Rendered Towns image area in host window coords
	bool pauseKey=false;

	unsigned int lowerRightIcon=LOWER_RIGHT_NONE;

	bool closeWindow=false; // Must be copied from WindowInterface::closeWindow in Communicate.

	/*! Updated on the VM thread in DevicePolling for TownsQt mouse-integration debug. */
	int debugGuestMx=0,debugGuestMy=0;
	int debugMosMx=0,debugMosMy=0;
	int debugTbiosMx=0,debugTbiosMy=0;
	int debugRawHostMx=0,debugRawHostMy=0;
	int debugCtrlMx=0,debugCtrlMy=0;
	int debugOriginX=0,debugOriginY=0;
	int debugZoom2xX=2,debugZoom2xY=2;
	int debugMousePage=0;
	int debugHwCursorX=0,debugHwCursorY=0;
	bool debugHwCursorDefined=false;
	unsigned int debugMosWorkPhysAddr=0;
	unsigned int debugTbiosMouseInfoOffset=0;
	bool debugGuestValid=false;
	bool debugMouseBIOSActive=false;
	bool debugEffectiveDifferential=false;
	bool debugForcedDifferentialByMouseBIOSStop=false;
	bool debugForcedDifferentialByMosUnused=false;
	bool debugMosUsageObserving=false;
	unsigned int debugMosCoordAppReads=0;
	unsigned int debugGameportMousePackets=0;
	bool debugMouseCaptureReleased=false;
	bool debugMouseFeedingEnabled=false;
	unsigned int debugTBIOSVersion=0;
	unsigned int debugAppSpecific=0;
	int debugSnapWarmupRemaining=0;
	int debugHSkip1X=0;
	int debugSpriteHOffset=0;
	int debugSpriteVOffset=0;
	int debugSpriteCursorX=-1,debugSpriteCursorY=-1;
	int debugSpriteCursorCount=0;
	int debugSpriteNearestX=-1,debugSpriteNearestY=-1;
	int debugSpriteHalfX=-1,debugSpriteHalfY=-1;
	bool debugSpriteSpen=false;
	int debugVramOffsetX=0,debugVramOffsetY=0;
	int debugVramOffsetX1=0,debugVramOffsetY1=0;
	int debugFa0_0=0,debugFa0_1=0;
	int debugMouseInfoHotX=0,debugMouseInfoHotY=0;
	int debugCursorDrawX=-1,debugCursorDrawY=-1;
	int debugOrg0X=0,debugOrg1X=0;
	int debugHSkip0=0,debugHSkip1=0;
	int debugZoom0X=2,debugZoom0Y=2,debugZoom1X=2,debugZoom1Y=2;
	int debugPageSize0X=0,debugPageSize1X=0;
	bool debugSinglePage=true;
	bool debugShowPage0=true,debugShowPage1=false;
	/*! VM UpdateMouseIntegrationDebug vs TownsQt guestMouseCoords string reads. */
	mutable std::mutex mouseDebugUiMutex;
	std::string debugSysRomVersion;
	std::string debugTbiosId;
	std::string debugTbiosDate;
	std::string debugTosVersion;
	std::string debugMouseInfoWords;
	std::string debugMosWorkWords;
	bool debugMouseSnapValid=false;
	bool debugMouseSnapApplied=false;
	int debugMouseInfoRepair=0;
	int debugMiPrevX=0,debugMiPrevY=0;
	int debugMiPaintX=0,debugMiPaintY=0;
	unsigned int debugVersionCacheTbiosPhys_=0;

	Outside_World();
	virtual ~Outside_World();

	// Directories
	/*! This function must return the directory where the executable is saved in Windows and Linux,
	    /Contents/Resources sub-diretory of the application bundle in macOS.
	    The returned string is in the system-encoding, which doesn't matter in macOS and Linux,
	    but may matter in Windows until shift-JIS is eradicated.
	*/
	virtual std::string GetProgramResourceDirectory(void) const=0;


	virtual void Start(void)=0;
	virtual void Stop(void)=0;
	virtual void DevicePolling(class FMTownsCommon &towns)=0;
	/*! Prefix for host-facing stdout (e.g. "Tsugaru_QT: "). Empty for CUI. */
	std::string hostLogPrefix;
	void LogHostMessage(const std::string &message);

	/*! Recompute effective path / feeding / showMouseCursor from MOS + preference + capture. */
	void UpdateEffectiveDifferentialMouseIntegration(class FMTownsCommon &towns);
	/*! App → TOS/TMENU return (standard CRTC + MOS): end profile apply, re-init mouse. */
	void HandleAppToDesktopReturn(class FMTownsCommon &towns);
	void UpdateMosUsageObservation(class FMTownsCommon &towns);
	/*! Middle button: MOS up → toggle preference; MOS down → release capture (show host cursor). */
	void HandleMouseIntegrationMiddleButton(class FMTownsCommon &towns);
	/*! Picture middle-button: resume feeding after capture release. */
	void ResumeMouseCapture(class FMTownsCommon &towns);
	/*! Release differential capture (show host cursor). Same as middle-button release. */
	void ReleaseMouseCapture(class FMTownsCommon &towns);
	/*! Set differential preference (settings / ENA/DIS DIFFMOUSE). */
	void SetDifferentialMouseIntegrationPreference(bool enabled,class FMTownsCommon *towns);
	void SetMouseFailsafeShowHostCursor(bool show);
	/*! When false, UpdateMouseIntegrationDebug is a no-op (skip string/POD churn). */
	void SetMouseIntegrationDebugEnabled(bool enabled);
	bool MouseIntegrationDebugEnabled(void) const{return mouseIntegrationDebugEnabled_;}
	void UpdateMouseIntegrationDebug(class FMTownsCommon &towns);
	void UpdateStatusBarInfo(class FMTownsCommon &towns);

	/*! Implementation should return true if the image needs to be flipped before drawn on the window.
	    The flag is transferred to rendering thread class at the beginning of the TownsThread::Start.
	*/
	virtual bool ImageNeedsFlip(void)=0;

	void SetKeyboardMode(unsigned int mode);
	virtual void SetKeyboardLayout(unsigned int layout)=0;

	void AddVirtualKey(unsigned int townsKey,int physicalId,unsigned int button);

	/*! Return pauseKey flag.  The flag is clear after this function.
	*/
	bool PauseKeyPressed(void);

	/*! Implementation should call this function for each inkey for application-specific augmentation to work correctly.
	*/
	void ProcessInkey(class FMTownsCommon &towns,int townsKey);

	/*! Implementation should call this function for each mouse reading for application-specific augmentation to work correctly.
	*/
	void ProcessMouse(class FMTownsCommon &towns,int lb,int mb,int rb,int mx,int my);

	/*!
	*/
	void ProcessMouseDifferential(class FMTownsCommon &towns,int lb,int mb,int rb,int dx,int dy,int refX,int refY);

	/*! Right now it updates mouse neutral position for Wing Commander 1 if app-specific augumentation is enabled.
	*/
	void ProcessAppSpecific(class FMTownsCommon &towns);

	virtual std::vector <std::string> MakeDefaultKeyMappingText(void) const;
	virtual std::vector <std::string> MakeKeyMappingText(void) const;
	virtual void LoadKeyMappingFromText(const std::vector <std::string> &text);


	/*! Cache gamePadsNeedUpdate member.
	    Reading game pad may not be the fastest function to call, and therefore reading same game pad multiple times
	    in one polling should be avoided.
	    This function caches which game pads needs to be updated.
	    If the sub-class overloads this function, call Outside_World::CacheGamePadIndicesThatNeedUpdates, and then
	    add an ID by calling UseGamePad function..
	*/
	virtual void CacheGamePadIndicesThatNeedUpdates(void);

	/*! Call this function to cache game pad index that needs to be updated every polling.
	*/
	void UseGamePad(unsigned int gamePadIndex);


	/*! Call this function to toggle host mouse cursor. */
	virtual void ToggleMouseCursor(void){};


	/*! Host-Key Label is implementation dependent.
	*/
	virtual void RegisterHostShortCut(std::string hostKeyLabel,bool ctrl,bool shift,std::string cmdStr);

	/*! Host-Key Label is implementation dependent.
	*/
	virtual void RegisterPauseResume(std::string hostKeyLabel);



	class WindowInterface
	{
	public:
		std::mutex deviceStateLock;
		std::mutex renderingLock;
		std::mutex newImageLock;

		static constexpr size_t VM_CAPTURE_QUEUE_DEPTH=3;

		struct VmCaptureSlot
		{
			unsigned char VRAM[TOWNS_MAX_VRAM_SIZE];
			uint32_t vramBytes=TOWNS_MAX_VRAM_SIZE;
			TownsCRTC::AnalogPalette palette;
			TownsCRTC::ChaseHQPalette chaseHQ;
			unsigned long long captureTownsTime=0;
			bool imageNeedsFlip=false;
			TownsRender::PreparedState rendererState;
		};

		std::deque<VmCaptureSlot> vmCaptureQueue;
		mutable std::mutex vmCaptureMutex;

		bool EnqueueCapture(class FMTownsCommon &towns,bool imageNeedsFlip);
		bool FlushOneCaptureToShared(void);
		size_t VmCaptureQueueDepth(void) const;
		/*! Drop VM-thread captures awaiting render (after LoadState / townsTime jump). */
		void ClearPendingCaptures(void);

		class SharedVariables
		{
		public:
			// Managed by deviceStateLock
			bool VMClosedFromVMThread=false;  // Written from the VM Thread.
			std::vector <unsigned int> gamePadsNeedUpdate;  // Copy of Outside_World's gamePadsNeedUpdate.
			bool showMouseCursor=true;
			bool differentialMouseIntegration=false;

			// Managed by renderingLock
			unsigned int dx=0,dy=0;  // Screen (0,0) will be window (dx,dy)
			unsigned int scalingX=100; // In Percent
			unsigned int scalingY=100; // In Percent
			unsigned int displayW=0,displayH=0;
			unsigned int lowerRightIcon=LOWER_RIGHT_NONE;

			// Managed by newImageLock
			bool needRender=false;
			bool imageNeedsFlip=false;
			unsigned long long captureTownsTime=0;
			TownsRender renderer;
			unsigned char VRAMCopy[TOWNS_MAX_VRAM_SIZE];
			TownsCRTC::AnalogPalette paletteCopy;
			TownsCRTC::ChaseHQPalette chaseHQPaletteCopy;
		};
		class VMThreadVariables
		{
		public:
		};
		class WindowThreadVariables
		{
		public:
			/* VM Thread writes VMClosedFromVMThread with deviceStateLock at the end of VMMainLoop.
			   Window Thread copies VMClosedFromVMThread to VMClosed in the Interval with deviceStateLock.
			   VMClosed is only accessed in the Window thread.  Save one lock.
			*/
			bool VMClosed=false;

			TownsRender::ImageCopy mostRecentImage;
			bool newImageRendered=false;
			unsigned long long lastCaptureTownsTime=0;
			std::vector <unsigned int> gamePadsNeedUpdate;  // Copy of Outside_World's gamePadsNeedUpdate.
			int winWid=640,winHei=480;

			unsigned char *statusBitmap;
		};
		SharedVariables shared;
		VMThreadVariables VMThr;
		WindowThreadVariables winThr;



		bool windowShift=false;
		bool autoScaling=false;
		bool maintainAspect=true;

		unsigned int windowModeOnStartUp=TownsStartParameters::WINDOW_NORMAL;
		unsigned int windowSizeOnStartUp[2]={640,480}; // Valid only with WINDOW_SPECIFY_SIZE

		bool closeWindow=false;  // Windows is closed from outside.

		/*! Extra height reserved at the top of the host window (TownsUI menu bar). */
		unsigned int menuBarHei=0;

		unsigned int ContentAreaHeight(unsigned winHei) const
		{
			const unsigned chrome=STATUS_HEI+menuBarHei;
			return (winHei>chrome) ? (winHei-chrome) : 0;
		}

		WindowInterface();
		~WindowInterface();

		virtual void Start(void)=0;
		virtual void Stop(void)=0;
		/*! Called from the Window thread.
		*/
		virtual void Interval(void)=0;
		/*! Called from the Window thread.
		      VM thread may access scaling, dx, dy, and lowerRightIcon, which therefore must be locked.
		*/
		virtual void Render(bool swapBuffers)=0;
		virtual void UpdateImage(TownsRender::ImageCopy &img)=0;

		/*! Interval function must call this function.
		    Called in the Window thread.
		*/
		void BaseInterval(void);

		/*! Called from the VM thread to tell the new image should be rendered.
		    It will try_lock the renderer, but it fails, it gives up not to block
		    the VM thread.
		*/
		bool SendNewImage(class FMTownsCommon &towns,bool imageNeedsFlip);

		/*! Called from the VM thread to tell VM is closed.
		*/
		void NotifyVMClosed(void);

		void ClearVMClosedFlag(void);

		/*! Called from the Window thread.
		*/
		bool CheckVMClosed(void) const;

		/*! Called in the VM thread.
		    WindowInterface  ->(Device State)-> Outside_World
		    WindowInterface  <-(Game Pads In Use)<- Outside_World
		*/
		virtual void Communicate(Outside_World *)=0;


		void Put16x16(int x0,int y0,const unsigned char icon16x16[]);
		void Put16x16Invert(int x0,int y0,const unsigned char icon16x16[]);

		void Put16x16Select(int x0,int y0,const unsigned char idleIcon16x16[],const unsigned char busyIcon16x16[],bool busy);
		void Put16x16SelectInvert(int x0,int y0,const unsigned char idleIcon16x16[],const unsigned char busyIcon16x16[],bool busy);

		void Print(int x0,const char str[]);

		static unsigned char font10x14[256][56];
	};
	virtual WindowInterface *CreateWindowInterface(void) const=0;
	virtual void DeleteWindowInterface(WindowInterface *) const=0;



	/*! Sound class will entirely stay within the VM thread.
	    TownsThread::VMMainLoop will Start and Stop the class.
	    Therefore, it must not be started or stopped outside of TownsThread::VMMainLoop.

	    Sound class will be referenced from TownsSound, TownsCDROM, TownsSCSI classes,
	    TownsSound class for FM/PCM/Beep, TownsCDROM and TownsSCSI for CDDA.
	*/
	class Sound
	{
	public:
		virtual void Start(void)=0;
		virtual void Stop(void)=0;

		virtual void Polling(void)=0;

		/*! Left level and right level can be 0 to 256.  Value above 256 will be rounded to 256.
		*/
		virtual void CDDAPlay(const DiscImage &discImg,DiscImage::MinSecFrm from,DiscImage::MinSecFrm to,bool repeat,unsigned int,unsigned int)=0;
		virtual void CDDASetVolume(float leftVol,float rightVol)=0;
		virtual void CDDAStop(void)=0;
		virtual void CDDAPause(void)=0;
		virtual void CDDAResume(void)=0;
		virtual bool CDDAIsPlaying(void)=0;
		virtual DiscImage::MinSecFrm CDDACurrentPosition(void)=0;

	public:
		virtual void FMPCMPlay(std::vector <unsigned char > &wave)=0;
		virtual void FMPCMPlayStop(void)=0;
		virtual bool FMPCMChannelPlaying(void)=0;

	public:
		virtual void BeepPlay(int samplingRate, std::vector<unsigned char>& wave) = 0;
		virtual void BeepPlayStop() = 0;
		virtual bool BeepChannelPlaying() const = 0;

		/*! Pull-model audio: advance PCM delivery contract to emulated time (nanoseconds). */
		virtual void SyncPcmContract(unsigned long long towns_time_ns)
		{
			(void)towns_time_ns;
		}

		/*! Pre-buffer PCM before the VM thread sleeps for real-time pacing. */
		virtual void PrepareAudioSleep(unsigned long long sleep_time_ns)
		{
			(void)sleep_time_ns;
		}

		/*! SPSC playback buffer fill in milliseconds (0 if not applicable). */
		virtual double PlaybackBufferMillisec() const
		{
			return 0.0;
		}
	};
	virtual Sound *CreateSound(void) const=0;
	virtual void DeleteSound(Sound *) const=0;
};


/* } */
#endif
