{
  description = "Uku native Raylib and Flint development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;

      mkPkgs = system: import nixpkgs {
        inherit system;
        config.allowUnsupportedSystem = true;
      };

      mkShell = system:
        let
          pkgs = mkPkgs system;
        in
          pkgs.mkShell {
            packages = with pkgs; [
              SDL2
              SDL2.dev
              cmake
              curl
              curl.dev
              gcc
              gnumake
              libdrm
              libdrm.dev
              libgbm
              libglvnd
              libglvnd.dev
              mesa
              pkg-config
              sqlite
              zlib
            ];

            shellHook = ''
              export RAY_PKGS="sdl2 libdrm gbm egl glesv2"
              export RAY_SDL_CFLAGS="$(pkg-config --cflags sdl2)"
              export RAY_SDL_LDLIBS="$(pkg-config --libs sdl2)"
              export RAY_GL_CFLAGS="$(pkg-config --cflags libdrm gbm egl glesv2)"
              export RAY_GL_LDLIBS="$(pkg-config --libs libdrm gbm egl glesv2)"
              export RAY_CFLAGS="$RAY_SDL_CFLAGS $RAY_GL_CFLAGS"
              export RAY_LDLIBS="$RAY_SDL_LDLIBS $RAY_GL_LDLIBS"
              export RAY_SDL_INCLUDE_DIR="$(pkg-config --variable=includedir sdl2 | sed 's,/SDL2$,,')"
              export RAY_RAYLIB_CONFIG="-DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0"
            '';
          };
    in {
      devShells = forAllSystems (system: {
        default = mkShell system;
      });
    };
}
