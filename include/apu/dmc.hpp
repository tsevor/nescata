#pragma once

#include <cstdint>

// Forward declaration
class Bus;

class DMC {
public:
	DMC() {}

	bool enabled = false;
	Bus* bus = nullptr;
	
	bool* frameIrqRef = nullptr;

	bool irqEnable = false;
	bool irqPending = false;
	bool loopFlag = false;

	uint16_t timerLoad = 0;
	uint16_t timer = 0;

	uint8_t outputLevel = 0;
	uint16_t sampleAddress = 0x0000;
	uint16_t sampleLength = 0;

	uint16_t currentAddress = 0x0000;
	uint16_t bytesRemaining = 0;

	bool sampleBufferEmpty = true;
	uint8_t sampleBuffer = 0;

	uint8_t shiftRegister = 0;
	uint8_t bitsRemaining = 8;
	bool silenceFlag = true;

	static const uint16_t rateTable[16];

	void write(uint16_t addr, uint8_t val);
	void clockTimer();
	void fetchSample();
	void restartSample();
	uint8_t getOutput();
	
	void reset();
	void powerOn();

	void connectBus(Bus* bus);
};

#include "bus.hpp"
