#include "composite.hpp"
#include "cart.hpp"
#include "ppu.hpp"

Composite::Composite() {

}






void Composite::renderScanline(int scanline) {
	int pixel = scanline * 256; // not << 8 in case of negative scanlines

	// overscan lines (not visible)
	if (scanline < 0 || scanline >= 240) {
		return;
	}

	// actual bg color
	uint32_t bgColorIdx = ppu->palette[0] & 0x3F; // get background color index from palette
	uint32_t bgColor = defaultARGBpal[bgColorIdx]; // get ARGB color from palette

	for (int x = 0; x < 256; x++) { // fill line with bg color
		frameBuffer[pixel + x] = bgColor;
	}

	// back priority sprites
	uint32_t spriteBackLine[256] = {0};
	if (ppu->MASKshowSprites())
		renderSpritesAtLine(scanline, 1, spriteBackLine);
	// render background tiles
	uint32_t bgLine[256] = {0};
	if (ppu->MASKshowBackground())
		renderBackgroundAtLine(scanline, bgLine);
	// front priority sprites
	uint32_t spriteFrontLine[256] = {0};
	if (ppu->MASKshowSprites())
		renderSpritesAtLine(scanline, 0, spriteFrontLine);

	// composite bg and sprites onto frame buffer
	for (int x = 0; x < 256; x++) {
		// if sprite pixel is not transparent, draw it over bg
		if ((spriteFrontLine[x] & 0xFF000000) != 0) {
			frameBuffer[pixel + x] = spriteFrontLine[x];
		} else if ((bgLine[x] & 0xFF000000) != 0) {
			frameBuffer[pixel + x] = bgLine[x];
		} else if ((spriteBackLine[x] & 0xFF000000) != 0) {
			frameBuffer[pixel + x] = spriteBackLine[x];
		}
	}
}

void Composite::renderBackgroundAtLine(int scanline, uint32_t* lineBuf) {
	LoopyRegister lineV = ppu->v;
	uint8_t fineX = ppu->x;

	for (int t = 0; t < 33; t++) {
		uint16_t ntBase = 0x2000 | (lineV.raw & 0x0C00);
		uint16_t tileAddr = ntBase | (lineV.raw & 0x03FF);

		uint8_t tileIdx = ppu->readNametable(tileAddr);

		uint16_t attrAddr = ntBase | 0x3C0 | ((lineV.coarseY / 4) << 3) | (lineV.coarseX / 4);
		uint8_t attrByte = ppu->readNametable(attrAddr);
		int quadrant = ((lineV.coarseY % 4) / 2) * 2 + ((lineV.coarseX % 4) / 2);
		uint8_t paletteIdx = (attrByte >> (quadrant * 2)) & 0x03;

		uint8_t low = cart->readChr(ppu->CTRLbackgroundPatternTableAddress() | (tileIdx * 16 + lineV.fineY));
		uint8_t high = cart->readChr(ppu->CTRLbackgroundPatternTableAddress() | (tileIdx * 16 + lineV.fineY + 8));

		for (int p = 0; p < 8; p++) {
			int screenX = (t * 8) + p - fineX;
			if (screenX < 0 || screenX >= 256) continue;

			if (screenX < 8 && !ppu->MASKshowBackgroundLeft()) continue;

			int bit = 7 - p;
			uint8_t colorIdx = ((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01);

			if (colorIdx != 0) {
				lineBuf[screenX] = ppu->decodedPalette[paletteIdx * 4 + colorIdx];
			}
		}

		if (lineV.coarseX == 31) {
			lineV.coarseX = 0;
			lineV.nametableX = ~lineV.nametableX;
		} else {
			lineV.coarseX++;
		}
	}
}

void Composite::renderSpritesAtLine(int scanline, int priority, uint32_t* lineBuf) {
	for (int s = 63; s >= 0; s--) { // 0 rendered on top
		if (((ppu->oam.sprites[s].attr & 0x20) >> 5) != priority) {
			continue; // skip sprites that don't match the priority
		}

		int spriteY = ppu->oam.sprites[s].y + 1;

		int y = scanline - spriteY;
		if (y < 0 || y >= 8) continue; // tile not on this line

		int spriteX = ppu->oam.sprites[s].x;
		int spriteIdx = ppu->oam.sprites[s].tileIdx;
		uint8_t attributes = ppu->oam.sprites[s].attr;

		bool flipX = (attributes & 0x40) != 0;
		bool flipY = (attributes & 0x80) != 0;
		uint8_t paletteIndex = (attributes & 0x03) + 4; // sprite palettes start at index 4

		// get the full tile data from CHR ROM/RAM
		// each tile is 16 bytes. the first 8 bytes are the low bits of each pixel row,
		// and the next 8 bytes are the high bits of each pixel row.
		// 2 bits per pixel, a bit from each byte, so 2 bytes per row:
		// (08)(08)(08)(08)(08)(08)(08)(08)
		// (19)(19)(19)(19)(19)(19)(19)(19)
		// (2A)(2A)(2A)(2A)(2A)(2A)(2A)(2A)
		// (3B)(3B)(3B)(3B)(3B)(3B)(3B)(3B)
		// etc...

		// we only need one row (2 bytes) at a time
		uint8_t highByte, lowByte;

		int row = flipY ? (7 - y) : y;
		highByte = cart->readChr(ppu->CTRLspritePatternTableAddress() | (spriteIdx * 16 + row));
		lowByte  = cart->readChr(ppu->CTRLspritePatternTableAddress() | (spriteIdx * 16 + row + 8));

		for (int x = 0; x < 8; x++) {
			int bit = flipX ? x : (7 - x);
			uint8_t bit0 = (highByte >> bit) & 0x01;
			uint8_t bit1 = (lowByte >> bit) & 0x01;
			uint8_t colorIdx = (bit1 << 1) | bit0;

			if (spriteX + x < 0 || spriteX + x >= 256) continue; // pixel out of bounds

			if (colorIdx != 0) {
				lineBuf[spriteX + x] = ppu->decodedPalette[paletteIndex * 4 + colorIdx];
			}
		}
	}
}

uint32_t* Composite::getBuffer() {
	return frameBuffer;
}

void Composite::connectPPU(PPU* ppuRef) {
	ppu = ppuRef;
}

void Composite::disconnectPPU() {
	ppu = nullptr;
}

void Composite::connectCart(Cart* cartRef) {
	cart = cartRef;
}

void Composite::disconnectCart() {
	cart = nullptr;
}
