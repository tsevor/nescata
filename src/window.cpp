#include "window.hpp"
#include "ui/font.hpp"

int Window::StartWindow() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		std::cout << "Failed to initialize the SDL2 library\n";
		return -1;
	}

	window = SDL_CreateWindow(
		"nescata",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WIDTH * PIXEL_SCALE,
		HEIGHT * PIXEL_SCALE,
		SDL_WINDOW_RESIZABLE // Added resizable as Renderer handles scaling nicely
	);

	if (!window) {
		std::cout << "Failed to create window\n";
		return -1;
	}

	// Create Renderer (Hardware Accelerated) and enable VSync to sync to display refresh
	renderer = SDL_CreateRenderer(window, -1, 0);// | SDL_RENDERER_PRESENTVSYNC);

	if (!renderer) {
		std::cout << "Failed to create renderer: " << SDL_GetError() << "\n";
		// Fallback to software if hardware fails
		renderer = SDL_CreateRenderer(window, -1, 0);
		if (!renderer) return -1;
	}

	// Setup Logical Size:
	// The renderer will act as if the screen is 256x240.
	// SDL will automatically scale this up to the actual window size (WIDTH * PIXEL_SCALE).
	SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);

	// Enable Alpha Blending (replaces helper compositeColors)
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	// Create a streaming texture for the NES buffer
	texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		WIDTH,
		HEIGHT
	);

	if (!texture) {
		std::cout << "Failed to create texture: " << SDL_GetError() << "\n";
		return -1;
	}

	if (!initAudio(44100, AUDIO_U8, 1, 1024)) {
		std::cout << "Audio initialization failed, continuing without sound.\n";
	}

	return 0;
}

bool Window::pollEvent(SDL_Event* event) {
	return SDL_PollEvent(event);
}

void Window::updateSurface(double emulationSpeed, bool skipRender) {
	if (emulationSpeed <= 0.0) emulationSpeed = 1.0;

	if (!skipRender) {
		SDL_RenderPresent(renderer);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
	}

	// If emulationSpeed is huge, just skip the delay
	if (emulationSpeed >= 100) {
		return;
	}

	static uint64_t lastTime = SDL_GetPerformanceCounter();
	uint64_t currentTime;
	double targetFrameTime = (1.0 / 60.0) / emulationSpeed; // Target time in SECONDS
	double elapsedTime;

	// Spin-lock loop for microsecond accuracy
	while (true) {
		currentTime = SDL_GetPerformanceCounter();
		elapsedTime = (double)(currentTime - lastTime) / SDL_GetPerformanceFrequency();

		if (elapsedTime >= targetFrameTime) {
			break;
		}

		// If we have more than 2 milliseconds to wait, yield to the OS to save CPU.
		// Otherwise, burn CPU cycles in the while loop to hit the exact microsecond.
		if (targetFrameTime - elapsedTime > 0.002) {
			SDL_Delay(1);
		}
	}

	// Do NOT set lastTime = SDL_GetPerformanceCounter().
	// Add the target time to the last time to ensure absolute sync over time,
	// absorbing any micro-stutters.
	lastTime += (uint64_t)(targetFrameTime * SDL_GetPerformanceFrequency());

	// If the emulator falls severely behind (e.g. window was dragged), reset the clock
	// so it doesn't run at 1000 FPS trying to catch up.
	if ((SDL_GetPerformanceCounter() - lastTime) > SDL_GetPerformanceFrequency()) {
		lastTime = SDL_GetPerformanceCounter();
	}
}

void Window::closeWindow() {
	closeAudio();
	if (texture) SDL_DestroyTexture(texture);
	if (renderer) SDL_DestroyRenderer(renderer);
	if (window) SDL_DestroyWindow(window);
	SDL_Quit();
}

void Window::waitForVsync() {
	// SDL_RENDERER_PRESENTVSYNC in StartWindow handles this generally,
	// but manual waiting can stay empty or use SDL_Delay logic.
}

// Helpers

void Window::setRenderColor(uint32_t color) {
	// Assuming ARGB8888 based on previous code usage
	Uint8 a = (color >> 24) & 0xFF;
	Uint8 r = (color >> 16) & 0xFF;
	Uint8 g = (color >> 8) & 0xFF;
	Uint8 b = (color >> 0) & 0xFF;
	SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

// Drawing functions

void Window::fillRect(int x, int y, int w, int h, uint32_t color) {
	// No need to multiply by PIXEL_SCALE, RenderSetLogicalSize handles it
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	setRenderColor(color);
	SDL_RenderFillRect(renderer, &rect);
}

void Window::fillScreen(uint32_t color) {
	setRenderColor(color);
	SDL_RenderClear(renderer);
}

uint32_t Window::getPixel(int x, int y) {
	// WARNING: Reading pixels from a hardware renderer is extremely slow (stall).
	// This functionality is deprecated in a Renderer workflow.

	void* pixels = malloc(sizeof(uint32_t) * 1);
	SDL_Rect rect = { x, y, 1, 1 };

	// We must read into the format we expect (ARGB8888)
	SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_ARGB8888, pixels, sizeof(uint32_t));

	uint32_t color = *(uint32_t*)pixels;
	free(pixels);
	return color;
}

