#pragma once

#include <cstdint>
#include <string>
#include <ctime>
#include <SDL2/SDL.h>

struct Message {
	std::string text;
	uint32_t textColor; // 0xAARRGGBB
	std::time_t timestamp = SDL_GetTicks64(); // time when message was created
	int timeToLive = 5000; // milliseconds, -1 means infinite

	Message(const std::string& text, uint32_t textColor, int timeToLive = 5000) :
		text(text), textColor(textColor), timeToLive(timeToLive) {}
};
