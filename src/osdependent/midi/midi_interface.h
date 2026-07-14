#ifndef MIDI_INTERFACE_IS_INCLUDED
#define MIDI_INTERFACE_IS_INCLUDED
/* { */

class MIDI_Interface
{
public:
	static MIDI_Interface *Create(void);
	static void Delete(MIDI_Interface *);
	virtual void SendCommand(const unsigned char cmdBuf[])=0;
	virtual void SendExclusiveCommand(const unsigned char cmdBuf[],int len)=0;
	/*! MPU-401 / i8251 reset hook: clear host synth state (FluidSynth SC-55 restore). */
	virtual void ResetPlaybackState(void) {}
};

/* } */
#endif
