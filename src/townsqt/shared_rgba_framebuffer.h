#ifndef SHARED_RGBA_FRAMEBUFFER_IS_INCLUDED
#define SHARED_RGBA_FRAMEBUFFER_IS_INCLUDED

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "render.h"

/*! Thread-safe RGBA staging for Qt EmuView (m88 SharedFramebufferDraw pattern). */
class SharedRgbaFramebuffer
{
public:
	struct QueuedFrame
	{
		std::vector<unsigned char> rgba;
		unsigned int wid=0;
		unsigned int hei=0;
		uint64_t capture_towns_time=0;
		uint64_t vsync_index=0;
	};

	void EnqueueFrame(QueuedFrame &&frame);

	/*! Present the oldest queued frame if its vsync_index <= due_index (at most one).
	    If presented, writes the frame vsync_index to presented_vsync_index. */
	bool PresentOneDueFrame(uint64_t due_index,uint64_t *presented_vsync_index);

	void StageFromImage(TownsRender::ImageCopy &&img);
	void StageFromImage(const TownsRender::ImageCopy &img);

	/*! GUI thread: metadata only (no pixel pointer — buffers may be recycled). */
	bool PeekLatest(unsigned int *wid,unsigned int *hei,uint64_t *serial) const;

	/*! GUI thread: copy latest pixels under the mutex so Present/Stage cannot free them. */
	bool CopyLatest(std::vector<unsigned char> *rgba,unsigned int *wid,unsigned int *hei,uint64_t *serial) const;

	uint64_t Serial() const;
	size_t QueueDepth() const;
	/*! Oldest queued frame VSYNC index, or 0 if empty. */
	uint64_t FrontVsyncIndex() const;

	/*! Drop all pending vsync-timed frames (call after LoadState / townsTime jump). */
	void ClearQueue();

	/*! Thread-safe; may be called from the VM thread. */
	void SetPresentCallback(std::function<void()> callback);

private:
	static constexpr size_t kMaxQueueDepth=3;

	static bool FramePixelBytesOk(const std::vector<unsigned char> &rgba,unsigned int wid,unsigned int hei);
	void StageRgba(std::vector<unsigned char> rgba,unsigned int wid,unsigned int hei);
	void NotifyPresent();

	std::function<void()> present_callback_;
	mutable std::mutex mutex_;
	std::deque<QueuedFrame> queue_;
	std::vector<unsigned char> latest_;
	unsigned int wid_ = 0;
	unsigned int hei_ = 0;
	uint64_t serial_ = 0;
};

#endif
