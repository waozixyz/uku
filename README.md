# Ukuvota

<p align="center">
  <img src="assets/app/readme-banner-1280x360.png" alt="Ukuvota" width="960">
</p>

Ukuvota helps a group compare proposals using scores from -3 to +3. Negative
scores carry a chosen weight so resistance stays visible. Every new process
includes Status quo and Repeat process, explicit abstention, timed phases,
quorum and tie handling.

This version collects decisions **on one device**. Save a ballot and choose
Next participant to hand over the device; tap a saved participant to edit their
ballot. Names are labels, with separate saved participant identities. Online
sharing, remote synchronization and server permissions are not connected.

The maintained application is Ziran using current Kryon widgets, with one
`Frame` shared by Linux, browser and Android hosts. Android's small C/Java
bridge supplies the native data directory and keyboard input. Dependencies
are pinned in `ziran.lock`; there is no vendored UI or parallel legacy client.

Project links: [website](https://uku.waozi.xyz),
[itch.io](https://waozi.itch.io/ukuvota). Source changes do not imply these
hosted versions have been updated.

## Build and check

Install the [Ziran compiler](https://github.com/ziranlang/ziran) using its
`make install-user` command. Clone this repository normally; no submodules
are required. Linux needs a C compiler, CMake, pkg-config, Python 3, SDL2,
Cairo and SQLite development packages. Ziran builds the pinned toolchain and
resolves package dependencies.

```sh
make test                 # source checks, native/portable core tests, private-display smoke
make run                  # build and open the Linux app
make install              # install under ~/.local
make deb                  # build a Debian package for this machine
make appimage             # requires linuxdeploy and appimagetool in PATH
```

For development in canonical organization checkouts, use an ignored
`ziran.local.toml`:

```toml
[overrides]
ziran = "../../ziranlang/ziran"
kryon = "../../kryonlabs/kryon"
sqlite = "../../ziranlang/packages/sqlite"
daochi_client = "../../ziranlang/packages/daochi-client"
kss = "../../kryonlabs/packages/kss"
```

Release builds use the exact lock **without** local overrides. The compiler
rejects `--locked` while overrides are present. CI installs the compiler
revision in the lock and builds without this file.

The browser build needs Emscripten (tested with 6.0.6), Tcl and CMake:

```sh
make web                  # HTML, JavaScript, Wasm and both distribution ZIPs
make serve                # http://127.0.0.1:8080
make site                 # static site and embedded app in build/site
make web-test             # complete browser flow in both packaged versions
```

`EMCC` and `EMCMAKE` can select Emscripten tools; the default is
`~/emsdk/upstream/emscripten`. Browser data lives in IndexedDB under the app's
origin. Use Download backup before clearing browser data. If saving fails,
keep the page open and download the backup, then use Retry saving. Browser
text fields use native editing, including selection, paste, IME and touch
keyboards. System appearance follows the browser's color preference.

The browser tests need Node 22, Chromium, Xvfb, xauth, Tesseract OCR and Pillow. They
use a disposable profile and private display, recognize rendered controls,
and verify proposals, distinct voters, ballot editing, ties, quorum, mobile
touch input, reload persistence and downloaded SQLite backups. Both the web
distribution and the website's app path run through the same tests.
To verify a deployed website with disposable browser data, run
`UKU_WEB_URL=https://uku.waozi.xyz node tests/web_test.mjs --site`.
The site's HTML headers prevent hosting proxies from injecting scripts. Keep
`no-transform` on HTML responses when configuring another host.

Android needs JDK 17 or newer, Android SDK 36, NDK 28.2.13676358 and CMake 3.22.1:

```sh
make android
sh scripts/android-build.sh -Puku.onlyAbi=arm64-v8a
ANDROID_TASK=assembleRelease sh scripts/android-build.sh -PfDroidBuild
ANDROID_TASK=bundleRelease sh scripts/android-build.sh -PfDroidBuild
```

The last two commands produce unsigned releases. For signed releases provide
`KEYSTORE_PASSWORD` and the Gradle `keystore.path`/`keystore.alias` properties.
Generated artifacts are under `droid/app/build/outputs/`.

## Existing data

Linux stores `uku.sqlite3` in `$XDG_DATA_HOME/ukuvota` or
`~/.local/share/ukuvota`. `UKUVOTA_DATA_DIR` selects an explicit directory.
Android uses its private internal files directory.

If an earlier Linux build stored `uku.sqlite3` in its working directory, close
that app and back up the file. Run the new build with
`UKUVOTA_DATA_DIR=/absolute/path/to/that/directory` to open it in place, or copy
it to the new data directory while both apps are closed. Supported schemas
upgrade transactionally. An unknown schema is refused without deleting it.
Do not run both versions against the same database at once.

## Release version

Add a numeric entry at the top of `CHANGELOG.md`, then run
`./update_version.sh` to synchronize Android's version name and code. Release
tags are created only by the GitHub Actions release workflow.
