#pragma once

#include <cstdint>
#include "mapper.hpp"

class MMC1 final : public Mapper {
private:
	// Internal State
	uint8_t shiftReg = 0;
	int shiftCount = 0;

	// Registers
	uint8_t control = 0x0C; // Power-on: 4KB CHR, Fix Last PRG
	uint8_t chrBank0 = 0;
	uint8_t chrBank1 = 0;
	uint8_t prgBank = 0;

	// Cached Absolute Byte Offsets for Flat Arrays
	uint32_t prgOffset8000 = 0;
	uint32_t prgOffsetC000 = 0;
	uint32_t chrOffset0000 = 0;
	uint32_t chrOffset1000 = 0;

	// PRG RAM (WRAM) - 8KB at $6000
	uint8_t prgRam[0x2000];
	bool prgRamEnabled = true;

	// Sizes for safe modulo wrapping
	uint32_t totalPrgSize;
	uint32_t totalChrSize;

	void updateBanks() {
		// Control Bits 2-3: PRG ROM Bank Mode
		int mode = (control >> 2) & 0x03;
		int pIdx = prgBank & 0x0F; 
		prgRamEnabled = (prgBank & 0x10) == 0;

		if (mode <= 1) {
			// 32KB Mode
			prgOffset8000 = ((pIdx & 0xFE) * 0x4000) % totalPrgSize;
			prgOffsetC000 = (((pIdx & 0xFE) + 1) * 0x4000) % totalPrgSize;
		} else if (mode == 2) {
			// Fix First Bank, Switch Last
			prgOffset8000 = 0;
			prgOffsetC000 = (pIdx * 0x4000) % totalPrgSize;
		} else {
			// Switch First, Fix Last Bank
			prgOffset8000 = (pIdx * 0x4000) % totalPrgSize;
			prgOffsetC000 = ((cart->romBankCount - 1) * 0x4000) % totalPrgSize;
		}

		// Control Bit 4: 0=8KB Mode, 1=4KB Mode
		bool chr4k = control & 0x10;

		if (chr4k) {
			// 4KB Mode
			chrOffset0000 = (chrBank0 * 0x1000) % totalChrSize;
			chrOffset1000 = (chrBank1 * 0x1000) % totalChrSize;
		} else {
			// 8KB Mode (Treat as two adjacent 4KB chunks)
			chrOffset0000 = ((chrBank0 & 0xFE) * 0x1000) % totalChrSize;
			chrOffset1000 = (((chrBank0 & 0xFE) + 1) * 0x1000) % totalChrSize;
		}
	}

public:
	MMC1(Cart* cartRef) {
		cart = cartRef;
		mapperID = 1;
		prgBankCount = cart->romBankCount;
		chrBankCount = cart->chrBankCount;

		totalPrgSize = prgBankCount * 0x4000;
		// If the cart has no CHR-ROM, Cart provides 8KB of CHR-RAM.
		totalChrSize = (chrBankCount == 0) ? 0x2000 : (chrBankCount * 0x2000);

		for (int i = 0; i < 0x2000; i++) prgRam[i] = 0;

		reset();
	}

	void reset() override {
		shiftReg = 0;
		shiftCount = 0;
		control = 0x0C;
		chrBank0 = 0;
		chrBank1 = 0;
		prgBank = 0;
		prgRamEnabled = true;
		updateBanks();
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000) {
			if (addr < 0xC000) {
				return cart->prgData[prgOffset8000 + (addr & 0x3FFF)];
			} else {
				return cart->prgData[prgOffsetC000 + (addr & 0x3FFF)];
			}
		} else if (addr >= 0x6000 && addr <= 0x7FFF) {
			if (prgRamEnabled) return prgRam[addr & 0x1FFF];
		}
		return 0;
	}

	void write(uint16_t addr, uint8_t value) override {
		if (addr >= 0x8000) {
			// Bit 7 set: Reset Shift Register
			if (value & 0x80) {
				shiftReg = 0;
				shiftCount = 0;
				control = control | 0x0C; // Reset to Mode 3
				updateBanks();
			} else {
				// Serial Load (LSB first)
				shiftReg = (shiftReg >> 1) | ((value & 1) << 4);
				shiftCount++;

				if (shiftCount == 5) {
					uint8_t target = (addr >> 13) & 0x03;
					switch (target) {
						case 0: control = shiftReg;  break; // $8000
						case 1: chrBank0 = shiftReg; break; // $A000
						case 2: chrBank1 = shiftReg; break; // $C000
						case 3: prgBank = shiftReg;  break; // $E000
					}
					shiftReg = 0;
					shiftCount = 0;
					updateBanks();
				}
			}
		} else if (addr >= 0x6000 && addr <= 0x7FFF) {
			if (prgRamEnabled) prgRam[addr & 0x1FFF] = value;
		}
	}

	uint8_t readChr(uint16_t addr) override {
		if (addr < 0x1000) {
			return cart->chrData[chrOffset0000 + (addr & 0x0FFF)];
		} else if (addr < 0x2000) {
			return cart->chrData[chrOffset1000 + (addr & 0x0FFF)];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		// Allow writing to CHR-RAM
		if (chrBankCount == 0 && addr < 0x2000) {
			if (addr < 0x1000) {
				cart->chrData[chrOffset0000 + (addr & 0x0FFF)] = value;
			} else {
				cart->chrData[chrOffset1000 + (addr & 0x0FFF)] = value;
			}
		}
	}

	int mirrorNametable(int ntIdx) override {
		switch (control & 0x03) {
			case 0: return 0;
			case 1: return 1;
			case 2: return (ntIdx % 2) ? 1 : 0; // Vertical
			case 3: return (ntIdx / 2) ? 1 : 0; // Horizontal
		}
		return 0;
	}
};