#pragma once

#include <cstdint>
#include "mapper.hpp"
#include "bus.hpp"

class NROM final : public Mapper {
private:
	uint16_t prgMask;

public:
	NROM(Cart* cartRef) {
		cart = cartRef;
		mapperID = 0;
		prgBankCount = cart->romBankCount;
		chrBankCount = cart->chrBankCount;

		prgMask = (prgBankCount > 1) ? 0x7FFF : 0x3FFF;
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000) {
			return cart->prgData[addr & prgMask];
		}
		return cart->bus->openBus;
	}

	void write(uint16_t addr, uint8_t value) override {
		// NROM has no bank switching, writes are ignored.
	}

	uint8_t readChr(uint16_t addr) override {
		if (addr < 0x2000) {
			return cart->chrData[addr];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		// Only allow writes if the cartridge is using CHR-RAM
		if (chrBankCount == 0 && addr < 0x2000) {
			cart->chrData[addr] = value;
		}
	}

	int mirrorNametable(int ntIdx) override {
		static const int mirrorLookup[3][4] = {
			{0, 0, 2, 2}, // HORIZONTAL
			{0, 1, 0, 1}, // VERTICAL
			{0, 1, 2, 3}  // FOUR_SCREEN
		};
		return mirrorLookup[cart->mirroring][ntIdx];
	}

	void reset() override {}
};