# Modified by Sunscreen under the AGPLv3 license; see the README at the
# repository root for more information

{ lib, stdenv, fetchurl, autoPatchelfHook, zlib }:

let
  version = "2025.11.21";
  fileVersion = builtins.replaceStrings [ "." ] [ "-" ] version;
  urlBase =
    "https://github.com/Sunscreen-tech/sunscreen-llvm/releases/download/v${version}";

in stdenv.mkDerivation rec {
  pname = "sunscreen-llvm";
  inherit version;

  src = if stdenv.isDarwin then
    fetchurl {
      url = "${urlBase}/parasol-compiler-macos-aarch64-${fileVersion}.tar.gz";
      sha256 = "15bskb9y7chwsxyc60rknkg8fg7zpqar3shhg98v4fhn7c153bn5";
    }
  else if stdenv.isLinux && stdenv.isAarch64 then
    fetchurl {
      url = "${urlBase}/parasol-compiler-linux-aarch64-${fileVersion}.tar.gz";
      sha256 = "0illxpr115jih1fsd8sfxg67231bbjygfwnyr3srvypppywlyy6d";
    }
  else if stdenv.isLinux && stdenv.isx86_64 then
    fetchurl {
      url = "${urlBase}/parasol-compiler-linux-x86-64-${fileVersion}.tar.gz";
      sha256 = "1bqkjra0czp9111idnwqpd1mj218gihh2wqisbrfwfl2pqn7c7wi";
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
