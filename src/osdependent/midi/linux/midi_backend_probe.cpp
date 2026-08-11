#include "midi_backend_probe.h"

#include "midi_alsa_seq_host.h"
#include "midi_fluidsynth_host.h"

namespace MidiBackendProbe
{
Kind g_userPreference=Kind::None;

void SetUserPreference(Kind kind)
{
	g_userPreference=kind;
}

Kind UserPreference(void)
{
	return g_userPreference;
}

Kind PreferredBackend(void)
{
	const auto available=[](Kind kind)
	{
		switch(kind)
		{
		case Kind::FluidSynth:
			return IsFluidSynthLibraryAvailable();
		case Kind::AlsaSeq:
			return IsAlsaSequencerAvailable();
		default:
			return false;
		}
	};

	if(Kind::None!=g_userPreference && true==available(g_userPreference))
	{
		return g_userPreference;
	}
	if(true==IsFluidSynthLibraryAvailable())
	{
		return Kind::FluidSynth;
	}
	if(true==IsAlsaSequencerAvailable())
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
