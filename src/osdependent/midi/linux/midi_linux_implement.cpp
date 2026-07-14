#include "../midi_interface.h"

#include "midi_alsa_seq_host.h"
#include "midi_fluidsynth_host.h"

#include <cstdio>

namespace
{
enum class LinuxMidiBackend
{
	Unprobed,
	Null,
	FluidSynth,
	AlsaSeq
};

LinuxMidiBackend g_backend=LinuxMidiBackend::Unprobed;

void ProbeBackend(void)
{
	if(LinuxMidiBackend::Unprobed!=g_backend)
	{
		return;
	}
	if(true==MidiFluidSynthHost::TryInitialize())
	{
		g_backend=LinuxMidiBackend::FluidSynth;
		return;
	}
	if(true==MidiAlsaSeqHost::TryInitialize())
	{
		g_backend=LinuxMidiBackend::AlsaSeq;
		return;
	}
	std::fprintf(stderr,"TownsEMU MIDI: no host backend available (FluidSynth / ALSA sequencer).\n");
	g_backend=LinuxMidiBackend::Null;
}

class MIDI_Null final : public MIDI_Interface
{
public:
	void SendCommand(const unsigned char cmdBuf[]) override
	{
		(void)cmdBuf;
	}
	void SendExclusiveCommand(const unsigned char cmdBuf[],int len) override
	{
		(void)cmdBuf;
		(void)len;
	}
};

class MIDI_FluidSynth final : public MIDI_Interface
{
public:
	void SendCommand(const unsigned char cmdBuf[]) override
	{
		MidiFluidSynthHost::SendShortMessage(cmdBuf);
	}
	void SendExclusiveCommand(const unsigned char cmdBuf[],int len) override
	{
		MidiFluidSynthHost::SendExclusiveMessage(cmdBuf,len);
	}
	void ResetPlaybackState(void) override
	{
		MidiFluidSynthHost::ResetPlaybackState();
	}
};

class MIDI_AlsaSeq final : public MIDI_Interface
{
public:
	MIDI_AlsaSeq()
	{
		port_=MidiAlsaSeqHost::AllocateOutputPort();
	}
	void SendCommand(const unsigned char cmdBuf[]) override
	{
		MidiAlsaSeqHost::SendShortMessage(port_,cmdBuf);
	}
	void SendExclusiveCommand(const unsigned char cmdBuf[],int len) override
	{
		MidiAlsaSeqHost::SendExclusiveMessage(port_,cmdBuf,len);
	}

private:
	int port_=-1;
};
}

MIDI_Interface *MIDI_Interface::Create(void)
{
	ProbeBackend();
	switch(g_backend)
	{
	default:
	case LinuxMidiBackend::Null:
		return new MIDI_Null;
	case LinuxMidiBackend::FluidSynth:
		return new MIDI_FluidSynth;
	case LinuxMidiBackend::AlsaSeq:
		return new MIDI_AlsaSeq;
	}
}

void MIDI_Interface::Delete(MIDI_Interface *itfc)
{
	delete itfc;
}
