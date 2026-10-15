# Command reference (v5.1.0)

Commands are typed in chat starting with `/` (Tab completes names, items, mobs, blocks, rules and structures). They
need cheats on (Game Menu). Selectors: `@p @a @r @s @e @n` with `[type=..,distance=..,limit=..,tag=..,name=..]`;
coordinates accept `~` (relative) and `^` (local). Ids may use `ultimatecraft:` or no namespace.

| Command | Purpose |
|---|---|
| `/help` | list commands |
| `/gamemode <mode> [target]`, `/difficulty <d>`, `/gamerule <rule> [value]` | modes, difficulty, the 57 game rules |
| `/give <target> <item> [count]`, `/clear`, `/item replace entity|block … with <item> [count]` | items |
| `/summon <mob or item_frame/painting/armor_stand> [pos]`, `/kill <targets>` | entities |
| `/tp`, `/spreadplayers <x> <z> <dist> <range> <respectTeams> <targets>`, `/spawnpoint`, `/setworldspawn` | movement |
| `/setblock`, `/fill`, `/clone`, `/fillbiome <from> <to> <biome>` | blocks and biomes |
| `/effect give|clear`, `/enchant`, `/experience`, `/attribute <target> <attr> get|base get|base set <v>` | player and mob state |
| `/damage <target> <amount> [type] [by <entity>]` | deal damage |
| `/time set|add`, `/weather clear|rain|thunder`, `/tick freeze|step|unfreeze` | world time |
| `/locate structure|biome <id> [tp]` | find structures (e.g. `underground_city`, `temple_of_dawn`, `spire_of_winds`, `arena_of_stone`, `fortress`) |
| `/execute as|at|positioned|rotated|facing|align|in|if|unless|store … run <command>` | conditional and repeated commands; `store result|success score <holder> <obj>`, `if score <h> <obj> matches <range>` / `<op> <h2> <obj2>` |
| `/scoreboard objectives add|remove|list|setdisplay sidebar`, `/scoreboard players set|add|remove|get|reset|list|operation` | scores (criteria `dummy`, `deathCount`, `totalKillCount`, `health`) |
| `/data get entity|block …`, `/data merge entity <t> {Health:..,Fire:..,Air:..,NoGravity:..}` | read and change data |
| `/worldborder get|set|add|center|damage|warning` | world border (pushes and hurts players outside, visible wall) |
| `/forceload add|remove|query` | keep chunks loaded (while inside the loaded window) |
| `/loot give|spawn|insert … loot <table>` | loot tables (`simple_dungeon`, `delvari_vault`, `sift_temple`, …) |
| `/function <ns:name>`, `/reload`, `/datapack list` | data packs (`docs/MODDING.md`) |
| `/advancement grant|revoke <id>|everything`, `/advancement list` | advancements |
| `/title`, `/tellraw`, `/say`, `/msg`, `/me`, `/playsound`, `/particle` | messages and effects |
| `/tag`, `/ride`, `/trigger`, `/recipe`, `/seed`, `/list` | misc |
| `/guide [page]`, `/credits`, `/siftgate`, `/advancements`, `/statistics` | UltimateCraft extras |

Command blocks (creative): place one and right-click to enter a command. Impulse blocks run when powered, repeating
blocks run 20×/s while powered, chain blocks run after a neighbour that ran. Output shows only with
`/gamerule commandBlockOutput true`.
