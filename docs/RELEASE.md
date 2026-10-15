# Release guide

1. Bump `UC_VERSION` in `src/core/common.h` and `AppVer` in `installer/UltimateCraft.iss`; update `CHANGELOG.md`.
2. Run the suite (`tests/run_all.sh`) and an ASan build (README → Development).
3. Push a tag `vX.Y.Z`: GitHub Actions builds Windows (portable zip + Inno Setup installer), Linux (.tar.gz, tests run
   headless) and macOS (.zip) and attaches them to a GitHub Release.
4. **itch.io**: `butler push UltimateCraft-windows.zip <user>/ultimatecraft:windows --userversion X.Y.Z` (same for
   linux / mac channels). **Steam**: one depot per OS containing the portable folder; launch option `ultimatecraft.exe`
   (Windows) / `ultimatecraft` (Linux); working directory = install folder. Store text must not use third-party
   trademarks; screenshots come from `screenshots/` (F2).
5. Ship `EULA.md` (after legal review), `PRIVACY.md` and `THIRD_PARTY_LICENSES.md` with every package (the
   packaging script does this).

macOS builds are unsigned: notarisation needs an Apple developer account (`codesign` + `notarytool`).
