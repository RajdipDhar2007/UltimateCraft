# Contributing to UltimateCraft

Thank you for helping! Bug reports, translations, textures, mobs, sounds and pull requests are all welcome.

## Quick start
```sh
git clone https://github.com/RajdipDhar2007/UltimateCraft.git
cd UltimateCraft
make -j4 && ./ultimatecraft          # see the README for Windows (MSYS2) and macOS notes
```

## Where things live
| You want to change... | Edit... |
|---|---|
| Block / item / recipe / biome / mob **tables**, textures and mob skins | `tools/content/*.py`, then run `python tools/gen_content.py` (needs Pillow) |
| HUD, font, particles, sky textures | `tools/gen_assets.py` |
| Sounds and creature voices | `tools/audio_recipes*.py` + `python tools/build_audio.py` (real recordings in `tools/audio_src/`); engine in `src/audio/audio.c` |
| Water, lava, falling blocks, leaves, redstone | `src/world/`, `src/entity/` |
| Translations | copy `res/lang/en_us.lang` to a new file |

Textures and skins are drawn by code; sounds are edited from real, openly licensed recordings. **Do not add third-party textures or models**, and never anything extracted from other games. New sound recordings must be
your own, public domain, CC0 or CC BY, and credited in `tools/audio_src/CREDITS.md`.

## Before you open a pull request
1. Build with no new warnings.
2. Run the test scripts your change touches: `./ultimatecraft --script tests/<name>.txt --no-audio`
   (`sh tests/run_all.sh` runs every script). Add a script under `tests/` for new behaviour - see
   [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for the script language.
3. If you changed `tools/content/`, commit the regenerated files under `src/gen/`, `src/entity/model_defs.h` and `res/textures/`.
4. Keep commits focused and describe *why* in the message.

By contributing you agree to the contribution terms in [LICENSE](LICENSE) (section 4).

## Code of conduct
Be kind, assume good faith and keep discussion about the work. Harassment or discrimination of any kind is not
tolerated; maintainers may remove comments and block repeat offenders.
