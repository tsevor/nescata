#include "core.hpp"


Core::Core() {
	cpu.connectBus(&bus);
	bus.connectAPU(&apu);
	bus.connectPPU(&ppu);
	ppu.connectComposite(&comp);
	ppu.connectCPU(&cpu);
	comp.connectPPU(&ppu);
	bus.connectController1(&controller1);
	bus.connectController2(&controller2);
}

void Core::run() {
	if (enableWindow) {
		if (window.StartWindow() != 0) {
			std::cerr << "Failed to start window!" << std::endl;
			return;
		}

	}

	cpu.powerOn();
	cpu.reset();
	while (true) {
		if (paused || emulationSpeed == 0.0) {
			// paused
			if (!passFrame) {
				SDL_Delay(100);
				uint32_t* frameBuffer = comp.getBuffer();
				if (frameBuffer) {
					window.drawBuffer(frameBuffer);
				}
				handleWindowEvents();
				window.updateSurface(1.0);
			}
		}
		if (cpu.clock()) { // returns true if frame has completed
			uint32_t* frameBuffer = comp.getBuffer();
			if (frameBuffer) {
				window.drawBuffer(frameBuffer);
			}
			std::vector<uint8_t> audioBuffer = apu.getAudioBuffer();
			window.queueAudio(&audioBuffer);
			handleWindowEvents();
			// 9999 to skip delay when advancing a single frame
			window.updateSurface(passFrame ? 9999 : emulationSpeed);
			passFrame = false;
		}
	}
}

void Core::reset() {
	cpu.reset();
	if (cart)
		if (cart->mapper)
			cart->mapper->reset();
}

void Core::powerOn() {
	cpu.powerOn();
}

void Core::fullReset() {
	cpu.powerOn();
	cpu.reset();
}

void Core::handleWindowEvents() {
	SDL_Event event;
	while (window.pollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				window.closeWindow();
				exit(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				handleKeyboardEvent(event.key);
				break;
			default:
				break;
		}
	}

	processHeldKeys();
	updateMessages();
	renderMessages();

	// update controller state from current keyboard state
	uint8_t buttonState = getControllerButtonState();
	controller1.setState(buttonState);
}

