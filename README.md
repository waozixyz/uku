# Uku

<p align="center">
  <img src="assets/app/readme-banner-1280x360.png" alt="Ukuvota" width="960">
</p>

Website: https://uku.waozi.xyz

Itch: https://waozi.itch.io/ukuvota

Native Ukuvota client — collective decisions without hidden resistance.
Groups score every option from -3 to +3; negative scores are weighted so
resistance is visible, and every ballot includes Status quo and Repeat
process as explicit choices. Voting needs no account: participants just
pick a name.

Runs on Linux, the web (Emscripten), and Android.

## Build

Clone with submodules:

```sh
git clone --recursive https://github.com/waozixyz/uku.git
cd uku
```

Linux prerequisites: a C compiler, cmake, pkg-config, SDL2, SQLite3, and
libcurl dev headers (or let the build compile the vendored curl and
liboqs itself). Then:

```sh
make linux-x86_64
./build/linux/uku-linux-x86_64
```

Web build (needs Emscripten):

```sh
make web
make run          # local dev server
make itch         # static itch.io HTML5 package
```

If `butler` is logged in, `make itch-push` uploads the HTML5 package to
`waozi/ukuvota:html5`.

Android (via Gradle, see `droid/`):

```sh
make android-debug
```
