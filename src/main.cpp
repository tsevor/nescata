#include "core.hpp"
#include "cart.hpp"


int main(int argc, char* argv[]) {
	Core core;
	Cart* cart = new Cart(argc > 1 ? argv[1] : "");

	core.connectCart(cart);
	core.setController1(STANDARD);
	// core.setController2(STANDARD);
	core.run();
	return 0;
}
