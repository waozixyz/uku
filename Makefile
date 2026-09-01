.DEFAULT_GOAL := run

APP_NAME := uku
APP_TITLE := Uku
ANDROID_APP_ID := xyz.waozi.uku
ANDROID_ACTIVITY := xyz.waozi.uku.MainActivity
GRADLE := ./droid/gradlew
ANDROID_KEYSTORE ?= $(HOME)/.android/uku-release.keystore
ANDROID_KEY_ALIAS ?= uku-key
SRC := src/main.c src/qrcodegen.c
KRYON_DIR := $(if $(wildcard vendor/kryon/mk/common.mk),vendor/kryon,../kryon)
KRYON_MAKE_DIR := $(KRYON_DIR)/mk/
# Prefer system libcurl; fall back to the kryon-vendored static build when
# no pkg-config metadata is installed (e.g. on omega, which has no nix).
KRYON_USE_SYSTEM_CURL := $(if $(shell pkg-config --atleast-version=7.86.0 libcurl 2>/dev/null && echo y),1,0)
CMAKE ?= $(if $(wildcard /usr/bin/cmake),/usr/bin/cmake,cmake)

# System sqlite3 via pkg-config, or a local build in ~/.local/sqlite3.
ifeq ($(shell pkg-config --exists sqlite3 2>/dev/null && echo y),y)
SQLITE_CFLAGS := $(shell pkg-config --cflags sqlite3)
SQLITE_LDLIBS := $(shell pkg-config --libs sqlite3)
else
SQLITE_CFLAGS := -I$(HOME)/.local/sqlite3/include
SQLITE_LDLIBS := -L$(HOME)/.local/sqlite3/lib -Wl,-rpath,$(HOME)/.local/sqlite3/lib -lsqlite3
endif
FONT_SUBSET_DIR := assets/fonts/subset
FONT_SUBSET_CORPUS := locales src/main.c
ANDROID_SQLITE_VERSION = 3530400
ANDROID_SQLITE_YEAR = 2026
ANDROID_SQLITE_DIR = vendor-builds/sqlite
ANDROID_SQLITE_ZIP = $(BUILD_OBJ_DIR)/sqlite-amalgamation-$(ANDROID_SQLITE_VERSION).zip
ANDROID_SQLITE_URL = https://www.sqlite.org/$(ANDROID_SQLITE_YEAR)/sqlite-amalgamation-$(ANDROID_SQLITE_VERSION).zip
ANDROID_SQLITE_C = $(ANDROID_SQLITE_DIR)/sqlite3.c
ANDROID_SQLITE_H = $(ANDROID_SQLITE_DIR)/sqlite3.h
FONT_FILES := \
	$(FONT_SUBSET_DIR)/NotoSans-Uku-Regular.ttf \
	$(FONT_SUBSET_DIR)/NotoSansSC-Uku-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansJP-Uku-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansKR-Uku-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansTC-Uku-Regular.otf
WEB_SHELL := src/web_shell.html
WEB_ITCH_SHELL := src/itch_shell.html
WEB_DEV_HOST ?= 127.0.0.1
WEB_DEV_PORT ?= 8080
XVFB_TMPDIR ?= $(BUILD_OBJ_DIR)/xvfb
BRAVE_BROWSER ?= brave-browser
WEB_ONLY_GOALS := web web-itch itch itch-push site run clean-web font-subsets font-bundle-check
NON_NATIVE_GOALS := $(WEB_ONLY_GOALS) \
	android-copy-assets android-copy-apks android-copy-bundle \
	android-copy-debug-apks android-copy-release-apks \
	android-copy-release-artifacts android-debug android-debug-ci \
	android-install android-install-release android-release android-release-ci \
	android-bundle $(FONT_FILES) build/obj/uku_embedded_assets.c

ifeq ($(filter-out $(NON_NATIVE_GOALS),$(MAKECMDGOALS)),)
KRYON_BACKEND := canvas
RAY_CFLAGS ?= -DPLATFORM_WEB
RAY_LDLIBS ?= -lm
RAY_SDL_LDLIBS ?= -lm
RAY_SDL_INCLUDE_DIR ?= .
endif

