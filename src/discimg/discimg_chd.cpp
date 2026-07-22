#include "discimg_chd.h"

#include <libchdr/cdrom.h>
#include <libchdr/chd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
constexpr unsigned int kCdTrackPadding=4;
constexpr unsigned int kTrackAudio=3;
constexpr unsigned int kTrackMode2=2;
constexpr unsigned int kTrackMode1=1;
constexpr unsigned int kLeadInFrames=150;

unsigned int SectorLengthFromType(const char type[])
{
	if(0==strcmp(type,"AUDIO"))
	{
		return 2352;
	}
	if(0==strcmp(type,"MODE1") || 0==strcmp(type,"MODE1/2048"))
	{
		return 2048;
	}
	if(0==strcmp(type,"MODE1_RAW") || 0==strcmp(type,"MODE1/2352"))
	{
		return 2352;
	}
	if(0==strcmp(type,"MODE2") || 0==strcmp(type,"MODE2/2336"))
	{
		return 2336;
	}
	if(0==strcmp(type,"MODE2_FORM1") || 0==strcmp(type,"MODE2/2048"))
	{
		return 2048;
	}
	if(0==strcmp(type,"MODE2_FORM2") || 0==strcmp(type,"MODE2/2324"))
	{
		return 2324;
	}
	if(0==strcmp(type,"MODE2_RAW") || 0==strcmp(type,"MODE2/2352") || 0==strcmp(type,"CDI/2352"))
	{
		return 2352;
	}
	return 0;
}

unsigned int TrackTypeFromName(const char type[])
{
	if(0==strcmp(type,"AUDIO"))
	{
		return kTrackAudio;
	}
	if(0==strcmp(type,"MODE2") || 0==strcmp(type,"MODE2/2336") ||
	   0==strcmp(type,"MODE2_FORM1") || 0==strcmp(type,"MODE2/2048") ||
	   0==strcmp(type,"MODE2_FORM2") || 0==strcmp(type,"MODE2/2324") ||
	   0==strcmp(type,"MODE2_RAW") || 0==strcmp(type,"MODE2/2352") || 0==strcmp(type,"CDI/2352"))
	{
		return kTrackMode2;
	}
	return kTrackMode1;
}

unsigned int SectorLengthFromChdTrackType(unsigned int trktype,unsigned int datasize)
{
	if(0<datasize)
	{
		return datasize;
	}
	switch(trktype)
	{
	case CD_TRACK_AUDIO:
		return 2352;
	case CD_TRACK_MODE1:
	case CD_TRACK_MODE2_FORM1:
		return 2048;
	case CD_TRACK_MODE2:
	case CD_TRACK_MODE2_FORM_MIX:
		return 2336;
	case CD_TRACK_MODE2_FORM2:
		return 2324;
	default:
		return 2352;
	}
}

unsigned int TrackTypeFromChdTrackType(unsigned int trktype)
{
	switch(trktype)
	{
	case CD_TRACK_AUDIO:
		return kTrackAudio;
	case CD_TRACK_MODE2:
	case CD_TRACK_MODE2_FORM1:
	case CD_TRACK_MODE2_FORM2:
	case CD_TRACK_MODE2_FORM_MIX:
	case CD_TRACK_MODE2_RAW:
		return kTrackMode2;
	default:
		return kTrackMode1;
	}
}

uint32_t ReadMetadataU32(const char metadata[],unsigned int offset)
{
	if(offset+sizeof(uint32_t)>512)
	{
		return 0;
	}
	uint32_t value=0;
	std::memcpy(&value,metadata+offset,sizeof(value));
	return value;
}

