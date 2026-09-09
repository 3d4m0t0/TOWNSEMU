#include "qt_sync_sound.h"

#include "cpputil.h"
#include "towns_audio_period.h"
#include "townsqt_miniaudio_devices.h"
#include "townsqt_settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#endif

#if defined(__linux__)
#include "linux/midi_fluidsynth_host.h"
#endif

#include "miniaudio.h"

namespace
{
void TryRaiseAudioThreadPriority()
{
#if defined(__linux__)
	static std::once_flag once;
	std::call_once(once,[](){
		sched_param sp{};
		sp.sched_priority=2;
		if(0!=pthread_setschedparam(pthread_self(),SCHED_FIFO,&sp))
		{
			setpriority(PRIO_PROCESS,0,-10);
		}
	});
#endif
}

int Clamp16(int v)
{
	if(v<-32768)
	{
		return -32768;
	}
	if(32767<v)
	{
		return 32767;
	}
	return v;
}

void AppendSigned16Stereo(const unsigned char *src,size_t src_frames,std::vector<int16_t> &dst)
{
	if(nullptr==src || 0==src_frames)
	{
		return;
	}

	const size_t old=dst.size();
	dst.resize(old+src_frames*2);
	for(size_t i=0; i<src_frames; ++i)
	{
		dst[old+i*2]=static_cast<int16_t>(cpputil::GetSignedWord(src+i*4));
		dst[old+i*2+1]=static_cast<int16_t>(cpputil::GetSignedWord(src+i*4+2));
	}
}
}

void QtSyncSoundConnection::DataCallback(ma_device *device,void *output,const void *,unsigned int frame_count)
{
	if(nullptr==device || nullptr==output)
	{
		return;
	}
	auto *self=static_cast<QtSyncSoundConnection *>(device->pUserData);
	if(nullptr==self)
	{
		return;
	}
	self->FillAudio(static_cast<int16_t *>(output),static_cast<int>(frame_count));
}

int QtSyncSoundConnection::ContractFramesDue() const
{
	return static_cast<int>((contract_ns_*static_cast<uint64_t>(sample_rate_))/1000000000ULL);
}

size_t QtSyncSoundConnection::PendingFrames() const
{
	const size_t total_frames=pending_pcm_.size()/2;
	if(pending_read_frame_>=total_frames)
	{
		return 0;
	}
	return total_frames-pending_read_frame_;
}

void QtSyncSoundConnection::CompactPendingIfNeeded()
{
	if(8192<pending_read_frame_ && 0<PendingFrames())
	{
		const size_t remain=PendingFrames();
		std::vector<int16_t> compact(remain*2);
		std::memcpy(compact.data(),pending_pcm_.data()+pending_read_frame_*2,remain*2*sizeof(int16_t));
		pending_pcm_.swap(compact);
		pending_read_frame_=0;
	}
	else if(0==PendingFrames())
	{
		pending_pcm_.clear();
		pending_read_frame_=0;
	}
}

int QtSyncSoundConnection::DrainPendingToSpsc(int target_frames)
{
	if(target_frames<=0 || 0==spsc_.Capacity())
	{
		return 0;
	}

	const int headroom=static_cast<int>(spsc_.Free());
	if(headroom<=0)
	{
		return 0;
	}
	target_frames=std::min(target_frames,headroom);

	int pushed=0;
	while(pushed<target_frames && 0<PendingFrames())
	{
		const int want=std::min({target_frames-pushed,kDrainChunkFrames,
		                          static_cast<int>(PendingFrames()),
		                          static_cast<int>(spsc_.Free())});
		if(want<=0)
		{
			break;
		}
		const size_t n=spsc_.Push(pending_pcm_.data()+pending_read_frame_*2,static_cast<size_t>(want));
		if(0==n)
		{
			break;
		}
		pending_read_frame_+=n;
		pushed+=static_cast<int>(n);
	}
	CompactPendingIfNeeded();
	return pushed;
}

