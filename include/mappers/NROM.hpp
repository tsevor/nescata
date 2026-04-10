#pragma once

#include <vector>
#include <array>
#include <cstdint>

#include "mapper.hpp"

class NROM : public Mapper {
public:
	NROM(Cart* cartRef) {
		cart = cartRef;
		prgBankCount = cart->prgBanks.size();
		chrBankCount = cart->chrBanks.size();
		mapperID = 0;
		if (prgBankCount == 1) {
			cart->prgBanks.push_back(cart->prgBanks[0]);
		} else if (prgBankCount == 2) {
			cart->prgBanks.push_back(cart->prgBanks[1]);
		}
		if (chrBankCount != 1) {
			cart->chrBanks.push_back(std::array<uint8_t, 0x2000>{});
		}
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000 && addr <= 0xBFFF) {
			return cart->prgBanks[0][addr - 0x8000];
		} else if (addr >= 0xC000 && addr <= 0xFFFF) {
			return cart->prgBanks[1][addr - 0xC000];
		} else {
			return 0; // Should not happen
		}
	}

	void write(uint16_t addr, uint8_t value) override {
		// NROM has no bank switching, so writes do nothing.
	}

	uint8_t readChr(uint16_t addr) override {
		if (addr < 0x2000) {
			return cart->chrBanks[0][addr];
		} else {
			return 0; // Should not happen
		}
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		if (chrBankCount == 0)
			cart->chrBanks[0][addr] = value;
	}

	int mirrorNametable(int ntIdx) override {
		return (int[][4]){
			{0, 0, 2, 2},
			{0, 1, 0, 1},
			{0, 1, 2, 3}
		}[cart->mirroring][ntIdx];
	}

	void reset() override {
		// nothing to reset on nrom
	}
};