bool ParseChdOldMetadata(
    const char metadata[],uint32_t meta_len,
    std::vector<DiscImageChdTrack> &tracks_out,
    std::string &error_message)
{
	if(meta_len<sizeof(uint32_t))
	{
		error_message="CHD old metadata too short";
		return false;
	}

	unsigned int num_tracks=ReadMetadataU32(metadata,0);
	if(0==num_tracks || CD_MAX_TRACKS<num_tracks)
	{
		// Some older CHDs store the count in opposite endianness.
		const unsigned int swapped=
		    ((num_tracks & 0x000000ffu) << 24) |
		    ((num_tracks & 0x0000ff00u) << 8) |
		    ((num_tracks & 0x00ff0000u) >> 8) |
		    ((num_tracks & 0xff000000u) >> 24);
		if(0<swapped && CD_MAX_TRACKS>=swapped)
		{
			num_tracks=swapped;
		}
		else
		{
			error_message="CHD old metadata has invalid track count";
			return false;
		}
	}

	const unsigned int words_per_track=6;
	const unsigned int required_bytes=sizeof(uint32_t)+num_tracks*words_per_track*sizeof(uint32_t);
	if(meta_len<required_bytes)
	{
		error_message="CHD old metadata truncated";
		return false;
	}

	unsigned int offset=sizeof(uint32_t);
	for(unsigned int i=0; i<num_tracks; ++i)
	{
		const unsigned int trktype=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);
		const unsigned int subtype=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);
		(void)subtype;
		const unsigned int datasize=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);
		const unsigned int subsize=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);
		(void)subsize;
		const unsigned int frames=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);
		const unsigned int extraframes=ReadMetadataU32(metadata,offset);
		offset+=sizeof(uint32_t);

		if(0==frames)
		{
			error_message="CHD old metadata track has no frames";
			return false;
		}

		const unsigned int sector_length=SectorLengthFromChdTrackType(trktype,datasize);
		if(0==sector_length)
		{
			error_message="Unsupported CHD old metadata track type";
			return false;
		}

		DiscImageChdTrack track;
		track.track_type=TrackTypeFromChdTrackType(trktype);
		track.sector_length=sector_length;
		track.start_hsg=0;
		track.end_hsg=frames-1;
		track.location_in_file=0;
		track.pregap=0;
		track.postgap=0;
		track.padframes=0;
		track.chd_frames=frames;
		track.extraframes=extraframes;
		tracks_out.push_back(track);
	}

	return true;
}

void FinalizeChdTrackLayout(
    std::vector<DiscImageChdTrack> &tracks,
    uint32_t bytes_per_frame,
    uint64_t &total_bin_length_out)
{
	unsigned int logofs=0;
	unsigned int chdofs=0;

	for(size_t track_index=0; track_index<tracks.size(); ++track_index)
	{
		auto &track=tracks[track_index];
		if(0==track.pgdatasize)
		{
			logofs+=track.pregap;
		}

		track.start_hsg=logofs;
		unsigned int logframes=track.chd_frames;
		if(0==track_index && 0==track.pgdatasize && 0<track.pregap && track.chd_frames>track.pregap)
		{
			logframes=track.chd_frames-track.pregap;
		}
		track.end_hsg=(0<logframes ? track.start_hsg+logframes-1-track.padframes : track.start_hsg);
		track.location_in_file=static_cast<uint64_t>(chdofs)*bytes_per_frame;

		logofs+=track.postgap;
		logofs+=track.chd_frames;

		const unsigned int padded=(track.chd_frames+kCdTrackPadding-1)/kCdTrackPadding*kCdTrackPadding;
		chdofs+=padded;
	}

	if(false==tracks.empty() && tracks.front().start_hsg<kLeadInFrames)
	{
		const unsigned int shift=kLeadInFrames-tracks.front().start_hsg;
		for(auto &track : tracks)
		{
			track.start_hsg+=shift;
			track.end_hsg+=shift;
		}
	}

	// Match .CUE addressing where track 1 starts at HSG 0 while the CHD stream
	// still contains the 2-second lead-in at the beginning of the data.
	if(false==tracks.empty() && kLeadInFrames==tracks.front().start_hsg)
	{
		tracks.front().start_hsg=0;
	}

	total_bin_length_out=static_cast<uint64_t>(chdofs)*bytes_per_frame;
}
}

DiscImageChdBackend::DiscImageChdBackend()
{
}

DiscImageChdBackend::~DiscImageChdBackend()
{
	Close();
}

void DiscImageChdBackend::Close(void)
{
	if(nullptr!=chd_)
	{
		chd_close(chd_);
		chd_=nullptr;
	}
	cached_hunk_=0xffffffff;
	hunk_cache_.clear();
}

