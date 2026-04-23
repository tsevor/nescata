#include "ppu.hpp"
#include "cart.hpp"
#include "composite.hpp"
#include "cpu.hpp"

PPU::PPU() {
	reset();
}

void PPU::reset() {
	// Initialize PPU state and memory to deterministic values
	cycle = 0;
	dot = 0;
	scanline = 0;
	frame = 0;
	buffer = 0;
	oamaddr = 0;
	oamdata = 0;
	ctrl.raw = 0;
	mask.raw = 0;
	stat.raw = 0;
	w = true;

	// Clear VRAM and OAM
	// VRAM expanded to 0x1000 to safely buffer 4-screen mirroring configs
	for (int i = 0; i < 0x1000; i++) vram[i] = 0;
	for (int i = 0; i < 256; i++) oam.raw[i] = 0;
}

bool PPU::step(int cycles) {
	bool frameComplete = false;

	while (cycles > 0) {
		dot++;
		cycle++;
		cycles--;

		// 1. Visible scanline and Pre-render line fetches
		if (scanline < 240 || scanline == 261) {
			if (dot == 257) {
				if (MASKshowBackground() || MASKshowSprites()) {
					v.coarseX = t.coarseX;
					v.nametableX = t.nametableX;
				}
			}
			if (dot == 260) {
				if (MASKshowBackground() || MASKshowSprites()) {
					cart->clockIRQ();
				}
			}
		}

		// 2. Sprite 0 Hit Evaluation
		if (MASKshowSprites() && scanline < 240 && dot == 260) {
			int spriteY = oam.sprites[0].y + 1;
			if (scanline >= spriteY && scanline < spriteY + 8) {
				int y = scanline - spriteY;
				int spriteIdx = oam.sprites[0].tileIdx;
				uint8_t attr = oam.sprites[0].attr;
				bool flipY = (attr & 0x80) != 0;
				int row = flipY ? (7 - y) : y;

				uint8_t low  = cart->readChr(CTRLspritePatternTableAddress() | (spriteIdx * 16 + row));
				uint8_t high = cart->readChr(CTRLspritePatternTableAddress() | (spriteIdx * 16 + row + 8));

				if (low | high) stat.S = 1;
			}
		}

		// 3. VBlank / NMI Trigger (Guaranteed to hit precisely once at dot 1)
		if (scanline == 241 && dot == 1) {
			stat.V = 1;
			stat.S = 0;
			if (CTRLgenerateNMI() && cpu) cpu->triggerNMI();
		}

		// 4. End of Scanline Logic
		if (dot == 341) {
			dot = 0;

			if (scanline == 261) {
				// End of Pre-render line -> Frame complete
				if (MASKshowBackground() || MASKshowSprites()) {
					v.raw = t.raw;
				}
				stat.V = 0;
				stat.S = 0;
				stat.O = 0;
				scanline = 0;
				frame++;
				frameComplete = true;
			} else {
				if (scanline < 240) {
					if (!skipFrame) {
						comp->renderScanline(scanline);
					}
					if (MASKshowBackground() || MASKshowSprites()) {
						incrementY();
					}
				}
				scanline++;
			}
		}
	}

	return frameComplete;
}

// PPU Register Read/Writes

// PPUCTRL

uint8_t PPU::CTRLread() {
	return ctrl.raw;
}

uint16_t PPU::CTRLnametableAddress() {
	switch (ctrl.N) {
		case 0: return 0x2000;
		case 1: return 0x2400;
		case 2: return 0x2800;
		case 3: return 0x2C00;
		default: return 0x2000; // Should not happen
	}
}

uint8_t PPU::CTRLvramAddressIncrement() {
	return (ctrl.I == 0) ? 1 : 32;
}

uint16_t PPU::CTRLspritePatternTableAddress() {
	return (ctrl.S == 0) ? 0x0000 : 0x1000;
}

uint16_t PPU::CTRLbackgroundPatternTableAddress() {
	return (ctrl.B == 0) ? 0x0000 : 0x1000;
}

uint8_t PPU::CTRLspriteSize() {
	return (ctrl.H == 0) ? 8 : 16;
}

bool PPU::CTRLisMaster() {
	return (ctrl.P == 0);
}

bool PPU::CTRLgenerateNMI() {
	return (ctrl.V != 0);
}

// PPUMASK

uint8_t PPU::MASKread() {
	return mask.raw;
}

void PPU::MASKwrite(uint8_t value) {
	mask.raw = value;
}

bool PPU::MASKisGrayscale() {
	return (mask.g != 0);
}

bool PPU::MASKshowBackgroundLeft() {
	return (mask.m != 0);
}

bool PPU::MASKshowSpritesLeft() {
	return (mask.M != 0);
}

bool PPU::MASKshowBackground() {
	return (mask.b != 0);
}

bool PPU::MASKshowSprites() {
	return (mask.s != 0);
}

ColorEmphasis PPU::MASKgetEmphasis() {
	ColorEmphasis emp;
	emp.R = mask.R;
	emp.G = mask.G;
	emp.B = mask.B;
	return emp;
}

// PPUSTAT

void PPU::STATwrite(uint8_t value) {
	// PPUSTAT writes are ignored
}

uint8_t PPU::STATread() {
	uint8_t value = stat.raw;
	stat.V = 0;
	w = true; // Reading status resets the write toggle
	return value;
}

bool PPU::STATisInVBlank() {
	return stat.V != 0;
}

bool PPU::STATsprite0Hit() {
	return stat.S != 0;
}

bool PPU::STATspriteOverflow() {
	return stat.O != 0;
}

// OAMADDR
uint8_t PPU::OAMADDRread() {
	return oamaddr;
}

