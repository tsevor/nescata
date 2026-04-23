#pragma once

#include <cstdint>
#include "mapper.hpp"
#include "bus.hpp"

class MMC3 final : public Mapper {
private:
	uint8_t targetRegister = 0;
	bool prgMode = false;
	bool chrMode = false;
	uint8_t bankRegisters[8] = {0};

	uint32_t prgOffset[4];
	uint32_t chrOffset[8];

	uint8_t prgRam[0x2000];
	uint8_t prgRamProtect = 0;

	uint32_t totalPrgSize;
	uint32_t totalChrSize;

	uint8_t irqCounter = 0;
	uint8_t irqReloadValue = 0;
	bool irqEnable = false;
	bool irqReloadRequested = false;
	bool irqActive = false;

	uint8_t mirroring = 0;

	void updateBanks() {
		uint32_t prgBankCount8K = prgBankCount * 2;
		uint32_t lastBank = prgBankCount8K - 1;
		uint32_t secondLastBank = prgBankCount8K - 2;

		// MMC3 explicitly ignores the top two bits of R6 and R7
		uint32_t r6 = bankRegisters[6] & 0x3F;
		uint32_t r7 = bankRegisters[7] & 0x3F;

		if (!prgMode) {
			prgOffset[0] = (r6 * 0x2000) % totalPrgSize;
			prgOffset[1] = (r7 * 0x2000) % totalPrgSize;
			prgOffset[2] = (secondLastBank * 0x2000) % totalPrgSize;
			prgOffset[3] = (lastBank * 0x2000) % totalPrgSize;
		} else {
			prgOffset[0] = (secondLastBank * 0x2000) % totalPrgSize;
			prgOffset[1] = (r7 * 0x2000) % totalPrgSize;
			prgOffset[2] = (r6 * 0x2000) % totalPrgSize;
			prgOffset[3] = (lastBank * 0x2000) % totalPrgSize;
		}

		if (!chrMode) {
			chrOffset[0] = ((bankRegisters[0] & 0xFE) * 0x0400) % totalChrSize;
			chrOffset[1] = ((bankRegisters[0] | 0x01) * 0x0400) % totalChrSize;
			chrOffset[2] = ((bankRegisters[1] & 0xFE) * 0x0400) % totalChrSize;
			chrOffset[3] = ((bankRegisters[1] | 0x01) * 0x0400) % totalChrSize;
			chrOffset[4] = (bankRegisters[2] * 0x0400) % totalChrSize;
			chrOffset[5] = (bankRegisters[3] * 0x0400) % totalChrSize;
			chrOffset[6] = (bankRegisters[4] * 0x0400) % totalChrSize;
			chrOffset[7] = (bankRegisters[5] * 0x0400) % totalChrSize;
		} else {
			chrOffset[0] = (bankRegisters[2] * 0x0400) % totalChrSize;
			chrOffset[1] = (bankRegisters[3] * 0x0400) % totalChrSize;
			chrOffset[2] = (bankRegisters[4] * 0x0400) % totalChrSize;
			chrOffset[3] = (bankRegisters[5] * 0x0400) % totalChrSize;
			chrOffset[4] = ((bankRegisters[0] & 0xFE) * 0x0400) % totalChrSize;
			chrOffset[5] = ((bankRegisters[0] | 0x01) * 0x0400) % totalChrSize;
			chrOffset[6] = ((bankRegisters[1] & 0xFE) * 0x0400) % totalChrSize;
			chrOffset[7] = ((bankRegisters[1] | 0x01) * 0x0400) % totalChrSize;
		}
	}

public:
	MMC3(Cart* cartRef) {
		cart = cartRef;
		mapperID = 4;
		prgBankCount = cart->romBankCount;
		chrBankCount = cart->chrBankCount;

		totalPrgSize = prgBankCount * 0x4000;
		totalChrSize = (chrBankCount == 0) ? 0x2000 : (chrBankCount * 0x2000);

		for (int i = 0; i < 0x2000; i++) prgRam[i] = 0;

		reset();
	}

