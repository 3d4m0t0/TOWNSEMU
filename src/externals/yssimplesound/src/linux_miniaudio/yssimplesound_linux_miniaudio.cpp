#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "yssimplesound.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace
{
class MixMutexGuard
{
public:
	explicit MixMutexGuard(std::mutex &m) : mtx_(m) { mtx_.lock(); }
	~MixMutexGuard() { mtx_.unlock(); }
private:
	std::mutex &mtx_;
};
}

class YsSoundPlayer::APISpecificData
{
public:
	class PlayingSound
	{
	public:
		SoundData *dat=nullptr;
		unsigned int ptr=0;
		int balance=0;
		YSBOOL loop=YSFALSE;
	};

	class PlayingStream
	{
	public:
		Stream *dat=nullptr;
		unsigned int ptr=0;
		int balance=0;
	};

	std::vector <PlayingSound> playing;
	std::vector <PlayingStream> playingStream;

	ma_device device{};
	bool deviceStarted=false;
	std::mutex mtx;
	unsigned int nChannel=2;
	unsigned int rate=44100;

	APISpecificData();
	~APISpecificData();
	void CleanUp(void);
	YSRESULT Start(void);
	YSRESULT End(void);
	void Render(ma_int16 *out,ma_uint32 frameCount);
	void DiscardEnded(void);
	double GetCurrentPosition(const SoundData &dat) const;
	static void DataCallback(ma_device *device,void *output,const void *input,ma_uint32 frameCount);
};

class YsSoundPlayer::Stream::APISpecificData
{
public:
	SoundData playing,standBy;
};

class YsSoundPlayer::SoundData::APISpecificDataPerSoundData
{
public:
	APISpecificDataPerSoundData()=default;
	~APISpecificDataPerSoundData()=default;
	void CleanUp(void) {}
};

void YsSoundPlayer::APISpecificData::DataCallback(ma_device *device,void *output,const void *,ma_uint32 frameCount)
{
	if(nullptr==device || nullptr==device->pUserData || nullptr==output)
	{
		return;
	}
	auto *api=static_cast<APISpecificData *>(device->pUserData);
	api->Render(static_cast<ma_int16 *>(output),frameCount);
}

YsSoundPlayer::APISpecificData::APISpecificData()
{
	std::memset(&device,0,sizeof(device));
}

YsSoundPlayer::APISpecificData::~APISpecificData()
{
	CleanUp();
}

void YsSoundPlayer::APISpecificData::CleanUp(void)
{
	if(true==deviceStarted)
	{
		ma_device_stop(&device);
		ma_device_uninit(&device);
		deviceStarted=false;
	}
	playing.clear();
	playingStream.clear();
}

YSRESULT YsSoundPlayer::APISpecificData::Start(void)
{
	if(true==deviceStarted)
	{
		return YSOK;
	}

	ma_device_config config=ma_device_config_init(ma_device_type_playback);
	config.playback.format=ma_format_s16;
	config.playback.channels=2;
	config.sampleRate=44100;
	config.dataCallback=APISpecificData::DataCallback;
	config.pUserData=this;

	if(MA_SUCCESS!=ma_device_init(nullptr,&config,&device))
	{
		std::fprintf(stderr,"YsSoundPlayer: miniaudio device init failed.\n");
		return YSERR;
	}

	nChannel=device.playback.channels;
	rate=device.sampleRate;

	if(MA_SUCCESS!=ma_device_start(&device))
	{
		std::fprintf(stderr,"YsSoundPlayer: miniaudio device start failed.\n");
		ma_device_uninit(&device);
		return YSERR;
	}

	deviceStarted=true;
	std::printf("YsSoundPlayer started (miniaudio, %u Hz).\n",rate);
	return YSOK;
}

YSRESULT YsSoundPlayer::APISpecificData::End(void)
{
	CleanUp();
	return YSOK;
}

void YsSoundPlayer::APISpecificData::DiscardEnded(void)
{
	for(long long int i=playing.size()-1; 0<=i; --i)
	{
		if(nullptr==playing[i].dat)
		{
			playing[i]=playing.back();
			playing.pop_back();
		}
	}
}

