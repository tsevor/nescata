#pragma once

#include <cstdint>

class Cart;

class Mapper {
protected:
	int mapperID;
	int prgBankCount;
	int chrBankCount;

	Cart* cart;

public:
	virtual uint8_t read(uint16_t addr) {(void)addr; return 0;}
	virtual void write(uint16_t addr, uint8_t value) {(void)addr; (void)value;}
	virtual uint8_t readChr(uint16_t addr) {(void)addr; return 0;}
	virtual void writeChr(uint16_t addr, uint8_t value) {(void)addr; (void)value;}
	virtual int mirrorNametable(int ntIdx) {return ntIdx;}
	virtual void clockIRQ() {}
	virtual void reset() {}
	virtual ~Mapper() = default;
};