bool DiscImageChdBackend::Open(const std::string &fName,std::string &error_message)
{
	Close();

	const chd_error err=chd_open(fName.c_str(),CHD_OPEN_READ,nullptr,&chd_);
	if(CHDERR_NONE!=err || nullptr==chd_)
	{
		if(CHDERR_REQUIRES_PARENT==err)
		{
			error_message="CHD requires parent image";
		}
		else
		{
			error_message=chd_error_string(err);
		}
		Close();
		return false;
	}

	const chd_header *header=chd_get_header(chd_);
	if(nullptr==header)
	{
		error_message="CHD header missing";
		Close();
		return false;
	}

	hunk_bytes_=header->hunkbytes;
	bytes_per_frame_=CD_FRAME_SIZE;
	if(0<hunk_bytes_ && 0==header->unitbytes)
	{
		if(0==hunk_bytes_%CD_FRAME_SIZE)
		{
			bytes_per_frame_=CD_FRAME_SIZE;
		}
		else if(0==hunk_bytes_%CD_MAX_SECTOR_DATA)
		{
			bytes_per_frame_=CD_MAX_SECTOR_DATA;
		}
	}
	else if(CD_FRAME_SIZE==header->unitbytes || CD_MAX_SECTOR_DATA==header->unitbytes)
	{
		bytes_per_frame_=header->unitbytes;
	}
	else if(0<header->unitbytes)
	{
		bytes_per_frame_=header->unitbytes;
	}

	if(0==bytes_per_frame_ || 0!=hunk_bytes_%bytes_per_frame_)
	{
		error_message="Unsupported CHD sector layout";
		Close();
		return false;
	}

	sectors_per_hunk_=hunk_bytes_/bytes_per_frame_;
	hunk_cache_.resize(hunk_bytes_);
	cached_hunk_=0xffffffff;
	total_bytes_=header->logicalbytes;
	if(0==total_bytes_ && 0<header->totalhunks)
	{
		total_bytes_=static_cast<uint64_t>(header->totalhunks)*hunk_bytes_;
	}

	if(0==total_bytes_)
	{
		error_message="CHD contains no data";
		Close();
		return false;
	}

	return true;
}

