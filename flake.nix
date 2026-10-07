{
  description = "Ukuvota Ziran and Kryon development environment";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
    in {
      devShells = nixpkgs.lib.genAttrs systems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              gcc gnumake cmake pkg-config python3 openssl
              SDL2 cairo freetype fontconfig sqlite tcl zlib
              xorg.xorgserver xorg.xauth xdotool imagemagick
            ];
          };
        });
    };
}
