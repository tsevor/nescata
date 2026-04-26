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
	dmcSum = 0;

	frameIrq = false;
	if (!dmc.irqPending) {
		*(bus->irqLine) = false;
	}
	
	pulse1.reset();
	pulse2.reset();
	triangle.reset();
	noise.reset();
	dmc.reset();
}

uint8_t APU::read(uint16_t addr) {
	uint8_t data = 0;
	if (addr == 0x4015) {
		if (pulse1.lengthCounter > 0) data |= 0x01;
		if (pulse2.lengthCounter > 0) data |= 0x02;
		if (triangle.lengthCounter > 0) data |= 0x04;
		if (noise.lengthCounter > 0) data |= 0x08;
		if (dmc.bytesRemaining > 0) data |= 0x10;
		if (frameIrq) data |= 0x40;
			if (dmc.irqPending) data |= 0x80;

			frameIrq = false; // Reading $4015 clears the frame IRQ flag
			if (!dmc.irqPending) {
				*(bus->irqLine) = false;
			}
		}
		return data;
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

			dmc.enabled = (val & 0x10);
			if (!dmc.enabled) {
				dmc.bytesRemaining = 0;
				dmc.irqPending = false;
				if (!frameIrq) {
					*(bus->irqLine) = false;
				}
			}
			break;
		case 0x4017:
			frameMode = (val >> 7) & 0x01;
			irqInhibit = (val >> 6) & 0x01;

			frameIrq = false;
			if (!dmc.irqPending) {
				*(bus->irqLine) = false;
			}

			frameCounterResetDelay = (totalCycles % 2 == 0) ? 3 : 4;

			break;
	}
}

#include <iostream>

void APU::step(int cpuCycles) {
	for (int i = 0; i < cpuCycles; i++) {
		// Handle the $4017 reset delay
		// In APU::step
		if (frameCounterResetDelay > 0) {
			frameCounterResetDelay--;
			if (frameCounterResetDelay == 0) {
				frameCounter = 0;
				// Apply the delayed Mode 1 clocks here!
				if (frameMode == 1) {
					clockQuarterFrame();
					clockHalfFrame();
				}
			}
		}

		frameCounter++;

		if (frameMode == 0) {
			// 4-step sequence timing
			if (frameCounter == 7457) {
				clockQuarterFrame();
			} else if (frameCounter == 14913) {
				clockQuarterFrame();
				clockHalfFrame();
			} else if (frameCounter == 22371) {
				clockQuarterFrame();
			} else if (frameCounter == 29828) {
				if (!irqInhibit) {
					frameIrq = true;
					*(bus->irqLine) = true;
				}
			} else if (frameCounter == 29829) {
				if (!irqInhibit) {
					frameIrq = true;
					*(bus->irqLine) = true;
				}
				clockQuarterFrame();
				clockHalfFrame();
			} else if (frameCounter == 29830) {
				if (!irqInhibit) {
					frameIrq = true;
					*(bus->irqLine) = true;
				}
				frameCounter = 0;
			}
		} else {
			// 5-step sequence timing
			if (frameCounter == 7457) {
				clockQuarterFrame();
			} else if (frameCounter == 14913) {
				clockQuarterFrame();
				clockHalfFrame();
			} else if (frameCounter == 22371) {
				clockQuarterFrame();
			} else if (frameCounter == 37281) {
				clockQuarterFrame();
				clockHalfFrame();
				frameCounter = 0;
			}
		}


		totalCycles++;

		triangle.clockTimer();
		dmc.clockTimer();

		if (totalCycles % 2 == 0) {
			pulse1.clockTimer();
			pulse2.clockTimer();
			noise.clockTimer();
		}

		p1Sum += pulse1.getOutput();
		p2Sum += pulse2.getOutput();
		trSum += triangle.getOutput();
		nsSum += noise.getOutput();
		dmcSum += dmc.getOutput();

		mixCycles++;
		audioTimer += 1.0;

		// 1789773.0 / 44100.0 = 40.58442... cycles per sample
		if (audioTimer >= 40.5844217687) {
			audioTimer -= 40.5844217687;

			if (sampleIndex < 735) {
				double p1 = p1Sum / (double)mixCycles;
				double p2 = p2Sum / (double)mixCycles;
				double tr = trSum / (double)mixCycles;
				double ns = nsSum / (double)mixCycles;
				double dm = dmcSum / (double)mixCycles;

				double pulseOut = 0.0;
				if (p1 + p2 > 0) {
					pulseOut = 95.88 / ((8128.0 / (p1 + p2)) + 100.0);
				}

				double tndOut = 0.0;
				if (tr + ns + dm > 0) {
					tndOut = 159.79 / (1.0 / ((tr / 8227.0) + (ns / 12241.0) + (dm / 22638.0)) + 100.0);
				}

				buffer[sampleIndex] = static_cast<int8_t>((pulseOut + tndOut) * 255.0 - 128.0);
				
				sampleIndex++;
			}

			p1Sum = 0;
			p2Sum = 0;
			trSum = 0;
			nsSum = 0;
			dmcSum = 0;
			mixCycles = 0;
		}
	}
}


int8_t* APU::getBuffer() {
	// if the frame ended early pad the rest of the buffer with the last audio state
	int8_t lastSample = (sampleIndex > 0) ? buffer[sampleIndex - 1] : 0;
	while (sampleIndex < 735) {
		buffer[sampleIndex] = lastSample;
		sampleIndex++;
	}

	sampleIndex = 0;

	return buffer;
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

}

void APU::connectBus(Bus* busRef) {
	bus = busRef;
	dmc.connectBus(busRef);
}
