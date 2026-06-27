.DEFAULT_GOAL := all

CC = gcc

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
	PLATFORM := linux
else
	PLATFORM := unknown
endif

ifeq ($(UNAME_M),x86_64)
	ARCH := x86_64
else ifeq ($(UNAME_M),aarch64)
	ARCH := aarch64
else
	ARCH := $(UNAME_M)
endif

BUILD_DIR := build
LINUX_BUILD_DIR := $(BUILD_DIR)/linux
TARGET := $(LINUX_BUILD_DIR)/uku-$(PLATFORM)-$(ARCH)
PACKAGE_ID := xyz.waozi.uku

SRC := src/main.c
FLINT_DIR := vendor/flint
FLINT_ICON_ASSETS_C := $(FLINT_DIR)/src/flint_icon_assets.c
FLINT_SRCS := $(filter-out $(FLINT_ICON_ASSETS_C),$(wildcard $(FLINT_DIR)/src/*.c) $(wildcard $(FLINT_DIR)/src/ui/*.c)) $(FLINT_ICON_ASSETS_C)
RAYLIB_DIR := $(FLINT_DIR)/vendor/raylib/src
RAYLIB_BUILD_ROOT := $(BUILD_DIR)/vendor/raylib
RAYLIB_BUILD_SRC_DIR := $(RAYLIB_BUILD_ROOT)/src
RAYLIB_BUILD_DIR := $(RAYLIB_BUILD_ROOT)/build/sdl
RAYLIB_A := $(RAYLIB_BUILD_DIR)/libraylib.a
RAYLIB_SOURCES := $(shell if [ -d "$(RAYLIB_DIR)" ]; then find "$(RAYLIB_DIR)" -type f \( -name '*.c' -o -name '*.h' \); fi)
LOCALE_FILES := $(wildcard locales/*.txt)
FONT_OUTPUTS := assets/fonts/locales.png assets/fonts/locales.dat
FONT_TOOL := $(FLINT_DIR)/tools/otfchop/otfchop
FONT_SOURCE := $(FLINT_DIR)/tools/otfchop/unifont-17.0.04.otf

UKU_RAYLIB_CONFIG := $(RAY_RAYLIB_CONFIG) -DSUPPORT_MODULE_RAUDIO=0
CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections
LDFLAGS := -Wl,--gc-sections -s

ifeq ($(strip $(RAY_CFLAGS)),)
$(error RAY_CFLAGS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_LDLIBS)),)
$(error RAY_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_LDLIBS)),)
$(error RAY_SDL_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_INCLUDE_DIR)),)
$(error RAY_SDL_INCLUDE_DIR is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_RAYLIB_CONFIG)),)
$(error RAY_RAYLIB_CONFIG is not set. Enter the ray flake shell with 'nix develop')
endif

all: native

native: $(TARGET)

linux: native

dist: dist-linux

dist-linux: $(TARGET)
	mkdir -p $(BUILD_DIR)/dist/linux
	tar -czf $(BUILD_DIR)/dist/linux/uku-linux-$(ARCH).tar.gz -C $(LINUX_BUILD_DIR) uku-$(PLATFORM)-$(ARCH)

$(LINUX_BUILD_DIR):
	mkdir -p $@

assets/fonts:
	mkdir -p $@

$(FONT_OUTPUTS): $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales

$(FONT_TOOL): $(FLINT_DIR)/tools/otfchop/otfchop.c $(FLINT_DIR)/tools/otfchop/stb_truetype.h $(FLINT_DIR)/tools/otfchop/stb_image_write.h
	$(MAKE) -C $(FLINT_DIR)/tools/otfchop otfchop

$(RAYLIB_BUILD_DIR):
	mkdir -p $@

$(RAYLIB_A): $(RAYLIB_SOURCES) | $(RAYLIB_BUILD_DIR)
	rm -rf $(RAYLIB_BUILD_SRC_DIR)
	mkdir -p $(RAYLIB_BUILD_SRC_DIR)
	cp -R $(RAYLIB_DIR)/. $(RAYLIB_BUILD_SRC_DIR)/
	$(MAKE) -j1 -C $(RAYLIB_BUILD_SRC_DIR) \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../build/sdl \
		RAYLIB_MODULE_AUDIO=FALSE \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(UKU_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(TARGET): $(SRC) $(FLINT_SRCS) $(FONT_OUTPUTS) $(RAYLIB_A) | $(LINUX_BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(FLINT_DIR)/include \
		-Isrc \
		$(RAY_CFLAGS) \
		-o $@ \
		$(SRC) \
		$(FLINT_SRCS) \
		$(RAYLIB_A) \
		$(RAY_LDLIBS) \
		-lsqlite3 -lm -lpthread -ldl -lrt \
		$(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) assets/fonts

.PHONY: all native linux dist dist-linux run clean
