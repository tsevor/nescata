#pragma once

#include <cstdint>



class ControllerStateHandler {
public:
	virtual void write(uint8_t value) = 0;
	virtual uint8_t read() = 0;
	virtual void setButtonState(uint8_t buttonMask, bool pressed) = 0;
	virtual void setState(uint8_t buttonMask) = 0;
};
