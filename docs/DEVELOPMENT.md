# UltimateCraft – developer documentation

> The technical companion to the README: build details, the full feature inventory, module layout, the test-script
> language and save formats. Players start with `docs/MANUAL.md`.


## Building

### Windows (MSYS2 **UCRT64** – the toolchain you already use)

Open the *MSYS2 UCRT64* terminal and install the tools once:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake make
```

Then, from the `UltimateCraft` folder:

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./ultimatecraft.exe
```

The executable is written next to `res/`, linked statically (no DLLs to copy), so you can also
double-click `ultimatecraft.exe` in Explorer.

GLFW is compiled straight from `lib/glfw/src`; its own outdated CMake scripts are never used, which
avoids the *"Compatibility with CMake < 3.5 has been removed"* error that stopped Craft from building
with CMake 4.x.

Alternative without CMake (same terminal): `make -j8` → `./ultimatecraft.exe`.
Or simply `sh build_windows.sh` (same terminal).

**No toolchain?** The repository includes a GitHub Actions workflow (`.github/workflows/build.yml`): push the
project to GitHub and every push builds `ultimatecraft.exe` on a Windows runner. Download it from the run's
*Artifacts* (`UltimateCraft-windows`, which contains the exe plus the `res/` folder it needs).

### Linux

```sh
sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
cmake -S . -B build && cmake --build build -j && ./ultimatecraft
```

or simply `make -j8`. Audio uses ALSA if present (loaded at runtime, no dev package needed).

### Requirements
OpenGL 3.3 graphics. The game runs silently if no audio device is available.

---

## Controls

