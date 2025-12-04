# Modified by Sunscreen under the AGPLv3 license; see the README at the
# repository root for more information

{ lib, stdenv, fetchurl, autoPatchelfHook, zlib }:

let
  version = "2025.11.24";
  fileVersion = builtins.replaceStrings [ "." ] [ "-" ] version;
  urlBase =
    "https://github.com/Sunscreen-tech/sunscreen-llvm/releases/download/v${version}";

in stdenv.mkDerivation rec {
  pname = "sunscreen-llvm";
  inherit version;

  src = if stdenv.isDarwin then
    fetchurl {
      url = "${urlBase}/parasol-compiler-macos-aarch64-${fileVersion}.tar.gz";
      sha256 = "sha256-DFvJYNYvLHl0IA9jAc1XOlFEcdojUElpY/s1FstNO3I=";
    }
  else if stdenv.isLinux && stdenv.isAarch64 then
    fetchurl {
      url = "${urlBase}/parasol-compiler-linux-aarch64-${fileVersion}.tar.gz";
      sha256 = "sha256-aH0wLwaBnAnlhw+0bIZrNYUHcU5b+aU6F/fzqLBq1FM=";
    }
  else if stdenv.isLinux && stdenv.isx86_64 then
    fetchurl {
      url = "${urlBase}/parasol-compiler-linux-x86-64-${fileVersion}.tar.gz";
      sha256 = "sha256-aS0WmsZKAHQ/DT2G0IFVcSZbz3RINWc3AC1OdMX6hMw=";
    }
  else
    throw "Unsupported platform: ${stdenv.system}";

  strictDeps = true;

  nativeBuildInputs = lib.optionals stdenv.isLinux [ autoPatchelfHook ];

  buildInputs = lib.optionals stdenv.isLinux [
    stdenv.cc.cc.lib # Provides libstdc++ and libgcc_s
    zlib
  ];

  # The tarball extracts to current directory, not a subdirectory
  sourceRoot = ".";

  # Don't run configure or build - this is a binary package
  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out
    cp -r * $out/

    runHook postInstall
  '';

  meta = with lib; {
    description =
      "Sunscreen LLVM compiler for parasol target (FHE compilation)";
    homepage = "https://github.com/Sunscreen-tech/sunscreen-llvm";
    license = licenses.agpl3Only;
    platforms = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
    mainProgram = "clang";
  };
}