void YsSoundPlayer::APISpecificData::Render(ma_int16 *out,ma_uint32 frameCount)
{
	MixMutexGuard guard(mtx);
	std::memset(out,0,sizeof(ma_int16)*frameCount*nChannel);

	auto mixSample=[&](int &mixL,int &mixR,const SoundData *wav,unsigned int wavPtr)
	{
		if(nullptr==wav || wavPtr>=wav->NTimeStep())
		{
			return;
		}
		const int inCh1=std::max(1,wav->GetNumChannel())-1;
		const int l=wav->GetSignedValue16(0,wavPtr);
		const int r=wav->GetSignedValue16(inCh1,wavPtr);
		const float volL=wav->playBackVolumeLeft;
		const float volR=wav->playBackVolumeRight;
		mixL+=static_cast<int>(l*volL);
		mixR+=static_cast<int>(r*volR);
	};

	auto advancePtr=[&](unsigned int &wavPtr,int &balance,const SoundData *wav)
	{
		if(nullptr==wav)
		{
			return;
		}
		balance-=static_cast<int>(wav->PlayBackRate());
		while(balance<0)
		{
			balance+=static_cast<int>(rate);
			++wavPtr;
		}
	};

	for(ma_uint32 frame=0; frame<frameCount; ++frame)
	{
		int mixL=0,mixR=0;

		for(auto &p : playing)
		{
			if(nullptr!=p.dat)
			{
				if(p.ptr<p.dat->NTimeStep())
				{
					mixSample(mixL,mixR,p.dat,p.ptr);
				}
				advancePtr(p.ptr,p.balance,p.dat);
				if(p.ptr>=p.dat->NTimeStep())
				{
					if(YSTRUE==p.loop)
					{
						p.ptr=0;
						p.balance=0;
					}
					else
					{
						p.dat=nullptr;
						p.ptr=0;
					}
				}
			}
		}

		for(auto &p : playingStream)
		{
			if(nullptr==p.dat)
			{
				continue;
			}
			SoundData *wav=nullptr;
			if(0<p.dat->api->playing.NTimeStep())
			{
				wav=&p.dat->api->playing;
			}
			else if(0<p.dat->api->standBy.NTimeStep())
			{
				p.dat->api->playing.CopyFrom(p.dat->api->standBy);
				p.dat->api->standBy.CleanUp();
				p.ptr=0;
				p.balance=0;
				wav=&p.dat->api->playing;
			}
			if(nullptr!=wav && p.ptr<wav->NTimeStep())
			{
				mixSample(mixL,mixR,wav,p.ptr);
				advancePtr(p.ptr,p.balance,wav);
				if(p.ptr>=wav->NTimeStep())
				{
					if(0<p.dat->api->standBy.NTimeStep())
					{
						p.dat->api->playing.CopyFrom(p.dat->api->standBy);
						p.dat->api->standBy.CleanUp();
						p.ptr=0;
						p.balance=0;
					}
					else
					{
						p.dat->api->playing.CleanUp();
						p.ptr=0;
						p.balance=0;
					}
				}
			}
		}

		mixL=std::clamp(mixL,-32768,32767);
		mixR=std::clamp(mixR,-32768,32767);
		out[frame*nChannel+0]=static_cast<ma_int16>(mixL);
		if(1<nChannel)
		{
			out[frame*nChannel+1]=static_cast<ma_int16>(mixR);
		}
	}

	DiscardEnded();
}

double YsSoundPlayer::APISpecificData::GetCurrentPosition(const SoundData &dat) const
{
	for(const auto &p : playing)
	{
		if(&dat==p.dat)
		{
			return static_cast<double>(p.ptr)/static_cast<double>(dat.PlayBackRate());
		}
	}
	return 0.0;
}

////////////////////////////////////////////////////////////

YsSoundPlayer::APISpecificData *YsSoundPlayer::CreateAPISpecificData(void)
{
	return new APISpecificData;
}

void YsSoundPlayer::DeleteAPISpecificData(APISpecificData *ptr)
{
	delete ptr;
}

YSRESULT YsSoundPlayer::StartAPISpecific(void)
{
	return api->Start();
}

YSRESULT YsSoundPlayer::EndAPISpecific(void)
{
	return api->End();
}

void YsSoundPlayer::KeepPlayingAPISpecific(void)
{
}

YSRESULT YsSoundPlayer::PlayOneShotAPISpecific(SoundData &dat)
{
	MixMutexGuard guard(api->mtx);
	for(const auto &p : api->playing)
	{
		if(p.dat==&dat)
		{
			return YSOK;
		}
	}
	APISpecificData::PlayingSound p;
	p.dat=&dat;
	p.ptr=0;
	p.balance=0;
	p.loop=YSFALSE;
	api->playing.push_back(p);
	return YSOK;
}

