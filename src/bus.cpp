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
	if (cheats.find(addr) != cheats.end()) {
		return cheats[addr];
	}

	switch (addr) {
		case 0x0000 ... 0x1FFF: // 2KB RAM
			// mirror the 2KB RAM every 0x800 bytes
			return memory[addr & 0x7FF];
		case 0x2002:
			return ppu->STATread();
		case 0x2004:
			return ppu->OAMDATAread();
		case 0x2007:
			return ppu->read();
		case 0x2008 ... 0x3FFF: // ppu registers mirror
			return read(0x2000 | (addr & 0x7));
		case 0x4000 ... 0x4015: // APU and I/O registers
			// Placeholder for APU and I/O register read
			return 0;
		case 0x4016:
			if (controller1) return controller1->read();
			else return 0;
		case 0x4017:
			if (controller2) return controller2->read();
			else return 0;
		case 0x4018 ... 0x401F: // APU and I/O functionality that is normally disabled
			// Placeholder for disabled APU and I/O read
			return 0;
		case 0x4020 ... 0xFFFF: // Cartridge space (PRG ROM, PRG RAM, and mapper registers)
			if (cart && !cart->blank) return cart->read(addr); // Delegate to cartridge
			else return 0;
		default:
			return 0;
	}
}

void Bus::write(uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x0000 ... 0x1FFF: // 2KB RAM
			memory[addr & 0x7FF] = val;
			break;
		case 0x2000:
			if (ppu) ppu->CTRLwrite(val);
			break;
		case 0x2001:
			if (ppu) ppu->MASKwrite(val);
			break;
		case 0x2003:
			if (ppu) ppu->OAMADDRwrite(val);
			break;
		case 0x2004:
			if (ppu) ppu->OAMDATAwrite(val);
			break;
		case 0x2005:
			if (ppu) ppu->SCRLwrite(val);
			break;
		case 0x2006:
			if (ppu) ppu->ADDRwrite(val);
			break;
		case 0x2007:
			if (ppu) ppu->write(val);
			break;
		case 0x2008 ... 0x3FFF:
			write(0x2000 | (addr & 0x7), val);
			break;
		case 0x4000 ... 0x4013: // APU/IO registers
			if (apu) apu->write(addr, val);
			break;
		case 0x4014: // OAM DMA
			if (ppu) {
				uint8_t data[256];
				uint16_t startAddr = val << 8;
				for (int i = 0; i < 256; i++) {
					data[i] = read(startAddr | (i & 0xFF));
				}
				ppu->OAMDMAwrite(data);
			}
			break;
		case 0x4015:
			if (apu) apu->write(addr, val);
			break;
		case 0x4016:
			if (controller1) controller1->write(val);
			break;
		case 0x4017:
			if (controller2) controller2->write(val);
			break;
		case 0x4020 ... 0xffff:
			if (cart && !cart->blank) cart->write(addr, val); // Delegate to cartridge
			break;
		default:
			// unmapped, do nothing
			break;
	}
}


bool Bus::clock(int cycles) {
	// cpu sends in cycles passed * 12 to get master clock cycles
	if (apu) {
		apu->step(cycles / 12);
	}
	// do ppu last to pass nmi
	if (ppu) {
		return ppu->step(cycles / 4);
	}
	// if there's no ppu, just return false
	return false;
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
