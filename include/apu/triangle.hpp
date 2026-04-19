#pragma once
#include <cstdint>

class Triangle {
public:
	Triangle();

	bool enabled = false;

	uint16_t timerLoad = 0;
	uint16_t timer = 0;
	uint8_t sequenceStep = 0;

	bool lengthEnable = true;
	uint8_t lengthCounter = 0;

	bool linearControl = false;
	bool linearReload = false;
	uint8_t linearLoad = 0;
	uint8_t linearCounter = 0;

	static const uint8_t sequenceTable[32];

	void write(uint16_t addr, uint8_t val);
	
	void clockTimer();
	void clockLinear();
	void clockLength();
	
	uint8_t getOutput();
};