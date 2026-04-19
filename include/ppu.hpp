#pragma once

#include <cstdint>
#include <iostream>

#include "registers.hpp"

// Forward declarations
class Cart;
class Composite;
class CPU;

class PPU {
	friend Composite;
private:


	// PPU REGISTERS
	PPUCTRL ctrl;
	PPUMASK mask;
	PPUSTAT stat;
	uint8_t oamaddr;
	uint8_t oamdata;

	LoopyRegister v; // Current VRAM address (15 bits)
	LoopyRegister t; // Temporary VRAM address (15 bits)
	uint8_t x;       // Fine X scroll (3 bits)
	bool w;          // Write toggle (1 bit)

	uint8_t vram[0x1000]; // PPU VRAM
	OAM oam;              // Object Attribute Memory (OAM)
	uint8_t palette[32];  // Palette Memory
	uint8_t buffer;       // Internal read buffer for PPUDATA reads
	
	uint32_t decodedPalette[32]; // Caching

	int cycle;          // Current PPU cycle
	int dot;            // Current PPU dot (pixel) within the scanline
	int scanline;       // Current PPU scanline
	int frame;          // Current frame count

	Cart* cart = nullptr;
	Composite* comp = nullptr;
	CPU* cpu = nullptr;

public:
	PPU();
	
	bool skipFrame = false;

	void reset();

	// PPU Register Read/Writes

	// PPUCTRL
	uint8_t CTRLread();
	void CTRLwrite(uint8_t value);
	uint16_t CTRLnametableAddress();
	uint8_t CTRLvramAddressIncrement();
	uint16_t CTRLspritePatternTableAddress();
	uint16_t CTRLbackgroundPatternTableAddress();
	uint8_t CTRLspriteSize();
	bool CTRLisMaster();
	bool CTRLgenerateNMI();

	// PPUMASK
	uint8_t MASKread();
	void MASKwrite(uint8_t value);
	bool MASKisGrayscale();
	bool MASKshowBackgroundLeft();
	bool MASKshowSpritesLeft();
	bool MASKshowBackground();
	bool MASKshowSprites();
	ColorEmphasis MASKgetEmphasis();

	// PPUSTAT
	uint8_t STATread();
	void STATwrite(uint8_t value);
	bool STATisInVBlank();
	bool STATsprite0Hit();
	bool STATspriteOverflow();

	// OAMADDR
	uint8_t OAMADDRread();
	void OAMADDRwrite(uint8_t value);

	// OAMDATA
	uint8_t OAMDATAread();
	void OAMDATAwrite(uint8_t value);

	void OAMDMAwrite(uint8_t* values);

	// LoopyRegisters
	void SCRLwrite(uint8_t value);
	void ADDRwrite(uint8_t value);
	void incrementX();
	void incrementY();

	// Read and Write
	uint8_t read();
	void write(uint8_t value);

	uint8_t readNametable(uint16_t addr);
	void writeNametable(uint16_t addr, uint8_t value);

	// PPU Cycles
	bool step(int cycles);

	uint8_t useBuffer(uint8_t value);

	void connectComposite(Composite* comp);
	void disconnectComposite();
	void connectCPU(CPU* cpu);
	void disconnectCPU();
	void connectCart(Cart* cartRef);
	void disconnectCart();
};

#include "cpu.hpp"
