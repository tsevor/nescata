#pragma once

#include <cstdint>

// Forward declarations
class APU;
class PPU;
class Cart;
class Controller;


class Bus {
private:
	uint8_t memory[0x800]; // 2KB internal memory

public:

	PPU* ppu = nullptr;
	APU* apu = nullptr;
	Cart* cart = nullptr;
	Controller* controller1 = nullptr;
	Controller* controller2 = nullptr;
	
	bool* irqLine = nullptr;

	int16_t cheats[0x10000]; // 0-255, -1 means no

	Bus();

	void clearMem();

	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t val);

	bool clock(int cycles);

	void connectAPU(APU* apu);
	void disconnectAPU();
	void connectPPU(PPU* ppu);
	void disconnectPPU();
	void connectCart(Cart* cart);
	void disconnectCart();
	void connectController1(Controller* controller1);
	void disconnectController1();
	void connectController2(Controller* controller2);
	void disconnectController2();
};
