#include "bus.hpp"
#include "apu.hpp"
#include "ppu.hpp"
#include "cart.hpp"
#include "controller.hpp"

Bus::Bus() {
	// Initialize memory
	std::fill(std::begin(memory), std::end(memory), 0);
	for (int i = 0; i < 0x10000; i++) {
		cheats[i] = -1;
	}
	cart = nullptr; // No cartridge loaded
}

void Bus::clearMem() {
	// used when a full reset is needed
	std::fill(std::begin(memory), std::end(memory), 0);
}

uint8_t Bus::read(uint16_t addr) {

	if (cheats[addr] > -1) {
		openBus = cheats[addr];
		return openBus;
	}

	if (addr < 0x2000) {
		openBus = memory[addr & 0x7FF];
	} else if (addr >= 0x4020) {
		openBus = cart->mapper->read(addr);
	} else {
		switch (addr) {
			case 0x2000:
			case 0x2001:
			case 0x2003:
			case 0x2005:
			case 0x2006:
				// Reading a write-only PPU register returns the PPU latch
				openBus = ppuOpenBus;
				break;
			case 0x2002:
				ppuOpenBus = (ppu->STATread() & 0xE0) | (ppuOpenBus & 0x1F);
				openBus = ppuOpenBus;
				break;
			case 0x2004:
				ppuOpenBus = ppu->OAMDATAread();
				openBus = ppuOpenBus;
				break;
			case 0x2007:
				ppuOpenBus = ppu->read();
				openBus = ppuOpenBus;
				break;
			case 0x2008 ... 0x3FFF: // ppu registers mirror
				openBus = read(0x2000 | (addr & 0x7));
				break;
			case 0x4015:
				openBus = (apu->read(0x4015) & 0xDF) | (openBus & 0x20);
				break;
			case 0x4016:
				openBus = (controller1->read() & 0x1F) | (openBus & 0xE0);
				break;
			case 0x4017:
				openBus = (controller2->read() & 0x1F) | (openBus & 0xE0);
				break;
			default:
				// Unmapped or write-only APU register.
				// Retains the global CPU openBus.
				break;
		}
	}

	return openBus;
}

void Bus::write(uint16_t addr, uint8_t val) {

	openBus = val;

	// faster to just pull it out of the switch
	if (addr < 0x2000) {
		memory[addr & 0x7FF] = val;
		return;
	} else if (addr >= 0x4020) {
		cart->mapper->write(addr, val);
		return;
	}

	if (addr >= 0x2000 && addr <= 0x3FFF) {
		ppuOpenBus = val;
	}

	switch (addr) {
		case 0x2000:
			ppu->CTRLwrite(val);
			break;
		case 0x2001:
			ppu->MASKwrite(val);
			break;
		case 0x2003:
			ppu->OAMADDRwrite(val);
			break;
		case 0x2004:
			ppu->OAMDATAwrite(val);
			break;
		case 0x2005:
			ppu->SCRLwrite(val);
			break;
		case 0x2006:
			ppu->ADDRwrite(val);
			break;
		case 0x2007:
			ppu->write(val);
			break;
		case 0x2008 ... 0x3FFF:
			write(0x2000 | (addr & 0x7), val);
			break;
		case 0x4000 ... 0x4013: // APU/IO registers
			apu->write(addr, val);
			break;
		case 0x4014: // OAM DMA
			{
				uint8_t data[256];
				uint16_t startAddr = val << 8;
				for (int i = 0; i < 256; i++) {
					data[i] = read(startAddr | (i & 0xFF));
				}
				ppu->OAMDMAwrite(data);
				// WARNING: CPU desync. You must instruct your CPU object to suspend itself
				// for 513 or 514 cycles right here.
				break;
			}
		case 0x4015:
			apu->write(addr, val);
			break;
		case 0x4016:
			// Writing to 0x4016 strobes BOTH controllers on the hardware level
			controller1->write(val);
			controller2->write(val);
			break;
		case 0x4017:
			// Writing to 0x4017 configures the APU Frame Counter, NOT the controller
			apu->write(addr, val);
			break;
		default:
			// unmapped, do nothing
			break;
	}
}

bool Bus::clock(int cycles) {
	// cpu sends in cycles passed * 12 to get master clock cycles
	apu->step(cycles / 12);
	// do ppu last to pass nmi
	return ppu->step(cycles / 4);
}

void Bus::connectAPU(APU* apuRef) {
	apu = apuRef;
}

void Bus::disconnectAPU() {
	apu = nullptr;
}

void Bus::connectPPU(PPU* ppuRef) {
	ppu = ppuRef;
}

void Bus::disconnectPPU() {
	ppu = nullptr;
}

void Bus::connectCart(Cart* cartRef) {
	cart = cartRef;
	cart->bus = this;
}

void Bus::disconnectCart() {
	cart->bus = nullptr;
	cart = nullptr;
}

void Bus::connectController1(Controller* controller1Ref) {
	controller1 = controller1Ref;
}

void Bus::disconnectController1() {
	controller1 = nullptr;
}

void Bus::connectController2(Controller* controller2Ref) {
	controller2 = controller2Ref;
}

void Bus::disconnectController2() {
	controller2 = nullptr;
}
