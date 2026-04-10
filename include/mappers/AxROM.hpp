#pragma once

#include <vector>
#include <array>
#include <cstdint>

#include "mapper.hpp"

class AxROM : public Mapper {
private:
	uint8_t prgBank = 0;
	uint8_t mirrorPage = 0;

public:
	AxROM(Cart* cartRef) {
		cart = cartRef;
		prgBankCount = cart->prgBanks.size();
		chrBankCount = cart->chrBanks.size();
		mapperID = 7;

		// AxROM usually uses CHR-RAM. If the cart file didn't have CHR ROM,
		// we need to allocate the 8KB RAM bank.
		if (chrBankCount == 0) {
			cart->chrBanks.push_back(std::array<uint8_t, 0x2000>{});
		}

		reset();
	}

	void reset() override {
		prgBank = 0;
		mirrorPage = 0;
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000) {
			// AxROM switches 32KB chunks.
			// Since our vector holds 16KB chunks, we multiply the index by 2.
			int baseBankIndex = (prgBank * 2);

			// Safety modulo to handle ROMs smaller than the bank selection allows
			// (Use bitmasking logic if strict hardware accuracy is required,
			// but modulo is safer for emulators handling bad headers).
			baseBankIndex %= prgBankCount;

			if (addr < 0xC000) {
				// Lower 16KB ($8000-$BFFF)
				return cart->prgBanks[baseBankIndex][addr - 0x8000];
			} else {
				// Upper 16KB ($C000-$FFFF)
				// We need the next 16KB chunk in the vector
				// Check bounds in case we are at the very end of the vector
				if (baseBankIndex + 1 < (int)cart->prgBanks.size()) {
					return cart->prgBanks[baseBankIndex + 1][addr - 0xC000];
				}
			}
		}
		return 0;
	}

	void write(uint16_t addr, uint8_t value) override {
		if (addr >= 0x8000) {
			// Bit 0-2: Select 32KB PRG Bank
			prgBank = value & 0x07;

			// Bit 4: Select Single Screen Mirroring Page (0 or 1)
			mirrorPage = (value & 0x10) >> 4;
		}
	}

	uint8_t readChr(uint16_t addr) override {
		// AxROM almost exclusively uses CHR-RAM (Bank 0)
		if (addr < 0x2000) {
			return cart->chrBanks[0][addr];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		// Allow writing to CHR-RAM
		if (addr < 0x2000) {
			cart->chrBanks[0][addr] = value;
		}
	}

	int mirrorNametable(int ntIdx) override {
		// Single Screen Mirroring.
		// Regardless of the virtual nametable index (0-3),
		// we return the physical page set by the register (0 or 1).
		return mirrorPage;
	}
};
