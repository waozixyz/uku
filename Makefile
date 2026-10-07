.DEFAULT_GOAL := build
ZIRAN ?= ./scripts/ziran.sh
CC ?= cc
PREFIX ?= $(HOME)/.local
SQLITE_PKG := $(shell pkg-config --exists sqlite3 2>/dev/null && echo yes)
SQLITE_LDLIBS := $(if $(SQLITE_PKG),$(shell pkg-config --libs sqlite3),-L$(HOME)/.local/sqlite3/lib -Wl,-rpath,$(HOME)/.local/sqlite3/lib -lsqlite3)
SQLITE_LIBRARY_PATH := $(if $(SQLITE_PKG),,$(HOME)/.local/sqlite3/lib)

.PHONY: build run check test core-test smoke web serve site itch android install clean clean-text-api-check deb appimage web-test
build:
	sh scripts/native_deps.sh "$(ZIRAN)"
	LIBRARY_PATH="$(SQLITE_LIBRARY_PATH)$${LIBRARY_PATH:+:$$LIBRARY_PATH}" \
		LD_RUN_PATH="$(SQLITE_LIBRARY_PATH)" $(ZIRAN) build --profile desktop

run: build
	./build/uku-desktop

check:
	python3 scripts/generate_locales.py --check
	$(ZIRAN) check
	python3 scripts/check-source.py

clean-text-api-check: check

core-test:
	SQLITE_LDLIBS="$(SQLITE_LDLIBS)" CC="$(CC)" sh scripts/core_test.sh "$(ZIRAN)"

# Runs only our app on a fresh private display and isolated data directory.
smoke: build
	python3 tests/desktop_test.py

test: check core-test smoke

web:
	sh scripts/web-build.sh "$(ZIRAN)"

serve: web
	python3 -m http.server --bind 127.0.0.1 --directory build/dist/web 8080

site: web
	sh site/build.sh

itch: web
	python3 scripts/package-web.py

android:
	sh scripts/android-build.sh

install: build
	$(ZIRAN) install --prefix "$(PREFIX)"

clean:
	rm -rf build

deb: build
	sh scripts/package-linux.sh deb

appimage: build
	sh scripts/package-linux.sh appimage

web-test: web
	sh site/build.sh
	node tests/web_test.mjs
	node tests/web_test.mjs --site
