# Pull Request Checklist

Please ensure that your pull request addresses the following points:

- [ ] Have any of the opcode or instruction bit formatting changed in a breaking way?
    - If yes, please bump the ABI version in `llvm/lib/Target/Parasol/MCTargetDesc/ParasolELFObjectWriter.cpp`.
- [ ] Run `./format-parasol.sh` from the root directory to ensure your code is properly formatted.
- [ ] Updated the license-sunscreen-changes.txt with any changes to comply with the AGPLv3 and Apache licenses.
- [ ] Ensured that any files in this PR contain the Sunscreen license notice.

## Release Checklist (if creating a new release)

If this PR is for creating a new release, please complete the following:

- [ ] Update the version in `sunscreen-llvm.nix` (format: YYYY.MM.DD)
- [ ] Run `./package-release.sh` for all supported architectures:
    - [ ] macOS aarch64
    - [ ] Linux aarch64
    - [ ] Linux x86-64
- [ ] Update SHA256 hashes in `sunscreen-llvm.nix` for all three tarballs
- [ ] Follow the tag naming format: vYYYY.MM.DD (e.g., v2025.09.30)
- [ ] Create GitHub release with all tarballs attached

---

Thank you for your contribution to the Sunscreen LLVM fork!
