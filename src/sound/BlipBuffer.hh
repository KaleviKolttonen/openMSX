// Heavily based on:
//
// Band-limited sound synthesis and buffering
// Blip_Buffer 0.4.0
// http://www.slack.net/~ant/

#ifndef BLIPBUFFER_HH
#define BLIPBUFFER_HH

#include "FixedPoint.hh"

namespace openmsx {

class BlipBuffer
{
public:
	// Number of bits in phase offset. Fewer than 6 bits (64 phase offsets) results
	// in noticeable broadband noise when synthesizing high frequency square waves.
	static constexpr int BLIP_PHASE_BITS = 10;

	using TimeIndex = FixedPoint<BLIP_PHASE_BITS>;

	BlipBuffer();

	// Update amplitude of waveform at given time. Time is in output sample
	// units and since the last time readSamples() was called.
	void addDelta(TimeIndex time, int delta);

	// Read the given amount of samples into destination buffer.
	template <unsigned PITCH>
	bool readSamples(int* dest, unsigned samples);

private:
	template <unsigned PITCH>
	void readSamplesHelper(int* out, unsigned samples) __restrict;

	static constexpr unsigned BUFFER_SIZE = 1 << 14;
	static constexpr unsigned BUFFER_MASK = BUFFER_SIZE - 1;
	int buffer[BUFFER_SIZE];
	unsigned offset;
	int accum;
	int availSamp;
};

} // namespace openmsx

#endif