YSRESULT YsSoundPlayer::PlayBackgroundAPISpecific(SoundData &dat)
{
	MixMutexGuard guard(api->mtx);
	for(const auto &p : api->playing)
	{
		if(p.dat==&dat)
		{
			return YSOK;
		}
	}
	APISpecificData::PlayingSound p;
	p.dat=&dat;
	p.ptr=0;
	p.balance=0;
	p.loop=YSTRUE;
	api->playing.push_back(p);
	return YSOK;
}

YSBOOL YsSoundPlayer::IsPlayingAPISpecific(const SoundData &dat) const
{
	MixMutexGuard guard(api->mtx);
	for(const auto &p : api->playing)
	{
		if(&dat==p.dat)
		{
			return YSTRUE;
		}
	}
	return YSFALSE;
}

double YsSoundPlayer::GetCurrentPositionAPISpecific(const SoundData &dat) const
{
	MixMutexGuard guard(api->mtx);
	return api->GetCurrentPosition(dat);
}

void YsSoundPlayer::StopAPISpecific(SoundData &dat)
{
	MixMutexGuard guard(api->mtx);
	for(auto &p : api->playing)
	{
		if(&dat==p.dat)
		{
			p.ptr=0;
			p.dat=nullptr;
		}
	}
	api->DiscardEnded();
}

void YsSoundPlayer::PauseAPISpecific(SoundData &)
{
}

void YsSoundPlayer::ResumeAPISpecific(SoundData &)
{
}

void YsSoundPlayer::SetVolumeAPISpecific(SoundData &dat,float leftVol,float rightVol)
{
	dat.playBackVolumeLeft=leftVol;
	dat.playBackVolumeRight=rightVol;
}

////////////////////////////////////////////////////////////

YsSoundPlayer::SoundData::APISpecificDataPerSoundData *YsSoundPlayer::SoundData::CreateAPISpecificData(void)
{
	return new APISpecificDataPerSoundData;
}

void YsSoundPlayer::SoundData::DeleteAPISpecificData(APISpecificDataPerSoundData *ptr)
{
	delete ptr;
}

bool YsSoundPlayer::SoundData::IsPrepared(YsSoundPlayer &)
{
	return prepared;
}

YSRESULT YsSoundPlayer::SoundData::PreparePlay(YsSoundPlayer &player)
{
	Resample(player.api->rate);
	ConvertToMono();
	ConvertTo16Bit();
	ConvertToSigned();
	return YSOK;
}

void YsSoundPlayer::SoundData::CleanUpAPISpecific(void)
{
	api->CleanUp();
}

////////////////////////////////////////////////////////////

YsSoundPlayer::Stream::APISpecificData *YsSoundPlayer::Stream::CreateAPISpecificData(void)
{
	return new APISpecificData;
}

void YsSoundPlayer::Stream::DeleteAPISpecificData(APISpecificData *ptr)
{
	delete ptr;
}

YSRESULT YsSoundPlayer::StartStreamingAPISpecific(Stream &stream,StreamingOption)
{
	MixMutexGuard guard(api->mtx);
	for(const auto &playingStream : api->playingStream)
	{
		if(playingStream.dat==&stream)
		{
			return YSOK;
		}
	}
	APISpecificData::PlayingStream newStream;
	newStream.dat=&stream;
	newStream.ptr=0;
	newStream.balance=0;
	api->playingStream.push_back(newStream);
	return YSOK;
}

void YsSoundPlayer::StopStreamingAPISpecific(Stream &stream)
{
	MixMutexGuard guard(api->mtx);
	for(int i=0; i<static_cast<int>(api->playingStream.size()); ++i)
	{
		if(api->playingStream[i].dat==&stream)
		{
			api->playingStream[i]=api->playingStream.back();
			api->playingStream.pop_back();
		}
	}
}

YSBOOL YsSoundPlayer::StreamPlayerReadyToAcceptNextNumSampleAPISpecific(const Stream &stream,unsigned int) const
{
	MixMutexGuard guard(api->mtx);
	if(0==stream.api->standBy.NTimeStep())
	{
		return YSTRUE;
	}
	return YSFALSE;
}

YSRESULT YsSoundPlayer::AddNextStreamingSegmentAPISpecific(Stream &stream,const SoundData &dat)
{
	MixMutexGuard guard(api->mtx);
	if(0==stream.api->playing.NTimeStep())
	{
		stream.api->playing.CopyFrom(dat);
		return YSOK;
	}
	if(0==stream.api->standBy.NTimeStep())
	{
		stream.api->standBy.CopyFrom(dat);
		return YSOK;
	}
	return YSERR;
}
