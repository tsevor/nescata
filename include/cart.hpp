#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <fstream>
#include <string>

#include "mappers/mapper.hpp"

enum MirroringType {
	HORIZONTAL = 0,
	VERTICAL = 1,
	FOUR_SCREEN = 2
};

class Cart {
public:
	bool blank = true; // is the cart empty?

	enum {
		LOAD_SUCCESS,
		LOAD_EMPTY,
		LOAD_FILE_NOT_FOUND,
		LOAD_INVALID_FORMAT,
		LOAD_UNSUPPORTED_MAPPER
	} loadStatus = LOAD_EMPTY;

	std::string filename;

	std::vector<std::array<uint8_t, 0x4000>> prgBanks;
	std::vector<std::array<uint8_t, 0x2000>> chrBanks;

	Mapper* mapper = nullptr;

	uint8_t header[16];
	int romBankCount = 0;
	int romSize = 0;
	int chrBankCount = 0;
	int chrSize = 0;

	int mapperID = 0;

	bool fourScreen = false;
	bool hasTrainer = false;
	bool batteryBacked = false;
	bool verticalMirroring = false;

	MirroringType mirroring = HORIZONTAL;

	int iNESVersion = 1;

	int trainerSize = 0;

	Cart();
	Cart(std::string fName);
	Cart(uint8_t* fileData); // for future plans, undefined at the moment

	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t val);
	uint8_t readChr(uint16_t addr);
	void writeChr(uint16_t addr, uint8_t val);

	int mirrorNametable(int ntIdx);

private:
	void pickMapper(int mapperID);
};
