#pragma once

#include <cstdint>

#include "palettes.hpp"

// Forward declarations
class Cart;
class PPU;

class Composite {
private:
	Cart* cart = nullptr;
	PPU* ppu = nullptr;

	uint32_t frameBuffer[256 * 240]; // NES resolution

public:
	Composite();

	void renderScanline(int scanline);

	void renderBackgroundAtLine(int scanline, uint32_t* lineBuf);
	void renderSpritesAtLine(int scanline, uint32_t* lineBuf);

	uint32_t* getBuffer();

	void connectPPU(PPU* ppu);
	void disconnectPPU();
	void connectCart(Cart* cart);
	void disconnectCart();
};
