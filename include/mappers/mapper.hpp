#pragma once

#include <vector>
#include <array>

#include <cstdint>

class Cart;

class Mapper {
protected:
	int mapperID;
	int prgBankCount;
	int chrBankCount;

	Cart* cart;

public:

	virtual uint8_t read(uint16_t addr) {return 0;}
	virtual void write(uint16_t addr, uint8_t value) {}
	virtual uint8_t readChr(uint16_t addr) {return 0;}
	virtual void writeChr(uint16_t addr, uint8_t value) {}
	virtual int mirrorNametable(int ntIdx) {return ntIdx;}
	virtual void reset() {}
	~Mapper() = default;
};
