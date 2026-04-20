#include "apu/dmc.hpp"

const uint16_t DMC::rateTable[16] = {
	428, 380, 340, 320, 286, 254, 226, 214,
	190, 160, 142, 128, 106, 84,  72,  54
};

void DMC::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x0000: // $4010
			irqEnable = (val >> 7) & 0x01;
			if (!irqEnable) irqPending = false;
			loopFlag = (val >> 6) & 0x01;
			timerLoad = rateTable[val & 0x0F];
			break;

		case 0x0001: // $4011
			outputLevel = val & 0x7F;
			break;

		case 0x0002: // $4012
			sampleAddress = 0xC000 + (val * 64);
			break;

		case 0x0003: // $4013
			sampleLength = (val * 16) + 1;
			break;
	}
}

void DMC::restartSample() {
	currentAddress = sampleAddress;
	bytesRemaining = sampleLength;
	if (sampleBufferEmpty && bytesRemaining > 0) {
		fetchSample();
	}
}

void DMC::fetchSample() {
	if (!bus) return;
	
	sampleBuffer = bus->read(currentAddress);
	sampleBufferEmpty = false;

	// would stall cpu 4 cycles
	
	currentAddress++;
	if (currentAddress == 0x0000) {
		currentAddress = 0x8000;
	}

	if (bytesRemaining == 0) {
		if (loopFlag) {
			restartSample();
		} else if (irqEnable) {
			irqPending = true;
			*(bus->irqLine) = true;
		}
	}
}

void DMC::clockTimer() {
	if (timer > 0) {
		timer--;
	} else {
		timer = timerLoad;

		if (!silenceFlag) {
			if (shiftRegister & 0x01) {
				if (outputLevel <= 125) outputLevel += 2;
			} else {
				if (outputLevel >= 2) outputLevel -= 2;
			}
		}

		shiftRegister >>= 1;
		bitsRemaining--;

		if (bitsRemaining == 0) {
			bitsRemaining = 8;
			if (sampleBufferEmpty) {
				silenceFlag = true;
			} else {
				silenceFlag = false;
				shiftRegister = sampleBuffer;
				sampleBufferEmpty = true;
				if (bytesRemaining > 0) {
					fetchSample();
				}
			}
		}
	}
}

uint8_t DMC::getOutput() {
	return outputLevel;
}

void DMC::connectBus(Bus* busRef) {
	bus = busRef;
}