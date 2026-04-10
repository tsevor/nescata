#pragma once

#include <cstdint>

#include "controllers/statehandler.hpp"
#include "controllers/standard.hpp"

enum ControllerType {
	DISCONNECTED,
	STANDARD,
};

class Controller {
private:
	ControllerStateHandler* stateHandler;
	ControllerType type = DISCONNECTED;
public:
	Controller(ControllerType controllerType = DISCONNECTED) {
		type = controllerType;
		switch (type) {
			case DISCONNECTED:
				stateHandler = nullptr;
				break;
			case STANDARD:
			default:
				stateHandler = new StandardStateHandler();
				break;
		}
	}
	void write(uint8_t value) {
		if (stateHandler) {
			stateHandler->write(value);
		}
	}
	uint8_t read() {
		if (stateHandler) {
			return stateHandler->read();
		}
		return 0;
	}
	void setButtonState(uint8_t buttonMask, bool pressed) {
		switch (type) {
			case DISCONNECTED:
				break;
			case STANDARD:
				stateHandler->setButtonState(buttonMask, pressed);
				break;
			default:
				break;
		}
	}
	void setState(uint8_t buttonMask) {
		stateHandler->setState(buttonMask);
	}
};
