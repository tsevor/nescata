#include "apu/pulse.hpp"

const uint8_t Pulse::dutyTable[4][8] = {
	{0, 1, 0, 0, 0, 0, 0, 0},
	{0, 1, 1, 0, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{1, 0, 0, 1, 1, 1, 1, 1}
};

const uint8_t Pulse::lengthTable[32] = {
	10, 254, 20,  2, 40,  4, 80,  6,
	160,  8, 60, 10, 14, 12, 26, 14,
	12,  16, 24, 18, 48, 20, 96, 22,
	192, 24, 72, 26, 16, 28, 32, 30
};

void Pulse::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x0000:
			dutyCycle      = (val >> 6) & 0x03;
			envelopeLoop   = (val >> 5) & 0x01;
			lengthEnable   = !envelopeLoop;
			constantVolume = (val >> 4) & 0x01;
			envelopePeriod = val & 0x0F;
			break;

		case 0x0001:
			sweepEnabled = (val >> 7) & 0x01;
			sweepPeriod  = (val >> 4) & 0x07;
			sweepNegate  = (val >> 3) & 0x01;
			sweepShift   = val & 0x07;
			sweepReload  = true;
			break;

		case 0x0002:
			timerLoad = (timerLoad & 0x0700) | val;
			break;

		case 0x0003:
			timerLoad = (timerLoad & 0x00FF) | ((val & 0x07) << 8);
			sequenceStep = 0;
			envelopeStart = true;
			if (enabled) {
				lengthCounter = lengthTable[val >> 3];
			}
			break;
	}
}

int32_t Pulse::calculateSweepTarget() {
	int32_t change = timerLoad >> sweepShift;
	if (sweepNegate) {
		if (isPulse1) {
			return timerLoad - change - 1;
		} else {
			return timerLoad - change;
		}
	} else {
		return timerLoad + change;
	}
}

void Pulse::clockTimer() {
	if (timer > 0) {
		timer--;
	} else {
		timer = timerLoad;
		sequenceStep = (sequenceStep - 1) & 0x07;
	}
}

void Pulse::clockEnvelope() {
	if (envelopeStart) {
		envelopeStart = false;
		envelopeDecay = 15;
		envelopeDivider = envelopePeriod;
	} else {
		if (envelopeDivider > 0) {
			envelopeDivider--;
		} else {
			envelopeDivider = envelopePeriod;
			if (envelopeDecay > 0) {
				envelopeDecay--;
			} else if (envelopeLoop) {
				envelopeDecay = 15;
			}
		}
	}
}

void Pulse::clockLength() {
	if (lengthEnable && lengthCounter > 0) {
		lengthCounter--;
	}
}

void Pulse::clockSweep() {
	uint16_t target = calculateSweepTarget();

	if (sweepDivider == 0 && sweepEnabled && sweepShift > 0) {
		if (timerLoad >= 8 && target <= 0x07FF) {
			timerLoad = target;
		}
	}

	if (sweepDivider == 0 || sweepReload) {
		sweepDivider = sweepPeriod;
		sweepReload = false;
	} else {
		sweepDivider--;
	}
}

uint8_t Pulse::getOutput() {
	if (!enabled) return 0;
	if (lengthCounter == 0) return 0;
	if (timerLoad < 8 || calculateSweepTarget() > 0x07FF) return 0;

	if (dutyTable[dutyCycle][sequenceStep]) {
		return constantVolume ? envelopePeriod : envelopeDecay;
	}
	return 0;
}