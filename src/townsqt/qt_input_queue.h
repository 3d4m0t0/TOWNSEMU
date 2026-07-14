#ifndef QT_INPUT_QUEUE_IS_INCLUDED
#define QT_INPUT_QUEUE_IS_INCLUDED

#include <atomic>
#include <mutex>
#include <vector>

#include "fssimplewindow.h"

/*! Thread-safe input events from Qt EmuView → window Interval (GUI thread). */
class QtInputQueue
{
public:
	struct MouseEvent
	{
		int evt=FSMOUSEEVENT_NONE;
		int lb=0,mb=0,rb=0;
		int mx=0,my=0;
	};

	void SetViewSize(int wid,int hei);
	void SetDisplayLayout(int emu_wid,int emu_hei,int display_x,int display_y,int display_w,int display_h);
	void KeyDown(int fsKey);
	void KeyUp(int fsKey);
	void CharInput(unsigned int ch);
	void MousePress(int button,int view_x,int view_y,int emu_x,int emu_y);
	void MouseRelease(int button,int view_x,int view_y,int emu_x,int emu_y);
	void MouseMove(int view_x,int view_y,int emu_x,int emu_y);

	/*! GUI thread: poll cursor position (CUI FsGetMouseState equivalent). */
	void PollMouseState(bool lb,bool mb,bool rb,int view_x,int view_y,int emu_x,int emu_y);

	enum
	{
		MOUSE_BTN_LEFT=1,
		MOUSE_BTN_RIGHT=2,
		MOUSE_BTN_MIDDLE=4,
	};

	/*! Called from window Interval on the controller thread. */
	void DrainTo(std::vector<unsigned int> &keyCode,
	             std::vector<unsigned int> &charCode,
	             unsigned char keyState[FSKEY_NUM_KEYCODE],
	             std::vector<MouseEvent> &mouseEvents,
	             MouseEvent &lastMouse,
	             int &lastViewMouseX,int &lastViewMouseY,
	             int &winWid,int &winHei,
	             int &emuWid,int &emuHei,
	             int &displayX,int &displayY,int &displayW,int &displayH,
	             bool &resetDiffMouse);

	/*! Request cursor warp in EmuView widget coordinates (controller thread). */
	void RequestCursorWarp(int view_x,int view_y);

	/*! Apply pending warp on the GUI thread. Returns true if a warp was performed. */
	bool TakeCursorWarp(int &view_x,int &view_y);

	/*! Drop any pending warp (e.g. differential mode off or window inactive). */
	void CancelCursorWarp();

private:
	std::mutex mutex_;
	int viewWid_=640;
	int viewHei_=480;
	int emuWid_=640;
	int emuHei_=480;
	int displayX_=0;
	int displayY_=0;
	int displayW_=640;
	int displayH_=480;
	unsigned char keyState_[FSKEY_NUM_KEYCODE]={};
	std::vector<unsigned int> pendingKeyCode_;
	std::vector<unsigned int> pendingCharCode_;
	std::vector<MouseEvent> pendingMouse_;
	MouseEvent lastMouse_;
	int lastViewMouseX_=0;
	int lastViewMouseY_=0;
	bool resetDiffMouse_=false;

	std::atomic<bool> pendingWarp_{false};
	std::atomic<int> pendingWarpX_{0};
	std::atomic<int> pendingWarpY_{0};
};

#endif
