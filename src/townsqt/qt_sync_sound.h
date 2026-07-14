#ifndef QT_SYNC_SOUND_IS_INCLUDED
#define QT_SYNC_SOUND_IS_INCLUDED

#include "outside_world.h"
#include "towns_spsc_pcm.h"

#include <atomic>
#include <cstdint>
#include <vector>

struct ma_context;
struct ma_device;

class QtSyncSoundConnection : public Outside_World::Sound
{
public:
	void Start(void) override;
	void Stop(void) override;
	void Polling(void) override;

	void CDDAPlay(const DiscImage &discImg,DiscImage::MinSecFrm from,DiscImage::MinSecFrm to,bool repeat,unsigned int,unsigned int) override;
	void CDDASetVolume(float leftVol,float rightVol) override;
	void CDDAStop(void) override;
	void CDDAPause(void) override;
	void CDDAResume(void) override;
	bool CDDAIsPlaying(void) override;
	DiscImage::MinSecFrm CDDACurrentPosition(void) override;

	void FMPCMPlay(std::vector<unsigned char> &wave) override;
	void FMPCMPlayStop(void) override;
	bool FMPCMChannelPlaying(void) override;
	void SyncPcmContract(unsigned long long towns_time_ns) override;
	void PrepareAudioSleep(unsigned long long sleep_time_ns) override;
	double PlaybackBufferMillisec() const override;

	void BeepPlay(int samplingRate,std::vector<unsigned char> &wave) override;
	void BeepPlayStop(void) override;
	bool BeepChannelPlaying() const override;

	/*! Restart miniaudio output (sample rate / backend / device). Thread-safe request. */
	void RequestRestartOutput(void);

private:
	static constexpr int kDefaultSampleRate=44100;
	static constexpr int kRingMs=250;
	static constexpr int kHighWaterMs=80;
	static constexpr int kDrainChunkFrames=512;
	static constexpr int kPrimePeriods=3;
	static constexpr int kTargetPeriods=5;
	static constexpr int kMaxPeriods=8;
	static constexpr int kCallbackSpinIters=4096;

	void FillAudio(int16_t *stream,int frame_count);
	void MixBeep(int16_t *stream,int frame_count);
	void CatchUpContract();
	void ResyncPcmContract();
	int ContractFramesDue() const;
	int DrainPendingToSpsc(int target_frames);
	int PrimeSpscSilence(int frames);
	size_t PendingFrames() const;
	void CompactPendingIfNeeded();

	static void DataCallback(ma_device *device,void *output,const void *input,unsigned int frame_count);

	TownsSpscPcmRing spsc_;
	void *device_=nullptr;
	ma_context *context_=nullptr;
	bool context_active_=false;
	bool device_started_=false;
	int sample_rate_=kDefaultSampleRate;
	int period_frames_=512;
	int target_spsc_frames_=1536;
	int max_spsc_frames_=3072;
	int primed_frames_remaining_=0;

	uint64_t contract_ns_=0;
	std::atomic<int64_t> played_frames_{0};

	std::vector<int16_t> pending_pcm_;
	size_t pending_read_frame_=0;

	std::vector<int16_t> beep_samples_;
	std::atomic<size_t> beep_pos_{0};
	std::atomic<bool> beep_active_{false};

	std::vector<int16_t> cdda_samples_;
	std::atomic<size_t> cdda_pos_{0};
	std::atomic<bool> cdda_active_{false};
	float cdda_vol_l_=1.0f;
	float cdda_vol_r_=1.0f;
	unsigned long long cdda_start_hsg_=0;

	std::atomic<bool> restart_output_requested_{false};
};

#endif
