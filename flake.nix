# Modified by Sunscreen under the AGPLv3 license; see the README at the
# repository root for more information

{
  description = "Sunscreen LLVM compiler for parasol target (FHE compilation)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Sunscreen LLVM compiler for parasol target
        sunscreen-llvm = pkgs.callPackage ./sunscreen-llvm.nix { };

        # Helper script to run compile-llvm.sh from anywhere
        compile-llvm = pkgs.writeShellScriptBin "compile-llvm" ''
          #!/usr/bin/env bash
          set -euo pipefail

          # Get the root directory of the git repository
          ROOT_DIR=$(git rev-parse --show-toplevel 2>/dev/null || echo "$PWD")

          # Run the compile-llvm.sh script in the root directory
          "$ROOT_DIR/compile-llvm.sh" "$@"
        '';

        # Helper script to run format-parasol.sh from anywhere
        format-parasol = pkgs.writeShellScriptBin "format-parasol" ''
          #!/usr/bin/env bash
          set -euo pipefail

          # Get the root directory of the git repository
          ROOT_DIR=$(git rev-parse --show-toplevel 2>/dev/null || echo "$PWD")

          # Run the format-parasol.sh script in the root directory
          "$ROOT_DIR/format-parasol.sh" "$@"
        '';

      in {
        packages = {
          inherit sunscreen-llvm;
          default = sunscreen-llvm;
        };

        apps.default = {
          type = "app";
          program = "${sunscreen-llvm}/bin/clang";
        };

        checks = {
          inherit sunscreen-llvm;
        };

        devShells.default = pkgs.mkShellNoCC {
          buildInputs = [
            # Build tools
            pkgs.ninja
            pkgs.diffoscopeMinimal

            # Helper scripts
            compile-llvm
            format-parasol
          ];

          shellHook = ''
            echo "Sunscreen LLVM development environment loaded."
            echo ""
            echo "Available tools:"
            echo "  compile-llvm     - Compile LLVM from source"
            echo "  format-parasol   - Format Parasol-specific code"
            echo "  ninja            - Build system"
            echo "  diffoscope       - Binary comparison tool"
            echo ""
            echo "To use the pre-built sunscreen-llvm compiler in your project,"
            echo "add it to your flake inputs or use nix-shell with sunscreen-llvm.nix"
          '';
        };
      });
}
