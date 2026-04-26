#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>

#include <SDL2/SDL.h>

#include "apu.hpp"
#include "bus.hpp"
#include "cart.hpp"
#include "composite.hpp"
#include "controller.hpp"
#include "cpu.hpp"
#include "palettes.hpp"
#include "ppu.hpp"
#include "window.hpp"
#include "ui/message.hpp"

class Core {
public:
	Bus bus;
	CPU cpu;
	PPU ppu;
	Composite comp;
	APU apu;
	Window window;
	Controller controller1;
	Controller controller2;

	Cart* cart = nullptr;

	bool enableWindow = true;

	// time management
	double emulationSpeed = 1.0; // 1.0 = normal speed
	double prevEmulationSpeed = 1.0;
	bool paused = false;
	bool passFrame = false; // used when paused to advance a single frame

	// audio
	uint8_t scaledAudioBuffer[7350]; // down to 10% speed
	double audioVolume = 0.5;

	// messaging system
	std::vector<Message> messages;
	void addMessage(const std::string& text, uint32_t textColor, int timeToLive = 5000);
	void dismissMessage(size_t index); // manual dismissal
	void dismissMessage(); // most recent infinite message
	void updatePromptMessage(std::string newString);
	void updateMessages();
	void renderMessages();

	// states
	void rebindKeys();
	int getScancodeOfSingleKey();
	std::string getStrInput(std::string prompt);
	void parseCommand(std::string command);

	// commands
	void commandTogglePause();
	void commandQuit();
	void commandFrameAdvance();
	void commandSpeedUp(double factor);
	void commandSlowDown(double factor);
	void commandSetSpeed(double speed);
	void commandVolumeUp(double factor);
	void commandVolumeDown(double factor);
	void commandSetVolume(double volume);
	void commandLoadROM(std::string filename);
	uint8_t gGCharToHex(char c);
	void addGameGenieCheat(std::string cheatCode);
	void addCheat(uint16_t addr, uint8_t val);

	int lastKeyScancode = -1;
	bool rebindInProgress = false;
	bool awaitingTextInput = false;
	uint8_t lastKeyStates[1];
	std::string inputString;
	std::string inputPrompt;

	enum class Keybind {
		A,
		B,
		START,
		SELECT,
		UP,
		DOWN,
		LEFT,
		RIGHT
	};

	int keys[8] = {
		SDL_SCANCODE_S,     // A
		SDL_SCANCODE_A,     // B
		SDL_SCANCODE_W,     // START
		SDL_SCANCODE_Q,     // SELECT
		SDL_SCANCODE_UP,    // UP
		SDL_SCANCODE_DOWN,  // DOWN
		SDL_SCANCODE_LEFT,  // LEFT
		SDL_SCANCODE_RIGHT, // RIGHT
	};

	Core();

	void run();
	void reset();
	void powerOn();
	void fullReset();

	void handleWindowEvents();
	void handleKeyboardEvent(SDL_KeyboardEvent keyEvent);
	uint8_t getControllerButtonState() const;
	void processHeldKeys();

	void connectCart(Cart* cart);
	void disconnectCart();
	void setController1(ControllerType type);
	void setController2(ControllerType type);

	// tieg
	void randomizeMemory(int numBytes);
};
