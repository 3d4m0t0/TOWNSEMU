#ifndef MIDI_FLUIDSYNTH_HOST_IS_INCLUDED
#define MIDI_FLUIDSYNTH_HOST_IS_INCLUDED
/* { */

#include <cstdint>

namespace MidiFluidSynthHost
{
	/*! Try to load libfluidsynth and create a synth instance. Safe to call once at startup. */
	bool TryInitialize(void);

	bool IsLibraryAvailable(void);
	bool IsActive(void);
	const char *BackendName(void);

	/*! TownsQt: mix rendered PCM in the miniaudio callback instead of opening a FluidSynth audio driver. */
	void SetMixInApplicationAudio(bool enabled);

	void SetSampleRate(int sample_rate_hz);
	void SetSoundFontPath(const char *path);
	void SetMasterVolumePercent(int percent);

	void SendShortMessage(const unsigned char cmdBuf[3]);
	void SendExclusiveMessage(const unsigned char *data,int len);

	/*! Clear sounding notes and SC-55 routing state (GS reset, song boundary). */
	void ResetPlaybackState(void);

	/*! Add FluidSynth output into interleaved stereo S16 (called from audio thread). */
	void MixInterleavedS16(int16_t *stream,int frame_count);
}

/* } */
#endif
