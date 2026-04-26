#include "core.hpp"
#include "cart.hpp"

#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {
	if (argc > 2) {
		std::cout << "usage: nescata rom.nes\n";
	}

	Core* core = new Core();
	Cart* cart = new Cart(argc == 2 ? argv[1] : "");

	core->connectCart(cart);
	core->setController1(STANDARD);
	// core.setController2(STANDARD);
	core->run();
	return 0;
}