int QtSyncSoundConnection::PrimeSpscSilence(int frames)
{
	if(frames<=0 || 0==spsc_.Capacity())
	{
		return 0;
	}

	int16_t silence[kDrainChunkFrames*2]={};
	int left=frames;
	int primed=0;
	while(0<left && 0<spsc_.Free())
	{
		const int chunk=std::min(left,kDrainChunkFrames);
		const size_t pushed=spsc_.Push(silence,static_cast<size_t>(chunk));
		if(0==pushed)
		{
			break;
		}
		left-=static_cast<int>(pushed);
		primed+=static_cast<int>(pushed);
	}
	return primed;
}

void QtSyncSoundConnection::CatchUpContract()
{
	if(0==spsc_.Capacity())
	{
		return;
	}

	const int due=ContractFramesDue();
	const int played=static_cast<int>(played_frames_.load(std::memory_order_relaxed));
	const int in_spsc=static_cast<int>(spsc_.Avail());
	const int pending=static_cast<int>(PendingFrames());
	const int backlog=due-played-in_spsc;

	int push_goal=0;
	if(0<backlog)
	{
		int cap=period_frames_;
		// Refill faster when the ring is starved; rate-limit only when buffered.
		if(in_spsc<period_frames_ && 0<pending)
		{
			cap=std::max(cap,target_spsc_frames_);
		}
		push_goal=std::min(backlog,cap);
	}
	else if(0<pending && in_spsc<target_spsc_frames_)
	{
		// pending_pcm_ is not in backlog; move staged audio when contract looks caught up.
		push_goal=std::min({pending,period_frames_,target_spsc_frames_-in_spsc});
	}

	if(0<push_goal)
	{
		DrainPendingToSpsc(push_goal);
	}
	else if(0<pending)
	{
		// Staged audio is not in the contract backlog; drain whenever the ring has headroom
		// so pending_pcm_ cannot wedge FMPCMChannelPlaying and block ProcessSound synthesis.
		const int spsc_free=static_cast<int>(spsc_.Free());
		if(0<spsc_free)
		{
			const int drain=std::min({pending,period_frames_,spsc_free});
			if(0<drain)
			{
				DrainPendingToSpsc(drain);
			}
		}
	}
}

void QtSyncSoundConnection::PrepareAudioSleep(unsigned long long sleep_time_ns)
{
	if(0==sleep_time_ns || 0==spsc_.Capacity())
	{
		return;
	}

	const int sleep_frames=static_cast<int>((sleep_time_ns*static_cast<uint64_t>(sample_rate_))/1000000000ULL);
	if(sleep_frames<=0)
	{
		return;
	}

	const int due=ContractFramesDue();
	const int played=static_cast<int>(played_frames_.load(std::memory_order_relaxed));
	const int in_spsc=static_cast<int>(spsc_.Avail());
	const int backlog=due+sleep_frames-played-in_spsc;
	if(0<backlog)
	{
		// Cover the upcoming sleep; do not cap to one DAC period.
		const int push_goal=std::min(backlog,sleep_frames+period_frames_);
		DrainPendingToSpsc(push_goal);
	}
}

double QtSyncSoundConnection::PlaybackBufferMillisec() const
{
	if(0==spsc_.Capacity())
	{
		return 0.0;
	}
	return static_cast<double>(spsc_.Avail())*1000.0/static_cast<double>(sample_rate_);
}

void QtSyncSoundConnection::ResyncPcmContract()
{
	played_frames_.store(ContractFramesDue(),std::memory_order_relaxed);
	primed_frames_remaining_=0;
}

void QtSyncSoundConnection::SyncPcmContract(unsigned long long towns_time_ns)
{
	contract_ns_=towns_time_ns;
	CatchUpContract();
}

