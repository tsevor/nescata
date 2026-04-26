#pragma once

#include <cstdint>

class Pulse {
public:

	bool isPulse1 = true;
	bool enabled = false;

	uint8_t dutyCycle = 0;
	uint16_t timerLoad = 0;
	uint16_t timer = 0;
	uint8_t sequenceStep = 0;

	bool constantVolume = false;
	bool envelopeLoop = false;
	bool envelopeStart = false;
	uint8_t envelopePeriod = 0;
	uint8_t envelopeDivider = 0;
	uint8_t envelopeDecay = 0;

	bool lengthEnable = true;
	uint8_t lengthCounter = 0;

	bool sweepEnabled = false;
	bool sweepReload = false;
	bool sweepNegate = false;
	uint8_t sweepPeriod = 0;
	uint8_t sweepDivider = 0;
	uint8_t sweepShift = 0;

	static const uint8_t dutyTable[4][8];
	static const uint8_t lengthTable[32];

	Pulse(bool isPulse1) : isPulse1(isPulse1) {}

	void write(uint16_t addr, uint8_t val);
	int32_t calculateSweepTarget();

	void clockTimer();
	void clockEnvelope();
	void clockLength();
	void clockSweep();

	uint8_t getOutput();


};
