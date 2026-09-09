#include "shared_rgba_framebuffer.h"

bool SharedRgbaFramebuffer::FramePixelBytesOk(const std::vector<unsigned char> &rgba,unsigned int wid,unsigned int hei)
{
	if(0==wid || 0==hei || rgba.empty())
	{
		return false;
	}
	const size_t need=static_cast<size_t>(wid)*static_cast<size_t>(hei)*4u;
	return rgba.size()>=need;
}

void SharedRgbaFramebuffer::SetPresentCallback(std::function<void()> callback)
{
	std::lock_guard<std::mutex> lock(mutex_);
	present_callback_=std::move(callback);
}

void SharedRgbaFramebuffer::NotifyPresent()
{
	std::function<void()> callback;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		callback=present_callback_;
	}
	if(callback)
	{
		callback();
	}
}

void SharedRgbaFramebuffer::StageRgba(std::vector<unsigned char> rgba,unsigned int wid,unsigned int hei)
{
	if(true!=FramePixelBytesOk(rgba,wid,hei))
	{
		return;
	}

	const size_t need=static_cast<size_t>(wid)*static_cast<size_t>(hei)*4u;
	if(rgba.size()!=need)
	{
		rgba.resize(need);
	}

	latest_=std::move(rgba);
	wid_=wid;
	hei_=hei;
	++serial_;
}

void SharedRgbaFramebuffer::EnqueueFrame(QueuedFrame &&frame)
{
	if(true!=FramePixelBytesOk(frame.rgba,frame.wid,frame.hei))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		if(kMaxQueueDepth<=queue_.size())
		{
			queue_.pop_front();
		}
		queue_.push_back(std::move(frame));
	}
}

bool SharedRgbaFramebuffer::PresentOneDueFrame(uint64_t due_index,uint64_t *presented_vsync_index)
{
	QueuedFrame frame;
	bool presented=false;

	{
		std::lock_guard<std::mutex> lock(mutex_);
		// After LoadState, townsTime can jump backward while queued frames still carry
		// the old (larger) vsync_index. Those would block the queue forever.
		// Normal pacing only leads by a frame or two; anything further is stale.
		constexpr uint64_t kMaxVsyncLead=4;
		while(!queue_.empty() && queue_.front().vsync_index>due_index+kMaxVsyncLead)
		{
			queue_.pop_front();
		}
		if(!queue_.empty() && queue_.front().vsync_index<=due_index)
		{
			frame=std::move(queue_.front());
			queue_.pop_front();
			if(true==FramePixelBytesOk(frame.rgba,frame.wid,frame.hei))
			{
				StageRgba(std::move(frame.rgba),frame.wid,frame.hei);
				presented=true;
				if(nullptr!=presented_vsync_index)
				{
					*presented_vsync_index=frame.vsync_index;
				}
			}
		}
	}

	return presented;
}

size_t SharedRgbaFramebuffer::QueueDepth() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.size();
}

uint64_t SharedRgbaFramebuffer::FrontVsyncIndex() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if(true==queue_.empty())
	{
		return 0;
	}
	return queue_.front().vsync_index;
}

void SharedRgbaFramebuffer::ClearQueue()
{
	std::lock_guard<std::mutex> lock(mutex_);
	queue_.clear();
}

void SharedRgbaFramebuffer::StageFromImage(TownsRender::ImageCopy &&img)
{
	if(true!=FramePixelBytesOk(img.rgba,img.wid,img.hei))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		StageRgba(std::move(img.rgba),img.wid,img.hei);
	}
	NotifyPresent();
}

void SharedRgbaFramebuffer::StageFromImage(const TownsRender::ImageCopy &img)
{
	if(true!=FramePixelBytesOk(img.rgba,img.wid,img.hei))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		StageRgba(img.rgba,img.wid,img.hei);
	}
	NotifyPresent();
}

bool SharedRgbaFramebuffer::PeekLatest(unsigned int *wid,unsigned int *hei,uint64_t *serial) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if(0==wid_ || 0==hei_ || latest_.empty())
	{
		return false;
	}
	if(nullptr!=wid)
	{
		*wid=wid_;
	}
	if(nullptr!=hei)
	{
		*hei=hei_;
	}
	if(nullptr!=serial)
	{
		*serial=serial_;
	}
	return true;
}

bool SharedRgbaFramebuffer::CopyLatest(std::vector<unsigned char> *rgba,unsigned int *wid,unsigned int *hei,uint64_t *serial) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if(0==wid_ || 0==hei_ || latest_.empty())
	{
		return false;
	}
	if(nullptr!=rgba)
	{
		*rgba=latest_;
	}
	if(nullptr!=wid)
	{
		*wid=wid_;
	}
	if(nullptr!=hei)
	{
		*hei=hei_;
	}
	if(nullptr!=serial)
	{
		*serial=serial_;
	}
	return true;
}

uint64_t SharedRgbaFramebuffer::Serial() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return serial_;
}
