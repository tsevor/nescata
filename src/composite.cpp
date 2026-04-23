#include "composite.hpp"
#include "cart.hpp"
#include "ppu.hpp"

Composite::Composite() {}

void Composite::renderScanline(int scanline) {
	int pixel = scanline * 256; // not << 8 in case of negative scanlines

	// overscan lines (not visible)
	if (scanline < 0 || scanline >= 240) {
		return;
	}

	// actual bg color
	uint32_t bgColorIdx = ppu->palette[0] & 0x3F; // get background color index from palette
	uint32_t bgColor = defaultARGBpal[bgColorIdx] | 0xFF000000; // Enforce opaque alpha

	for (int x = 0; x < 256; x++) { // fill line with bg color
		frameBuffer[pixel + x] = bgColor;
	}

	// Single pass sprite rendering to enforce correct OAM multiplexing
	uint32_t spriteLine[256] = {0};
	if (ppu->MASKshowSprites())
		renderSpritesAtLine(scanline, spriteLine);

	// render background tiles
	uint32_t bgLine[256] = {0};
	if (ppu->MASKshowBackground())
		renderBackgroundAtLine(scanline, bgLine);

	// composite bg and sprites onto frame buffer
	for (int x = 0; x < 256; x++) {
		uint32_t spr = spriteLine[x];
		uint32_t bg = bgLine[x];
		
		bool hasSpr = (spr & 0xFF000000) != 0;
		bool hasBg = (bg & 0xFF000000) != 0;
		
		// Priority bit was encoded into the alpha channel during the sprite pass
		bool backPriority = (spr & 0xFF000000) == 0xFE000000;

		if (hasSpr && hasBg) {
			if (backPriority) {
				frameBuffer[pixel + x] = bg; // BG wins against back-priority sprite
			} else {
				frameBuffer[pixel + x] = spr | 0xFF000000; // Front sprite wins (restore pure alpha)
			}
		} else if (hasSpr) {
			frameBuffer[pixel + x] = spr | 0xFF000000;
		} else if (hasBg) {
			frameBuffer[pixel + x] = bg;
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
			lineV.nametableX ^= 1;
		} else {
			lineV.coarseX++;
		}
	}
}

void Composite::renderSpritesAtLine(int scanline, uint32_t* lineBuf) {
	int height = ppu->CTRLspriteSize();

	for (int s = 63; s >= 0; s--) { // 0 rendered on top
		int spriteY = ppu->oam.sprites[s].y + 1;
		int y = scanline - spriteY;
		if (y < 0 || y >= height) continue; // tile not on this line

		int spriteX = ppu->oam.sprites[s].x;
		int spriteIdx = ppu->oam.sprites[s].tileIdx;
		uint8_t attributes = ppu->oam.sprites[s].attr;

		bool flipX = (attributes & 0x40) != 0;
		bool flipY = (attributes & 0x80) != 0;
		bool backPriority = (attributes & 0x20) != 0;
		uint8_t paletteIndex = (attributes & 0x03) + 4; // sprite palettes start at index 4

		int row = flipY ? ((height - 1) - y) : y;
		uint16_t patTable;
		uint8_t tile;

		if (height == 8) {
			patTable = ppu->CTRLspritePatternTableAddress();
			tile = spriteIdx;
		} else {
			// 8x16 mode ignores PPUCTRL pattern table and uses the tile's lowest bit
			patTable = (spriteIdx & 1) ? 0x1000 : 0x0000;
			tile = spriteIdx & 0xFE;
			// If the calculated row falls into the bottom 8 pixels, jump to the next tile
			if (row >= 8) {
				tile++;
				row -= 8;
			}
		}

		uint8_t lowByte  = cart->readChr(patTable | (tile * 16 + row));
		uint8_t highByte = cart->readChr(patTable | (tile * 16 + row + 8));

		for (int x = 0; x < 8; x++) {
			int screenX = spriteX + x;
			if (screenX < 0 || screenX >= 256) continue; // pixel out of bounds
			
			// Hardware accuracy: Clip left 8 pixels if requested by PPUMASK
			if (screenX < 8 && !ppu->MASKshowSpritesLeft()) continue;

			int bit = flipX ? x : (7 - x);
			uint8_t bit0 = (lowByte >> bit) & 0x01;
			uint8_t bit1 = (highByte >> bit) & 0x01;
			uint8_t colorIdx = (bit1 << 1) | bit0;

			if (colorIdx != 0) {
				uint32_t color = ppu->decodedPalette[paletteIndex * 4 + colorIdx];
				// Encode priority directly into the alpha channel so the composite loop can resolve it
				lineBuf[screenX] = (color & 0x00FFFFFF) | (backPriority ? 0xFE000000 : 0xFF000000);
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