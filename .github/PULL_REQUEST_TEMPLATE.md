# Pull Request Checklist

Please ensure that your pull request addresses the following points:

- [ ] Have any of the opcode or instruction bit formatting changed in a breaking way?
    - If yes, please bump the ABI version in `llvm/lib/Target/Parasol/MCTargetDesc/ParasolELFObjectWriter.cpp`.
- [ ] Run `./format-parasol.sh` from the root directory to ensure your code is properly formatted.
- [ ] Updated the license-sunscreen-changes.txt with any changes to comply with the AGPLv3 and Apache licenses.
- [ ] Ensured that any files in this PR contain the Sunscreen license notice.

## Release Checklist (if creating a new release)

If this PR is for creating a new release, use `./release-all.sh` to build all architectures and update the Nix configuration:

```bash
./release-all.sh              # Uses today's date
./release-all.sh 2025.11.21   # Custom version
```

The script builds macOS (native) and Linux (Docker) tarballs, calculates hashes, and updates `sunscreen-llvm.nix`.

Before merging:
- [ ] Review changes: `git diff sunscreen-llvm.nix`
- [ ] Commit: `git add sunscreen-llvm.nix && git commit -m 'chore: release vYYYY.MM.DD'`

After PR is merged:
- [ ] Create tag on main branch: `git tag vYYYY.MM.DD`
- [ ] Push tag: `git push origin vYYYY.MM.DD`
- [ ] Create GitHub release with tarballs from `releases/` directory

---

Thank you for your contribution to the Sunscreen LLVM fork!
