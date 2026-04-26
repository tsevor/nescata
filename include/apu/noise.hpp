#pragma once

#include <cstdint>

class Noise {
public:
	Noise();

	bool enabled = false;

	uint16_t shiftRegister = 1;
	bool mode = false;

	uint16_t timer = 0;
	uint16_t timerLoad = 0;

	bool constantVolume = false;
	bool envelopeLoop = false;
	bool envelopeStart = false;
	uint8_t envelopePeriod = 0;
	uint8_t envelopeDivider = 0;
	uint8_t envelopeDecay = 0;

	bool lengthEnable = true;
	uint8_t lengthCounter = 0;

	static const uint16_t periodTable[16];

	void write(uint16_t addr, uint8_t val);

	void clockTimer();
	void clockEnvelope();
	void clockLength();
	
	void reset();

	uint8_t getOutput();
};