void Core::handleKeyboardEvent(SDL_KeyboardEvent keyEvent) {
	bool pressed = (keyEvent.type == SDL_KEYDOWN && keyEvent.repeat == 0);
	const uint8_t* keyboardState = SDL_GetKeyboardState(NULL);
	bool modShift = keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];

	if (awaitingTextInput) {
		if (pressed) {
			if (keyEvent.keysym.sym == SDLK_ESCAPE) {
				awaitingTextInput = false;
				inputString.clear();
			} else if (keyEvent.keysym.sym == SDLK_RETURN) {
				awaitingTextInput = false;
			} else if (keyEvent.keysym.sym == SDLK_BACKSPACE) {
				if (!inputString.empty()) {
					inputString.pop_back();
				}
			} else {
				char c = keyEvent.keysym.sym;
				if (c >= 32 && c <= 126) { // printable characters
					inputString += c;
				}
			}
			updatePromptMessage(inputPrompt + inputString);
		}
		return;
	}

	switch (keyEvent.keysym.sym) {
		case SDLK_r: // reset
			if (pressed) {
				if (modShift) {
					// reset everything
					bus.clearMem();
					cpu.powerOn();
					ppu.reset();
					cart->mapper->reset();
				}
				reset();
			}
			break;
		case SDLK_ESCAPE: // quit
			if (pressed) commandTogglePause();
			break;
		case SDLK_b: // start rebind flow
			if (pressed) rebindKeys();
			break;
		case SDLK_f: // advance a single frame when paused
			if (pressed) commandFrameAdvance();
			break;
		case SDLK_k:
			if (pressed) {
				cpu.jammed = !cpu.jammed;
				if (cpu.jammed)
					addMessage("CPU killed", 0xFFFF0000);
				else
					addMessage("CPU revived??", 0xFF00FF00);
			}
			break;
		case SDLK_g:
			if (pressed) randomizeMemory(100);
			break;
		// speed controls
		case SDLK_EQUALS: // speed up
			if (pressed) commandSpeedUp(1.1);
			break;
		case SDLK_MINUS: // slow down
			if (pressed) commandSlowDown(1.1);
			break;
		case SDLK_h: // help
			if (pressed) {
				addMessage("Keybinds:", 0xFFFFFF00);
				addMessage("R - Reset", 0xFFFFFF00);
				addMessage("ESC - Pause/Unpause", 0xFFFFFF00);
				addMessage("B - Rebind Controller Keys", 0xFFFFFF00);
				addMessage("F - Advance Single Frame (when paused)", 0xFFFFFF00);
				addMessage("K - Toggle CPU killed state", 0xFFFFFF00);
				addMessage("G - Randomize 100 Bytes in Memory", 0xFFFFFF00);
				addMessage(". - Sprint while held", 0xFFFFFF00);
				addMessage("+ - Increase Emulation Speed", 0xFFFFFF00);
				addMessage("- - Decrease Emulation Speed", 0xFFFFFF00);
				addMessage("; - Command line mode", 0xFFFFFF00);
			}
			break;
		case SDLK_SEMICOLON:
			if (pressed) {
				std::string input = getStrInput("> ");
				parseCommand(input);
			}
			break;
		case SDLK_PERIOD:
			if (pressed) {
				prevEmulationSpeed = emulationSpeed;
				emulationSpeed = 99999;
			}
		default:
			if (pressed && rebindInProgress) {
				lastKeyScancode = keyEvent.keysym.scancode;
				rebindInProgress = false;
			}
			break;
	}
}

uint8_t Core::getControllerButtonState() const {
	const uint8_t* keyboardState = SDL_GetKeyboardState(NULL);
	uint8_t buttonState = (
		(keyboardState[keys[(int)Keybind::A]]      << 0) | // A
		(keyboardState[keys[(int)Keybind::B]]      << 1) | // B
		(keyboardState[keys[(int)Keybind::SELECT]] << 2) | // Select
		(keyboardState[keys[(int)Keybind::START]]  << 3) | // Start
		(keyboardState[keys[(int)Keybind::UP]]     << 4) | // Up
		(keyboardState[keys[(int)Keybind::DOWN]]   << 5) | // Down
		(keyboardState[keys[(int)Keybind::LEFT]]   << 6) | // Left
		(keyboardState[keys[(int)Keybind::RIGHT]]  << 7)   // Right
	);
	return buttonState;
}

void Core::processHeldKeys() {
	const uint8_t* keyboardState = SDL_GetKeyboardState(NULL);

	if (keyboardState[SDL_SCANCODE_PERIOD]) {
		lastKeyStates[0] = 1;
	} else if (lastKeyStates[0]) {
		emulationSpeed = prevEmulationSpeed;
		lastKeyStates[0] = 0;
	}
}

void Core::addMessage(const std::string& text, uint32_t textColor, int timeToLive) {
	// add to back of message list
	messages.emplace_back(Message(text, textColor, timeToLive));
}

void Core::dismissMessage(size_t index) {
	// dismiss message at index
	if (index < messages.size()) {
		messages.erase(messages.begin() + index);
	}
}

void Core::dismissMessage() {
	// dismiss most recent infinite message
	for (size_t i = 0; i < messages.size(); ++i) {
		if (messages[i].timeToLive == -1) {
			messages.erase(messages.begin() + i);
			break;
		}
	}
}

void Core::updatePromptMessage(std::string newString) {
	for (size_t i = 0; i < messages.size(); ++i) {
		if (messages[i].timeToLive == -1) {
			messages[i].text = newString;
			break;
		}
	}
}

