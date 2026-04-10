#pragma once

#include <cstdint>

#include "statehandler.hpp"

union StandardControllerState {
	struct {
		uint8_t a: 1;
		uint8_t b: 1;
		uint8_t select: 1;
		uint8_t start: 1;
		uint8_t up: 1;
		uint8_t down: 1;
		uint8_t left: 1;
		uint8_t right: 1;
	};
	uint8_t raw;
};

class StandardStateHandler : public ControllerStateHandler {
private:
	StandardControllerState state;
	bool strobe = false;
	int readIndex = 1;
public:
	StandardStateHandler() {

	}

	void write(uint8_t value) {
		strobe = value & 1;
		if (strobe) {
			readIndex = 0;
		}
	}

	uint8_t read() {
		uint8_t ret = 0;
		if (readIndex < 8) {
			ret = (state.raw >> readIndex) & 1;
		} else {
			ret = 1; // Open bus behavior
		}
		if (!strobe) {
			readIndex++;
		}
		return ret;
	}

	void setButtonState(uint8_t buttonMask, bool pressed) {
		if (pressed) {
			state.raw |= buttonMask;
		} else {
			state.raw &= ~buttonMask;
		}
	}

	void setState(uint8_t buttonMask) {
		state.raw = buttonMask;
	}
};