void QtSyncSoundConnection::Start(void)
{
	sample_rate_=kDefaultSampleRate;
#if defined(__linux__)
	MidiFluidSynthHost::SetSampleRate(sample_rate_);
#endif
	const QString backend_q=TownsQtSettings::audioBackend();
	const QString device_q=TownsQtSettings::audioDevice();
	const QByteArray backend_utf8=backend_q.toUtf8();
	const QByteArray device_utf8=device_q.toUtf8();
	const char *backend_name=backend_utf8.constData();
	const char *device_name=device_utf8.constData();

	const size_t ring_frames=static_cast<size_t>(sample_rate_)*kRingMs/1000;
	if(true!=spsc_.Init(ring_frames))
	{
		std::fprintf(stderr,"Tsugaru_QT: SPSC ring init failed.\n");
		return;
	}

	period_frames_=static_cast<int>(TownsAudioPeriodFrames(sample_rate_));
	target_spsc_frames_=period_frames_*kTargetPeriods;
	max_spsc_frames_=period_frames_*kMaxPeriods;

	pending_pcm_.clear();
	pending_read_frame_=0;
	contract_ns_=0;
	played_frames_.store(0,std::memory_order_relaxed);
	primed_frames_remaining_=0;

	auto *dev=new ma_device;
	std::memset(dev,0,sizeof(*dev));

	ma_device_config config=ma_device_config_init(ma_device_type_playback);
	config.playback.format=ma_format_s16;
	config.playback.channels=2;
	config.sampleRate=static_cast<ma_uint32>(sample_rate_);
	config.periodSizeInFrames=static_cast<ma_uint32>(period_frames_);
	config.dataCallback=DataCallback;
	config.pUserData=this;

	ma_context stack_context{};
	ma_device_id chosen_id{};
	const ma_device_id *device_id=nullptr;
	ma_context *init_context=nullptr;

	if(true!=TownsQtMiniaudioDevices::IsAutoBackend(backend_name))
	{
		if(true!=TownsQtMiniaudioDevices::InitContext(backend_name,&stack_context))
		{
			std::fprintf(stderr,"Tsugaru_QT: miniaudio context init failed (backend=%s).\n",
			             backend_name[0] ? backend_name : "auto");
			delete dev;
			return;
		}
		context_=new ma_context(stack_context);
		context_active_=true;
		init_context=context_;

		if('\0'!=device_name[0])
		{
			if(true!=TownsQtMiniaudioDevices::ResolvePlaybackIdInContext(context_,device_name,&chosen_id))
			{
				std::fprintf(stderr,"Tsugaru_QT: audio device not found: %s (backend=%s)\n",
				             device_name,backend_name);
				ma_context_uninit(context_);
				delete context_;
				context_=nullptr;
				context_active_=false;
				delete dev;
				return;
			}
			device_id=&chosen_id;
		}
	}
	config.playback.pDeviceID=device_id;

	const ma_result init_result=init_context ?
	    ma_device_init(init_context,&config,dev) :
	    ma_device_init(nullptr,&config,dev);
	if(MA_SUCCESS!=init_result)
	{
		std::fprintf(stderr,"Tsugaru_QT: miniaudio device init failed.\n");
		if(context_active_ && nullptr!=context_)
		{
			ma_context_uninit(context_);
			delete context_;
			context_=nullptr;
			context_active_=false;
		}
		delete dev;
		return;
	}

	if(MA_SUCCESS!=ma_device_start(dev))
	{
		std::fprintf(stderr,"Tsugaru_QT: miniaudio device start failed.\n");
		ma_device_uninit(dev);
		if(context_active_ && nullptr!=context_)
		{
			ma_context_uninit(context_);
			delete context_;
			context_=nullptr;
			context_active_=false;
		}
		delete dev;
		return;
	}

	sample_rate_=static_cast<int>(dev->sampleRate);
	device_=dev;
	device_started_=true;

	const int prime_frames=period_frames_*kPrimePeriods;
	const int primed=PrimeSpscSilence(0<prime_frames ? prime_frames : target_spsc_frames_);
	contract_ns_=0;
	played_frames_.store(0,std::memory_order_relaxed);
	primed_frames_remaining_=primed;

	std::printf("Tsugaru_QT: sync audio started (%u Hz, backend=%s, ring %d ms, period %d frames).\n",
	            dev->sampleRate,
	            backend_name[0] ? backend_name : "auto",
	            kRingMs,period_frames_);
}