void Core::updateMessages() {
	for (int i = 0; i < messages.size();) {
		int currentTime = SDL_GetTicks64();
		if (currentTime - messages[i].timestamp >= messages[i].timeToLive && messages[i].timeToLive != -1) {
			// remove message
			messages.erase(messages.begin() + i);
		} else {
			i++;
		}
	}
}

void Core::renderMessages() {
	int xOffset = 10;
	int yOffset = 10;
	for (const Message& message : messages) {
		// render each message at (xOffset, yOffset)
		window.drawText(xOffset, yOffset, message.text, message.textColor);
		yOffset += 8; // move down for next message
	}
}

std::string Core::getStrInput(std::string prompt) {
	awaitingTextInput = true;
	inputString.clear();
	addMessage(prompt, 0xFFFFFF00, -1); // infinite time to live
	inputPrompt = prompt;
	// wait until input is complete
	while (awaitingTextInput) {
		SDL_Delay(100);
		uint32_t* frameBuffer = comp.getBuffer();
		if (frameBuffer) {
			window.drawBuffer(frameBuffer);
		}
		handleWindowEvents();
		window.updateSurface(1.0);
	}
	dismissMessage(); // remove prompt message
	return inputString;
}

void Core::rebindKeys() {
	paused = true;
	// Start interactive rebind: ask user to choose which button to rebind
	addMessage("Rebind mode", 0xFFFFFF00);
	addMessage("Press the key for A button", 0xFFFFFF00);
	int newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::A] = newScancode;
	addMessage("A button rebound", 0xFF00FF00);
	addMessage("Press the key for B button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::B] = newScancode;
	addMessage("B button rebound", 0xFF00FF00);
	addMessage("Press the key for SELECT button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::SELECT] = newScancode;
	addMessage("SELECT button rebound", 0xFF00FF00);
	addMessage("Press the key for START button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::START] = newScancode;
	addMessage("START button rebound", 0xFF00FF00);
	addMessage("Press the key for UP button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::UP] = newScancode;
	addMessage("UP button rebound", 0xFF00FF00);
	addMessage("Press the key for DOWN button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::DOWN] = newScancode;
	addMessage("DOWN button rebound", 0xFF00FF00);
	addMessage("Press the key for LEFT button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::LEFT] = newScancode;
	addMessage("LEFT button rebound", 0xFF00FF00);
	addMessage("Press the key for RIGHT button", 0xFFFFFF00);
	newScancode = getScancodeOfSingleKey();
	keys[(int)Keybind::RIGHT] = newScancode;
	addMessage("RIGHT button rebound", 0xFF00FF00);
	addMessage("Rebind complete", 0xFF00FF00);
	paused = false;
}

int Core::getScancodeOfSingleKey() {
	rebindInProgress = true;
	lastKeyScancode = -1;
	// wait until a key is pressed
	while (rebindInProgress) {
		SDL_Delay(100);
		uint32_t* frameBuffer = comp.getBuffer();
		if (frameBuffer) {
			window.drawBuffer(frameBuffer);
		}
		handleWindowEvents();
		window.updateSurface(1.0);
	}
	return lastKeyScancode;
}

void Core::parseCommand(std::string command) {
	// split command into tokens at spaces
	std::istringstream iss(command);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token) {
		tokens.push_back(token);
	}
	if (tokens.empty()) return;
	// handle commands
	if (tokens[0] == "reset") {
		reset();
	} else if (tokens[0] == "power") {
		powerOn();
	} else if (tokens[0] == "pause") {
		commandTogglePause();
	} else if (tokens[0] == "quit" || tokens[0] == "exit") {
		commandQuit();
	} else if (tokens[0] == "speed") {
		if (tokens.size() == 2) {
			double speed = std::stod(tokens[1]);
			commandSetSpeed(speed);
		}
	} else if (tokens[0] == "setmem") {
		if (tokens.size() == 3) {
			// parse address and value (accept hex like 0xNNNN or decimal)
			try {
				unsigned long a = std::stoul(tokens[1], nullptr, 0);
				unsigned long v = std::stoul(tokens[2], nullptr, 0);
				uint16_t addr = static_cast<uint16_t>(a & 0xFFFF);
				uint8_t value = static_cast<uint8_t>(v & 0xFF);
				bus.write(addr, value);
				std::ostringstream oss;
				oss << "Memory at 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr
					<< " set to 0x" << std::setw(2) << static_cast<int>(value);
				addMessage(oss.str(), 0xFFFFFF00);
			} catch (...) {
				addMessage("Invalid address or value for setmem", 0xFFFF0000);
			}
		}
	} else if (tokens[0] == "getmem") {
		if (tokens.size() == 2) {
			try {
				unsigned long a = std::stoul(tokens[1], nullptr, 0);
				uint16_t addr = static_cast<uint16_t>(a & 0xFFFF);
				uint8_t value = bus.read(addr);
				std::ostringstream oss;
				oss << "Memory at 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr
					<< " = 0x" << std::setw(2) << static_cast<int>(value);
				addMessage(oss.str(), 0xFFFFFF00);
			} catch (...) {
				addMessage("Invalid address for getmem", 0xFFFF0000);
			}
		}
	} else if (tokens[0] == "loadrom") {
		if (tokens.size() == 2) {
			// join rest of tokens as filename (in case of spaces)
			std::string filename = tokens[1];
			for (size_t i = 2; i < tokens.size(); ++i) {
				filename += " " + tokens[i];
			}
			commandLoadROM(filename);
		}
	} else if (tokens[0] == "randomize") {
		if (tokens.size() == 2) {
			try {
				int n = std::stoi(tokens[1]);
				randomizeMemory(n);
				addMessage("Randomized memory: " + std::to_string(n) + " bytes", 0xFFFFFF00);
			} catch (...) {
				addMessage("Invalid number for randomize", 0xFFFF0000);
			}
		} else {
			addMessage("Usage: randomize <bytes>", 0xFFFFFF00);
		}
	} else if (tokens[0] == "cheat") {
		if (tokens.size() == 3) {
			try {
				unsigned long a = std::stoul(tokens[1], nullptr, 0);
				unsigned long v = std::stoul(tokens[2], nullptr, 0);
				uint16_t addr = a & 0xFFFF;
				uint8_t value = v & 0xFF;
				addCheat(addr, value);
				std::ostringstream oss;
				oss << "Cheat at 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr
					<< " set to 0x" << std::setw(2) << value;
				addMessage(oss.str(), 0xFFFFFF00);
			} catch (...) {
				addMessage("Invalid address or value for cheat", 0xFFFF0000);
			}
		}
	} else if (tokens[0] == "ggcheat") {
		if (tokens.size() == 2) {
			addGameGenieCheat(tokens[1]);
		}
	} else if (tokens[0] == "cheats") {
		if (tokens.size() == 1) {
			for (std::pair<uint16_t, uint8_t> cheat : bus.cheats) {
				std::ostringstream oss;
				oss << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
					<< "0x" << cheat.first << " = 0x" << std::setw(2) << cheat.second;
				addMessage(oss.str(), 0xFFFFFF00);
			}
		}
	} else if (tokens[0] == "rmcheat") {
		if (tokens.size() == 2) {
			try {
				unsigned long a = std::stoul(tokens[1], nullptr, 0);
				uint16_t addr = a & 0xFFFF;
				bus.cheats.erase(addr);
				std::ostringstream oss;
				oss << "removed cheat at 0x" << std::hex << std::uppercase
					<< std::setfill('0') << std::setw(4) << addr;
			} catch (...) {
				addMessage("Invalid address or value for setmem", 0xFFFF0000);
			}
		}
	} else if (tokens[0] == "help") {
		addMessage("available commands:", 0xFFFFFF00);
		addMessage("reset - reset the rom", 0xFFFFFF00);
		addMessage("poweron - power cycle the emulator", 0xFFFFFF00);
		addMessage("pause - toggle pause/unpause", 0xFFFFFF00);
		addMessage("quit/exit - quit the emulator", 0xFFFFFF00);
		addMessage("speed <speed> - set emulation speed", 0xFFFFFF00);
		addMessage("setmem <addr> <value> - set memory", 0xFFFFFF00);
		addMessage("getmem <addr> - get memory at address", 0xFFFFFF00);
		addMessage("loadrom <filename> - load ROM from file", 0xFFFFFF00);
		addMessage("cheat <addr> <value> - set a cheat", 0xFFFFFF00);
		addMessage("ggcheat <code> - set a game genie cheat", 0xFFFFFF00);
		addMessage("cheats - list all cheats", 0xFFFFFF00);
		addMessage("rmcheat <addr> - removes a cheat by addr", 0xFFFFFF00);
	} else {
		addMessage("Unknown command: " + tokens[0], 0xFFFF0000);
	}
}

void Core::commandTogglePause() {
	paused = !paused;
	if (paused) {
		addMessage("Emulation paused.", 0xFFFFFF00);
	} else {
		addMessage("Emulation resumed.", 0xFF00FF00);
	}
}

void Core::commandQuit() {
	window.closeWindow();
	exit(0);
}

void Core::commandFrameAdvance() {
	passFrame = true;
	if (!paused) paused = true;
}

void Core::commandSpeedUp(double factor) {
	emulationSpeed *= factor;
	if (emulationSpeed > 50) {
		emulationSpeed = 50;
	}
	addMessage("Emulation speed: " + std::to_string(emulationSpeed) + "x", 0xFFFFFF00);
}

void Core::commandSlowDown(double factor) {
	emulationSpeed /= factor;
	if (emulationSpeed < 0.1) {
		emulationSpeed = 0.1;
	}
	addMessage("Emulation speed: " + std::to_string(emulationSpeed) + "x", 0xFFFFFF00);
}

void Core::commandSetSpeed(double speed) {
	emulationSpeed = speed;
	addMessage("Emulation speed: " + std::to_string(emulationSpeed) + "x", 0xFFFFFF00);
}

void Core::commandLoadROM(std::string filename) {
	// load ROM from filename
	Cart* newCart = new Cart(filename);
	if (newCart->loadStatus != Cart::LOAD_SUCCESS) {
		switch (newCart->loadStatus) {
			case Cart::LOAD_FILE_NOT_FOUND:
				addMessage("File not found: " + newCart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_INVALID_FORMAT:
				addMessage("Invalid ROM format: " + newCart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_UNSUPPORTED_MAPPER:
				addMessage("Unsupported mapper: " + newCart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_EMPTY:
				addMessage("Empty ROM: " + newCart->filename, 0xFFFF0000);
				break;
			default:
				addMessage("Failed to load ROM: " + newCart->filename, 0xFFFF0000);
				break;
		}
		// keep the current cart if load failed
		delete newCart;
		return;
	}

	// connect new cart (this takes ownership via pointer)
	connectCart(newCart);
	// fullReset();
	// dont reset, leave that to user
}

uint8_t Core::gGCharToHex(char c) {
	c = toupper(c);
    switch (c) {
        case 'A': return 0x0;
        case 'P': return 0x1;
        case 'Z': return 0x2;
        case 'L': return 0x3;
        case 'G': return 0x4;
        case 'I': return 0x5;
        case 'E': return 0x6;
        case 'Y': return 0x7;
        case 'X': return 0x8;
        case 'U': return 0x9;
        case 'K': return 0xA;
        case 'O': return 0xB;
        case 'T': return 0xC;
        case 'V': return 0xD;
        case 'S': return 0xE;
        case 'N': return 0xF;
        default: return 0xFF; // Error case
    }
}

void Core::addGameGenieCheat(std::string cheatCode) {
    if (cheatCode.length() != 6) {
        addMessage("Code must be 6 characters!", 0xFFFF0000);
        return;
    }

    // Convert string characters to their 4-bit integer values (C0 through C5)
    uint8_t C0 = gGCharToHex(cheatCode[0]);
    uint8_t C1 = gGCharToHex(cheatCode[1]);
    uint8_t C2 = gGCharToHex(cheatCode[2]);
    uint8_t C3 = gGCharToHex(cheatCode[3]);
    uint8_t C4 = gGCharToHex(cheatCode[4]);
    uint8_t C5 = gGCharToHex(cheatCode[5]);

    if (C0 == 0xFF || C1 == 0xFF || C2 == 0xFF ||
        C3 == 0xFF || C4 == 0xFF || C5 == 0xFF) {
        addMessage("Invalid code!", 0xFFFF0000);
        return;
    }

    // Decode the Address (15-bit value starting at 0x8000)
    // Formula: 0x8000 + ((C3&7)<<12) | ((C5&7)<<8) | ((C4&8)<<8) | ((C2&7)<<4) | ((C1&8)<<4) | (C4&7) | (C3&8)
    uint16_t addr = 0x8000 |
        ((C3 & 7) << 12) |
        ((C5 & 7) << 8)  |
        ((C4 & 8) << 8)  |
        ((C2 & 7) << 4)  |
        ((C1 & 8) << 4)  |
        (C4 & 7)         |
        (C3 & 8);

    // Decode the Value (8-bit replacement data)
    // Formula: ((C1&7)<<4) | ((C0&8)<<4) | (C0&7) | (C5&8)
    uint8_t val =
        ((C1 & 7) << 4)  |
        ((C0 & 8) << 4)  |
        (C0 & 7)         |
        (C5 & 8);

    addCheat(addr, val);
	std::ostringstream oss;
	oss << "Cheat at 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr
		<< " set to 0x" << std::setw(2) << static_cast<int>(val);
	addMessage(oss.str(), 0xFFFFFF00);
}


void Core::addCheat(uint16_t addr, uint8_t val) {
	bus.cheats[addr] = val;
}

void Core::connectCart(Cart* cart) {
	this->cart = cart;
	bus.connectCart(cart);
	comp.connectCart(cart);
	ppu.connectCart(cart);
	if (!cart) {

	} else if (cart->blank) {
		switch (cart->loadStatus) {
			case Cart::LOAD_FILE_NOT_FOUND:
				addMessage("File not found: " + cart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_INVALID_FORMAT:
				addMessage("Invalid ROM format: " + cart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_UNSUPPORTED_MAPPER:
				addMessage("Unsupported mapper: " + cart->filename, 0xFFFF0000);
				break;
			case Cart::LOAD_EMPTY:
				addMessage("Empty ROM: " + cart->filename, 0xFFFF0000);
				break;
		}
	} else { // success
		addMessage("ROM loaded: " + cart->filename, 0xFF00FF00);
	}
}

void Core::disconnectCart() {
	this->cart = nullptr;
	bus.disconnectCart();
	comp.disconnectCart();
	ppu.disconnectCart();
}

void Core::setController1(ControllerType type) {
	controller1 = Controller(type);
}

void Core::setController2(ControllerType type) {
	controller2 = Controller(type);
}

// tieg
#include <random>

void Core::randomizeMemory(int numBytes) {
    // Set up a random number generator
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<uint16_t> addr_dist(0, 0xFFFF); // Distribution for 16-bit addresses
    std::uniform_int_distribution<uint8_t> value_dist(0, 0xFF);   // Distribution for 8-bit values

    for (int i = 0; i < numBytes; ++i) {
        uint16_t addr = addr_dist(gen);
        uint8_t value = value_dist(gen);
        bus.write(addr, value);
    }

    addMessage("Randomized!", 0xFFFFFF00);
}
