#include "midi_output_ports.h"

#include <alsa/asoundlib.h>

#include <cstdio>
#include <string>

std::vector<MidiOutputPortEntry> ListAlsaMidiOutputDestinations(void)
{
	std::vector<MidiOutputPortEntry> out;
	snd_seq_t *seq=nullptr;
	if(0!=snd_seq_open(&seq,"default",SND_SEQ_OPEN_DUPLEX,0))
	{
		return out;
	}

	snd_seq_client_info_t *cinfo=nullptr;
	snd_seq_port_info_t *pinfo=nullptr;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_client_info_set_client(cinfo,-1);
	while(0<=snd_seq_query_next_client(seq,cinfo))
	{
		const int client=snd_seq_client_info_get_client(cinfo);
		if(SND_SEQ_CLIENT_SYSTEM==client)
		{
			continue;
		}
		const char *client_name=snd_seq_client_info_get_name(cinfo);

		snd_seq_port_info_set_client(pinfo,client);
		snd_seq_port_info_set_port(pinfo,-1);
		while(0<=snd_seq_query_next_port(seq,pinfo))
		{
			if(snd_seq_port_info_get_client(pinfo)!=client)
			{
				break;
			}
			const unsigned int caps=snd_seq_port_info_get_capability(pinfo);
			if(0==(caps & SND_SEQ_PORT_CAP_WRITE) && 0==(caps & SND_SEQ_PORT_CAP_SUBS_WRITE))
			{
				continue;
			}
			const int port=snd_seq_port_info_get_port(pinfo);
			const char *port_name=snd_seq_port_info_get_name(pinfo);
			MidiOutputPortEntry entry;
			entry.id=std::to_string(client)+":"+std::to_string(port);
			entry.label=std::string(client_name)+": "+port_name;
			out.push_back(std::move(entry));
		}
	}

	snd_seq_close(seq);
	return out;
}
