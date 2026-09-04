#include "shared_rgba_framebuffer.h"

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
	if(0==wid || 0==hei || rgba.empty())
	{
		return;
	}

	const int write_index=1-read_index_;
	auto &dst=buffers_[write_index];
	dst=std::move(rgba);
	wid_=wid;
	hei_=hei;
	read_index_=write_index;
	++serial_;
}

void SharedRgbaFramebuffer::EnqueueFrame(QueuedFrame &&frame)
{
	if(0==frame.wid || 0==frame.hei || frame.rgba.empty())
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
			StageRgba(std::move(frame.rgba),frame.wid,frame.hei);
			presented=true;
			if(nullptr!=presented_vsync_index)
			{
				*presented_vsync_index=frame.vsync_index;
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
	if(0==img.wid || 0==img.hei || img.rgba.empty())
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
	if(0==img.wid || 0==img.hei || img.rgba.empty())
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		StageRgba(img.rgba,img.wid,img.hei);
	}
	NotifyPresent();
}

bool SharedRgbaFramebuffer::Acquire(const unsigned char **rgba,unsigned int *wid,unsigned int *hei,uint64_t *serial) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if(0==wid_ || 0==hei_ || buffers_[read_index_].empty())
	{
		return false;
	}
	if(nullptr!=rgba)
	{
		*rgba=buffers_[read_index_].data();
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
