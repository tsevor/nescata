#pragma once

#include <SDL2/SDL_render.h>
#include <cstdint>
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <ctime>

#include <SDL2/SDL.h>

const int WIDTH = 256;
const int HEIGHT = 240;
const int PIXEL_SCALE = 2;

class Window {
private:
	bool keep_window_open = true;
	int64_t timeAlive = 0;

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr; // Used for the emulator framebuffer

	SDL_AudioDeviceID audio_device = 0;
	SDL_AudioSpec audio_spec;
	std::queue<std::vector<uint8_t>> audio_queue;

	SDL_Texture* cachedFont = nullptr;

	// Helper to extract RGBA from uint32 (ARGB8888)
	void setRenderColor(uint32_t color);

	void cacheFont();

public:
	Window() {};
	~Window() {
		SDL_DestroyTexture(cachedFont);
	}

	int StartWindow();
	bool pollEvent(SDL_Event* event);
	void updateSurface(double emulationSpeed = 1.0, bool skipRender = false);
	void closeWindow();

	void waitForVsync();

	// Drawing functions
	void fillRect(int x, int y, int w, int h, uint32_t color);
	void fillScreen(uint32_t color);

	// Note: getPixel is very slow on Renderers and is rarely needed for rendering logic.
	// It is kept for compatibility but should be avoided.
	uint32_t getPixel(int x, int y);

	void drawPixel(int x, int y, uint32_t color);
	void drawBuffer(uint32_t* buffer);
	void setLogicalSize(int width, int height);

	// Audio functions
	bool initAudio(int frequency = 44100, uint16_t format = AUDIO_S16SYS, int channels = 1, int samples = 2048);
	void setAudioQueue(uint8_t* buffer, int size);
	void queueAudio(uint8_t* buffer, int size);
	void pauseAudio(bool pause_on);
	uint32_t getQueuedAudioSize() const;
	void clearAudioQueue();
	void closeAudio();

	// text rendering functions
	void drawChar(int x, int y, char c, uint32_t textColor = 0xFFFFFFFF, uint32_t bgColor = 0x7F000000);
	void drawText(int x, int y, const std::string& text, uint32_t textColor = 0xFFFFFFFF, uint32_t bgColor = 0x7F000000);
};