void Window::drawPixel(int x, int y, uint32_t color) {
	// Logic for Alpha Blending is now handled by SDL_SetRenderDrawBlendMode
	// set in StartWindow. We don't need manual composition or getPixel.

	setRenderColor(color);
	SDL_RenderDrawPoint(renderer, x, y);
}

void Window::drawBuffer(uint32_t* buffer) {
	if (!buffer || !texture) {
		return;
	}

	// Update the texture with the new pixel data
	// 256 * sizeof(uint32_t) is the pitch (bytes per row)
	SDL_UpdateTexture(texture, nullptr, buffer, 256 * sizeof(uint32_t));

	// Copy the texture to the renderer.
	// passing nullptr for source/dest rects uses the full texture and fills the logical screen
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);
}

void Window::setLogicalSize(int width, int height) {
	if (renderer) {
		SDL_RenderSetLogicalSize(renderer, width, height);
	}
}

// Audio functions (Unchanged)
bool Window::initAudio(int frequency, uint16_t format, int channels, int samples) {
	SDL_AudioSpec wanted;
	wanted.freq = frequency;
	wanted.format = format;
	wanted.channels = channels;
	wanted.samples = samples;
	wanted.callback = nullptr;
	wanted.userdata = nullptr;

	audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &audio_spec, 0);
	if (audio_device == 0) {
		std::cout << "Failed to open audio device: " << SDL_GetError() << std::endl;
		return false;
	}

	SDL_PauseAudioDevice(audio_device, 0);
	return true;
}

void Window::queueAudio(uint8_t* buffer, int size) {
	if (audio_device != 0 && buffer) {
		SDL_QueueAudio(audio_device, buffer, size);
	}
}

void Window::pauseAudio(bool pause_on) {
	if (audio_device != 0) {
		SDL_PauseAudioDevice(audio_device, pause_on ? 1 : 0);
	}
}

uint32_t Window::getQueuedAudioSize() const {
	if (audio_device != 0) {
		return SDL_GetQueuedAudioSize(audio_device);
	}
	return 0;
}

void Window::clearAudioQueue() {
	if (audio_device != 0) {
		SDL_ClearQueuedAudio(audio_device);
	}
}

void Window::closeAudio() {
	if (audio_device != 0) {
		SDL_CloseAudioDevice(audio_device);
		audio_device = 0;
	}
}

void Window::cacheFont() {
	cachedFont = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		FONT_TABLE_LENGTH * FONT_WIDTH,
		FONT_HEIGHT
	);

	SDL_SetTextureBlendMode(cachedFont, SDL_BLENDMODE_BLEND);

	uint32_t* pixels;
	int pitch;

	SDL_LockTexture(cachedFont, NULL, (void**)&pixels, &pitch);

	for (int c = 0; c < FONT_TABLE_LENGTH; c++) {
		for (int row = 0; row < FONT_HEIGHT; row++) {
			uint8_t bits = font6x8[c][row];
			for (int col = 0; col < FONT_WIDTH; col++) {
				bool bit = (bits >> (7 - col)) & 1;
				int pixelX = (c * FONT_WIDTH) + col;
				int pixelY = row;
				pixels[pixelY * (pitch / 4) + pixelX] = bit ? 0xFFFFFFFF : 0x00000000;
			}
		}
	}

	SDL_UnlockTexture(cachedFont);
	SDL_SetTextureBlendMode(cachedFont, SDL_BLENDMODE_BLEND);
}

void Window::drawChar(int x, int y, char c, uint32_t textColor, uint32_t bgColor) {
	if (!cachedFont) cacheFont();

	uint8_t code = c;

	if (code < FONT_TABLE_START || code > FONT_TABLE_START + FONT_TABLE_LENGTH) {
		code = FONT_TABLE_START;
	}

	code = code - FONT_TABLE_START;

	SDL_Rect src = {code * FONT_WIDTH, 0, FONT_WIDTH, FONT_HEIGHT};
	SDL_Rect dst = {x, y, FONT_WIDTH, FONT_HEIGHT};

	if ((bgColor >> 24) & 0xFF) {
		SDL_SetRenderDrawColor(
			renderer,
			(bgColor >> 16) & 0xFF,
			(bgColor >> 8) & 0xFF,
			bgColor & 0xFF,
			(bgColor >> 24) & 0xFF
		);
		SDL_RenderFillRect(renderer, &dst);
	}

	SDL_SetTextureColorMod(cachedFont, (textColor >> 16) & 0xFF, (textColor >> 8) & 0xFF, textColor & 0xFF);
	SDL_SetTextureAlphaMod(cachedFont, (textColor >> 24) & 0xFF);

	SDL_RenderCopy(renderer, cachedFont, &src, &dst);
}

void Window::drawText(int x, int y, const std::string& text, uint32_t textColor, uint32_t bgColor) {
	if (!renderer) return;

	int px = x;
	for (char c : text) {
		uint8_t code = static_cast<uint8_t>(c);

		if (code == '\n') {
			px = x;
			y += 8;
			continue;
		}

		drawChar(px, y, code, textColor, bgColor);

		px += 6;
	}
}