	void reset() override {
		targetRegister = 0;
		prgMode = false;
		chrMode = false;
		for (int i = 0; i < 8; i++) bankRegisters[i] = 0;
		prgRamProtect = 0x80; // Explicitly enable WRAM on boot
		mirroring = 0;

		irqCounter = 0;
		irqReloadValue = 0;
		irqEnable = false;
		irqReloadRequested = false;
		irqActive = false;

		if (cart && cart->bus && cart->bus->irqLine) {
			*(cart->bus->irqLine) = false;
		}

		updateBanks();
	}

	uint8_t read(uint16_t addr) override {
		if (addr >= 0x8000) {
			uint16_t offset = addr & 0x1FFF;
			if (addr < 0xA000) return cart->prgData[prgOffset[0] + offset];
			if (addr < 0xC000) return cart->prgData[prgOffset[1] + offset];
			if (addr < 0xE000) return cart->prgData[prgOffset[2] + offset];
			return cart->prgData[prgOffset[3] + offset];
		} else if (addr >= 0x6000 && addr <= 0x7FFF) {
			// Read permitted if Enable bit (7) is set
			if (prgRamProtect & 0x80) {
				return prgRam[addr & 0x1FFF];
			}
		}
		return 0;
	}

	void write(uint16_t addr, uint8_t value) override {
		if (addr >= 0x8000) {
			bool isEven = !(addr & 1);
			if (addr <= 0x9FFF) {
				if (isEven) {
					targetRegister = value & 0x07;
					prgMode = (value & 0x40) != 0;
					chrMode = (value & 0x80) != 0;
					updateBanks();
				} else {
					bankRegisters[targetRegister] = value;
					updateBanks();
				}
			} else if (addr <= 0xBFFF) {
				if (isEven) {
					mirroring = value & 1;
				} else {
					prgRamProtect = value;
				}
			} else if (addr <= 0xDFFF) {
				if (isEven) {
					irqReloadValue = value;
				} else {
					irqReloadRequested = true;
				}
			} else {
				if (isEven) {
					irqEnable = false;
					irqActive = false;
					if (cart && cart->bus && cart->bus->irqLine) {
						*(cart->bus->irqLine) = false;
					}
				} else {
					irqEnable = true;
				}
			}
		} else if (addr >= 0x6000 && addr <= 0x7FFF) {
			// Write permitted if Enable bit (7) is set AND Write-Protect bit (6) is clear
			if ((prgRamProtect & 0x80) && !(prgRamProtect & 0x40)) {
				prgRam[addr & 0x1FFF] = value;
			}
		}
	}

	uint8_t readChr(uint16_t addr) override {
		if (addr < 0x2000) {
			return cart->chrData[chrOffset[addr >> 10] + (addr & 0x03FF)];
		}
		return 0;
	}

	void writeChr(uint16_t addr, uint8_t value) override {
		if (chrBankCount == 0 && addr < 0x2000) {
			cart->chrData[chrOffset[addr >> 10] + (addr & 0x03FF)] = value;
		}
	}

	int mirrorNametable(int ntIdx) override {
		if (cart->fourScreen) {
			return ntIdx; // Or however your bus routes 4 independent nametables
		}
		if (mirroring == 0) {
			return (ntIdx % 2) ? 1 : 0;
		} else {
			return (ntIdx / 2) ? 1 : 0;
		}
	}

	bool irqState()  {
		return irqActive;
	}

	void clockIRQ() override {
		if (irqCounter == 0 || irqReloadRequested) {
			irqCounter = irqReloadValue;
			irqReloadRequested = false;
		} else {
			irqCounter--;
		}

		if (irqCounter == 0 && irqEnable) {
			irqActive = true;
			if (cart && cart->bus && cart->bus->irqLine) {
				*(cart->bus->irqLine) = true;
			}
		}
	}
};
