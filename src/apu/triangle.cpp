#include "apu/triangle.hpp"
#include "apu/pulse.hpp" // to reuse Pulse::lengthTable

const uint8_t Triangle::sequenceTable[32] = {
	15, 14, 13, 12, 11, 10,  9,  8,
	 7,  6,  5,  4,  3,  2,  1,  0,
	 0,  1,  2,  3,  4,  5,  6,  7,
	 8,  9, 10, 11, 12, 13, 14, 15
};

Triangle::Triangle() {}

void Triangle::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x0000: // $4008
			linearControl = (val >> 7) & 0x01;
			lengthEnable  = !linearControl; // Shared bit
			linearLoad    = val & 0x7F;
			break;

		case 0x0001: // $4009
			// Unused
			break;

		case 0x0002: // $400A
			timerLoad = (timerLoad & 0x0700) | val;
			break;

		case 0x0003: // $400B
			timerLoad = (timerLoad & 0x00FF) | ((val & 0x07) << 8);
			if (enabled) {
				lengthCounter = Pulse::lengthTable[val >> 3];
			}
			linearReload = true;
			break;
	}
}

void Triangle::clockTimer() {
	if (timer > 0) {
		timer--;
	} else {
		timer = timerLoad;
		if (lengthCounter > 0 && linearCounter > 0 && timerLoad >= 2) {
			sequenceStep = (sequenceStep + 1) & 0x1F;
		}
	}
}

void Triangle::clockLinear() {
	if (linearReload) {
		linearCounter = linearLoad;
	} else if (linearCounter > 0) {
		linearCounter--;
	}

	if (!linearControl) {
		linearReload = false;
	}
}

void Triangle::clockLength() {
	if (lengthEnable && lengthCounter > 0) {
		lengthCounter--;
	}
}

uint8_t Triangle::getOutput() {
	if (!enabled) return 0;
	return sequenceTable[sequenceStep];
}