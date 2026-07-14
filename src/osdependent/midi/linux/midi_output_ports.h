#ifndef MIDI_OUTPUT_PORTS_IS_INCLUDED
#define MIDI_OUTPUT_PORTS_IS_INCLUDED
/* { */

#include <string>
#include <vector>

struct MidiOutputPortEntry
{
	std::string id;
	std::string label;
};

std::vector<MidiOutputPortEntry> ListAlsaMidiOutputDestinations(void);

/* } */
#endif