void PPU::OAMADDRwrite(uint8_t value) {
	oamaddr = value;
}

// OAMDATA
uint8_t PPU::OAMDATAread() {
	return oam.raw[oamaddr];
}

void PPU::OAMDATAwrite(uint8_t value) {
	oam.raw[oamaddr] = value;
	oamaddr++; // increment address (wraps naturally via uint8)
}

void PPU::OAMDMAwrite(uint8_t* values) {
	// Write 256 bytes into OAM starting at current OAMADDR and wrap around (uint8)
	uint8_t addr = oamaddr;
	for (int i = 0; i < 256; i++) {
		oam.raw[addr] = values[i];
		addr++;
	}
	oamaddr = addr; // store updated address (wraps naturally via uint8)
}

// LoopyRegisters

void PPU::CTRLwrite(uint8_t value) {
	ctrl.raw = value;
	t.nametableX = ctrl.N & 0x01;
	t.nametableY = (ctrl.N >> 1) & 0x01;
}

void PPU::SCRLwrite(uint8_t value) {
	if (w) {
		t.coarseX = value >> 3;
		x = value & 0x07;
	} else {
		t.fineY = value & 0x07;
		t.coarseY = value >> 3;
	}
	w = !w;
}

void PPU::ADDRwrite(uint8_t value) {
	if (w) {
		t.raw = (t.raw & 0x80FF) | ((value & 0x3F) << 8);
	} else {
		t.raw = (t.raw & 0xFF00) | value;
		v.raw = t.raw;
	}
	w = !w;
}

void PPU::incrementX() {
	if (v.coarseX == 31) {
		v.coarseX = 0;
		v.nametableX ^= 1;
	} else {
		v.coarseX++;
	}
}

void PPU::incrementY() {
	if (v.fineY < 7) {
		v.fineY++;
	} else {
		v.fineY = 0;
		if (v.coarseY == 29) {
			v.coarseY = 0;
			v.nametableY ^= 1;
		} else if (v.coarseY == 31) {
			v.coarseY = 0;
		} else {
			v.coarseY++;
		}
	}
}

uint8_t PPU::read() {
	uint16_t a = v.raw & 0x3FFF;
	if (a >= 0x3000 && a <= 0x3EFF) a -= 0x1000;

	uint8_t result = 0;
	if (a < 0x2000)
		result = useBuffer(cart->readChr(a));
	else if (a < 0x3000)
		result = useBuffer(readNametable(a));
	else
	switch (a) {
		case 0x3F10:
		case 0x3F14:
		case 0x3F18:
		case 0x3F1C:
			result = palette[(a ^ 0x10) & 0x1F];
			buffer = readNametable(a - 0x1000);
			break;
		case 0x3F00 ... 0x3F0F:
		case 0x3F11 ... 0x3F13:
		case 0x3F15 ... 0x3F17:
		case 0x3F19 ... 0x3F1B:
		case 0x3F1D ... 0x3FFF:
			result = palette[a & 0x1F];
			buffer = readNametable(a - 0x1000);
			break;
		default:
			result = buffer;
			break;
	}

	v.raw += CTRLvramAddressIncrement();
	return result;
}

void PPU::write(uint8_t value) {
	uint16_t a = v.raw & 0x3FFF;
	if (a >= 0x3000 && a <= 0x3EFF) a -= 0x1000;

	switch (a) {
		case 0x0000 ... 0x1FFF:
			cart->writeChr(a, value);
			break;
		case 0x2000 ... 0x2FFF:
			writeNametable(a, value);
			break;
		case 0x3F10:
		case 0x3F14:
		case 0x3F18:
		case 0x3F1C:
			palette[(a ^ 0x10) & 0x1F] = value;
			decodedPalette[(a ^ 0x10) & 0x1F] = defaultARGBpal[value & 0x3F] | 0xFF000000;
			break;
		case 0x3F00 ... 0x3F0F:
		case 0x3F11 ... 0x3F13:
		case 0x3F15 ... 0x3F17:
		case 0x3F19 ... 0x3F1B:
		case 0x3F1D ... 0x3FFF:
			palette[a & 0x1F] = value;
			decodedPalette[a & 0x1F] = defaultARGBpal[value & 0x3F] | 0xFF000000;
			break;
		default:
			break;
	}

	v.raw += CTRLvramAddressIncrement();
}

uint8_t PPU::readNametable(uint16_t addr) {
	uint16_t ntIndex = (addr >> 10) & 0x03;
	uint16_t mirroredNtIndex = cart->mirrorNametable(ntIndex);
	uint16_t ntAddress = mirroredNtIndex << 10;
	return vram[ntAddress | (addr & 0x3FF)];
}

void PPU::writeNametable(uint16_t addr, uint8_t value) {
	uint16_t ntIndex = (addr >> 10) & 0x03;
	uint16_t mirroredNtIndex = cart->mirrorNametable(ntIndex);
	uint16_t ntAddress = mirroredNtIndex << 10;
	vram[ntAddress | (addr & 0x3FF)] = value;
}

uint8_t PPU::useBuffer(uint8_t value) {
	uint8_t tmp = buffer;
	buffer = value;
	return tmp;
}

void PPU::connectComposite(Composite* compRef) {
	comp = compRef;
}

void PPU::disconnectComposite() {
	comp = nullptr;
}

void PPU::connectCPU(CPU* cpuRef) {
	cpu = cpuRef;
}

void PPU::disconnectCPU() {
	cpu = nullptr;
}

void PPU::connectCart(Cart* cartRef) {
	cart = cartRef;
}

void PPU::disconnectCart() {
	cart = nullptr;
}
