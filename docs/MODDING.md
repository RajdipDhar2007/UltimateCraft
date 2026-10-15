# Modding reference (v5.1.0)

## Data packs (no programming)
`saves/<world>/datapacks/<pack>/data/<namespace>/function/<name>.mcfunction` (one command per line, `#` comments)
or the same layout in `./datapacks/` for every world. `data/ultimatecraft/tags/function/load.json` and `tick.json`
(`{"values": ["ns:name", …]}`) run functions when a world loads and every game tick (20/s). `/function ns:name`,
`/reload`, `/datapack list`. `maxCommandChainLength` limits commands per call. Example: `datapacks/ultimatecraft_examples`.

## Languages
Copy `res/lang/en_us.lang` to `res/lang/<code>.lang` and translate the right-hand side (keep `%s`/`%d` in order,
UTF-8, Latin-1 letters are in the font). Add the code and its name to `LANG_CODES`/`LANG_NAMES` in `src/ui/lang.c`.

## Content (C build)
Blocks, items, mobs, biomes, effects and recipes are generated from Python tables in `tools/content/`:
`items_list.csv` (append rows), `blocks.py` (block shapes/textures), `items.py`, `mobs.py` (models, stats, spawns),
`biomes.py`, `recipes.py`. Run `python3 tools/gen_content.py` (deterministic) and rebuild. Append new biomes, effects and
mobs at the end of their lists – indices are stored in saves; blocks and items are saved by key. Mob behaviour lives in
`src/entity/mob.c` (AI kinds), structures in `src/world/structures_build.inc`, dimensions in `src/world/dimension.c`.

## Skins, textures and sounds
Everything is generated; nothing needs an image editor.
* **Block textures** are painter functions (`p_stone`, `p_planks`, ...) in `tools/content/paint.py`; `tnoise()` gives tileable
  fractal noise. Re-run `python3 tools/gen_content.py` to rebuild `res/textures/blocks.png`.
* **Mob skins** come from `paint_skin()` in `tools/content/mobs.py`: set a mob's `colors` (`"*"` for the body, one entry per
  part role such as `"HEAD"` or `"LEG_FR"`, plus `"eyes"`, `"spots"` and `"pattern"`), and add its key to the `FUR`, `WOOL`,
  `FEATHER` or `SCALE` sets for a material texture.
* **Sounds** are real recordings. `res/audio/<name>_<take>.ogg` (mono Ogg Vorbis, any sample rate) replaces the built-in
  synthesised sound `<name>`; several takes (`_0`, `_1`, ... up to 6) are chosen at random. Names: `dig_stone`, `step_grass`,
  `door_open`, `explode` ... for effects and `mob_<kind>_<idle|hurt|death>` for creatures (`mob_cow_idle_0.ogg`; the kind list is
  `SOUND_KINDS` in `tools/content/mobs.py`). To change one, drop in your own file - or edit its recipe in
  `tools/audio_recipes*.py` (trim, transpose, layer, filter, reverb) and run `python tools/build_audio.py cow` to rebuild just
  the sounds whose name contains "cow". Source recordings live in `tools/audio_src/`; **only add recordings that are public
  domain, CC0 or CC BY, and credit them in `tools/audio_src/CREDITS.md`.** A creature's voice kind is the `sound=("cow", 1.0)`
  argument of `M(...)`; to add a new voice kind append it to `SOUND_KINDS`, raise `MOB_SOUND_KINDS` and `SND_MOB_LAST` in
  `src/audio/audio.h`, add it to `MOB_KIND_FILE` and `MOB_CAPTION` in `src/audio/audio.c` (subtitles), and add three recipes.
  Audition with `./ultimatecraft --export-sounds out/`, then `python tools/sound_lab.py out out/lab.jpg`.

## Saves
`saves/<world>/level.txt` (key=value), `player.dat`, `entities.dat` (format UCE5), `columns/` (palette by key:variant),
`scoreboard.txt`, `stats.txt`, `advancements.txt`; per dimension `forceload.txt`, `commandblocks.txt`. All files are
written atomically (temp + rename).

## Testing
`./ultimatecraft --script tests/<name>.txt --no-audio` runs a scripted test (see the README for the script language);
`tests/run_all.sh` runs the whole suite.
