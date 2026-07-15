#include "midi_alsa_seq_host.h"

#include <alsa/asoundlib.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace
{
std::mutex g_mutex;
snd_seq_t *g_seq=nullptr;
bool g_active=false;
int g_next_port=0;

void DispatchShortMessageLocked(int port,const unsigned char cmdBuf[3])
{
	if(nullptr==g_seq)
	{
		return;
	}
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev,port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);

	const unsigned char status=cmdBuf[0];
	const unsigned char type=status & 0xf0;
	const int chan=status & 0x0f;
	switch(type)
	{
	case 0x80:
		snd_seq_ev_set_noteoff(&ev,chan,cmdBuf[1],cmdBuf[2]);
		break;
	case 0x90:
		if(0==cmdBuf[2])
		{
			snd_seq_ev_set_noteoff(&ev,chan,cmdBuf[1],0);
		}
		else
		{
			snd_seq_ev_set_noteon(&ev,chan,cmdBuf[1],cmdBuf[2]);
		}
		break;
	case 0xa0:
		snd_seq_ev_set_keypress(&ev,chan,cmdBuf[1],cmdBuf[2]);
		break;
	case 0xb0:
		snd_seq_ev_set_controller(&ev,chan,cmdBuf[1],cmdBuf[2]);
		break;
	case 0xc0:
		snd_seq_ev_set_pgmchange(&ev,chan,cmdBuf[1]);
		break;
	case 0xd0:
		snd_seq_ev_set_chanpress(&ev,chan,cmdBuf[1]);
		break;
	case 0xe0:
		{
			const int value=(int(cmdBuf[2])<<7)|int(cmdBuf[1]);
			snd_seq_ev_set_pitchbend(&ev,chan,value);
		}
		break;
	default:
		return;
	}
	snd_seq_event_output(g_seq,&ev);
	snd_seq_drain_output(g_seq);
}

void SendSysexLocked(int port,const unsigned char *data,int len)
{
	if(nullptr==g_seq || nullptr==data || len<=0)
	{
		return;
	}
	std::string sysex;
	sysex.reserve(static_cast<size_t>(len)+2);
	sysex.push_back(static_cast<char>(0xf0));
	sysex.append(reinterpret_cast<const char *>(data),static_cast<size_t>(len));
	sysex.push_back(static_cast<char>(0xf7));

	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev,port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_sysex(&ev,static_cast<unsigned int>(sysex.size()),sysex.data());
	snd_seq_event_output(g_seq,&ev);
	snd_seq_drain_output(g_seq);
}
}

bool MidiAlsaSeqHost::IsDriverAvailable(void)
{
	snd_seq_t *seq=nullptr;
	if(0!=snd_seq_open(&seq,"default",SND_SEQ_OPEN_OUTPUT,0))
	{
		return false;
	}
	snd_seq_close(seq);
	return true;
}

bool MidiAlsaSeqHost::TryInitialize(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(true==g_active)
	{
		return true;
	}
	if(0!=snd_seq_open(&g_seq,"default",SND_SEQ_OPEN_OUTPUT,0))
	{
		return false;
	}
	snd_seq_set_client_name(g_seq,"Tsugaru_QT");
	g_active=true;
	g_next_port=0;
	std::fprintf(stderr,"Tsugaru_QT: using ALSA sequencer.\n");
	return true;
}

bool MidiAlsaSeqHost::IsActive(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_active;
}

const char *MidiAlsaSeqHost::BackendName(void)
{
	return "ALSA Sequencer";
}

int MidiAlsaSeqHost::AllocateOutputPort(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active || nullptr==g_seq)
	{
		return -1;
	}
	char port_name[32];
	std::snprintf(port_name,sizeof(port_name),"MIDI Out %d",g_next_port);
	const int port=snd_seq_create_simple_port(
	    g_seq,
	    port_name,
	    SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_WRITE,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if(0<=port)
	{
		++g_next_port;
	}
	return port;
}

void MidiAlsaSeqHost::SendShortMessage(int port,const unsigned char cmdBuf[3])
{
	if(nullptr==cmdBuf || port<0)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active)
	{
		return;
	}
	DispatchShortMessageLocked(port,cmdBuf);
}

void MidiAlsaSeqHost::SendExclusiveMessage(int port,const unsigned char *data,int len)
{
	if(port<0)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active)
	{
		return;
	}
	SendSysexLocked(port,data,len);
}
