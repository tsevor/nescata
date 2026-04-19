#pragma once

#include <cstdint>
#include "apu/pulse.hpp"
#include "apu/triangle.hpp"

class APU {
private:
	uint8_t bufferA[735];
	uint8_t bufferB[735];
	uint8_t* activeBuffer = bufferA;
	uint8_t* workingBuffer = bufferB;

	Pulse pulse1 {true};
	Pulse pulse2 {false};
	Triangle triangle;

	uint64_t totalCycles = 0;
	uint64_t frameCounter = 0;

	int sampleIndex = 0;

	int mixCycles = 0;
	int p1Sum = 0;
	int p2Sum = 0;
	int trSum = 0;

public:
	APU() {}

	void clockQuarterFrame();
	void clockHalfFrame();

	void reset();
	void step(int cpuCycles);
	void write(uint16_t addr, uint8_t val);

	uint8_t* swapBuffers();
};
