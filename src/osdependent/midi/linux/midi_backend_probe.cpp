#include "midi_backend_probe.h"

#include "midi_alsa_seq_host.h"
#include "midi_fluidsynth_host.h"

namespace MidiBackendProbe
{
Kind PreferredBackend(void)
{
	if(true==MidiFluidSynthHost::IsLibraryAvailable())
	{
		return Kind::FluidSynth;
	}
	if(true==MidiAlsaSeqHost::IsDriverAvailable())
	{
		return Kind::AlsaSeq;
	}
	return Kind::None;
}

bool IsFluidSynthLibraryAvailable(void)
{
	return MidiFluidSynthHost::IsLibraryAvailable();
}

bool IsAlsaSequencerAvailable(void)
{
	return MidiAlsaSeqHost::IsDriverAvailable();
}
}
