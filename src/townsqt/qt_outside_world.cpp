#include "qt_outside_world.h"

#include "qt_sync_sound.h"
#include "townsdef.h"
#include "ysgamepad.h"

#include <algorithm>
#include <cmath>

QtOutsideWorld::QtOutsideWorld(QtInputQueue *inputQueue,SharedRgbaFramebuffer *framebuffer)
	: inputQueue_(inputQueue),framebuffer_(framebuffer)
{
	SetKeyboardMode(TOWNS_KEYBOARD_MODE_DIRECT);
	SetKeyboardLayout(KEYBOARD_LAYOUT_JP);
}

Outside_World::WindowInterface *QtOutsideWorld::CreateWindowInterface(void) const
{
	return new QtWindowConnection(const_cast<QtOutsideWorld *>(this),inputQueue_,framebuffer_);
}

Outside_World::Sound *QtOutsideWorld::CreateSound(void) const
{
	return new QtSyncSoundConnection;
}

void QtOutsideWorld::DeleteSound(Sound *ptr) const
{
	auto *sound=dynamic_cast<QtSyncSoundConnection *>(ptr);
	if(nullptr!=sound)
	{
		delete sound;
	}
}

QtOutsideWorld::QtWindowConnection::QtWindowConnection(QtOutsideWorld *owner,QtInputQueue *inputQueue,SharedRgbaFramebuffer *framebuffer)
	: owner_(owner),inputQueue_(inputQueue),framebuffer_(framebuffer)
{
}

void QtOutsideWorld::QtWindowConnection::Start(void)
{
	winThr.winWid=640;
	winThr.winHei=480;
	diffMouseXY[0]=0;
	diffMouseXY[1]=0;
	diff_mouse_tracking_ready_=false;

	if(true!=winThrEx.gamePadInitialized)
	{
		YsGamePadInitialize();
		winThrEx.gamePadInitialized=true;
	}
	const auto nGameDevs=YsGamePadGetNumDevices();
	if(0<nGameDevs)
	{
		winThrEx.primary.gamePads.resize(nGameDevs);
		for(unsigned int i=0; i<nGameDevs; ++i)
		{
			YsGamePadRead(&winThrEx.primary.gamePads[i],i);
		}
	}
}

void QtOutsideWorld::QtWindowConnection::Stop(void)
{
}

