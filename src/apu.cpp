#include "apu.hpp"

void APU::reset() {
	pulse1.enabled = false;
	pulse2.enabled = false;
	sampleIndex = 0;
	totalCycles = 0;
	frameCounter = 0;

	mixCycles = 0;
	p1Sum = 0;
	p2Sum = 0;
	trSum = 0;
	nsSum = 0;
}

void APU::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x4000 ... 0x4003:
			pulse1.write(addr & 0x03, val);
			break;
		case 0x4004 ... 0x4007:
			pulse2.write(addr & 0x03, val);
			break;
		case 0x4008 ... 0x400B:
			triangle.write(addr & 0x03, val);
			break;
		case 0x400C ... 0x400F:
			noise.write(addr & 0x03, val);
			break;
		case 0x4015:
			pulse1.enabled = (val & 0x01);
			if (!pulse1.enabled) pulse1.lengthCounter = 0;

			pulse2.enabled = (val & 0x02);
			if (!pulse2.enabled) pulse2.lengthCounter = 0;

			triangle.enabled = (val & 0x04);
			if (!triangle.enabled) triangle.lengthCounter = 0;

			noise.enabled = (val & 0x08);
			if (!noise.enabled) noise.lengthCounter = 0;
			break;
	}
}

void APU::step(int cpuCycles) {
	for (int i = 0; i < cpuCycles; i++) {

		frameCounter++;

		// 7457 CPU cycles is about one quarter-frame
		if (frameCounter == 7457) {
			clockQuarterFrame();
		} else if (frameCounter == 14913) {
			clockQuarterFrame();
			clockHalfFrame();
		} else if (frameCounter == 22371) {
			clockQuarterFrame();
		} else if (frameCounter == 29829) {
			clockQuarterFrame();
			clockHalfFrame();
			frameCounter = 0;
		}

		totalCycles++;

		triangle.clockTimer();

		if (totalCycles % 2 == 0) {
			pulse1.clockTimer();
			pulse2.clockTimer();
			noise.clockTimer();
		}

		p1Sum += pulse1.getOutput();
		p2Sum += pulse2.getOutput();
		trSum += triangle.getOutput();
		nsSum += noise.getOutput();
		mixCycles++;

		if (mixCycles == 41) {
			if (sampleIndex < 735) {
				uint8_t p1 = p1Sum / 41;
				uint8_t p2 = p2Sum / 41;
				uint8_t tr = trSum / 41;
				uint8_t ns = nsSum / 41;
				
				workingBuffer[sampleIndex] = (p1 + p2 + tr + ns) * 4;
				sampleIndex++;
			}

			p1Sum = 0;
			p2Sum = 0;
			trSum = 0;
			nsSum = 0;
			mixCycles = 0;
		}
	}
}

uint8_t* APU::swapBuffers() {
	// if the frame ended early pad the rest of the buffer with the last audio state
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
	triangle.clockLinear();
	noise.clockEnvelope();
}

void APU::clockHalfFrame() {
	pulse1.clockLength();
	pulse2.clockLength();
	triangle.clockLength();
	noise.clockLength();

	pulse1.clockSweep();
	pulse2.clockSweep();
	triangle.clockLinear();

}
