# UltimateCraft – Player Manual (v5.1.0)

## Getting started
Run `ultimatecraft` (Linux/macOS) or `ultimatecraft.exe` (Windows) from its folder – it needs the `res/` folder next
to it. **Singleplayer → Create New World**, pick a name, seed and game mode. New players get short tutorial hints at
the top of the screen (turn them off with `tutorial=99` in `settings.txt`).

Default controls (all rebindable in Options → Controls & Accessibility): **WASD** move, **Space** jump (double-tap to
fly in creative), **Left Shift** sneak, **Left Ctrl** sprint, **E** inventory, **Q** drop, **F** swap offhand,
**1–9 / wheel** hotbar, **left mouse** attack/break, **right mouse** use/place, **T** chat, **/** command,
**F1** hide HUD, **F2** screenshot, **F3** debug info, **F11** fullscreen, **Esc** Game Menu. Controllers work too
(see README).

## The journey
1. **Overworld** – gather wood and stone, craft tools, find iron and diamonds, build a home. Nights bring monsters.
2. **Hell** (obsidian portal, flint and steel) – fire, ghosts of every biome, fortresses with the Brimstone Revenant.
   Bring gold weapons, holy water and a soul lantern.
3. **The Underground City** (`/locate underground_city`, around y −48) – the lamplit city of the Delvari. Trade with
   merchants for Delvari Coins, beware Hollow Sentinels, and defeat **Morrowgate, Keeper of the Deep** in the keep for
   the **Void Sigil**.
4. **Space** – find a stronghold with Star Eyes, fill the portal frame and offer the Void Sigil. Space has no air:
   wear a Space Helmet, drink Oxygen Canisters or stay near Oxygen Blooms. Defeat **Zorvath, the Cosmic Tyrant**
   (three phases) for the Sift Key and the Tyrant's Crown; a gold gateway to the Sift opens.
5. **The Sift** – sky islands of eternal day. Talk to wandering souls; pass the trials of **Solenne** (Temple of Dawn:
   bring 4 Radiant Crystals), **Zephyrine** (Spire of Winds: climb to her) and **Thalor** (Arena of Stone: defeat his
   Avatar) to receive the three blessings and the **Ascension** ending.

The in-game **Guide** (Game Menu) summarises this. **Advancements** and **Statistics** are in the Game Menu too.

## Survival notes
* Hunger, health, armour, enchanting, brewing, anvils, villagers and trading, fishing, farming and redstone work much
  like you would expect from a block-building game. Villagers keep a daily routine and remember how you treat them.
* Ghosts phase through walls and take double damage from gold, divine and blessed weapons, holy water and Smite.
* Sculk sensors hear your footsteps (sneak to stay quiet); four shrieks summon a warden.
* Worlds autosave; each time a world opens a backup goes to `backups/<world>/1..3`. The world list has **Backup**,
  **Export** (to `exports/`) and **Import** (from `imports/`) buttons.

## Game modes and difficulty
Survival, Creative, Adventure, Spectator; Peaceful/Easy/Normal/Hard (bosses scale with difficulty); Hardcore worlds.
Cheats (commands) can be toggled in the Game Menu unless the world is Hardcore. See `docs/COMMANDS.md`.

## Troubleshooting
* Black or very slow screen: lower the render distance or pick **Graphics: Low** in Options; update your GPU driver
  (OpenGL 3.3 is required).
* Crashes write `crash.log` (and on Windows the log goes to `latest.log`) – please attach it to bug reports.
