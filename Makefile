.PHONY: all clean windows linux run

all: windows linux

windows: clean
	mkdir -p build/obj
	magick resources/logo.svg -background none -define icon:auto-resize=256,128,64,48,32,16 resources/logo.ico
	x86_64-w64-mingw32-windres src/resources.rc -o build/obj/resources.o
	cp /usr/x86_64-w64-mingw32/bin/SDL2.dll build/
	x86_64-w64-mingw32-g++ -std=c++17 -O3 -flto -Werror -Wall -Iinclude -o build/nescata.exe `find src -name "*.cpp"` build/obj/resources.o -static -static-libgcc -static-libstdc++ -L/usr/x86_64-w64-mingw32/lib -lmingw32 -lSDL2main /usr/x86_64-w64-mingw32/lib/libSDL2.a -mwindows -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid

linux: clean
	mkdir -p build
	g++ -std=c++17 -O3 -flto -Werror -Wall -Iinclude -o build/nescata `find src -name "*.cpp"` `pkg-config --cflags --libs sdl2`

run: clean linux
	./build/nescata "$(ARGS)"

debug:
	mkdir -p build
	g++ -std=c++17 -g -O3 -flto -Werror -Wall -Iinclude -o build/nescata `find src -name "*.cpp"` `pkg-config --cflags --libs sdl2`
	gdb build/nescata


clean:
	rm -rf build
