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
	/*! Override auto preference (Kind::None restores Fluid-then-ALSA auto order). */
	void SetUserPreference(Kind kind);
	Kind UserPreference(void);
	bool IsFluidSynthLibraryAvailable(void);
	bool IsAlsaSequencerAvailable(void);
}

/* } */
#endif
