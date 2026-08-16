.DEFAULT_GOAL := run

APP_NAME := uku
ANDROID_APP_ID := xyz.waozi.uku
SRC := src/main.c src/qrcodegen.c
KRYON_DIR := $(if $(wildcard vendor/kryon/mk/common.mk),vendor/kryon,../kryon)
KRYON_MAKE_DIR := $(KRYON_DIR)/mk/
# Prefer system libcurl; fall back to the kryon-vendored static build when
# no pkg-config metadata is installed (e.g. on omega, which has no nix).
KRYON_USE_SYSTEM_CURL := $(if $(shell pkg-config --exists libcurl 2>/dev/null && echo y),1,0)

# System sqlite3 via pkg-config, or a local build in ~/.local/sqlite3.
ifeq ($(shell pkg-config --exists sqlite3 2>/dev/null && echo y),y)
SQLITE_CFLAGS := $(shell pkg-config --cflags sqlite3)
SQLITE_LDLIBS := $(shell pkg-config --libs sqlite3)
else
SQLITE_CFLAGS := -I$(HOME)/.local/sqlite3/include
SQLITE_LDLIBS := -L$(HOME)/.local/sqlite3/lib -Wl,-rpath,$(HOME)/.local/sqlite3/lib -lsqlite3
endif
FONT_MODE ?= font
FONT_SOURCE ?= $(KRYON_DIR)/fonts/noto/NotoSans-Regular.ttf

# Generated UI font embedded as assets/fonts/ui.ttf (gitignored).
FONT_FILES := assets/fonts/ui.ttf
FONT_RANGES ?= ascii latin latin-extended punctuation greek cyrillic
FONT_MISSING ?= skip
FONT_BASE_SIZE ?= 32
FONT_CELL_SIZE ?= 48
WEB_SHELL := src/web_shell.html
WEB_DEV_HOST ?= 127.0.0.1
WEB_DEV_PORT ?= 8080
BRAVE_BROWSER ?= brave-browser

KRYON_NATIVE_SUPPORT_FLAGS := -DSUPPORT_MODULE_RAUDIO=0

include $(KRYON_MAKE_DIR)common.mk

KRYON_RAYLIB_MODULE_AUDIO = FALSE
APP_RAYLIB_CONFIG = $(RAY_RAYLIB_CONFIG) -DSUPPORT_MODULE_RAUDIO=0

KRY_SRCS := $(shell find src -type f -name '*.kry' 2>/dev/null | LC_ALL=C sort)
KRY_GEN_DIR := $(BUILD_DIR)/kryon/generated
KRY_GEN_SRCS := $(patsubst %.kry,$(KRY_GEN_DIR)/%.c,$(KRY_SRCS))
KRY_GEN_HEADERS := $(patsubst %.kry,$(KRY_GEN_DIR)/%.h,$(KRY_SRCS))
SRC += $(KRY_GEN_SRCS)
APP_INCLUDE += -I$(KRY_GEN_DIR)

LINUX_BIN_DIR := $(BUILD_DIR)/linux
# common.mk defines the dir rule for its default build/bin/linux; the override
# above needs its own or `make linux` finds no rule to create build/linux.
$(LINUX_BIN_DIR):
	mkdir -p $@
KRYON_VENDOR_BUILD_DIR := $(BUILD_DIR)/vendor
KRYON_LIBOQS_BUILD_DIR := $(KRYON_VENDOR_BUILD_DIR)/liboqs
KRYON_CURL_BUILD_TYPE := Release

include $(KRYON_MAKE_DIR)raylib.mk

# uku is a UI-only app: drop the 2D physics subsystem (Box2D) entirely --
# its sources need box2d.h, which is not vendored here. Must precede the
# vendor.mk include (which defaults the flag to 1).
KRYON_WITH_PHYSICS := 0

include $(KRYON_MAKE_DIR)vendor.mk

KRYON_INCLUDE += $(KRYON_PHYSICS_CPPFLAGS)
KRYON_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_SRCS))

ifeq ($(KRYON_USE_SYSTEM_CURL),1)
KRYON_SYSTEM_CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
KRYON_SYSTEM_CURL_LDLIBS := $(shell pkg-config --libs libcurl)
KRYON_NATIVE_DEPS := $(KRYON_LIBOQS_A)
KRYON_NATIVE_CFLAGS := $(KRYON_LIBOQS_INCLUDE) $(KRYON_SYSTEM_CURL_CFLAGS) $(SQLITE_CFLAGS) -DHAS_LIBOQS=1 -DKRYON_HAS_LIBOQS=1
KRYON_NATIVE_LDLIBS := $(KRYON_LIBOQS_A) $(KRYON_SYSTEM_CURL_LDLIBS) $(SQLITE_LDLIBS)
else
# common.mk clears KRYON_RUNTIME_ASSET_CFLAGS when pkg-config finds no
# system libcurl; the vendored static build needs it set explicitly.
KRYON_RUNTIME_ASSET_CFLAGS := -DHAS_LIBCURL=1 $(KRYON_CURL_CFLAGS)
KRYON_NATIVE_DEPS := $(KRYON_LIBOQS_A) $(KRYON_CURL_SO)
KRYON_NATIVE_CFLAGS := $(KRYON_LIBOQS_INCLUDE) $(KRYON_CURL_CFLAGS) $(SQLITE_CFLAGS) -DHAS_LIBOQS=1 -DKRYON_HAS_LIBOQS=1
KRYON_NATIVE_LDLIBS := $(KRYON_LIBOQS_A) $(KRYON_CURL_LDLIBS) $(KRYON_CURL_TRANSITIVE_LDLIBS) $(SQLITE_LDLIBS)
endif

