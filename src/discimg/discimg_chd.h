#ifndef DISCIMG_CHD_IS_INCLUDED
#define DISCIMG_CHD_IS_INCLUDED
/* { */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <libchdr/chd.h>

#include <mutex>

struct DiscImageChdTrack
{
	unsigned int track_type=0;
	unsigned int sector_length=2352;
	unsigned int start_hsg=0;
	unsigned int end_hsg=0;
	uint64_t location_in_file=0;
	unsigned int pregap=0;
	unsigned int postgap=0;
	unsigned int padframes=0;
	unsigned int chd_frames=0;
	unsigned int extraframes=0;
	unsigned int pgdatasize=0;
};

class DiscImageChdBackend
{
public:
	DiscImageChdBackend();
	~DiscImageChdBackend();

	DiscImageChdBackend(const DiscImageChdBackend &)=delete;
	DiscImageChdBackend &operator=(const DiscImageChdBackend &)=delete;

	bool Open(const std::string &fName,std::string &error_message);
	void Close();

	chd_file *Handle(void) const
	{
		return chd_;
	}

	bool Read(uint64_t offset,unsigned char *buf,size_t len) const;

	uint64_t TotalBytes(void) const
	{
		return total_bytes_;
	}
	uint32_t BytesPerFrame(void) const
	{
		return bytes_per_frame_;
	}

private:
	chd_file *chd_=nullptr;
	uint32_t bytes_per_frame_=2448;
	uint32_t sectors_per_hunk_=0;
	uint32_t hunk_bytes_=0;
	uint64_t total_bytes_=0;

	mutable uint32_t cached_hunk_=0xffffffff;
	mutable std::vector<unsigned char> hunk_cache_;
	/*! Serializes CHD hunk cache + chd_read (CDDA async GetWave vs data ReadSector). */
	mutable std::mutex io_mutex_;

	bool ReadHunk(uint32_t hunk) const;
};

bool DiscImageParseChdTracks(
    chd_file *chd,
    uint32_t bytes_per_frame,
    bool &need_audio_byte_swap,
    std::vector<DiscImageChdTrack> &tracks_out,
    uint64_t &total_bin_length_out,
    std::string &error_message);

/* } */
#endif