bool DiscImageParseChdTracks(
    chd_file *chd,
    uint32_t bytes_per_frame,
    bool &need_audio_byte_swap,
    std::vector<DiscImageChdTrack> &tracks_out,
    uint64_t &total_bin_length_out,
    std::string &error_message)
{
	tracks_out.clear();
	need_audio_byte_swap=false;
	if(nullptr==chd || 0==bytes_per_frame)
	{
		error_message="Invalid CHD track parser state";
		return false;
	}

	for(unsigned int index=0;; ++index)
	{
		char metadata[512];
		uint32_t meta_len=0,meta_tag=0;
		uint8_t meta_flags=0;
		char type[32]={0},subtype[32]={0},pgtype[32]={0},pgsub[32]={0};
		int track_id=-1,frames=0,pregap=0,postgap=0,padframes=0;
		bool is_gdrom=false;

		chd_error meta_err=chd_get_metadata(
		    chd,CDROM_TRACK_METADATA2_TAG,index,metadata,sizeof(metadata),
		    &meta_len,&meta_tag,&meta_flags);
		if(CHDERR_NONE==meta_err)
		{
			if(8!=std::sscanf(metadata,CDROM_TRACK_METADATA2_FORMAT,
			                  &track_id,type,subtype,&frames,&pregap,pgtype,pgsub,&postgap))
			{
				error_message="Invalid CHD track metadata";
				return false;
			}
		}
		else
		{
			meta_err=chd_get_metadata(
			    chd,CDROM_TRACK_METADATA_TAG,index,metadata,sizeof(metadata),
			    &meta_len,&meta_tag,&meta_flags);
			if(CHDERR_NONE==meta_err)
			{
				if(4!=std::sscanf(metadata,CDROM_TRACK_METADATA_FORMAT,
				                  &track_id,type,subtype,&frames))
				{
					error_message="Invalid CHD track metadata";
					return false;
				}
			}
			else
			{
				meta_err=chd_get_metadata(
				    chd,GDROM_TRACK_METADATA_TAG,index,metadata,sizeof(metadata),
				    &meta_len,&meta_tag,&meta_flags);
				if(CHDERR_NONE==meta_err)
				{
					is_gdrom=true;
					if(9!=std::sscanf(metadata,GDROM_TRACK_METADATA_FORMAT,
					                  &track_id,type,subtype,&frames,&padframes,&pregap,pgtype,pgsub,&postgap))
					{
						error_message="Invalid GD-ROM CHD track metadata";
						return false;
					}
				}
				else
				{
					break;
				}
			}
		}

		if(track_id!=(int)tracks_out.size()+1)
		{
			error_message="Unexpected CHD track number";
			return false;
		}
		if(0>=frames)
		{
			error_message="CHD track has no frames";
			return false;
		}

		const unsigned int sector_length=SectorLengthFromType(type);
		if(0==sector_length)
		{
			error_message=std::string("Unsupported CHD track type: ")+type;
			return false;
		}

		DiscImageChdTrack track;
		track.track_type=TrackTypeFromName(type);
		track.sector_length=sector_length;
		track.pregap=static_cast<unsigned int>(pregap);
		track.postgap=static_cast<unsigned int>(postgap);
		track.padframes=static_cast<unsigned int>(padframes);
		track.chd_frames=static_cast<unsigned int>(frames);
		track.extraframes=0;
		track.pgdatasize=0;
		if(0<track.pregap && 'V'==pgtype[0])
		{
			const unsigned int pg_sector_length=SectorLengthFromType(pgtype+1);
			if(0<pg_sector_length)
			{
				track.pgdatasize=pg_sector_length;
			}
		}
		tracks_out.push_back(track);

		if(is_gdrom)
		{
			need_audio_byte_swap=true;
		}
	}

	if(tracks_out.empty())
	{
		char metadata[512];
		uint32_t meta_len=0;
		const chd_error old_err=chd_get_metadata(
		    chd,CDROM_OLD_METADATA_TAG,0,metadata,sizeof(metadata),&meta_len,nullptr,nullptr);
		if(CHDERR_NONE!=old_err || true!=ParseChdOldMetadata(metadata,meta_len,tracks_out,error_message))
		{
			if(tracks_out.empty())
			{
				error_message="CHD contains no tracks";
			}
			return false;
		}
	}

	FinalizeChdTrackLayout(tracks_out,bytes_per_frame,total_bin_length_out);
	return true;
}

bool DiscImageChdBackend::ReadHunk(uint32_t hunk) const
{
	if(hunk==cached_hunk_)
	{
		return true;
	}
	if(nullptr==chd_ || hunk_cache_.size()<hunk_bytes_)
	{
		return false;
	}
	if(CHDERR_NONE!=chd_read(chd_,hunk,hunk_cache_.data()))
	{
		return false;
	}
	cached_hunk_=hunk;
	return true;
}

bool DiscImageChdBackend::Read(uint64_t offset,unsigned char *buf,size_t len) const
{
	if(nullptr==buf || 0==len || nullptr==chd_ || 0==bytes_per_frame_ || 0==sectors_per_hunk_)
	{
		return false;
	}

	// Lock per hunk so a long CDDA GetWave does not stall MODE1/2 sector reads
	// (and the VM thread) for the entire audio range.
	size_t written=0;
	while(written<len)
	{
		const uint64_t byte_pos=offset+written;
		const uint64_t frame=byte_pos/bytes_per_frame_;
		const uint32_t frame_byte_ofs=static_cast<uint32_t>(byte_pos%bytes_per_frame_);
		const uint32_t hunk=static_cast<uint32_t>(frame/sectors_per_hunk_);
		const uint32_t hunk_sector_ofs=static_cast<uint32_t>(frame%sectors_per_hunk_);

		const size_t src_ofs=static_cast<size_t>(hunk_sector_ofs)*bytes_per_frame_+frame_byte_ofs;
		const size_t avail=bytes_per_frame_-frame_byte_ofs;
		const size_t to_copy=std::min(len-written,avail);

		{
			std::lock_guard<std::mutex> lock(io_mutex_);
			if(true!=ReadHunk(hunk))
			{
				return false;
			}
			std::memcpy(buf+written,hunk_cache_.data()+src_ofs,to_copy);
		}
		written+=to_copy;
	}
	return true;
}
