#pragma once

#include <cstdint>

#include "apu/pulse.hpp"
#include "apu/triangle.hpp"
#include "apu/noise.hpp"
#include "apu/dmc.hpp"

// Forward declaration
class Bus; 

class APU {
private:
	uint8_t bufferA[735];
	uint8_t bufferB[735];
	uint8_t* activeBuffer = bufferA;
	uint8_t* workingBuffer = bufferB;

	Bus* bus = nullptr;
	
	Pulse pulse1 {true};
	Pulse pulse2 {false};
	Triangle triangle;
	Noise noise;
	DMC dmc;

	uint64_t totalCycles = 0;
	uint64_t frameCounter = 0;
	
	uint8_t frameMode = 0;
	bool irqInhibit = false;
	bool frameIrq = false;

	int sampleIndex = 0;

	double audioTimer = 0.0;
	int mixCycles = 0;
	int p1Sum = 0;
	int p2Sum = 0;
	int trSum = 0;
	int nsSum = 0;
	int dmcSum = 0;
	
	int frameCounterResetDelay = 0;

public:
	APU() {}

	void clockQuarterFrame();
	void clockHalfFrame();

	void reset();
	void step(int cpuCycles);
	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t val);

	uint8_t* swapBuffers();
	
	void connectBus(Bus* busRef);
};