KRYON_NATIVE_SUPPORT_FLAGS := -DSUPPORT_MODULE_RAUDIO=0
# raylib is built without the audio module, so the generated compat
# wrappers must drop the audio forwarders as well.
export KRYON_COMPAT_AUDIO := 0

include $(KRYON_MAKE_DIR)common.mk

.PHONY: font-subsets font-bundle-check

font-subsets:
	$(MAKE) -C $(KRYON_DIR) font-subsets \
		FONT_SUBSET_OUT_DIR="$(abspath $(FONT_SUBSET_DIR))" \
		FONT_SUBSET_SOURCE_DIR="$(abspath $(KRYON_DIR)/fonts/noto)" \
		FONT_SUBSET_PREFIX=Uku \
		FONT_SUBSET_CORPUS="$(abspath $(FONT_SUBSET_CORPUS))"

font-bundle-check:
	for font in $(FONT_FILES); do \
		case "$$font" in \
			$(KRYON_DIR)/fonts/noto/*) \
				echo "Full Noto font must not be embedded: $$font"; \
				exit 1; \
				;; \
		esac; \
	done

APP_ID := $(ANDROID_APP_ID)
APP_DESKTOP_ID := $(APP_ID)
APP_ICON_NAME := $(APP_ID)
APP_ICON_SIZE := 512x512
APP_COMMENT := Collective decisions without hidden resistance
APP_DESC := Uku is a free, open-source collective decision app for score voting, consent checks, proposals, and explicit status quo options.
APP_CATEGORIES := Office;Utility;
APP_MAINTAINER := Waozi <waozi@waozi.xyz>
APP_WWW := https://uku.waozi.xyz/
APP_LICENSE := BSD-3-Clause
APP_VERSION := $(shell awk '/versionName "/ { gsub(/^.*versionName "/, ""); gsub(/".*$$/, ""); print; exit }' droid/app/build.gradle 2>/dev/null)
APP_VERSION_FALLBACK := $(if $(APP_VERSION),$(APP_VERSION),0.1.0)
LINUX_APPIMAGE_BUILD_DIR := $(BUILD_OBJ_DIR)/appimage/linux
LINUX_APPDIR := $(LINUX_APPIMAGE_BUILD_DIR)/$(APP_NAME).AppDir
LINUX_APPIMAGE_DIR := packaging/linux/appimage
LINUX_APPIMAGE_APPRUN := $(LINUX_APPIMAGE_DIR)/AppRun
LINUX_APPIMAGE_DESKTOP := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).desktop
LINUX_APPIMAGE_ICON := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).png
LINUX_APPIMAGE_APPDATA := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).appdata.xml
APP_DESKTOP := $(LINUX_APPIMAGE_DESKTOP)
APP_ICON := $(LINUX_APPIMAGE_ICON)
APP_METAINFO := $(LINUX_APPIMAGE_APPDATA)
APPIMAGE_NAME = $(APP_NAME)-linux-$(ARCH).AppImage
APPIMAGE_TARGET = $(LINUX_DIST_DIR)/$(APPIMAGE_NAME)
APPIMAGE_INTERPRETER ?= $(if $(filter x86_64,$(ARCH)),/lib64/ld-linux-x86-64.so.2,$(if $(filter aarch64,$(ARCH)),/lib/ld-linux-aarch64.so.1,))
LINUXDEPLOY ?= linuxdeploy
DEB_BUILD_DIR := $(BUILD_OBJ_DIR)/deb
DEB_ROOT := $(DEB_BUILD_DIR)/root
DEB_DIST_DIR := $(BUILD_DIST_DIR)/deb
DEB_ARCH ?= $(if $(filter x86_64 amd64,$(ARCH)),amd64,$(if $(filter aarch64 arm64,$(ARCH)),arm64,$(ARCH)))
DEB_BIN_SOURCE ?=
DEB_BIN_INPUT = $(if $(strip $(DEB_BIN_SOURCE)),$(DEB_BIN_SOURCE),$(if $(filter linux,$(PLATFORM)),$(TARGET),))
DEB_PACKAGE_NAME ?= $(APP_NAME)
DEB_MAINTAINER ?= $(APP_MAINTAINER)
DEB_SECTION ?= utils
DEB_PRIORITY ?= optional
DEB_DEPENDS ?= libc6, libsdl2-2.0-0, libgtk-3-0, libcurl4, libsqlite3-0, libdrm2, libgbm1, libegl1, libgles2, hicolor-icon-theme
DEB_TARGET = $(DEB_DIST_DIR)/$(DEB_PACKAGE_NAME)_$(APP_VERSION_FALLBACK)_$(DEB_ARCH).deb

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
$(LINUX_APPIMAGE_BUILD_DIR): | $(BUILD_OBJ_DIR)
	mkdir -p $@
$(DEB_BUILD_DIR): | $(BUILD_OBJ_DIR)
	mkdir -p $@
$(DEB_DIST_DIR): | $(BUILD_DIST_DIR)
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

ifneq ($(filter-out $(NON_NATIVE_GOALS),$(MAKECMDGOALS)),)
include $(KRYON_MAKE_DIR)native.mk
else
run:
	python3 scripts/uku-web-dev.py --host "$(WEB_DEV_HOST)" --port "$(WEB_DEV_PORT)" --browser "$(BRAVE_BROWSER)"
endif
include $(KRYON_MAKE_DIR)dist.mk
include $(KRYON_MAKE_DIR)android.mk
include $(KRYON_MAKE_DIR)clean.mk

WEB_EMSDK_BIN ?= $(HOME)/emsdk/upstream/emscripten
WEB_CC ?= $(if $(wildcard $(WEB_EMSDK_BIN)/emcc),$(WEB_EMSDK_BIN)/emcc,emcc)
WEB_AR ?= $(if $(wildcard $(WEB_EMSDK_BIN)/emar),$(WEB_EMSDK_BIN)/emar,emar)
WEB_JS_TARGET = $(WEB_BUILD_DIR)/index.js
WEB_TARGET = $(WEB_DIST_DIR)/index.html
WEB_ITCH_DIST_DIR = $(BUILD_DIST_DIR)/itch
WEB_ITCH_TARGET = $(WEB_ITCH_DIST_DIR)/index.html
WEB_ITCH_ZIP = $(BUILD_DIST_DIR)/$(APP_NAME)-itch-html5.zip
WEB_CACHE_BUSTER ?= $(shell if git diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then git rev-parse --short HEAD 2>/dev/null; else date +%s; fi)
ITCH_PROJECT ?= waozi/ukuvota
ITCH_CHANNEL ?= html5
BUTLER ?= butler
WEB_LIBOQS_BUILD_DIR = $(KRYON_WEB_LIBOQS_BUILD_DIR)
WEB_LIBOQS_A = $(KRYON_WEB_LIBOQS_A)
WEB_LIBOQS_INCLUDE = -I$(WEB_LIBOQS_BUILD_DIR)/include
WEB_KRYON_SRCS = $(filter-out $(KRYON_DIR)/src/file_dialog/file_dialog.c,$(KRYON_SRCS))

WEB_PUBLIC_FILES = $(wildcard manifest.json) $(shell find web-assets -type f 2>/dev/null)
WEB_CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-typedef-redefinition -std=gnu99 -O0 -DPLATFORM_WEB -DKRYON_BACKEND_CANVAS=1 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_JPG=1 -DUI_EMBEDDED_ONLY=1 -DHAS_LIBOQS=1 -DKRYON_HAS_LIBOQS=1
WEB_LDFLAGS = -sFETCH=1 -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 -fexceptions -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=33554432 -lidbfs.js

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

$(WEB_ITCH_DIST_DIR): | $(BUILD_DIST_DIR)
	mkdir -p $@

$(FONT_FILES): $(LOCALE_FILES) src/main.c
	$(MAKE) font-subsets

$(ANDROID_SQLITE_DIR):
	mkdir -p $@

$(ANDROID_SQLITE_ZIP): | $(BUILD_OBJ_DIR)
	curl -fL "$(ANDROID_SQLITE_URL)" -o $@

$(ANDROID_SQLITE_C): $(ANDROID_SQLITE_ZIP) | $(ANDROID_SQLITE_DIR)
	python3 -c 'import pathlib,zipfile; z=zipfile.ZipFile("$(ANDROID_SQLITE_ZIP)"); out=pathlib.Path("$(ANDROID_SQLITE_DIR)"); [out.joinpath(pathlib.Path(n).name).write_bytes(z.read(n)) for n in z.namelist() if pathlib.Path(n).name in {"sqlite3.c","sqlite3.h"}]'

$(ANDROID_SQLITE_H): $(ANDROID_SQLITE_C)
	test -f $@

android-copy-assets: $(ANDROID_SQLITE_C) $(ANDROID_SQLITE_H)

$(WEB_JS_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_KRYON_SRCS) $(CORE_SRCS) $(WEB_LIBOQS_A) $(WEB_PUBLIC_FILES) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(WEB_LIBOQS_INCLUDE) \
		$(CORE_INCLUDE) \
		-o $@ \
		$(SRC) \
		$(WEB_KRYON_SRCS) \
		$(CORE_SRCS) \
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

$(WEB_ITCH_TARGET): $(WEB_ITCH_SHELL) $(WEB_JS_TARGET) | $(WEB_ITCH_DIST_DIR)
	rm -rf $(WEB_ITCH_DIST_DIR)
	mkdir -p $(WEB_ITCH_DIST_DIR)
	find $(WEB_BUILD_DIR) -maxdepth 1 -type f \
		\( -name '*.js' -o -name '*.wasm' -o -name '*.data' -o -name '*.json' -o -name '*.png' -o -name '*.ico' -o -name '*.webmanifest' \) \
		-exec cp {} $(WEB_ITCH_DIST_DIR)/ \;
	@if [ -d $(WEB_BUILD_DIR)/web-assets ]; then cp -R $(WEB_BUILD_DIR)/web-assets $(WEB_ITCH_DIST_DIR)/; fi
	sed -e 's#{{{ SCRIPT }}}#<script async src="index.js?v=WEB_CACHE_BUSTER"></script>#g' \
		-e 's#WEB_CACHE_BUSTER#$(WEB_CACHE_BUSTER)#g' \
		$(WEB_ITCH_SHELL) > $(WEB_ITCH_TARGET)

$(WEB_ITCH_ZIP): $(WEB_ITCH_TARGET)
	@if command -v zip >/dev/null 2>&1; then rm -f $@; cd $(WEB_ITCH_DIST_DIR) && zip -r $(abspath $@) .; else echo "zip not installed; skipping $(WEB_ITCH_ZIP)"; fi

web-itch itch: $(WEB_ITCH_TARGET) $(WEB_ITCH_ZIP)

itch-push: $(WEB_ITCH_ZIP)
	$(BUTLER) push $(WEB_ITCH_ZIP) $(ITCH_PROJECT):$(ITCH_CHANNEL)

linux: native

dist: dist-linux

site: web
	sh site/build.sh

smoke: $(TARGET)
	mkdir -p $(XVFB_TMPDIR)
	TMPDIR="$(abspath $(XVFB_TMPDIR))" xvfb-run -a timeout 8s ./$(TARGET) || test $$? -eq 124

no-vendor-edits:
	scripts/check-no-vendor-edits.sh

test: no-vendor-edits smoke

.PHONY: web web-itch itch itch-push site smoke test no-vendor-edits

appimage: $(APPIMAGE_TARGET)

$(APPIMAGE_TARGET): $(TARGET) $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) | $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR)
	@test -n "$(strip $(APPIMAGE_INTERPRETER))" || { \
		echo "No AppImage interpreter is configured for ARCH=$(ARCH)"; \
		exit 1; \
	}
	@command -v linuxdeploy >/dev/null || { echo "linuxdeploy is missing"; exit 1; }
	@command -v linuxdeploy-plugin-appimage >/dev/null || { echo "linuxdeploy-plugin-appimage is missing"; exit 1; }
	@command -v appimagetool >/dev/null || { echo "appimagetool is missing"; exit 1; }
	@command -v patchelf >/dev/null || { echo "patchelf is missing"; exit 1; }
	rm -rf $(LINUX_APPDIR) $(LINUX_DIST_DIR)/*.AppDir
	rm -f $(LINUX_DIST_DIR)/*.AppImage
	mkdir -p $(LINUX_APPDIR)/usr/bin $(LINUX_APPDIR)/usr/lib $(LINUX_APPDIR)/usr/share/applications $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps $(LINUX_APPDIR)/usr/share/metainfo $(LINUX_APPDIR)/usr/share/appdata
	cp $(TARGET) $(LINUX_APPDIR)/usr/bin/$(APP_NAME)
	patchelf --set-interpreter $(APPIMAGE_INTERPRETER) $(LINUX_APPDIR)/usr/bin/$(APP_NAME)
	cp $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPDIR)/AppRun
	chmod +x $(LINUX_APPDIR)/AppRun
	sed -e 's/^Icon=.*/Icon=$(APP_ICON_NAME)/' $(LINUX_APPIMAGE_DESKTOP) > $(LINUX_APPDIR)/$(APP_DESKTOP_ID).desktop
	cp $(LINUX_APPDIR)/$(APP_DESKTOP_ID).desktop $(LINUX_APPDIR)/usr/share/applications/$(APP_DESKTOP_ID).desktop
	sed -e 's/<release version="[^"]*"/<release version="$(APP_VERSION_FALLBACK)"/' \
		-e 's#<launchable type="desktop-id">[^<]*</launchable>#<launchable type="desktop-id">$(APP_DESKTOP_ID).desktop</launchable>#' \
		$(LINUX_APPIMAGE_APPDATA) > $(LINUX_APPDIR)/usr/share/metainfo/$(APP_ID).metainfo.xml
	cp $(LINUX_APPDIR)/usr/share/metainfo/$(APP_ID).metainfo.xml $(LINUX_APPDIR)/usr/share/appdata/$(APP_ID).appdata.xml
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/$(APP_ICON_NAME).png
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_ICON_NAME).png
	@loader=$$(LC_ALL=C readelf -l $(TARGET) | sed -n 's#.*Requesting program interpreter: \(.*\)]#\1#p'); \
	if printf '%s\n' "$$loader" | grep -q '^/nix/store/.*glibc.*/'; then \
		glibc_lib_dir=$$(dirname "$$loader"); \
		cp "$$loader" $(LINUX_APPDIR)/usr/lib/; \
		for lib in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 libnss_files.so.2; do \
			if [ -f "$$glibc_lib_dir/$$lib" ]; then cp "$$glibc_lib_dir/$$lib" $(LINUX_APPDIR)/usr/lib/; fi; \
		done; \
	fi
	@for lib in libX11.so.6 libXext.so.6 libdrm.so.2 libgbm.so.1 libEGL.so.1 libGLESv2.so.2 libGLdispatch.so.0 libglapi.so.0; do \
		found=$$(find /usr/lib /lib -name "$$lib" 2>/dev/null | head -n 1); \
		if [ -n "$$found" ]; then cp "$$found" $(LINUX_APPDIR)/usr/lib/ 2>/dev/null || true; fi; \
	done
	cd $(LINUX_APPIMAGE_BUILD_DIR) && env -u SOURCE_DATE_EPOCH ARCH=$(ARCH) LDAI_OUTPUT=$(abspath $(APPIMAGE_TARGET)) LDAI_UPDATE_INFORMATION='gh-releases-zsync|waozixyz|uku|latest|$(APPIMAGE_NAME)' $(LINUXDEPLOY) \
		--appdir $(APP_NAME).AppDir \
		--executable $(abspath $(LINUX_APPDIR)/usr/bin/$(APP_NAME)) \
		--desktop-file $(abspath $(LINUX_APPDIR)/usr/share/applications/$(APP_DESKTOP_ID).desktop) \
		--icon-file $(abspath $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_ICON_NAME).png) \
		--output appimage
	test -f $@

