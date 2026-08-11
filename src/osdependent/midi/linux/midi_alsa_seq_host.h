#ifndef MIDI_ALSA_SEQ_HOST_IS_INCLUDED
#define MIDI_ALSA_SEQ_HOST_IS_INCLUDED
/* { */

namespace MidiAlsaSeqHost
{
	bool IsDriverAvailable(void);
	bool TryInitialize(void);
	bool IsActive(void);
	const char *BackendName(void);

	int AllocateOutputPort(void);
	/*! Destination as "client:port". Empty clears and falls back to subscribers. */
	void SetDestination(const char *id);
	void SendShortMessage(int port,const unsigned char cmdBuf[3]);
	void SendExclusiveMessage(int port,const unsigned char *data,int len);
}

/* } */
#endif