void QtOutsideWorld::QtWindowConnection::Interval(void)
{
	auto enqueueRenderedImage=[this](){
		if(true!=winThr.newImageRendered || nullptr==framebuffer_ || winThr.mostRecentImage.rgba.empty())
		{
			return false;
		}
		SharedRgbaFramebuffer::QueuedFrame frame;
		frame.rgba=std::move(winThr.mostRecentImage.rgba);
		frame.wid=winThr.mostRecentImage.wid;
		frame.hei=winThr.mostRecentImage.hei;
		frame.capture_towns_time=winThr.lastCaptureTownsTime;
		frame.vsync_index=frame.capture_towns_time/TOWNS_RENDERING_FREQUENCY;
		framebuffer_->EnqueueFrame(std::move(frame));
		winThr.mostRecentImage.rgba.clear();
		winThr.newImageRendered=false;
		return true;
	};

	BaseInterval();
	enqueueRenderedImage();
	if(0<VmCaptureQueueDepth())
	{
		BaseInterval();
		enqueueRenderedImage();
	}

	if(nullptr!=inputQueue_)
	{
		std::vector<unsigned int> keyCode;
		std::vector<unsigned int> charCode;
		unsigned char keyState[FSKEY_NUM_KEYCODE];
		std::vector<QtInputQueue::MouseEvent> mouseEvents;
		QtInputQueue::MouseEvent lastMouse;
		int lastViewMouseX=0,lastViewMouseY=0;
		int winWid=640,winHei=480;
		int emuWid=640,emuHei=480;
		int displayX=0,displayY=0,displayW=640,displayH=480;
		bool resetDiffMouse=false;

		inputQueue_->DrainTo(keyCode,charCode,keyState,mouseEvents,lastMouse,
		                     lastViewMouseX,lastViewMouseY,winWid,winHei,
		                     emuWid,emuHei,displayX,displayY,displayW,displayH,
		                     resetDiffMouse);

		winThrEx.primary.winWid=winWid;
		winThrEx.primary.winHei=winHei;
		winThrEx.primary.keyCode=std::move(keyCode);
		winThrEx.primary.charCode=std::move(charCode);
		for(int i=0; i<FSKEY_NUM_KEYCODE; ++i)
		{
			winThrEx.primary.keyState[i]=keyState[i];
		}
		winThrEx.primary.mouseEvents.clear();
		for(const auto &m : mouseEvents)
		{
			FsSimpleWindowConnection::MouseEvent ev;
			ev.evt=m.evt;
			ev.lb=m.lb;
			ev.mb=m.mb;
			ev.rb=m.rb;
			ev.mx=m.mx;
			ev.my=m.my;
			winThrEx.primary.mouseEvents.push_back(ev);
		}
		winThrEx.primary.lastKnownMouse.evt=FSMOUSEEVENT_NONE;
		winThrEx.primary.lastKnownMouse.lb=lastMouse.lb;
		winThrEx.primary.lastKnownMouse.mb=lastMouse.mb;
		winThrEx.primary.lastKnownMouse.rb=lastMouse.rb;
		// lastMouse.mx/my are already emulator image coordinates from EmuView::mapToEmu.
		winThrEx.primary.lastKnownMouse.mx=lastMouse.mx;
		winThrEx.primary.lastKnownMouse.my=lastMouse.my;

		const bool differential=(nullptr!=owner_ && true==owner_->effectiveDifferentialMouseIntegration);
		const bool relative_ptr=(nullptr!=inputQueue_ && true==inputQueue_->RelativePointerActive());

		if(true==differential)
		{
			if(true==relative_ptr)
			{
				double rdx=0.0,rdy=0.0;
				if(true==inputQueue_->TakeRelativeMotion(rdx,rdy))
				{
					// The compositor delivers relative-pointer deltas in physical device pixels,
					// so undo the desktop (HiDPI) scale to get view/logical-pixel motion that
					// matches absolute integration, then map view pixels to emulator pixels.
					const double dpr=inputQueue_->DevicePixelRatio();
					const double invDpr=(0.0<dpr) ? (1.0/dpr) : 1.0;
					rdx*=invDpr;
					rdy*=invDpr;
					if(0<displayW && 0<displayH && 0<emuWid && 0<emuHei)
					{
						winThrEx.primary.mouseMoveXY[0]+=
						    static_cast<int>(std::lround(rdx*static_cast<double>(emuWid)/static_cast<double>(displayW)));
						winThrEx.primary.mouseMoveXY[1]+=
						    static_cast<int>(std::lround(rdy*static_cast<double>(emuHei)/static_cast<double>(displayH)));
					}
					else
					{
						winThrEx.primary.mouseMoveXY[0]+=static_cast<int>(std::lround(rdx));
						winThrEx.primary.mouseMoveXY[1]+=static_cast<int>(std::lround(rdy));
					}
				}
				diff_mouse_tracking_ready_=false;
				inputQueue_->CancelCursorWarp();
			}
			else
			{
			const int mx=lastViewMouseX;
			const int my=lastViewMouseY;

			if(true==resetDiffMouse || true!=diff_mouse_tracking_ready_ ||
			   true!=prev_effective_differential_)
			{
				diffMouseXY[0]=mx;
				diffMouseXY[1]=my;
				diff_mouse_tracking_ready_=true;
			}
			else
			{
				const int rawDx=mx-diffMouseXY[0];
				const int rawDy=my-diffMouseXY[1];
				if(0<displayW && 0<displayH && 0<emuWid && 0<emuHei)
				{
					winThrEx.primary.mouseMoveXY[0]+=rawDx*emuWid/displayW;
					winThrEx.primary.mouseMoveXY[1]+=rawDy*emuHei/displayH;
				}
				else
				{
					winThrEx.primary.mouseMoveXY[0]+=rawDx;
					winThrEx.primary.mouseMoveXY[1]+=rawDy;
				}
				diffMouseXY[0]=mx;
				diffMouseXY[1]=my;
			}

			const int minX=winWid/4;
			const int minY=winHei/4;
			const int maxX=winWid*3/4;
			const int maxY=winHei*3/4;

			if(mx<minX || my<minY || maxX<mx || maxY<my)
			{
				const int cx=winWid/2;
				const int cy=winHei/2;

				diffMouseXY[0]=cx;
				diffMouseXY[1]=cy;
				diff_mouse_tracking_ready_=false;
				inputQueue_->RequestCursorWarp(cx,cy);
			}
			}
		}
		else
		{
			diff_mouse_tracking_ready_=false;
			if(true==prev_effective_differential_)
			{
				inputQueue_->CancelCursorWarp();
			}
			if(nullptr!=inputQueue_)
			{
				inputQueue_->ClearRelativeMotion();
			}
		}
		prev_effective_differential_=differential;

		{
			std::lock_guard<std::mutex> lock(renderingLock);
			// lastKnownMouse is already in emulator image coordinates.
			shared.scalingX=100;
			shared.scalingY=100;
			shared.dx=0;
			shared.dy=0;
			shared.displayW=emuWid;
			shared.displayH=emuHei;
		}
	}

	if(true!=winThrEx.gamePadInitialized)
	{
		YsGamePadInitialize();
		winThrEx.gamePadInitialized=true;
	}
	{
		const auto nGameDevs=YsGamePadGetNumDevices();
		if(nGameDevs!=static_cast<int>(winThrEx.primary.gamePads.size()))
		{
			winThrEx.primary.gamePads.resize(nGameDevs);
		}
		for(unsigned int i=0; i<winThrEx.primary.gamePads.size(); ++i)
		{
			YsGamePadRead(&winThrEx.primary.gamePads[i],i);
		}
	}
	PollGamePads();

	{
		std::lock_guard<std::mutex> lock(deviceStateLock);
		winThr.VMClosed=shared.VMClosedFromVMThread;
		winThr.gamePadsNeedUpdate=shared.gamePadsNeedUpdate;

		// Mouse position is polled every interval — always propagate even when discrete
		// events are still waiting in readyToSend for the VM thread to consume.
		sharedEx.readyToSend.lastKnownMouse=winThrEx.primary.lastKnownMouse;
		sharedEx.readyToSend.winWid=winThrEx.primary.winWid;
		sharedEx.readyToSend.winHei=winThrEx.primary.winHei;

		if(nullptr!=owner_ && true==owner_->effectiveDifferentialMouseIntegration)
		{
			sharedEx.readyToSend.mouseMoveXY[0]+=winThrEx.primary.mouseMoveXY[0];
			sharedEx.readyToSend.mouseMoveXY[1]+=winThrEx.primary.mouseMoveXY[1];
			winThrEx.primary.mouseMoveXY[0]=0;
			winThrEx.primary.mouseMoveXY[1]=0;
		}
		else
		{
			sharedEx.readyToSend.mouseMoveXY[0]=0;
			sharedEx.readyToSend.mouseMoveXY[1]=0;
		}

		if(true==sharedEx.readyToSend.EventEmpty())
		{
			sharedEx.readyToSend.keyCode=std::move(winThrEx.primary.keyCode);
			sharedEx.readyToSend.charCode=std::move(winThrEx.primary.charCode);
			for(int i=0; i<FSKEY_NUM_KEYCODE; ++i)
			{
				sharedEx.readyToSend.keyState[i]=winThrEx.primary.keyState[i];
			}
			sharedEx.readyToSend.mouseEvents=std::move(winThrEx.primary.mouseEvents);
			sharedEx.readyToSend.gamePads=winThrEx.primary.gamePads;
			winThrEx.primary.CleanUpEvents();
		}
		else
		{
			sharedEx.readyToSend.keyCode.insert(
			    sharedEx.readyToSend.keyCode.end(),
			    winThrEx.primary.keyCode.begin(),
			    winThrEx.primary.keyCode.end());
			sharedEx.readyToSend.charCode.insert(
			    sharedEx.readyToSend.charCode.end(),
			    winThrEx.primary.charCode.begin(),
			    winThrEx.primary.charCode.end());
			for(int i=0; i<FSKEY_NUM_KEYCODE; ++i)
			{
				sharedEx.readyToSend.keyState[i]=winThrEx.primary.keyState[i];
			}
			sharedEx.readyToSend.mouseEvents.insert(
			    sharedEx.readyToSend.mouseEvents.end(),
			    winThrEx.primary.mouseEvents.begin(),
			    winThrEx.primary.mouseEvents.end());
			sharedEx.readyToSend.gamePads=winThrEx.primary.gamePads;
			winThrEx.primary.keyCode.clear();
			winThrEx.primary.charCode.clear();
			winThrEx.primary.mouseEvents.clear();
		}
	}
}

void QtOutsideWorld::QtWindowConnection::Render(bool /*swapBuffers*/)
{
}

void QtOutsideWorld::QtWindowConnection::UpdateImage(TownsRender::ImageCopy &img)
{
	winThr.newImageRendered=true;
	if(nullptr!=framebuffer_)
	{
		framebuffer_->StageFromImage(std::move(img));
	}
	else
	{
		std::lock_guard<std::mutex> lock(renderingLock);
		winThr.mostRecentImage=std::move(img);
	}
}
