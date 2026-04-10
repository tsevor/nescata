#pragma once

#include <vector>
#include <array>
#include <cstdint>

#include "mapper.hpp"

class MMC1 : public Mapper {
private:
	// Internal State
	uint8_t shiftReg = 0;
	int shiftCount = 0;

	// Registers
	// Control: Mirroring, PRG/CHR Bank Modes
	uint8_t control = 0x0C; // Power-on: 4KB CHR, Fix Last PRG

	// Data Registers
	uint8_t chrBank0 = 0;
	uint8_t chrBank1 = 0;
	uint8_t prgBank = 0;

	// Resolved Offsets
	int prgBankIdx8000 = 0;
	int prgBankIdxC000 = 0;
	int chrBankIdx0000 = 0; // Represents 4KB chunk index
	int chrBankIdx1000 = 0; // Represents 4KB chunk index

	// PRG RAM (WRAM) - MMC1 usually has 8KB at $6000
	std::array<uint8_t, 0x2000> prgRam;
	bool prgRamEnabled = true;

public:
	MMC1(Cart* cartRef) {
		cart = cartRef;
		mapperID = 1;
		prgBankCount = cart->prgBanks.size();
		chrBankCount = cart->chrBanks.size();

		// MMC1 usually has 8KB WRAM
		prgRam.fill(0);

		// If no CHR ROM is present, allocate 8KB CHR RAM (treated as 1 bank)
		if (chrBankCount == 0) {
			cart->chrBanks.push_back(std::array<uint8_t, 0x2000>{});
		}

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
		// WRAM
		if (addr >= 0x6000 && addr <= 0x7FFF) {
			// Some MMC1 variants return open bus if disabled, others 0
			if (prgRamEnabled) return prgRam[addr & 0x1FFF];
			return 0;
		}

		// PRG ROM
		if (addr >= 0x8000) {
			int bankIdx = (addr < 0xC000) ? prgBankIdx8000 : prgBankIdxC000;

			// Safety mask to keep index within vector size
			if (prgBankCount > 0) bankIdx %= prgBankCount;

			return cart->prgBanks[bankIdx][addr & 0x3FFF];
		}
		return 0;
	}

	void write(uint16_t addr, uint8_t value) override {
		// WRAM
		if (addr >= 0x6000 && addr <= 0x7FFF) {
			if (prgRamEnabled) prgRam[addr & 0x1FFF] = value;
			return;
		}

		// Registers $8000-$FFFF (Serial Interface)
		if (addr >= 0x8000) {
			// Bit 7 set: Reset Shift Register
			if (value & 0x80) {
				shiftReg = 0;
				shiftCount = 0;
				control = control | 0x0C; // Reset to Mode 3 (Fix Last PRG)
				updateBanks();
			} else {
				// Serial Load (LSB first)
				shiftReg = (shiftReg >> 1) | ((value & 1) << 4);
				shiftCount++;

				if (shiftCount == 5) {
					// Register selected by bits 13-14 of address
					uint8_t target = (addr >> 13) & 0x03;
					switch (target) {
						case 0: control = shiftReg;  break; // $8000 Control
						case 1: chrBank0 = shiftReg; break; // $A000 CHR 0
						case 2: chrBank1 = shiftReg; break; // $C000 CHR 1
						case 3: prgBank = shiftReg;  break; // $E000 PRG
					}

					// Clear shift
					shiftReg = 0;
					shiftCount = 0;
					updateBanks();
				}
			}
		}
	}

	uint8_t readChr(uint16_t addr) override {
		// Resolve the 4KB chunk index
		int bank4k = (addr < 0x1000) ? chrBankIdx0000 : chrBankIdx1000;

		// The cart vector holds 8KB chunks. We need to split them logic-wise.
		// Vector Index = bank4k / 2
		// Offset = (bank4k % 2) * 4096

		// If using CHR RAM (bank count 0 -> we added 1 in constructor),
		// we just map to the first available bank usually.
		int vecIdx = bank4k / 2;
		int offset = (bank4k % 2) * 0x1000;

		// Handle CHR RAM wrap-around or safety
		if (cart->chrBanks.size() > 0) {
			vecIdx %= cart->chrBanks.size();
			return cart->chrBanks[vecIdx][offset + (addr & 0x0FFF)];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		// Allow writing to CHR RAM
		if (chrBankCount == 0) {
			// Simple mapping for CHR RAM (usually just one 8KB bank)
			cart->chrBanks[0][addr & 0x1FFF] = value;
		}
	}

	int mirrorNametable(int ntIdx) override {
		// Control Register Bits 0-1:
		// 0: One Screen Lower (Physical 0)
		// 1: One Screen Upper (Physical 1)
		// 2: Vertical
		// 3: Horizontal
		switch (control & 0x03) {
			case 0: return 0;
			case 1: return 1;
			case 2: return (ntIdx % 2) ? 1 : 0; // Vertical (0,1,0,1)
			case 3: return (ntIdx / 2) ? 1 : 0; // Horizontal (0,0,1,1)
		}
		return 0;
	}

private:
	void updateBanks() {
		// --- PRG Banking ---
		// Control Bits 2-3:
		// 0, 1: Switch 32KB at $8000 (ignore low bit)
		// 2: Fix First ($8000), Switch 16KB at $C000
		// 3: Fix Last ($C000), Switch 16KB at $8000
		int mode = (control >> 2) & 0x03;
		int pIdx = prgBank & 0x0F; // 4-bit bank select (support 256k)

		// Bit 4 of PRG Register is usually WRAM Enable (0=Enable)
		prgRamEnabled = (prgBank & 0x10) == 0;

		if (mode <= 1) {
			// 32KB Mode
			prgBankIdx8000 = pIdx & 0xFE; // Align to even
			prgBankIdxC000 = (pIdx & 0xFE) + 1;
		} else if (mode == 2) {
			// Fix First Bank
			prgBankIdx8000 = 0;
			prgBankIdxC000 = pIdx;
		} else {
			// Fix Last Bank
			prgBankIdx8000 = pIdx;
			prgBankIdxC000 = prgBankCount - 1;
		}

		// --- CHR Banking ---
		// Control Bit 4: 0=8KB Mode, 1=4KB Mode
		bool chr4k = control & 0x10;

		if (chr4k) {
			// 4KB Mode: Reg0 = Low 4k, Reg1 = High 4k
			chrBankIdx0000 = chrBank0;
			chrBankIdx1000 = chrBank1;
		} else {
			// 8KB Mode: Reg0 = All 8k (align to even), Reg1 ignored
			chrBankIdx0000 = chrBank0 & 0xFE;
			chrBankIdx1000 = (chrBank0 & 0xFE) + 1;
		}
	}
};
