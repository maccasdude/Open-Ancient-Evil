CXX ?= g++
CXXFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++17 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -fno-strict-aliasing -fwrapv $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2)
override LDLIBS += $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2) -lpthread -lm

# Optional FFmpeg libraries for the cut-scene movies and CD music files.
# Build with "make NO_FFMPEG=1" to leave them out.
ifndef NO_FFMPEG
FFMPEG_PKGS := libavformat libavcodec libswscale libswresample libavutil
ifeq ($(shell pkg-config --exists $(FFMPEG_PKGS) && echo yes),yes)
override CXXFLAGS += -DHAVE_LIBAV $(shell pkg-config --cflags $(FFMPEG_PKGS))
override LDLIBS += $(shell pkg-config --libs $(FFMPEG_PKGS))
endif
endif
SRC := $(wildcard src/engine/*.cpp src/platform/*.cpp src/game/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
DEP := $(OBJ:.o=.d)

all: ancientevil

ancientevil: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

PREFIX ?= /usr
DESTDIR ?=

install: ancientevil
	install -Dm755 ancientevil $(DESTDIR)$(PREFIX)/bin/ancientevil
	install -Dm644 packaging/ancientevil.desktop $(DESTDIR)$(PREFIX)/share/applications/ancientevil.desktop
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/ancientevil/README.md
	install -Dm644 CHANGELOG.md $(DESTDIR)$(PREFIX)/share/doc/ancientevil/CHANGELOG.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/ancientevil/LICENSE

# Windows (cross-compiled with mingw-w64): see tools/build-windows.sh
windows:
	tools/build-windows.sh

# Release packages into dist/ (Linux, Arch, and Windows when mingw-w64 is there)
dist:
	tools/package.sh

clean:
	rm -rf build ancientevil build-win dist

-include $(DEP)
.PHONY: all clean install windows dist