ifneq ($(filter-out web run clean-web,$(MAKECMDGOALS)),)
include $(KRYON_MAKE_DIR)native.mk
else
run:
	python3 scripts/uku-web-dev.py --host "$(WEB_DEV_HOST)" --port "$(WEB_DEV_PORT)" --browser "$(BRAVE_BROWSER)"
endif
include $(KRYON_MAKE_DIR)dist.mk
include $(KRYON_MAKE_DIR)clean.mk

WEB_CC ?= emcc
WEB_AR ?= emar
WEB_JS_TARGET = $(WEB_BUILD_DIR)/index.js
WEB_TARGET = $(WEB_DIST_DIR)/index.html
WEB_RAYLIB_BUILD_DIR = $(WEB_BUILD_DIR)/raylib
WEB_RAYLIB_A = $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_LIBOQS_BUILD_DIR = $(KRYON_WEB_LIBOQS_BUILD_DIR)
WEB_LIBOQS_A = $(KRYON_WEB_LIBOQS_A)
WEB_LIBOQS_INCLUDE = -I$(WEB_LIBOQS_BUILD_DIR)/include
WEB_KRYON_SRCS = $(filter-out $(KRYON_DIR)/src/file_dialog/file_dialog.c,$(KRYON_SRCS))

# The web raylib is compiled with the backend rename header (InitWindow ->
# KryonRaylibBackend_InitWindow etc.), so plain-name calls resolve through the
# generated wrappers. The native raylib keeps plain symbols and needs none.
WEB_PUBLIC_FILES = $(wildcard manifest.json) $(shell find web-assets -type f 2>/dev/null)
WEB_CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-typedef-redefinition -std=gnu99 -O0 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_JPG=1 -DUI_EMBEDDED_ONLY=1 -DHAS_LIBOQS=1 -DKRYON_HAS_LIBOQS=1
WEB_LDFLAGS = -sUSE_GLFW=3 -sFETCH=1 -sASYNCIFY -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=8388608 -sGLOBAL_BASE=16777216 -lidbfs.js

$(eval $(call KRYON_RAYLIB_WEB_RULE,$(WEB_RAYLIB_A),$(or $(WEB_RAYLIB_SOURCE_BUILD_DIR),$(dir $(WEB_RAYLIB_BUILD_DIR))raylib-src),$(WEB_RAYLIB_BUILD_DIR),$(WEB_CC),$(WEB_AR)))

# All compiler sources: a bare prerequisite never rebuilds on compiler
# changes (mirrors kryon's own K2C_SRCS).
K2C_SRCS := $(sort $(wildcard $(KRYON_DIR)/cmd/k2c/*.c)) \
	$(KRYON_DIR)/cmd/kir/kir.c $(KRYON_DIR)/cmd/kir/kir_parse.c
$(K2C): $(K2C_SRCS)
	$(MAKE) -C $(KRYON_DIR) k2c

$(KRY_GEN_SRCS) $(KRY_GEN_HEADERS): $(KRY_SRCS) $(K2C) | $(KRY_GEN_DIR)
	$(K2C) --no-main --root . -o $(KRY_GEN_DIR) $(KRY_SRCS)

$(KRY_GEN_DIR):
	mkdir -p $@

$(FONT_FILES): $(FONT_SOURCE)
	@mkdir -p $(dir $@)
	cp $(FONT_SOURCE) $@

$(WEB_JS_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_KRYON_SRCS) $(CORE_SRCS) $(WEB_RAYLIB_A) $(WEB_LIBOQS_A) $(WEB_PUBLIC_FILES) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(WEB_LIBOQS_INCLUDE) \
		$(CORE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-o $@ \
		$(SRC) \
		$(WEB_KRYON_SRCS) \
		$(CORE_SRCS) \
		$(WEB_RAYLIB_A) \
		$(WEB_LIBOQS_A) \
	$(WEB_LDFLAGS)
	@if [ -d web-assets ]; then cp -R web-assets $(WEB_BUILD_DIR)/; fi
	@if [ -f web-assets/uku-logo.svg ]; then cp web-assets/uku-logo.svg $(WEB_BUILD_DIR)/favicon.ico; fi
	@if [ -f manifest.json ]; then cp manifest.json $(WEB_BUILD_DIR)/; fi

$(WEB_BUILD_DIR)/index.html: $(WEB_SHELL) $(WEB_JS_TARGET) | $(WEB_BUILD_DIR)
	sed 's#{{{ SCRIPT }}}#<script async src="index.js"></script>#g' $(WEB_SHELL) > $@

$(WEB_TARGET): $(WEB_BUILD_DIR)/index.html | $(WEB_DIST_DIR)
	rm -rf $(WEB_DIST_DIR)
	mkdir -p $(WEB_DIST_DIR)
	find $(WEB_BUILD_DIR) -maxdepth 1 -type f \
		\( -name '*.html' -o -name '*.js' -o -name '*.wasm' -o -name '*.data' -o -name '*.json' -o -name '*.png' -o -name '*.ico' -o -name '*.webmanifest' \) \
		-exec cp {} $(WEB_DIST_DIR)/ \;
	@if [ -d $(WEB_BUILD_DIR)/web-assets ]; then cp -R $(WEB_BUILD_DIR)/web-assets $(WEB_DIST_DIR)/; fi

$(WEB_ZIP): $(WEB_TARGET)
	@if command -v zip >/dev/null 2>&1; then rm -f $@; cd $(WEB_DIST_DIR) && zip -r $(abspath $@) .; else echo "zip not installed; skipping $(WEB_ZIP)"; fi

web: $(WEB_TARGET) $(WEB_ZIP)

linux: native

dist: dist-linux

.PHONY: all native linux web dist run clean
