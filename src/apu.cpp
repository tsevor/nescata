#include "apu.hpp"

void APU::reset() {
	pulse1.enabled = false;
	pulse2.enabled = false;
	sampleIndex = 0;
	totalCycles = 0;
}

void APU::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x4000 ... 0x4003:
			pulse1.write(addr & 0x03, val);
			break;
		case 0x4004 ... 0x4007:
			pulse2.write(addr & 0x03, val);
			break;
		case 0x4015:
			pulse1.enabled = (val & 0x01);
			if (!pulse1.enabled) pulse1.lengthCounter = 0;
			
			pulse2.enabled = (val & 0x02);
			if (!pulse2.enabled) pulse2.lengthCounter = 0;
			
			break;
	}
}

void APU::step(int cpuCycles) {
	for (int i = 0; i < cpuCycles; i++) {

		frameCounter++;

		// 7457 CPU cycles is roughly one quarter-frame
		if (frameCounter == 7457) {
			clockQuarterFrame(); // Clocks Envelopes
		} else if (frameCounter == 14913) {
			clockQuarterFrame();
			clockHalfFrame();    // Clocks Length and Sweep
		} else if (frameCounter == 22371) {
			clockQuarterFrame();
		} else if (frameCounter == 29829) {
			clockQuarterFrame();
			clockHalfFrame();
			frameCounter = 0;
		}

		totalCycles++;

		// APU timers clock every 2 CPU cycles
		if (totalCycles % 2 == 0) {
			pulse1.clockTimer();
			pulse2.clockTimer();
		}

		// Naive downsampling: ~1.789MHz CPU / 44100Hz Audio = ~40.5 CPU cycles per sample.
		// Grab a sample every 41 cycles to fill the buffer incrementally.
		if (totalCycles % 41 == 0 && sampleIndex < 735) {
			uint8_t p1 = pulse1.getOutput();
			uint8_t p2 = pulse2.getOutput();

			// Mix and scale the 4-bit volumes up to 8-bit space
			workingBuffer[sampleIndex] = (p1 + p2) * 4;
			sampleIndex++;
		}
	}
}

uint8_t* APU::swapBuffers() {
	// If the frame ended early, pad the rest of the buffer with the last known audio state
	uint8_t lastSample = (sampleIndex > 0) ? workingBuffer[sampleIndex - 1] : 0;
	while (sampleIndex < 735) {
		workingBuffer[sampleIndex] = lastSample;
		sampleIndex++;
	}

	uint8_t* temp = activeBuffer;
	activeBuffer = workingBuffer;
	workingBuffer = temp;

	sampleIndex = 0;

	return activeBuffer;
}

void APU::clockQuarterFrame() {
	pulse1.clockEnvelope();
	pulse2.clockEnvelope();
}

void APU::clockHalfFrame() {
	pulse1.clockLength();
	pulse2.clockLength();

	pulse1.clockSweep();
	pulse2.clockSweep();

}
