#include "bus.hpp"
#include "apu.hpp"
#include "ppu.hpp"
#include "cart.hpp"
#include "controller.hpp"

#include <algorithm>


Bus::Bus() {
	// Initialize memory
	std::fill(std::begin(memory), std::end(memory), 0);
	cart = nullptr; // No cartridge loaded
}

void Bus::clearMem() {
	// used when a full reset is needed
	std::fill(std::begin(memory), std::end(memory), 0);
}

uint8_t Bus::read(uint16_t addr) {

	// check if there's a cheat for the address
	if (cheatsEnabled) {
		auto it = cheats.find(addr);
		if (it != cheats.end()) {
			return it->second;
		}
	}

	// faster to just pull it out of the switch
	if (addr < 0x2000) {
		return memory[addr & 0x7FF];
	}
	if (addr >= 0x8000) {
		return cart->mapper->read(addr);
	}
	if (addr >= 0x4020) { // Catch $4020 - $7FFF
		return cart->mapper->read(addr);
	}

	switch (addr) {
		case 0x2002:
			return ppu->STATread();
		case 0x2004:
			return ppu->OAMDATAread();
		case 0x2007:
			return ppu->read();
		case 0x2008 ... 0x3FFF: // ppu registers mirror
			return read(0x2000 | (addr & 0x7));
		case 0x4000 ... 0x4014: // APU and I/O registers
			// Placeholder for APU and I/O register read
			return 0;
		case 0x4015:
			return apu->read(0x4015);
		case 0x4016:
			return controller1->read();
		case 0x4017:
			return controller2->read();
		case 0x4018 ... 0x401F: // APU and I/O functionality that is normally disabled
			// Placeholder for disabled APU and I/O read
			return 0;
		default:
			return 0;
	}
}

void Bus::write(uint16_t addr, uint8_t val) {


	// faster to just pull it out of the switch
	if (addr < 0x2000) {
		memory[addr & 0x7FF] = val;
		return;
	}
	if (addr >= 0x8000) {
		cart->mapper->write(addr, val); // Delegate to cartridge
		return;
	}
	if (addr >= 0x4020) {
		cart->mapper->write(addr, val);
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
				break;
			}
		case 0x4015:
			apu->write(addr, val);
			break;
		case 0x4016:
			controller1->write(val);
			break;
		case 0x4017:
			controller2->write(val);
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
}

void Bus::disconnectCart() {
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
