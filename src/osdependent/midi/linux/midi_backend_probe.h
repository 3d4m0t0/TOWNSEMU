#ifndef MIDI_BACKEND_PROBE_IS_INCLUDED
#define MIDI_BACKEND_PROBE_IS_INCLUDED
/* { */

namespace MidiBackendProbe
{
	enum class Kind
	{
		None,
		FluidSynth,
		AlsaSeq
	};

	Kind PreferredBackend(void);
	bool IsFluidSynthLibraryAvailable(void);
	bool IsAlsaSequencerAvailable(void);
}

/* } */
#endif