deb package-deb: $(DEB_TARGET)

deb-check:
	@command -v dpkg-deb >/dev/null 2>&1 || { echo "dpkg-deb is missing"; exit 1; }
	@if [ -z "$(strip $(DEB_BIN_INPUT))" ]; then echo "No Linux binary is available for the Debian package"; exit 1; fi

$(DEB_TARGET): $(DEB_BIN_INPUT) $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) deb-check | $(DEB_BUILD_DIR) $(DEB_DIST_DIR)
	rm -rf $(DEB_ROOT)
	mkdir -p $(DEB_ROOT)/DEBIAN $(DEB_ROOT)/usr/bin $(DEB_ROOT)/usr/share/applications $(DEB_ROOT)/usr/share/icons/hicolor/512x512/apps $(DEB_ROOT)/usr/share/metainfo
	cp $(DEB_BIN_INPUT) $(DEB_ROOT)/usr/bin/$(APP_NAME)
	chmod 755 $(DEB_ROOT)/usr/bin/$(APP_NAME)
	sed -e 's/^Icon=.*/Icon=$(APP_ICON_NAME)/' $(LINUX_APPIMAGE_DESKTOP) > $(DEB_ROOT)/usr/share/applications/$(APP_DESKTOP_ID).desktop
	cp $(LINUX_APPIMAGE_ICON) $(DEB_ROOT)/usr/share/icons/hicolor/512x512/apps/$(APP_ICON_NAME).png
	sed -e 's/<release version="[^"]*"/<release version="$(APP_VERSION_FALLBACK)"/' \
		-e 's#<launchable type="desktop-id">[^<]*</launchable>#<launchable type="desktop-id">$(APP_DESKTOP_ID).desktop</launchable>#' \
		$(LINUX_APPIMAGE_APPDATA) > $(DEB_ROOT)/usr/share/metainfo/$(APP_ID).metainfo.xml
	@installed_size=$$(find $(DEB_ROOT)/usr -type f -exec wc -c {} + | awk '$$2 != "total" { bytes += $$1 } END { print int((bytes + 1023) / 1024) }'); \
	{ \
		printf 'Package: %s\n' '$(DEB_PACKAGE_NAME)'; \
		printf 'Version: %s\n' '$(APP_VERSION_FALLBACK)'; \
		printf 'Architecture: %s\n' '$(DEB_ARCH)'; \
		printf 'Maintainer: %s\n' '$(DEB_MAINTAINER)'; \
		printf 'Section: %s\n' '$(DEB_SECTION)'; \
		printf 'Priority: %s\n' '$(DEB_PRIORITY)'; \
		printf 'Installed-Size: %s\n' "$$installed_size"; \
		printf 'Depends: %s\n' '$(DEB_DEPENDS)'; \
		printf 'Homepage: %s\n' '$(APP_WWW)'; \
		printf 'Description: %s\n' '$(APP_COMMENT)'; \
		printf ' %s\n' '$(APP_DESC)'; \
	} > $(DEB_ROOT)/DEBIAN/control
	rm -f $(DEB_DIST_DIR)/$(DEB_PACKAGE_NAME)_*_$(DEB_ARCH).deb
	dpkg-deb --build --root-owner-group $(DEB_ROOT) $(DEB_TARGET)
	test -f $@

