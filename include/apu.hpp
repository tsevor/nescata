#pragma once

#include <cstdint>
#include <vector>

class APU {
public:
	APU();

	void reset();

	void step(int cycles);

	std::vector<uint8_t> getAudioBuffer();
};
