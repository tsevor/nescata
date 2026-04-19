#pragma once

#include <cstdint>
#include <cmath>
#include <vector>

class APU {
private:
	uint8_t bufferA[735];
	uint8_t bufferB[735];
	uint8_t* activeBuffer = bufferA;
	uint8_t* workingBuffer = bufferB;

public:
	APU();

	void reset();
	void step(int cycles);
	
	uint8_t* swapBuffers();
};
