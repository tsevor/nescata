#pragma once

#include <cstdint>
#include "mapper.hpp"

class AxROM final : public Mapper {
private:
	uint8_t mirrorPage = 0;
	uint32_t prgOffset = 0; // Absolute byte offset in the flat PRG array

public:
	AxROM(Cart* cartRef) {
		cart = cartRef;
		// romBankCount is 16KB units. AxROM switches 32KB units.
		prgBankCount = cart->romBankCount;
		chrBankCount = cart->chrBankCount;
		mapperID = 7;
		reset();
	}

	void reset() override {
		mirrorPage = 0;
		prgOffset = 0;
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000) {
			// Mask the address to 0x7FFF (32KB range) and add the cached offset
			return cart->prgData[prgOffset + (addr & 0x7FFF)];
		}
		return 0;
	}

	void write(uint16_t addr, uint8_t value) override {
		if (addr >= 0x8000) {
			uint8_t prgBank = value & 0x07;

			// Pre-calculate the absolute byte offset in the flat array (Bank * 32KB).
			// The modulo ensures safety if the ROM header is malformed.
			prgOffset = (prgBank * 0x8000) % (cart->romBankCount * 0x4000);

			mirrorPage = (value & 0x10) >> 4;
		}
	}

	uint8_t readChr(uint16_t addr) override {
		if (addr < 0x2000) {
			return cart->chrData[addr];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		// Only allow writes if using CHR-RAM
		if (chrBankCount == 0 && addr < 0x2000) {
			cart->chrData[addr] = value;
		}
	}

	int mirrorNametable(int ntIdx) override {
		(void)ntIdx;
		return mirrorPage;
	}
};
