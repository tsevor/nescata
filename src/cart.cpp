#include "cart.hpp"
#include "mappers/NROM.hpp"  // mapper 0
#include "mappers/MMC1.hpp"  // mapper 1
#include "mappers/AxROM.hpp" // mapper 7

Cart::Cart() {

}

Cart::Cart(std::string fName) {
	blank = true; // Default to blank until successfully loaded
	mapper = nullptr;

	filename = fName;

	if (filename.empty()) {
		loadStatus = LOAD_EMPTY;
		return; // silently return if no filename provided
	}

	// Load ROM file
	FILE* romFile = fopen(filename.c_str(), "rb");
	if (!romFile) {
		loadStatus = LOAD_FILE_NOT_FOUND;
		return; // silently return if file cannot be opened
	}

	// Read header
	fread(header, sizeof(uint8_t), 16, romFile);

	// Verify NES file format
	if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
		fclose(romFile);
		loadStatus = LOAD_INVALID_FORMAT;
		return; // silently return if invalid NES file format
	}

	// Parse header information
	romBankCount = header[4];
	romSize = romBankCount * 0x4000; // 16KB units
	chrBankCount = header[5];
	chrSize = chrBankCount * 0x2000; // 8KB units

	uint8_t control1 = header[6];
	uint8_t control2 = header[7];

	mapperID = (control1 >> 4) | (control2 & 0xF0);

	fourScreen        = (control1 & 0b00001000) != 0;
	hasTrainer        = (control1 & 0b00000100) != 0;
	batteryBacked     = (control1 & 0b00000010) != 0;
	verticalMirroring = (control1 & 0b00000001) != 0;

	if (fourScreen) {
		mirroring = FOUR_SCREEN;
	} else if (verticalMirroring) {
		mirroring = VERTICAL;
	} else {
		mirroring = HORIZONTAL;
	}

	iNESVersion = (control2 >> 2) & 0b00000011;
	trainerSize = hasTrainer ? 512 : 0;

	// Skip trainer if present
	if (hasTrainer) {
		fseek(romFile, 512, SEEK_CUR);
	}

	// Load PRG ROM banks
	for (int i = 0; i < romBankCount; i++) {
		std::array<uint8_t, 0x4000> bank{};
		fread(bank.data(), sizeof(uint8_t), 0x4000, romFile);
		prgBanks.push_back(bank);
	}
	// Load CHR ROM banks
	for (int i = 0; i < chrBankCount; i++) {
		std::array<uint8_t, 0x2000> bank{};
		fread(bank.data(), sizeof(uint8_t), 0x2000, romFile);
		chrBanks.push_back(bank);
	}

	fclose(romFile);

	pickMapper(mapperID);

	if (mapper) {
		loadStatus = LOAD_SUCCESS;
		blank = false;
	} else {
		loadStatus = LOAD_UNSUPPORTED_MAPPER;
	}
}

uint8_t Cart::read(uint16_t addr) {
	if (mapper && !blank) {
		return mapper->read(addr);
		// leave handling up to mapper
	}
	return 0;
}

void Cart::write(uint16_t addr, uint8_t val) {
	if (mapper && !blank) {
		mapper->write(addr, val);
		// leave handling up to mapper
	}
}

uint8_t Cart::readChr(uint16_t addr) {
	if (mapper && !blank) {
		return mapper->readChr(addr);
		// leave handling up to mapper
	}
	return 0;
}

void Cart::writeChr(uint16_t addr, uint8_t val) {
	if (mapper && !blank) {
		mapper->writeChr(addr, val);
		// leave handling up to mapper
	}
}

int Cart::mirrorNametable(int ntIdx) {
	if (mapper)
		return mapper->mirrorNametable(ntIdx);
	return ntIdx; // default no mirror
}

void Cart::pickMapper(int mapperID) {
	switch (mapperID) {
		case 0:
			mapper = new NROM(this);
			break;
		case 1:
			mapper = new MMC1(this);
			break;
		case 7:
			mapper = new AxROM(this);
			break;
		default:
			loadStatus = LOAD_UNSUPPORTED_MAPPER;
			blank = true;
			break;
	}
}
