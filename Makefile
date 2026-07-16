.DEFAULT_GOAL := web

APP_NAME := uku
ANDROID_APP_ID := xyz.waozi.uku
SRC := src/main.c
FLINT_DIR := $(if $(wildcard vendor/flint/mk/common.mk),vendor/flint,../flint)
FLINT_MAKE_DIR := $(FLINT_DIR)/mk/
FLINT_USE_SYSTEM_CURL := 1
WEB_SHELL := src/web_shell.html
WEB_DEV_HOST ?= 127.0.0.1
WEB_DEV_PORT ?= 8080
BRAVE_BROWSER ?= brave-browser

FLINT_RAYLIB_MODULE_AUDIO := FALSE
FLINT_NATIVE_SUPPORT_FLAGS := -DSUPPORT_MODULE_RAUDIO=0
APP_RAYLIB_CONFIG := $(RAY_RAYLIB_CONFIG) -DSUPPORT_MODULE_RAUDIO=0

include $(FLINT_MAKE_DIR)common.mk

LINUX_BIN_DIR := $(BUILD_DIR)/linux
FLINT_VENDOR_BUILD_DIR := $(BUILD_DIR)/vendor
FLINT_LIBOQS_BUILD_DIR := $(FLINT_VENDOR_BUILD_DIR)/liboqs
FLINT_CURL_BUILD_TYPE := Release
FLINT_LIBRESSL_BUILD_TYPE := Release

include $(FLINT_MAKE_DIR)vendor.mk

ifeq ($(FLINT_USE_SYSTEM_CURL),1)
FLINT_SYSTEM_CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
FLINT_SYSTEM_CURL_LDLIBS := $(shell pkg-config --libs libcurl)
FLINT_NATIVE_DEPS := $(FLINT_LIBOQS_A)
FLINT_NATIVE_CFLAGS := $(FLINT_LIBOQS_INCLUDE) $(FLINT_SYSTEM_CURL_CFLAGS) -DFLINT_HAS_LIBOQS=1
FLINT_NATIVE_LDLIBS := $(FLINT_LIBOQS_A) $(FLINT_SYSTEM_CURL_LDLIBS) -lsqlite3
else
FLINT_NATIVE_DEPS := $(FLINT_LIBOQS_A) $(FLINT_CURL_SO)
FLINT_NATIVE_CFLAGS := $(FLINT_LIBOQS_INCLUDE) $(FLINT_CURL_CFLAGS) -DFLINT_HAS_LIBOQS=1
FLINT_NATIVE_LDLIBS := $(FLINT_LIBOQS_A) $(FLINT_CURL_LDLIBS) -lsqlite3
endif

include $(FLINT_MAKE_DIR)raylib.mk

ifneq ($(filter-out web run clean-web,$(MAKECMDGOALS)),)
include $(FLINT_MAKE_DIR)native.mk
endif
include $(FLINT_MAKE_DIR)dist.mk
include $(FLINT_MAKE_DIR)clean.mk

WEB_CC ?= emcc
WEB_AR ?= emar
WEB_JS_TARGET = $(WEB_BUILD_DIR)/index.js
WEB_TARGET = $(WEB_DIST_DIR)/index.html
WEB_RAYLIB_BUILD_DIR = $(WEB_BUILD_DIR)/raylib
WEB_RAYLIB_A = $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_LIBOQS_BUILD_DIR = $(FLINT_WEB_LIBOQS_BUILD_DIR)
WEB_LIBOQS_A = $(FLINT_WEB_LIBOQS_A)
WEB_LIBOQS_INCLUDE = -I$(WEB_LIBOQS_BUILD_DIR)/include
WEB_FLINT_SRCS = $(filter-out $(FLINT_DIR)/src/file_dialog/file_dialog.c,$(FLINT_SRCS))
WEB_PUBLIC_FILES = $(wildcard manifest.json) $(shell find web-assets -type f 2>/dev/null)
WEB_CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-typedef-redefinition -std=gnu99 -O0 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_JPG=1 -DUI_EMBEDDED_ONLY=1 -DHAS_LIBOQS=1 -DFLINT_HAS_LIBOQS=1
WEB_LDFLAGS = -sUSE_GLFW=3 -sFETCH=1 -sASYNCIFY -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=8388608 -sGLOBAL_BASE=16777216 -lidbfs.js

$(eval $(call FLINT_RAYLIB_WEB_RULE,$(WEB_RAYLIB_A),$(or $(WEB_RAYLIB_SOURCE_BUILD_DIR),$(dir $(WEB_RAYLIB_BUILD_DIR))raylib-src),$(WEB_RAYLIB_BUILD_DIR),$(WEB_CC),$(WEB_AR)))

$(WEB_JS_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_FLINT_SRCS) $(CORE_SRCS) $(WEB_RAYLIB_A) $(WEB_LIBOQS_A) $(WEB_PUBLIC_FILES) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(WEB_LIBOQS_INCLUDE) \
		$(CORE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-o $@ \
		$(SRC) \
		$(WEB_FLINT_SRCS) \
		$(CORE_SRCS) \
		$(WEB_RAYLIB_A) \
		$(WEB_LIBOQS_A) \
	$(WEB_LDFLAGS)
	@if [ -d web-assets ]; then rsync -a web-assets/ $(WEB_BUILD_DIR)/web-assets/; fi
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
	rm -f $@
	cd $(WEB_DIST_DIR) && zip -r $(abspath $@) .

web: $(WEB_TARGET) $(WEB_ZIP)

linux: native

dist: dist-linux

run:
	python3 scripts/uku-web-dev.py --host "$(WEB_DEV_HOST)" --port "$(WEB_DEV_PORT)" --browser "$(BRAVE_BROWSER)"

.PHONY: all native linux web dist run clean