void QtSyncSoundConnection::Stop(void)
{
	if(nullptr!=device_)
	{
		auto *dev=static_cast<ma_device *>(device_);
		if(true==device_started_)
		{
			ma_device_stop(dev);
			ma_device_uninit(dev);
			device_started_=false;
		}
		delete dev;
		device_=nullptr;
	}
	if(context_active_ && nullptr!=context_)
	{
		ma_context_uninit(context_);
		delete context_;
		context_=nullptr;
		context_active_=false;
	}
	spsc_.Reset();
	pending_pcm_.clear();
	pending_read_frame_=0;
	contract_ns_=0;
	played_frames_.store(0,std::memory_order_relaxed);
	primed_frames_remaining_=0;
	beep_active_.store(false,std::memory_order_relaxed);
	cdda_active_.store(false,std::memory_order_relaxed);
}

void QtSyncSoundConnection::RequestRestartOutput(void)
{
	restart_output_requested_.store(true,std::memory_order_release);
}

void QtSyncSoundConnection::Polling(void)
{
	if(true==restart_output_requested_.exchange(false,std::memory_order_acq_rel))
	{
		const float vol_l=cdda_vol_l_;
		const float vol_r=cdda_vol_r_;
		Stop();
		Start();
		CDDASetVolume(vol_l,vol_r);
	}
	CatchUpContract();
}

void QtSyncSoundConnection::CDDAPlay(const DiscImage &discImg,DiscImage::MinSecFrm from,DiscImage::MinSecFrm to,bool repeat,unsigned int,unsigned int)
{
	auto wave=discImg.GetWave(from,to);
	const size_t num_samples=wave.size()/4;
	{
		std::lock_guard<std::mutex> lock(mix_aux_mutex_);
		cdda_samples_.clear();
		AppendSigned16Stereo(wave.data(),num_samples,cdda_samples_);
		cdda_pos_.store(0,std::memory_order_relaxed);
		cdda_active_.store(0<cdda_samples_.size()/2,std::memory_order_relaxed);
		cdda_start_hsg_=from.ToHSG();
	}
	(void)repeat;
}

void QtSyncSoundConnection::CDDASetVolume(float leftVol,float rightVol)
{
	cdda_vol_l_=leftVol;
	cdda_vol_r_=rightVol;
}

void QtSyncSoundConnection::CDDAStop(void)
{
	cdda_active_.store(false,std::memory_order_relaxed);
	cdda_pos_.store(0,std::memory_order_relaxed);
}

void QtSyncSoundConnection::CDDAPause(void)
{
	cdda_active_.store(false,std::memory_order_relaxed);
}

void QtSyncSoundConnection::CDDAResume(void)
{
	std::lock_guard<std::mutex> lock(mix_aux_mutex_);
	if(false==cdda_samples_.empty())
	{
		cdda_active_.store(true,std::memory_order_relaxed);
	}
}

bool QtSyncSoundConnection::CDDAIsPlaying(void)
{
	return cdda_active_.load(std::memory_order_relaxed);
}

DiscImage::MinSecFrm QtSyncSoundConnection::CDDACurrentPosition(void)
{
	const size_t pos=cdda_pos_.load(std::memory_order_relaxed);
	const double sec=static_cast<double>(pos)/static_cast<double>(sample_rate_);
	const unsigned long long sec_hsg=static_cast<unsigned long long>(sec*75.0);
	const unsigned long long pos_in_disc=sec_hsg+cdda_start_hsg_;

	DiscImage::MinSecFrm msf;
	msf.FromHSG(pos_in_disc);
	return msf;
}

void QtSyncSoundConnection::FMPCMPlay(std::vector<unsigned char> &wave)
{
	const size_t num_samples=wave.size()/4;
	if(0==num_samples)
	{
		wave.clear();
		return;
	}

	AppendSigned16Stereo(wave.data(),num_samples,pending_pcm_);
	wave.clear();
}

void QtSyncSoundConnection::FMPCMPlayStop(void)
{
	spsc_.Reset();
	pending_pcm_.clear();
	pending_read_frame_=0;
	ResyncPcmContract();
}

bool QtSyncSoundConnection::FMPCMChannelPlaying(void)
{
	// Backpressure for FMPCMPlay only: the SPSC ring is what the DAC consumes.
	// pending_pcm_ is VM staging drained by CatchUpContract; counting it here wedged
	// synthesis while staged audio had not yet reached the ring.
	const int in_spsc=static_cast<int>(spsc_.Avail());
	const int high_water=static_cast<int>(sample_rate_)*kHighWaterMs/1000;
	return in_spsc>=high_water;
}

