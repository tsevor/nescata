#include "core.hpp"
#include "cart.hpp"

#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {
	if (argc > 2) {
		std::cout << "usage: nescata rom.nes\n";
	}
	Core core;
	Cart* cart = new Cart(argv[1]);
	
	if (cart->loadStatus != Cart::LOAD_SUCCESS) {
		switch (cart->loadStatus) {
			case Cart::LOAD_FILE_NOT_FOUND:
				std::cout << "File not found: ";
				break;
			case Cart::LOAD_INVALID_FORMAT:
				std::cout << "Invalid ROM format: ";
				break;
			case Cart::LOAD_UNSUPPORTED_MAPPER:
				std::cout << "Unsupported mapper: ";
				break;
			case Cart::LOAD_EMPTY:
				std::cout << "Empty ROM: ";
				break;
			default:
				std::cout << "Failed to load ROM: ";
				break;
		}
		return 1;
	}

	core.connectCart(cart);
	core.setController1(STANDARD);
	// core.setController2(STANDARD);
	core.run();
	return 0;
}