android-release-ci:
	$(MAKE) android-copy-assets
	$(GRADLE) -p droid assembleRelease bundleRelease \
		-Pkeystore.path="$(ANDROID_KEYSTORE)" \
		-Pkeystore.alias="$(ANDROID_KEY_ALIAS)" \
		-Pkeystore.password="$(PASSWORD)" \
		--stacktrace
	$(MAKE) android-copy-release-artifacts

android-copy-release-artifacts: | $(ANDROID_BUILD_DIR)
	@if [ -z "$(APP_VERSION)" ]; then echo "Could not read versionName from droid/app/build.gradle"; exit 1; fi; \
	release_universal=$$(find droid/app/build/outputs/apk -path '*/release/*' -name 'app-universal-release*.apk' | head -n 1); \
	if [ -z "$$release_universal" ] || [ ! -f "$$release_universal" ]; then echo "No universal release APK was produced"; exit 1; fi; \
	release_bundle=$$(find droid/app/build/outputs/bundle -path '*/release/*' -name '*.aab' | head -n 1); \
	if [ -z "$$release_bundle" ] || [ ! -f "$$release_bundle" ]; then echo "No release AAB was produced"; exit 1; fi; \
	cp "$$release_universal" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-$(APP_VERSION).apk"; \
	cp "$$release_bundle" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-$(APP_VERSION).aab"; \
	cp "$(ANDROID_BUILD_DIR)/$(APP_NAME)-$(APP_VERSION).apk" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-latest.apk"; \
	cp "$(ANDROID_BUILD_DIR)/$(APP_NAME)-$(APP_VERSION).aab" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-latest.aab"

android-debug-ci:
	$(MAKE) android-copy-assets
	$(GRADLE) -p droid assembleDebug --stacktrace
	$(MAKE) android-copy-debug-apks

.PHONY: all native linux web site dist run clean smoke no-vendor-edits test appimage deb package-deb deb-check android-release-ci android-copy-release-artifacts android-debug-ci
