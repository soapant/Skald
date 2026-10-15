{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  name = "flint";

  nativeBuildInputs = with pkgs; [
    gcc
    pkg-config
    patchelf
  ];

  buildInputs = with pkgs; [
    libx11
    libxft
    libxrender
    libxrandr
    libxext
    libxtst
    libxi
    xorgproto
    freetype.dev
    glib
    glib.dev
    libpng.dev
    fontconfig.dev
    wayland
    pulseaudio
    curl.dev
  ];

  shellHook = ''
    echo "flint dev shell - run ./nob to build"
  '';
}
