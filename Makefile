.PHONY: all clean windows linux run

all: windows linux

# ------------------------------------------
# Windows Build (Separated Objects)
# ------------------------------------------
windows:
	mkdir -p build/obj
	# 1. Generate Icon
	magick resources/logo.svg -background none -define icon:auto-resize=256,128,64,48,32,16 resources/logo.ico
	# 2. Compile Resource Object to build/obj
	x86_64-w64-mingw32-windres src/resources.rc -o build/obj/resources.o
	# 3. Final Link
	cp /usr/x86_64-w64-mingw32/bin/SDL2.dll build/
	x86_64-w64-mingw32-g++ -std=c++17 -g -Iinclude -o build/nescata.exe src/*.cpp build/obj/resources.o -static -static-libgcc -static-libstdc++ -L/usr/x86_64-w64-mingw32/lib -lmingw32 -lSDL2main /usr/x86_64-w64-mingw32/lib/libSDL2.a -mwindows -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid

# ------------------------------------------
# Linux Build (Dynamic)
# ------------------------------------------
linux:
	mkdir -p build
	g++ -std=c++17 -g -Iinclude -o build/nescata src/*.cpp `pkg-config --cflags --libs sdl2`

# ------------------------------------------
# Helper Commands
# ------------------------------------------
run:
	mkdir -p build
	g++ -std=c++17 -g -Iinclude -o build/nescata src/*.cpp `pkg-config --cflags --libs sdl2`
	./build/nescata

clean:
	rm -rf build
