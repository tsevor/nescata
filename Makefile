CC = g++
WIN_CC = x86_64-w64-mingw32-g++

CFLAGS = -std=c++17 -O3 -flto -Werror -Wall -Wextra -Iinclude -MMD -MP
WIN_CFLAGS = $(CFLAGS)

LDFLAGS = $(shell pkg-config --cflags --libs sdl2)
WIN_LDFLAGS = -static -static-libgcc -static-libstdc++ -L/usr/x86_64-w64-mingw32/lib -lmingw32 -lSDL2main /usr/x86_64-w64-mingw32/lib/libSDL2.a -mwindows -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid

SOURCES = $(shell find src -name "*.cpp")

LINUX_OBJECTS = $(patsubst src/%.cpp,build/obj/linux/%.o,$(SOURCES))
WIN_OBJECTS = $(patsubst src/%.cpp,build/obj/windows/%.o,$(SOURCES))

.PHONY: all clean windows linux test run debug

all: windows linux



linux: build/nescata

build/nescata: $(LINUX_OBJECTS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/obj/linux/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@



windows: build/nescata.exe

build/nescata.exe: $(WIN_OBJECTS) build/obj/windows/resources.o
	@mkdir -p build
	$(WIN_CC) $(WIN_CFLAGS) -o $@ $^ $(WIN_LDFLAGS)

build/obj/windows/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

build/obj/windows/resources.o: src/resources.rc resources/logo.svg
	@mkdir -p build/obj/windows
	magick resources/logo.svg -background none -define icon:auto-resize=256,128,64,48,32,16 resources/logo.ico
	x86_64-w64-mingw32-windres src/resources.rc -o $@



run: linux
	./build/nescata "$(ARGS)"

debug: CFLAGS = -std=c++17 -g -Iinclude
debug: build/nescata
	gdb build/nescata --args "$(ARGS)"



clean:
	rm -rf build


-include $(LINUX_OBJECTS:.o=.d)
-include $(WIN_OBJECTS:.o=.d)