| Action | Key |
|---|---|
| Move / jump | `W A S D` / `Space` |
| Sprint / sneak | `Ctrl` (while moving forward) / `Shift` (won't walk off edges) |
| Break / attack | Left mouse (hold to mine) |
| Place / use / eat / draw bow | Right mouse (hold to eat or charge the bow) |
| Pick block | Middle mouse |
| Hotbar | `1`–`9`, mouse wheel |
| Inventory | `E` |
| Drop item / whole stack | `Q` / `Ctrl+Q` |
| Chat / command | `T` / `/` |
| Pause | `Esc` |
| Hide HUD / screenshot / debug | `F1` / `F2` / `F3` |
| Creative flight | double-tap `Space` (Space up, Shift down) |
| Swap item to off-hand | `F` |
| Elytra glide | `Space` while falling with an elytra equipped (fireworks boost) |

Inventory screens: left-click picks up/places/swaps, right-click splits or places one, `Shift`-click
quick-moves, number keys swap with the hotbar, clicking outside the window drops the held stack.

---

## Features

**World**
- Fixed-height columns (16×128×16), generated on background threads, loaded/unloaded around the player.
- Deterministic generation: the same seed always gives the same world, independent of load order.
- Four dimensions: the Overworld, **Hell** (the old Nether), **Space** (the old End, low gravity, vacuum) and
  **the Sift** (sky islands over a cloud sea). One is loaded at a time; each saves separately.
- 65 biomes chosen with a multi-noise climate model (temperature, humidity, continentalness, erosion,
  weirdness) like Minecraft's, with biome-coloured grass, foliage and water.
- Rivers, mountain ranges and peaks, mushroom islands, worm caves + caverns, lava below y 9, every ore at
  Minecraft-like depths (deepslate variants deep down, emeralds in mountains, extra gold in badlands),
  27 tree types, coral reefs, kelp, icebergs, ice spikes, bamboo, terracotta bands, lush/dripstone/deep-dark caves.
- Sky light + coloured (RGB) block light with flood-fill propagation and correct removal.
- Smooth lighting with per-vertex ambient occlusion, face shading, fog, frustum culling,
  hidden-face removal, waving leaves/plants, animated water and lava.
- Day/night cycle (20 min): sunrise/sunset colours, sun, moon, twinkling stars, drifting clouds, moonlight.
- Block updates: support rules for plants/torches/doors/beds/ladders, spreading grass,
  crops and stems growing, saplings (including 2×2 trees), concrete powder hardening.
- **Falling blocks are entities** (sand, gravel, concrete powder, anvils ...): they fall under gravity, sink through water,
  land as a block (or break into their drops when the landing cell is blocked), are saved with the world, and
  anvils crush whoever is underneath. Far from the player they settle instantly instead.
- **Leaf decay** follows the real rule: a leaf survives while a log is reachable through connected leaves
  (at most 6 blocks), found with a breadth-first search; felling a trunk lets the canopy decay. Every leaf kind has a
  second, *persistent* state that the game uses for leaves a player places; those never decay.
- **Flowing water and lava** with Minecraft's rules: 8 levels + falling fluid, flow toward the nearest drop
  (within 4 blocks), infinite water sources, slower lava (faster in the Nether), sloped surfaces, currents
  that push the player, mobs, dropped items and boats (waterfalls pull things down); lava + water make obsidian /
  cobblestone / stone (and basalt over soul soil); lava randomly sets adjacent flammable blocks alight.
- **Structures** (deterministic, seamless across columns, with their own loot tables, spawners and mobs):
  villages (5 biome styles, roads, a well and bell, houses with job sites, farms, libraries, smithies,
  villagers with professions, iron golem, cats), pillager outposts, desert pyramids (TNT trap and treasure
  room), jungle temples (arrow dispensers), witch huts, igloos (with a secret basement), ocean monuments
  (elder guardians, gold), shipwrecks, ocean ruins, buried treasure, ruined portals (Overworld and Nether),
  mineshafts (rails, chest minecarts, cave-spider spawners), dungeons, woodland mansions, ancient cities,
  trial chambers (trial spawners), stronghold library and store rooms (silverfish spawner), nether
  fortresses (blaze spawners, nether wart) and bastion remnants (piglin brutes), end cities with end ships
  (shulkers, elytra). Blazes, wither skeletons, guardians, witches and pillagers keep spawning inside their
  structures. `/locate <structure> [tp]` finds them.

**Player & survival**
- Walking, sprinting (with sprint-jump boost), sneaking with edge protection, swimming, creative flight.
- 10 hearts with half-hearts, hunger + saturation + exhaustion, natural regeneration, starvation.
- Damage: falling, lava, fire, drowning (air bubbles), cactus, explosions, mobs, arrows, the void.
- Invulnerability frames, red damage flash, camera hurt-tilt, explosion screen-shake.
- Death screen with cause and score, items and XP dropped where you died, respawn at the world spawn.
- XP orbs that fly to you, Minecraft's level curve, XP bar + level number, level-up sound.

**Mining, items & crafting**
- 1,539 items and 3,509 block states (1,059 block kinds), generated from the full Minecraft item list by `tools/gen_content.py`.
- Hardness and tool rules: wrong tool is slower, ores need the right pickaxe tier to drop.
  Crack overlay while mining, debris particles, per-material sounds, tool durability.
- 2×2 crafting in the inventory, 3×3 on the crafting table: 1,036 shaped/shapeless recipes with tags.
- Furnace, blast furnace (2× ores), smoker (2× food), stonecutter, smithing table (netherite upgrades),
  brewing stand (all base potions, splash/lingering, Redstone-free strength upgrades).
- Chests, barrels, trapped/copper chests, ender chest (shared inventory), hopper/dispenser storage,
  armour (7 materials with toughness) and an off-hand slot.
- Creative inventory with Minecraft's tabs and a search box, plus a creative game mode.

**Redstone**
- Dust networks (power 0–15, losing one per block, connecting up/down steps, glowing by strength), levers,
  buttons, pressure plates (weighted ones by entity count), redstone torches (inverters, 1-tick delay),
  repeaters (1–4 tick delay, locking), comparators (compare/subtract, container fill levels, cake,
  composter, jukebox), observers, daylight detectors (and inverted), targets, redstone blocks.
- Strong vs weak power through solid blocks like Minecraft. Pistons and sticky pistons (push 12 blocks,
  break fragile blocks, shove entities), redstone lamps, iron/wooden doors, trapdoors, fence gates, TNT,
  note blocks, dispensers (arrows, projectiles, buckets, fire, bone meal, spawn eggs, TNT) and droppers,
  hoppers (pull/push, locked by power), powered / detector / activator rails.

**Enchanting, anvils & other workstations**
- All 42 enchantments with their gameplay effects (Sharpness/Smite/Bane, Knockback, Fire Aspect, Looting,
  Sweeping Edge, Efficiency, Silk Touch, Fortune, Unbreaking, Mending, Protection ×5 kinds, Thorns,
  Respiration, Aqua Affinity, Depth Strider, Frost Walker, Soul Speed, Swift Sneak, Power, Punch, Flame,
  Infinity, Multishot, Quick Charge, Piercing, Loyalty, Impaling, Riptide, Density, Wind Burst, curses...).
  Enchanted items shimmer and list their enchantments in tooltips.
- Enchanting table (bookshelf count, three offers with hints, lapis + level cost), anvil (repair with
  material or a second item, merge enchanted books, rename, prior-work penalty, "Too Expensive!",
  chipping), grindstone (disenchant for XP, combine), loom (27 banner patterns incl. pattern items; placed
  banners show their patterns), cartography table (zoom out, copy, lock), lectern.
- Maps: empty maps become filled maps that draw the terrain around you while held (height shading,
  biome colours, water depth) and show in the HUD with your position arrow.
- **Recipe book** in the inventory and crafting table: recipes unlock as you pick up ingredients; search,
  "craftable only" filter, click to fill the grid from your inventory (shift-click fills a stack).

- Shulker boxes keep their contents when broken (listed in the tooltip); stairs form inner/outer corners.

**Villagers & trading**
- Villagers take a profession from the nearest job-site block (13 professions + nitwits), trade with
  Minecraft's offers for each level (novice → master), gain experience and level up, restock twice a day.
  Librarians sell enchanted books, smiths sell enchanted gear. Wandering traders have their own offers.

**Riding**
- Boats (every wood, rafts, chest boats) float, follow currents and steer with WASD; minecarts follow
  rails (slopes, curves), speed up on powered rails, trigger detector rails; chest, furnace (coal-fuelled),
  TNT and hopper minecarts. Horses (tame by riding, saddle, jump), donkeys, mules, camels (dash),
  skeleton/zombie horses, llamas, pigs (carrot on a stick, boost) and striders (warped fungus on a stick,
  walk on lava). Shift dismounts.

**Mobs**
- All 88 mobs with Minecraft health, damage, speed, drops and XP (zombies still use your skin).
- 26 AI kinds: melee, ranged (arrows, tipped arrows, fireballs, wither skulls, shulker bullets, potions,
  llama spit, wind charges, tridents), creeper fuse, slime splitting, fliers, swimmers, endermen
  (stare to provoke, teleport), phantoms, guardian lasers, warden (vibrations, sonic boom), creaking
  (moves only unobserved), evoker fangs + vexes, golems, villagers fleeing zombies, Wither and Ender Dragon bosses.
- Breeding, babies, taming (wolves, cats, parrots, horses...), shearing, milking, egg-laying, frogs eating slimes.
- Textured 3D box models in Minecraft's UV layout, with walk, attack, head-look, death and burning
  animations, red hurt tint and blob shadows.
- AI: wander and idle, target detection with line of sight, chase, jump obstacles, avoid cliffs and
  lava, swim, push apart from other entities.
- Undead burn in sunlight. Hostiles spawn in darkness or at night and despawn far away; animals
  spawn on grass.
- Loot and XP on death; animals killed by fire drop cooked meat.

**Combat**
- Attack cooldown (Minecraft 1.9 style) with an indicator under the crosshair.
- Weapon damage by tier, critical hits when falling, knockback (sprint for extra), hit and crit sounds and particles.
- Bow with charge-up, arrow drop and recoverable arrows.

**Presentation**
- Title screen, world list (select, play, delete with confirmation), world creation (name, seed,
  game mode), settings, pause, death, loading, chat and commands.
- HUD: hotbar, hearts (shake at low health), hunger, armour, air, XP, held-item name, chat, F3 debug screen.
- Particles: block debris, smoke, explosions, crits, damage hearts, bubbles, splashes, flames.
- Audio: 171 sound effects built from real recordings (public domain / CC0 / CC BY; credits in `tools/audio_src/CREDITS.md`):
  layered per-material block impacts and footsteps, an idle / hurt / death voice for each of the 35 creature voice kinds,
  doors, chests, levers, pistons, anvils, weather. Files live in `res/audio/<name>_<take>.ogg` (several takes per sound, one
  picked at random) and are decoded at start-up with stb_vorbis; generative ambient music is synthesised. 3D positional mixing
  with distance-based air absorption. WinMM on Windows, ALSA on Linux. A built-in synthesiser (`src/audio/audio.c`,
  `src/audio/voices.inc`) is the automatic fallback for any missing file. `tools/build_audio.py` rebuilds the files and
  `tools/sound_lab.py` renders spectrograms.

**Saving**
`saves/<world>/` contains:
- `level.txt`: seed, time, spawn, game mode, current dimension, whether the Void Wyrm and Zorvath are defeated, the Sift gateway.
- `player.dat`: position, health, hunger, XP, effects, inventory, off-hand and ender chest.
- `entities.dat` + `columns/` (Overworld), `DIM-1/` (Hell), `DIM1/` (Space), `DIM2/` (the Sift): mobs, items, orbs,
  container contents, and only the terrain you changed. Everything is stored by Minecraft id.

The game autosaves every 5 minutes, on *Save & Quit*, and when you close the window.

---

## Commands (chat: `T`, or type `/`)
`/help`, `/give <item> [n]`, `/summon <mob> [n]`, `/time set day|noon|sunset|night|midnight|<ticks>`,
`/gamemode survival|creative`, `/tp x y z` (supports `~` relative coordinates), `/kill`, `/killall`,
`/heal`, `/feed`, `/xp <n>`, `/seed`, `/spawnpoint`, `/setblock x y z <block>[:state]`, `/fly`, `/save`,
`/screenshot [name]`, `/biome`, `/locate biome <id> [tp]`, `/locate stronghold`,
`/dimension overworld|hell|space|sift` (old `nether`/`the_nether`/`end`/`the_end` still work), `/effect <effect> [seconds] [level]` / `/effect clear`, `/hud`,
`/locate <structure> [tp]` (village, pillager_outpost, desert_pyramid, jungle_pyramid, swamp_hut, igloo,
monument, shipwreck, ocean_ruin, buried_treasure, ruined_portal, mineshaft, mansion, ancient_city,
trial_chambers, stronghold, fortress, bastion_remnant, ruined_portal_nether, end_city),
`/enchant <enchantment> [level]` (held item), `/book <enchantment> [level]` (enchanted book).

Items, blocks and mobs accept Minecraft ids (`/give minecraft:netherite_sword`, `/summon wandering_trader`,
`/setblock ~ ~ ~2 oak_stairs:3`); potions take a type: `/give potion 1 swiftness`.

---

## Project layout

```
src/
  core/     common types + math, RNG/hashing, platform layer (time, files, threads)
  gfx/      window + input, shaders, textures, 2D/3D batch renderers, frustum, PNG screenshots
  world/    block registry, columns + streaming, lighting, mesher, world generation, sky
  item/     item registry, inventory, recipes, smelting
  entity/   entity pool + physics + damage, mob table + AI + spawning, box models, rendering
  game/     game loop + screens state, player, particles, saving, commands / test scripting
  ui/       UI toolkit (font, panels, slots, buttons, sliders), HUD, all menus and containers
  audio/    sound synthesis, mixer, WinMM / ALSA backends
  gen/      AUTO-GENERATED tables: blocks, items, recipes, biomes, mobs (do not edit by hand)
res/        shaders and textures (generated by the tools)
tools/      gen_content.py: blocks, items, recipes, biomes, mobs, block/item atlases, mob models + skins
            content/: the rule tables it reads (items_list.csv, blocks.py, items.py, recipes.py, biomes.py, mobs.py, paint.py)
            gen_assets.py: GUI, font, particles, sky textures (then runs gen_content.py)
tests/      scripted play-throughs (run with --script)
lib/        GLFW, glad, stb_image, noise1234 (vendored)
```

**Adding things** — edit the Python tables, then run `python tools/gen_content.py` (needs Pillow) and rebuild:
- New item/block: add a row to `tools/content/items_list.csv`; blocks are classified by id rules in
  `blocks.py` (add a `SPECIAL` entry for anything unusual), items in `items.py`.
- New recipe: one `shaped(...)` / `shapeless(...)` / `smelt(...)` line in `recipes.py`.
- New biome: one `B(...)` entry in `biomes.py` (+ a slot in the climate tables in `worldgen.c`).
- New mob: one `M(...)` entry in `mobs.py` (stats, AI kind, drops, colours, model) + spawn rules there.
- Saves store Minecraft ids, so renumbering never breaks worlds. Worlds from before this content update
  (save version 1) can't be loaded.

---

## Where it came from (analysis summary)

| Project | Tech | Kept / ported |
|---|---|---|
| **minecraft-weekend** | C11, OpenGL 3.3, GLFW, glad, ECS | Foundation: rendering approach, biome concept, RGB lighting idea, collision approach (its textures are *not* used any more: every texture is generated) |
| **Craft** | C99, OpenGL 2, SQLite, multiplayer | Ambient occlusion, frustum culling, worker-thread generation, "store only modified terrain" saving, chat commands |
| **Minecraft (Python/pyglet)** | Python, pyglet | Nothing is used any more; the code itself was too basic to port. |

Bugs found in the original foundation and fixed by the rewrite:
- **Windows-specific**
  - The `sort_r` Windows branch did not compile.
  - Lighting allocated 1–3 MB queues on the stack, which crashes on Windows' 1 MB stack.
- **Lighting**
  - `light_apply` read `elements[i]` instead of `elements[j]`.
  - Sunlight did not propagate across chunk borders.
- **World**
  - The bordering-chunk test used `x` instead of `y`.
  - Trees crossing chunk borders lost their leaves after reloading.
  - Heightmap smoothing left seams at chunk borders.
  - There were no caves, no ores and no frustum culling, and worst-case memory use was about 1 GB.
- **Rendering**
  - Animated water and lava copied only the red channel.
  - The aspect ratio never updated on window resize.
  - Nights were pitch black.
  - The star renderer reseeded the global RNG every frame.
- **Input**
  - Mouse movement events overwrote each other instead of accumulating.
  - Mouse look was framerate-dependent.

---

## UltimateCraft's own dimensions (new)

| Dimension | id (aliases) | What's there |
|---|---|---|
| Hell | `hell` (`the_nether`, `nether`) | the old Nether terrain, Hell fortresses and bastions, and the **Ghost** |
| Space | `space` (`the_end`, `end`) | asteroid islands under a full starfield, 0.4× gravity, no air: wear a **Space Helmet** or have **Oxygen** |
| the Sift | `sift` (`heaven`) | floating islands of Holy Stone and Golden Grass, Crystal Trees, a Cloud Sea, eternal soft daylight |

- **Ghost**: flies, phases through walls now and then, drains health and leaves you **Haunted**
  (no natural healing). Gold weapons, Smite, soul items and flaming arrows hit it twice as hard;
  anything else does half. Drops Ectoplasm.
- **Reaching the Sift**: build a portal-shaped frame (like a Hell portal, 2×3 inside or larger) out of
  gold blocks, with glowstone allowed anywhere in the frame, in the Overworld, and use the **Sift Key** on it.
  Defeating Zorvath also raises a gold gateway to the Sift on Space's central island (`/siftgate` teleports to it).

### The road to the Sift (v5)

1. **The Underground City** (`/locate underground_city`) – a walled city of the Delvari, lantern-keepers of
   the deep, in a vast cavern around y −48. Built from modular pieces on a 7×7 grid: the **Central Keep**
   (curtain wall, four gates, throne hall, vault), the **Market** (stalls and *Delvari Merchants* who trade in
   Delvari Coins), the **Temple**, the **Prison** and barracks (*Hollow Sentinels*), a residential row, plazas with
   fountains, moss gardens and collapsed ruins. Amber **Umbral Lamps** light the streets.
   On the throne waits **Morrowgate, Keeper of the Deep** (mini-boss, 320 HP, ground slam, lantern fire, summons
   sentinels at half health). He drops the **Void Sigil**.
2. **The Space portal** – Star Eyes (the old Eyes of Ender) still align the stronghold frame, but it only opens when
   you offer a **Void Sigil** (right-click a full frame with it, or carry one when you place the last eye).
   Creative mode skips the sigil.
3. **Space** – Oxygen Blooms grow on the outer asteroids and refill your air nearby; **Oxygen Canisters**
   (crafted from a bloom, glass and iron, or bought from Delvari merchants) give 3 minutes of Oxygen.
   New mobs: the **Void Drifter** (hovers, then dives and stings: brief levitation) and the armoured **Asteroid Crawler**.
4. **Zorvath, the Cosmic Tyrant** – the ultimate boss of Space (replaces the old dragon fight; the dragon is now the
   *Void Wyrm*, an optional `/summon`). It wakes when you approach and has three phases, each announced with a title:
   *Herald of Stars* (star-bolt volleys, healed by Star Crystals), *The Gravity Well* (66 %: periodic pulls toward
   itself, Void Drifter minions) and *Collapse of the Stars* (33 %: five-bolt volleys, meteor rain, crawler and
   drifter minions). Health and damage scale with difficulty (Easy ×0.75, Hard ×1.5). The fight survives
   save/reload. Drops the **Tyrant's Crown** (a wearable trophy helmet), the **Sift Key**, nether stars and
   stardust, 15,000 XP the first time, and opens the gateway to the Sift.

**Hell, expanded (v5):** every Hell biome now has its own ghost: the **Ember Wraith** (Basalt Deltas, sets you on
fire), the **Fungal Shade** (Crimson and Warped Forests, poison) and the **Cinder Specter** (Hell Wastes, weakness and
Haunted), besides the Ghost of the Soul Sand Valley. All are translucent, phase through walls and take double damage
from holy sources. Holy tools against them:
**Holy Water** (splash; 8 damage to ghosts and undead in 3.5 blocks, cures Haunted, puts out fire), the **Blessed
Sword** (iron sword + 2 gold + holy water), and **soul lanterns**: hold one, or place one, and ghosts are driven off
and seared. Ectoplasm brews the **Tonic of Ghost Sight** (ghosts within 24 blocks glow and cannot phase near you)
and the **Phasing Pearl** (steps you through up to 10 blocks of wall). Where the bridges of every Hell fortress
cross now stands an arena ruled by the **Brimstone Revenant** (mini-boss: fire nova, fireball volleys, summons wither
skeletons and a specter at half health); it drops the **Brimstone Heart** (8 bottles of holy water at once).
Hell has its own ambience (a deep rumble, distant wails) and richer fog colours.

**Re-themed (display names, models and colours; ids unchanged):** Enderman → Voidwalker, Endermite → Void Mite,
Ender Dragon → Void Wyrm, Eye of Ender → Star Eye, Ender Pearl → Void Pearl, Ender Chest → Void Chest,
Dragon's Breath/Egg/Head → Wyrm's Breath/Egg/Head, Shulker → Starshell, Purpur → Nebulite, Chorus → Starbloom.
The Space starfield now has bigger, brighter stars and glowing giants.

5. **The Sift and the Luminaries** – an original, fictional pantheon (no real-world faith):

   | Luminary | Domain / temple (`/locate`) | Trial | Blessing |
   |---|---|---|---|
   | **Solenne, the Dawnkeeper** | Golden Meadows – Temple of Dawn (`temple_of_dawn`) | bring 4 Radiant Crystals | **Light**: purges Haunted/Darkness/Blindness, sears nearby undead, your blows are holy |
   | **Zephyrine, Lady of the High Winds** | Cloud Sea – Spire of Winds (`spire_of_winds`) | climb her 34-block spire; afterwards she trades | **Sky**: slow falling, no fall damage, higher jumps |
   | **Thalor the Stoneheart** | Crystal Grove – Arena of Stone (`arena_of_stone`) | defeat the **Avatar of Thalor** (boss, crystal eruptions) | **Vigor**: +2 hearts per level and regeneration |

   Right-click a Luminary (or a soul) to talk. **Wandering Souls** drift over the islands: they share memories and
   hints, leave a small blessing and Soul Essence once, and trade if you sneak-click them. Passing all three trials
   triggers the **Ascension** ending and the credits (`/credits` replays them). Trial progress is saved.
   **Divine gear** (sword, tools, armour; Divine Ingot = 2 gold ingots + Radiant Crystal + Soul Essence) is the
   strongest tier; divine weapons are holy, and the Divine Helmet lets you breathe in Space. The Sift has its own
   music (bells over a pad) and a deeper blue sky. Health Boost now works too.

---

## Testing

`tests/*.txt` are scripted play-throughs that drive the real game with simulated input and assert on
game state. Run them all with `tests/run_all.sh`, or one at a time:

```sh
./ultimatecraft --script tests/gameplay.txt --no-audio
```

`tests/content.txt` checks that all 1,539 items exist, that every one of the 1,036 recipes reproduces
its own result through the matcher, that all surface biomes generate within 6,000 blocks, that every mob
spawns and ticks, and Overworld → Nether → End travel. `tests/tour.txt` captures a biome / dimension / UI
screenshot tour (`screenshots/tour_*.png`).

`tests/redstone.txt` checks dust power levels, lamps, torch inverters, repeater delays, pistons, sticky
pistons, TNT and fluid flow/levels/reactions. `tests/features.txt` checks the enchanting table, anvil
merge + rename, grindstone, loom, maps and the cartography table, the recipe book, villager jobs and
trades, riding horses, boats and minecarts on powered rails. `tests/structures.txt` visits villages,
outposts, witch huts, monuments, mineshafts, nether fortresses, bastions and end cities and checks their
mobs, chests and spawners.

`tests/dimensions.txt` covers dimension ids and aliases, Space's vacuum / helmet / Oxygen / low gravity, lighting
and walking through a Sift gate, saving and reloading inside the Sift and returning. `tests/hell.txt` covers the Ghost
(damage rules, Haunted, phasing, holy gold, ectoplasm). `tests/space.txt` (29 checks) covers the Space mobs,
oxygen canisters and blooms, and the whole Zorvath fight: waking, three phases with titles, a save/reload in the
middle of phase II, gravity pulls, minion summons, meteors, the kill, drops, the Sift gateway and walking through
it. `tests/city.txt` (23 checks) covers `/locate underground_city`, the districts, merchants and their offers,
sentinels, loot chests, the Keeper's phases and slam, the Void Sigil drop and the sigil-gated Space portal.
`tests/blessings.txt` covers Health Boost and the three blessings; `tests/sift.txt` covers souls, all three
temples and trials, the Avatar, saving trial progress, the Ascension/credits and divine gear.
`tests/hellextras.txt` covers the three ghost variants, holy water, the blessed sword, soul lanterns, Ghost Sight,
the Phasing Pearl, Hell ambience and the Brimstone Revenant's arena, phases and drop.

They cover mining, placing, crafting through mouse clicks, combat to XP, mobs, night spawning,
explosions, skeleton arrows, death/respawn, furnace smelting, fall damage, eating, the bow, menus,
save → quit → reload, and world streaming. Other useful flags: `--size W H`, and `--export-sounds <dir>`,
which writes every synthesised sound as a WAV file.

## Safety, saves and legal (v5)

* Saves are written atomically (temp file + fsync + rename); a half-finished save is recovered from its `.tmp`.
* Each time you open a world, a backup is copied to `backups/<world>/1..3` (newest first, worlds up to 256 MB).
* `crash.log` gets a stack trace if the game crashes (`kill -USR1 <pid>` dumps one on Linux/macOS without quitting).
* Data packs: `saves/<world>/datapacks/<pack>/data/<namespace>/function/*.mcfunction`, or `./datapacks/` for every
  world; `data/ultimatecraft/tags/function/load.json` and `tick.json` list functions to run. See the example pack in
  `datapacks/ultimatecraft_examples`.
* `THIRD_PARTY_LICENSES.md`, `PRIVACY.md` and the draft `EULA.md` ship with the game. UltimateCraft is an independent
  game and is not affiliated with Mojang Studios or Microsoft.

## Settings, accessibility, controllers and languages (v5)

Options → **Controls & Accessibility…** holds every rebindable key (click a key, then press the new one; duplicates
are marked), Sneak/Sprint **Hold or Toggle**, **Less Motion** (no view bobbing, hurt tilt or shake), **Subtitles**
(captions with direction arrows), a **Colour-blind** palette (reds → orange, greens → blue), **High Contrast** text,
the **Narrator** hook (chat, titles and captions printed to stdout as `[narrate] …`, ready to pipe into a screen
reader or TTS), **Fullscreen** (or F11), a **Max FPS** cap and the **Simulation** distance (mobs and random ticks
stop beyond it; terrain still renders out to the render distance). Options also has **Language** (English, Deutsch
– add more by copying `res/lang/en_us.lang`) and **Clouds**. Everything is saved in `settings.txt`.

**Controllers** (any pad SDL/GLFW knows as a gamepad): left stick move, right stick look, A jump, B sneak, L3 sprint,
Y inventory, X drop, LB/RB hotbar, RT attack, LT use, Start menu; in menus the left stick moves a cursor and A clicks.
