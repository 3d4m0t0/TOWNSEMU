#pragma once

#include <algorithm>
#include <cstdint>

// ~12 ms per miniaudio callback (same rationale as m88).
inline unsigned TownsAudioPeriodFrames(unsigned sample_rate_hz)
{
	if(sample_rate_hz<8000)
	{
		return 512;
	}
	constexpr unsigned kTargetMs=12;
	unsigned period=std::max(128u,(sample_rate_hz*kTargetMs+999)/1000);
	return std::min(period,512u);
}
