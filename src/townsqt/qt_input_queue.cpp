#include "qt_input_queue.h"

#include <algorithm>

void QtInputQueue::SetViewSize(int wid,int hei)
{
	const int new_wid=std::max(1,wid);
	const int new_hei=std::max(1,hei);
	std::lock_guard<std::mutex> lock(mutex_);
	if(new_wid!=viewWid_ || new_hei!=viewHei_)
	{
		viewWid_=new_wid;
		viewHei_=new_hei;
		resetDiffMouse_=true;
	}
}

void QtInputQueue::SetDisplayLayout(int emu_wid,int emu_hei,int display_x,int display_y,int display_w,int display_h)
{
	std::lock_guard<std::mutex> lock(mutex_);
	emuWid_=std::max(1,emu_wid);
	emuHei_=std::max(1,emu_hei);
	displayX_=display_x;
	displayY_=display_y;
	displayW_=std::max(1,display_w);
	displayH_=std::max(1,display_h);
}

void QtInputQueue::KeyDown(int fsKey)
{
	if(FSKEY_NULL==fsKey || FSKEY_NUM_KEYCODE<=fsKey)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	keyState_[fsKey]=1;
	pendingKeyCode_.push_back(fsKey);
}

void QtInputQueue::KeyUp(int fsKey)
{
	if(FSKEY_NULL==fsKey || FSKEY_NUM_KEYCODE<=fsKey)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	keyState_[fsKey]=0;
}

void QtInputQueue::CharInput(unsigned int ch)
{
	if(0==ch)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	pendingCharCode_.push_back(ch);
}

void QtInputQueue::MousePress(int button,int view_x,int view_y,int emu_x,int emu_y)
{
	std::lock_guard<std::mutex> lock(mutex_);
	MouseEvent ev;
	ev.mx=emu_x;
	ev.my=emu_y;
	if(MOUSE_BTN_LEFT==button)
	{
		ev.evt=FSMOUSEEVENT_LBUTTONDOWN;
		ev.lb=1;
		lastMouse_.lb=1;
	}
	else if(MOUSE_BTN_RIGHT==button)
	{
		ev.evt=FSMOUSEEVENT_RBUTTONDOWN;
		ev.rb=1;
		lastMouse_.rb=1;
	}
	else if(MOUSE_BTN_MIDDLE==button)
	{
		ev.evt=FSMOUSEEVENT_MBUTTONDOWN;
		ev.mb=1;
		lastMouse_.mb=1;
	}
	else
	{
		return;
	}
	lastMouse_.mx=emu_x;
	lastMouse_.my=emu_y;
	lastViewMouseX_=view_x;
	lastViewMouseY_=view_y;
	pendingMouse_.push_back(ev);
}

void QtInputQueue::MouseRelease(int button,int view_x,int view_y,int emu_x,int emu_y)
{
	std::lock_guard<std::mutex> lock(mutex_);
	MouseEvent ev;
	ev.mx=emu_x;
	ev.my=emu_y;
	if(MOUSE_BTN_LEFT==button)
	{
		ev.evt=FSMOUSEEVENT_LBUTTONUP;
		lastMouse_.lb=0;
	}
	else if(MOUSE_BTN_RIGHT==button)
	{
		ev.evt=FSMOUSEEVENT_RBUTTONUP;
		lastMouse_.rb=0;
	}
	else if(MOUSE_BTN_MIDDLE==button)
	{
		ev.evt=FSMOUSEEVENT_MBUTTONUP;
		lastMouse_.mb=0;
	}
	else
	{
		return;
	}
	lastMouse_.mx=emu_x;
	lastMouse_.my=emu_y;
	lastViewMouseX_=view_x;
	lastViewMouseY_=view_y;
	pendingMouse_.push_back(ev);
}

void QtInputQueue::MouseMove(int view_x,int view_y,int emu_x,int emu_y)
{
	std::lock_guard<std::mutex> lock(mutex_);
	lastMouse_.mx=emu_x;
	lastMouse_.my=emu_y;
	lastViewMouseX_=view_x;
	lastViewMouseY_=view_y;
}

void QtInputQueue::PollMouseState(bool lb,bool mb,bool rb,int view_x,int view_y,int emu_x,int emu_y)
{
	std::lock_guard<std::mutex> lock(mutex_);
	lastMouse_.lb=lb ? 1 : 0;
	lastMouse_.mb=mb ? 1 : 0;
	lastMouse_.rb=rb ? 1 : 0;
	lastMouse_.mx=emu_x;
	lastMouse_.my=emu_y;
	lastViewMouseX_=view_x;
	lastViewMouseY_=view_y;
}

void QtInputQueue::DrainTo(std::vector<unsigned int> &keyCode,
                           std::vector<unsigned int> &charCode,
                           unsigned char keyState[FSKEY_NUM_KEYCODE],
                           std::vector<MouseEvent> &mouseEvents,
                           MouseEvent &lastMouse,
                           int &lastViewMouseX,int &lastViewMouseY,
                           int &winWid,int &winHei,
                           int &emuWid,int &emuHei,
                           int &displayX,int &displayY,int &displayW,int &displayH,
                           bool &resetDiffMouse)
{
	std::lock_guard<std::mutex> lock(mutex_);
	winWid=viewWid_;
	winHei=viewHei_;
	emuWid=emuWid_;
	emuHei=emuHei_;
	displayX=displayX_;
	displayY=displayY_;
	displayW=displayW_;
	displayH=displayH_;
	keyCode=pendingKeyCode_;
	pendingKeyCode_.clear();
	charCode=pendingCharCode_;
	pendingCharCode_.clear();
	for(int i=0; i<FSKEY_NUM_KEYCODE; ++i)
	{
		keyState[i]=keyState_[i];
	}
	mouseEvents=pendingMouse_;
	pendingMouse_.clear();
	lastMouse=lastMouse_;
	lastViewMouseX=lastViewMouseX_;
	lastViewMouseY=lastViewMouseY_;
	resetDiffMouse=resetDiffMouse_;
	resetDiffMouse_=false;
}

void QtInputQueue::RequestCursorWarp(int view_x,int view_y)
{
	pendingWarpX_.store(view_x,std::memory_order_relaxed);
	pendingWarpY_.store(view_y,std::memory_order_relaxed);
	pendingWarp_.store(true,std::memory_order_release);
}

bool QtInputQueue::TakeCursorWarp(int &view_x,int &view_y)
{
	if(true!=pendingWarp_.load(std::memory_order_acquire))
	{
		return false;
	}
	view_x=pendingWarpX_.load(std::memory_order_relaxed);
	view_y=pendingWarpY_.load(std::memory_order_relaxed);
	pendingWarp_.store(false,std::memory_order_release);
	return true;
}

void QtInputQueue::CancelCursorWarp()
{
	pendingWarp_.store(false,std::memory_order_release);
}
