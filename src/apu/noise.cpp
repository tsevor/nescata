#include "apu/noise.hpp"
#include "apu/pulse.hpp" // reuse Pulse::lengthTable

const uint16_t Noise::periodTable[16] = {
	4, 8, 16, 32, 64, 96, 128, 160,
	202, 254, 380, 508, 762, 1016, 2034, 4068
};

Noise::Noise() {}

void Noise::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x0000: // $400C
			envelopeLoop   = (val >> 5) & 0x01;
			lengthEnable   = !envelopeLoop;
			constantVolume = (val >> 4) & 0x01;
			envelopePeriod = val & 0x0F;
			break;

		case 0x0001: // $400D
			// unused
			break;

		case 0x0002: // $400E
			mode = (val >> 7) & 0x01;
			timerLoad = periodTable[val & 0x0F];
			break;

		case 0x0003: // $400F
			if (enabled) {
				lengthCounter = Pulse::lengthTable[val >> 3];
			}
			envelopeStart = true;
			break;
	}
}

void Noise::clockTimer() {
	if (timer > 0) {
		timer--;
	} else {
		timer = timerLoad;

		uint8_t shift = mode ? 6 : 1;
		uint16_t feedback = (shiftRegister & 0x01) ^ ((shiftRegister >> shift) & 0x01);

		shiftRegister = (shiftRegister >> 1) | (feedback << 14);
	}
}

void Noise::clockEnvelope() {
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

void Noise::clockLength() {
	if (lengthEnable && lengthCounter > 0) {
		lengthCounter--;
	}
}

uint8_t Noise::getOutput() {
	if (!enabled) return 0;
	if (lengthCounter == 0) return 0;

	if (shiftRegister & 0x01) return 0;

	return constantVolume ? envelopePeriod : envelopeDecay;
}