void QtSyncSoundConnection::BeepPlay(int samplingRate,std::vector<unsigned char> &wave)
{
	const size_t num_samples=wave.size()/4;
	if(0==num_samples)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(mix_aux_mutex_);
	beep_samples_.clear();
	AppendSigned16Stereo(wave.data(),num_samples,beep_samples_);
	beep_pos_.store(0,std::memory_order_relaxed);
	beep_active_.store(0<beep_samples_.size()/2,std::memory_order_relaxed);
	(void)samplingRate;
}

void QtSyncSoundConnection::BeepPlayStop(void)
{
	beep_active_.store(false,std::memory_order_relaxed);
	beep_pos_.store(0,std::memory_order_relaxed);
}

bool QtSyncSoundConnection::BeepChannelPlaying() const
{
	if(true!=beep_active_.load(std::memory_order_relaxed))
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(mix_aux_mutex_);
	return beep_pos_.load(std::memory_order_relaxed)*2<beep_samples_.size();
}

void QtSyncSoundConnection::MixBeep(int16_t *stream,int frame_count)
{
	if(true!=beep_active_.load(std::memory_order_relaxed))
	{
		return;
	}

	std::lock_guard<std::mutex> lock(mix_aux_mutex_);
	size_t pos=beep_pos_.load(std::memory_order_relaxed);
	for(int i=0; i<frame_count; ++i)
	{
		if(pos*2+1>=beep_samples_.size())
		{
			beep_active_.store(false,std::memory_order_relaxed);
			break;
		}
		stream[i*2]=Clamp16(stream[i*2]+beep_samples_[pos*2]);
		stream[i*2+1]=Clamp16(stream[i*2+1]+beep_samples_[pos*2+1]);
		++pos;
	}
	beep_pos_.store(pos,std::memory_order_relaxed);
}

void QtSyncSoundConnection::FillAudio(int16_t *stream,int frame_count)
{
	if(nullptr==stream || frame_count<=0)
	{
		return;
	}

	TryRaiseAudioThreadPriority();

	size_t written=spsc_.Pop(stream,static_cast<size_t>(frame_count));
	int spin=0;
	while(written<static_cast<size_t>(frame_count) && spin<kCallbackSpinIters)
	{
		if((spin++ & 63)==0)
		{
			std::this_thread::yield();
		}
		const size_t more=spsc_.Pop(stream+written*2,static_cast<size_t>(frame_count)-written);
		if(0==more)
		{
			continue;
		}
		written+=more;
		spin=0;
	}
	if(written<static_cast<size_t>(frame_count))
	{
		const size_t missing=static_cast<size_t>(frame_count)-written;
		std::memset(stream+written*2,0,missing*2*sizeof(int16_t));
	}

	if(0<written)
	{
		int countable=static_cast<int>(written);
		if(0<primed_frames_remaining_)
		{
			const int from_prime=std::min(countable,primed_frames_remaining_);
			primed_frames_remaining_-=from_prime;
			countable-=from_prime;
		}
		if(0<countable)
		{
			played_frames_.fetch_add(countable,std::memory_order_relaxed);
		}
	}

	MixBeep(stream,frame_count);

	if(true==cdda_active_.load(std::memory_order_relaxed))
	{
		std::lock_guard<std::mutex> lock(mix_aux_mutex_);
		size_t pos=cdda_pos_.load(std::memory_order_relaxed);
		for(int i=0; i<frame_count; ++i)
		{
			if(pos*2+1>=cdda_samples_.size())
			{
				cdda_active_.store(false,std::memory_order_relaxed);
				break;
			}
			const int l=static_cast<int>(cdda_samples_[pos*2]*cdda_vol_l_);
			const int r=static_cast<int>(cdda_samples_[pos*2+1]*cdda_vol_r_);
			stream[i*2]=Clamp16(stream[i*2]+l);
			stream[i*2+1]=Clamp16(stream[i*2+1]+r);
			++pos;
		}
		cdda_pos_.store(pos,std::memory_order_relaxed);
	}

#if defined(__linux__)
	MidiFluidSynthHost::MixInterleavedS16(stream,frame_count);
#endif
}
