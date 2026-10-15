"""Item registry: every row of items_list.csv becomes an item.  Block items share
their id with the canonical block state; pure items get ids >= ITEM_BASE."""
from colors import *
import blocks as B

MOBS = """allay armadillo axolotl bat camel camel_husk cat chicken cod copper_golem cow donkey frog glow_squid happy_ghast horse
mooshroom mule ocelot parrot pig rabbit salmon sheep skeleton_horse sniffer snow_golem squid strider sulfur_cube tadpole
tropical_fish turtle villager wandering_trader zombie_horse bee cave_spider dolphin drowned enderman fox goat iron_golem llama
nautilus panda piglin polar_bear pufferfish spider trader_llama wolf zombie_nautilus zombified_piglin blaze bogged breeze
creaking creeper elder_guardian endermite evoker ghast guardian hoglin husk magma_cube parched phantom piglin_brute pillager
ravager shulker silverfish skeleton slime stray vex vindicator warden witch wither_skeleton zoglin zombie zombie_villager
ender_dragon wither
ghost void_drifter asteroid_crawler cosmic_tyrant delvari_merchant hollow_sentinel deep_keeper sift_soul god_solenne god_zephyrine god_thalor thalor_avatar ember_wraith fungal_shade cinder_specter brimstone_revenant""".split()

CATS = ["BLOCK", "MATERIAL", "TOOL", "WEAPON", "FOOD", "ARMOR", "MISC"]
TABS = ["BUILDING", "COLORED", "NATURAL", "FUNCTIONAL", "REDSTONE", "TOOLS", "COMBAT", "FOOD", "INGREDIENTS", "SPAWN_EGGS", "OPERATOR"]
USES = ["NONE", "FOOD", "PLACE", "BUCKET", "FLUID_BUCKET", "MILK", "MOB_BUCKET", "THROW", "SPAWN_EGG", "BOW", "CROSSBOW", "TRIDENT",
        "FLINT_STEEL", "FIRE_CHARGE", "SHEARS", "BONE_MEAL", "SHIELD", "POTION", "ELYTRA", "TOTEM", "ENDER_EYE", "END_CRYSTAL",
        "SPYGLASS", "GOAT_HORN", "DISC", "FIREWORK", "COMPASS", "CLOCK", "MAP", "VEHICLE", "MACE", "BRUSH", "WIND", "OXYGEN", "INSTANT", "FISH", "DECOR"]
PROJ = ["ARROW", "SPECTRAL_ARROW", "TRIDENT", "SNOWBALL", "EGG", "ENDER_PEARL", "EYE", "SMALL_FIREBALL", "FIREBALL", "WITHER_SKULL",
        "DRAGON_FIREBALL", "SHULKER_BULLET", "LLAMA_SPIT", "POTION", "XP_BOTTLE", "WIND_CHARGE", "FIREWORK", "STAR_BOLT", "HOLY_WATER", "BOBBER"]
# status effects (id order matters: mirrored in C)
EFFECTS = ["none", "speed", "slowness", "haste", "mining_fatigue", "strength", "instant_health", "instant_damage", "jump_boost",
           "nausea", "regeneration", "resistance", "fire_resistance", "water_breathing", "invisibility", "blindness", "night_vision",
           "hunger", "weakness", "poison", "wither", "absorption", "saturation", "levitation", "slow_falling", "darkness", "glowing",
           "luck", "unluck", "health_boost", "conduit_power", "dolphins_grace", "bad_omen", "hero_of_the_village", "raid_omen",
           "trial_omen", "wind_charged", "weaving", "oozing", "infested",
           # UltimateCraft effects (appended: ids above are stable)
           "haunted", "oxygen", "blessing_of_light", "blessing_of_the_sky", "blessing_of_vigor", "ghost_sight"]
# potion types stored in ItemStack.damage for potion / splash_potion / lingering_potion / tipped_arrow
POTIONS = [  # key, display, effect, amplifier, seconds, colour
    ("water", "Water Bottle", "none", 0, 0, 0x385DC6), ("mundane", "Mundane Potion", "none", 0, 0, 0x385DC6),
    ("thick", "Thick Potion", "none", 0, 0, 0x385DC6), ("awkward", "Awkward Potion", "none", 0, 0, 0x385DC6),
    ("night_vision", "Potion of Night Vision", "night_vision", 0, 180, 0x1F1FA1), ("invisibility", "Potion of Invisibility", "invisibility", 0, 180, 0x7F8392),
    ("leaping", "Potion of Leaping", "jump_boost", 0, 180, 0x22FF4C), ("fire_resistance", "Potion of Fire Resistance", "fire_resistance", 0, 180, 0xE49A3A),
    ("swiftness", "Potion of Swiftness", "speed", 0, 180, 0x7CAFC6), ("slowness", "Potion of Slowness", "slowness", 0, 90, 0x5A6C81),
    ("water_breathing", "Potion of Water Breathing", "water_breathing", 0, 180, 0x2E5299), ("healing", "Potion of Healing", "instant_health", 0, 0, 0xF82423),
    ("harming", "Potion of Harming", "instant_damage", 0, 0, 0x430A09), ("poison", "Potion of Poison", "poison", 0, 45, 0x4E9331),
    ("regeneration", "Potion of Regeneration", "regeneration", 0, 45, 0xCD5CAB), ("strength", "Potion of Strength", "strength", 0, 180, 0x932423),
    ("weakness", "Potion of Weakness", "weakness", 0, 90, 0x484D48), ("slow_falling", "Potion of Slow Falling", "slow_falling", 0, 90, 0xFFEFD1),
    ("strong_healing", "Potion of Healing II", "instant_health", 1, 0, 0xF82423), ("strong_strength", "Potion of Strength II", "strength", 1, 90, 0x932423),
    ("strong_swiftness", "Potion of Swiftness II", "speed", 1, 90, 0x7CAFC6), ("strong_regeneration", "Potion of Regeneration II", "regeneration", 1, 22, 0xCD5CAB),
]

TIERS = {  # tier: (harvest level, speed, durability, sword, axe, axe_speed, pick, shovel, hoe_speed)
    "wooden": (1, 2, 59, 4, 7, 0.8, 2, 2.5, 1.0), "stone": (2, 4, 131, 5, 9, 0.8, 3, 3.5, 2.0), "copper": (2, 5, 190, 5, 9, 0.8, 3, 3.5, 2.0),
    "iron": (3, 6, 250, 6, 9, 0.9, 4, 4.5, 3.0), "golden": (1, 12, 32, 4, 7, 1.0, 2, 2.5, 1.0), "diamond": (4, 8, 1561, 7, 9, 1.0, 5, 5.5, 4.0),
    "netherite": (4, 9, 2031, 8, 10, 1.0, 6, 6.5, 4.0),
    "divine": (4, 11, 2800, 9, 11, 1.1, 7, 7.5, 4.0)}   # UltimateCraft: forged from the Sift's gifts
ARMOR = {  # material: (points h,c,l,b), durability multiplier, toughness, knockback resistance*10
    "leather": ((1, 3, 2, 1), 5, 0, 0), "copper": ((2, 4, 3, 1), 11, 0, 0), "chainmail": ((2, 5, 4, 1), 15, 0, 0),
    "iron": ((2, 6, 5, 2), 15, 0, 0), "golden": ((2, 5, 3, 1), 7, 0, 0), "diamond": ((3, 8, 6, 3), 33, 2, 0), "netherite": ((3, 8, 6, 3), 37, 3, 1), "divine": ((3, 8, 6, 3), 42, 4, 1)}
SLOT_BASE = (11, 16, 15, 13)
FOOD = {"apple": (4, 2.4), "baked_potato": (5, 6), "beef": (3, 1.8), "beetroot": (1, 1.2), "beetroot_soup": (6, 7.2), "bread": (5, 6),
        "carrot": (3, 3.6), "chicken": (2, 1.2), "chorus_fruit": (4, 2.4), "cod": (2, 0.4), "cooked_beef": (8, 12.8), "cooked_chicken": (6, 7.2),
        "cooked_cod": (5, 6), "cooked_mutton": (6, 9.6), "cooked_porkchop": (8, 12.8), "cooked_rabbit": (5, 6), "cooked_salmon": (6, 9.6),
        "cookie": (2, 0.4), "dried_kelp": (1, 0.6), "enchanted_golden_apple": (4, 9.6), "golden_apple": (4, 9.6), "golden_carrot": (6, 14.4),
        "glow_berries": (2, 0.4), "honey_bottle": (6, 1.2), "melon_slice": (2, 1.2), "mushroom_stew": (6, 7.2), "mutton": (2, 1.2),
        "poisonous_potato": (2, 1.2), "porkchop": (3, 1.8), "potato": (1, 0.6), "pufferfish": (1, 0.2), "pumpkin_pie": (8, 4.8),
        "rabbit": (3, 1.8), "rabbit_stew": (10, 12), "rotten_flesh": (4, 0.8), "salmon": (2, 0.4), "spider_eye": (2, 3.2),
        "suspicious_stew": (6, 7.2), "sweet_berries": (2, 0.4), "tropical_fish": (1, 0.2)}
# food side effects: (effect, amplifier, seconds, chance%)
FOOD_FX = {"golden_apple": [("regeneration", 1, 5, 100), ("absorption", 0, 120, 100)],
           "enchanted_golden_apple": [("regeneration", 1, 20, 100), ("absorption", 3, 120, 100), ("resistance", 0, 300, 100), ("fire_resistance", 0, 300, 100)],
           "rotten_flesh": [("hunger", 0, 30, 80)], "chicken": [("hunger", 0, 30, 30)], "spider_eye": [("poison", 0, 5, 100)],
           "poisonous_potato": [("poison", 0, 5, 60)], "pufferfish": [("hunger", 2, 15, 100), ("nausea", 0, 15, 100), ("poison", 1, 60, 100)],
           "suspicious_stew": [("night_vision", 0, 7, 100)]}
FOOD_REMAINDER = {"mushroom_stew": "bowl", "rabbit_stew": "bowl", "beetroot_soup": "bowl", "suspicious_stew": "bowl", "honey_bottle": "glass_bottle"}
PLACES = {"wheat_seeds": "wheat_crop", "carrot": "carrots", "potato": "potatoes", "beetroot_seeds": "beetroots", "melon_seeds": "melon_stem",
          "pumpkin_seeds": "pumpkin_stem", "sweet_berries": "sweet_berry_bush", "glow_berries": "cave_vines", "cocoa_beans": "cocoa",
          "redstone": "redstone_wire", "string": "tripwire", "torchflower_seeds": "torchflower_crop", "pitcher_pod": "pitcher_crop",
          "nether_wart": "nether_wart_crop"}
DISCS = ["13", "cat", "blocks", "chirp", "creator", "creator_music_box", "far", "lava_chicken", "mall", "mellohi", "stal", "strad",
         "ward", "11", "wait", "otherside", "relic", "5", "pigstep", "precipice", "tears"]

FUEL = {"coal": 1600, "charcoal": 1600, "coal_block": 16000, "lava_bucket": 20000, "blaze_rod": 2400, "dried_kelp_block": 4001,
        "stick": 100, "bowl": 100, "bamboo": 50, "scaffolding": 50, "crafting_table": 300, "chest": 300, "trapped_chest": 300, "barrel": 300,
        "bookshelf": 300, "chiseled_bookshelf": 300, "jukebox": 300, "note_block": 300, "ladder": 300, "lectern": 300, "composter": 300,
        "cartography_table": 300, "fletching_table": 300, "smithing_table": 300, "loom": 300, "daylight_detector": 300, "bow": 300,
        "crossbow": 300, "fishing_rod": 300, "wooden_sword": 200, "wooden_shovel": 200, "wooden_pickaxe": 200, "wooden_axe": 200,
        "wooden_hoe": 200, "azalea": 100, "flowering_azalea": 100, "mangrove_roots": 300, "dead_bush": 100, "bamboo_mosaic": 300,
        "bamboo_block": 300, "stripped_bamboo_block": 300}

def tab_of_block(key, kind):
    s = kind.states[0]
    if any(key.endswith(x) for x in ("_spawn_egg",)): return "SPAWN_EGGS"
    if s["flags"] & B.F_CREATIVE: return "OPERATOR"
    if any(d in key for d in DYES) and any(key.endswith(x) for x in ("wool", "carpet", "concrete", "concrete_powder", "terracotta", "glass", "pane", "shulker_box", "bed", "candle", "banner")): return "COLORED"
    if key in ("terracotta", "glass", "glass_pane", "shulker_box", "candle", "tinted_glass"): return "COLORED"
    if key in ("redstone_block", "redstone_torch", "repeater", "comparator", "piston", "sticky_piston", "observer", "hopper", "dispenser",
               "dropper", "lectern", "target", "lever", "daylight_detector", "sculk_sensor", "calibrated_sculk_sensor", "tripwire_hook",
               "trapped_chest", "tnt", "redstone_lamp", "note_block", "slime_block", "honey_block", "crafter") or "rail" in key or "lightning_rod" in key \
            or key.endswith("_button") or key.endswith("_pressure_plate"): return "REDSTONE"
    if s["ui"] != "NONE" or s["be"] != "NONE" or key in ("torch", "soul_torch", "copper_torch", "lantern", "soul_lantern", "end_rod", "campfire",
                                                       "soul_campfire", "bell", "beacon", "conduit", "flower_pot", "decorated_pot", "scaffolding",
                                                       "ladder", "iron_chain", "lodestone", "respawn_anchor", "beehive", "bee_nest", "end_portal_frame",
                                                       "dragon_egg", "sea_lantern", "glowstone", "shroomlight") or "sign" in key or "head" in key or "skull" in key \
            or "lantern" in key or "shelf" in key or "golem_statue" in key or "froglight" in key: return "FUNCTIONAL"
    natural = ("ore", "leaves", "sapling", "propagule", "coral", "grass", "fern", "flower", "mushroom", "fungus", "roots", "vines", "dirt", "sand",
               "gravel", "clay", "snow", "ice", "moss", "petals", "bush", "kelp", "cane", "bamboo", "dripleaf", "amethyst", "dripstone", "sculk",
               "nylium", "netherrack", "egg", "melon", "pumpkin", "cactus", "lily", "tulip", "orchid", "allium", "bluet", "daisy", "cornflower",
               "valley", "rose", "torchflower", "pitcher", "blossom", "eyeblossom", "lichen", "debris", "obsidian", "podzol", "mycelium", "mud",
               "bedrock", "seagrass", "pickle", "lilac", "peony", "sunflower", "azalea", "litter", "wildflowers", "cobweb", "dandelion", "poppy",
               "raw_", "soul_s", "magma", "basalt", "end_stone", "chorus", "hay", "bone_block", "sponge", "spawner", "wart_block", "honeycomb",
               "glow", "dried_ghast", "frogspawn", "creaking_heart", "vault", "spawner", "budding")
    if any(n in key for n in natural) and "bricks" not in key and "stairs" not in key and "slab" not in key and "wall" not in key: return "NATURAL"
    if key.endswith("_log") or key.endswith("_stem") or key in ("tuff", "calcite", "deepslate", "stone", "granite", "diorite", "andesite") and False: return "NATURAL"
    return "BUILDING"

class Item:
    def __init__(self, key, name, stack, obt):
        self.key, self.name, self.stack, self.obt = key, name, stack, obt
        self.cat, self.tab, self.icon, self.block = "MISC", "INGREDIENTS", None, None
        self.tool, self.tier, self.speed, self.dur, self.dmg, self.aspd = "NONE", 0, 0, 0, 1, 4
        self.food, self.sat10, self.slot, self.armor, self.tough10, self.kbres10 = 0, 0, -1, 0, 0, 0
        self.fuel, self.use, self.arg, self.remainder, self.fx = FUEL.get(key, 0), "NONE", 0, None, []
        self.flat = False   # block item drawn as a flat sprite of the block tile

def build(kinds):
    items = []
    kmap = {k.key: k for k in kinds}
    for key, name, stack, obt in B.ITEM_ROWS:
        it = Item(key, name, stack, obt)
        if key in kmap:
            k = kmap[key]
            s = k.states[0]
            it.block, it.cat, it.tab = key, "BLOCK", tab_of_block(key, k)
            if s["mesh"] in ("CROSS", "TORCH", "FLAT", "CROP") or k.place in ("WALLONLY", "DOUBLE"): it.flat = True
            if key.endswith("_log") or key.endswith("_wood") or key.endswith("_planks") or key.endswith("_stem") or key.endswith("_hyphae") or key.endswith("_sapling"):
                if not (key.startswith("crimson") or key.startswith("warped") or "stripped_crimson" in key or "stripped_warped" in key):
                    it.fuel = it.fuel or (100 if key.endswith("_sapling") else 300)
            if any(key.endswith(x) for x in ("_slab",)) and kmap[key].states[0]["flags"] & B.F_FLAMMABLE: it.fuel = 150
            if any(key.endswith(x) for x in ("_stairs", "_fence", "_fence_gate", "_trapdoor", "_pressure_plate")) and s["flags"] & B.F_FLAMMABLE: it.fuel = 300
            if key.endswith("_door") and s["flags"] & B.F_FLAMMABLE: it.fuel = 200
            if key.endswith("_button") and s["flags"] & B.F_FLAMMABLE: it.fuel = 100
            if key.endswith("_sign") and s["flags"] & B.F_FLAMMABLE: it.fuel = 200
            if key.endswith("_wool"): it.fuel = 100
            if key.endswith("_carpet"): it.fuel = 67
            if key.endswith("_banner"): it.fuel = 300
            if key.endswith("_bed"): it.stack = 1
            items.append(it)
            continue
        classify_item(it)
        items.append(it)
    return items

def classify_item(it):
    k = it.key
    it.cat, it.tab = "MATERIAL", "INGREDIENTS"
    for t, (lvl, spd, dur, sw, ax, axs, pk, sh, hs) in TIERS.items():
        for kind in ("sword", "shovel", "pickaxe", "axe", "hoe"):
            if k == f"{t}_{kind}":
                it.cat, it.tab = ("WEAPON", "COMBAT") if kind == "sword" else ("TOOL", "TOOLS")
                it.tool = {"sword": "SWORD", "shovel": "SHOVEL", "pickaxe": "PICKAXE", "axe": "AXE", "hoe": "HOE"}[kind]
                it.tier, it.speed, it.dur = lvl, spd, dur
                it.dmg = {"sword": sw, "axe": ax, "pickaxe": pk, "shovel": sh, "hoe": 1}[kind]
                it.aspd = {"sword": 1.6, "axe": axs, "pickaxe": 1.2, "shovel": 1.0, "hoe": hs}[kind]
                it.icon = ("tool", kind, t)
                return
    for m, (pts, mul, tough, kb) in ARMOR.items():
        for i, sl in enumerate(("helmet", "chestplate", "leggings", "boots")):
            if k == f"{m}_{sl}":
                it.cat, it.tab, it.slot, it.armor, it.dur, it.tough10, it.kbres10 = "ARMOR", "COMBAT", i, pts[i], SLOT_BASE[i] * mul, tough * 10, kb
                it.icon = ("armor", sl, m)
                return
    if k == "turtle_helmet":
        it.cat, it.tab, it.slot, it.armor, it.dur, it.icon = "ARMOR", "COMBAT", 0, 2, 275, ("armor", "helmet", "turtle")
        return
    if k == "space_helmet":   # protects from vacuum in Space
        it.cat, it.tab, it.slot, it.armor, it.dur, it.icon = "ARMOR", "COMBAT", 0, 2, 407, ("armor", "helmet", "iron")
        return
    if k == "blessed_sword":   # gold-infused and blessed: holy against ghosts and the undead
        it.cat, it.tab, it.tool, it.tier, it.speed, it.dur, it.dmg, it.aspd, it.icon = "WEAPON", "COMBAT", "SWORD", 3, 6, 600, 7, 1.6, ("tool", "sword", "golden")
        return
    if k == "umbral_blade":   # Delvari steel, found in the Underground City
        it.cat, it.tab, it.tool, it.tier, it.speed, it.dur, it.dmg, it.aspd, it.icon = "WEAPON", "COMBAT", "SWORD", 4, 9, 1800, 8, 1.6, ("tool", "sword", "diamond")
        return
    if k == "tyrant_crown":   # trophy from the Cosmic Tyrant: a wearable crown
        it.cat, it.tab, it.slot, it.armor, it.dur, it.tough10, it.icon = "ARMOR", "COMBAT", 0, 4, 1200, 30, ("armor", "helmet", "golden")
        return
    if k == "elytra":
        it.cat, it.tab, it.slot, it.dur, it.use, it.icon = "ARMOR", "TOOLS", 1, 432, "ELYTRA", ("mask", "elytra", 0x8A8AA8)
        return
    if k in FOOD:
        h, s = FOOD[k]
        it.cat, it.tab, it.food, it.sat10, it.use = "FOOD", "FOOD", h, int(round(s * 10)), "FOOD"
        it.fx = FOOD_FX.get(k, [])
        it.remainder = FOOD_REMAINDER.get(k)
        if k in PLACES: it.block_place = PLACES[k]
        it.icon = food_icon(k)
        return
    if k in PLACES:
        it.use, it.arg_block, it.tab = "PLACE", PLACES[k], "NATURAL" if k != "redstone" and k != "string" else ("REDSTONE" if k == "redstone" else "INGREDIENTS")
        it.icon = misc_icon(k)
        return
    if k.endswith("_spawn_egg"):
        mob = k[:-10]
        it.cat, it.tab, it.use, it.arg_mob = "MISC", "SPAWN_EGGS", "SPAWN_EGG", mob
        it.icon = ("egg", mob)
        return
    if k.startswith("music_disc_"):
        it.cat, it.tab, it.use, it.arg = "MISC", "TOOLS", "DISC", DISCS.index(k[11:])
        it.icon = ("mask", "disc", [0x60A040, 0xE0E040, 0xE06020, 0x40C0C0, 0xC060C0, 0x6080E0, 0xE0A060][it.arg % 7])
        return
    if k.endswith("_pottery_sherd"): it.icon = ("mask", "sherd", 0xA0603A); return
    if k.endswith("_smithing_template"): it.icon = ("mask", "template", 0x3A6A6A if k.startswith("netherite") else 0x6A5A8A); return
    if k.endswith("_banner_pattern"): it.icon = ("mask", "paper", 0xE8DCC8); it.stack = 1; return
    if k.endswith("_dye"): it.icon = ("mask", "dye", DYE_RGB[k[:-4]]); return
    if k.endswith("_harness"): it.tab, it.icon = "TOOLS", ("mask", "harness", DYE_RGB[k[:-8]]); return
    if k.endswith("_bundle") or k == "bundle":
        it.tab, it.icon = "TOOLS", ("mask", "bundle", DYE_RGB[k[:-7]] if k != "bundle" else 0x9C5A35); return
    if k.endswith("_horse_armor"): it.tab, it.icon = "COMBAT", ("mask", "horse_armor", tier_rgb(k[:-12])); return
    if k.endswith("_boat") or k.endswith("_raft") or "minecart" in k:
        w = k.replace("_chest_boat", "").replace("_boat", "").replace("_chest_raft", "").replace("_raft", "")
        it.tab, it.use = "TOOLS", "VEHICLE"
        it.icon = ("mask", "minecart" if "minecart" in k else "boat", PLANKS_RGB.get(w, 0x8A8A8A))
        return
    if k.endswith("_bucket"):
        it.tab = "TOOLS"
        if k == "water_bucket": it.use, it.arg_block = "FLUID_BUCKET", "water"
        elif k == "lava_bucket": it.use, it.arg_block = "FLUID_BUCKET", "lava"
        elif k == "powder_snow_bucket": it.use, it.arg_block = "FLUID_BUCKET", "powder_snow"
        elif k == "milk_bucket": it.use = "MILK"
        else: it.use, it.arg_mob = "MOB_BUCKET", k[:-7]
        it.remainder = "bucket"
        it.icon = ("bucket", {"water_bucket": 0x3F76E4, "lava_bucket": 0xE8701A, "milk_bucket": 0xF8F8F8, "powder_snow_bucket": 0xF0F8FF}.get(k, 0x4A80D0))
        return
    if k.endswith("_ingot") or k.endswith("_nugget"):
        col = {"iron": 0xD8D8D8, "gold": 0xF8D840, "copper": 0xE07A50, "netherite": 0x4A4245, "divine": 0xF6E7A8}[k.split("_")[0]]
        it.icon = ("mask", "ingot" if k.endswith("ingot") else "nugget", col)
        return
    if k == "carrot_on_a_stick" or k == "warped_fungus_on_a_stick":
        it.tab, it.dur, it.icon = "TOOLS", 25 if k.startswith("carrot") else 100, ("mask", "fishing_rod", 0xF08A20 if k.startswith("carrot") else 0x2AA595)
        return
    SPECIAL_ITEMS.get(k, lambda it: misc(it))(it)

def food_icon(k):
    cols = {"apple": ("fruit", 0xD02020), "golden_apple": ("fruit", 0xF8D840), "enchanted_golden_apple": ("fruit", 0xF8E870), "chorus_fruit": ("fruit", 0x9A6A9A),
            "baked_potato": ("raw", 0xC8903A), "potato": ("raw", 0xC8A050), "poisonous_potato": ("raw", 0x9AB040), "beetroot": ("fruit", 0xA02040),
            "carrot": ("carrot", 0xF08A20), "golden_carrot": ("carrot", 0xF8D840), "bread": ("bread", 0xC08A3A), "cookie": ("cookie", 0xC0803A),
            "pumpkin_pie": ("pie", 0xD88A2A), "melon_slice": ("fruit", 0xE04040), "dried_kelp": ("leather", 0x3A4A2A), "sweet_berries": ("berries", 0xC02030),
            "glow_berries": ("berries", 0xF0B030), "honey_bottle": ("bottle", 0xF5B020), "spider_eye": ("orb", 0xA02040), "rotten_flesh": ("meat", 0x8A6A3A),
            "mushroom_stew": ("bowl", 0xC0A080), "rabbit_stew": ("bowl", 0xB0703A), "beetroot_soup": ("bowl", 0xA02040), "suspicious_stew": ("bowl", 0x90A060)}
    if k in cols: return ("mask",) + cols[k]
    if k in ("cod", "salmon", "tropical_fish", "pufferfish", "cooked_cod", "cooked_salmon"):
        return ("mask", "fish", {"cod": 0xB09A7A, "salmon": 0xC05040, "tropical_fish": 0xF08A20, "pufferfish": 0xE0C040, "cooked_cod": 0xD8C8A0, "cooked_salmon": 0xD87A5A}[k])
    if k in ("chicken", "cooked_chicken", "rabbit", "cooked_rabbit", "mutton", "cooked_mutton"):
        return ("mask", "leg", {"chicken": 0xF0C0B0, "cooked_chicken": 0xC88A4A, "rabbit": 0xE8A090, "cooked_rabbit": 0xB0703A, "mutton": 0xD84A4A, "cooked_mutton": 0x9A5A3A}[k])
    return ("mask", "meat", {"beef": 0xC83A3A, "cooked_beef": 0x8A4A2A, "porkchop": 0xF09090, "cooked_porkchop": 0xC8805A}.get(k, 0xC86A4A))

def misc_icon(k):
    m = {"wheat_seeds": ("seeds", 0x5AA02A), "beetroot_seeds": ("seeds", 0xC0A060), "melon_seeds": ("seeds", 0x2A2A2A), "pumpkin_seeds": ("seeds", 0xE8D8A0),
         "torchflower_seeds": ("seeds", 0x8A6A3A), "pitcher_pod": ("seeds", 0x6A8A9A), "cocoa_beans": ("dye", 0x7A4A2A), "redstone": ("dust", 0xC01010),
         "string": ("string", 0xF0F0F0), "nether_wart": ("berries", 0xA01A2A)}
    return ("mask",) + m.get(k, ("generic", 0x8A8A8A))

def misc(it, tmpl="generic", col=0x9A9A9A, tab="INGREDIENTS"):
    it.icon = ("mask", tmpl, col); it.tab = tab

def S(tmpl, col, tab="INGREDIENTS", **kw):
    def f(it):
        it.icon, it.tab = ("mask", tmpl, col), tab
        for a, v in kw.items(): setattr(it, a, v)
    return f

SPECIAL_ITEMS = {
    "stick": S("stick", 0x8A6A3A), "coal": S("lump", 0x2A2A2A), "charcoal": S("lump", 0x3A3020), "diamond": S("gem", 0x5DECF5),
    "emerald": S("gem", 0x17DD62), "lapis_lazuli": S("gem", 0x1D47A5), "quartz": S("gem", 0xEAE4DA), "amethyst_shard": S("shard", 0xA87ADA),
    "raw_iron": S("raw", 0xD8AF93), "raw_copper": S("raw", 0xE0734F), "raw_gold": S("raw", 0xFCEE4B), "netherite_scrap": S("raw", 0x6A4A40),
    "flint": S("shard", 0x3A3A3A), "feather": S("feather", 0xF0F0F0), "gunpowder": S("dust", 0x5A5A5A), "wheat": S("wheat", 0xD8B850),
    "leather": S("leather", 0x9C5A35), "rabbit_hide": S("leather", 0xC8A070), "brick": S("ingot", 0xB0603A), "nether_brick": S("ingot", 0x4A2020),
    "resin_brick": S("ingot", 0xD9631B), "clay_ball": S("lump", 0xA1A7B2), "paper": S("paper", 0xF0F0E8), "book": S("book", 0x7A3A2A),
    "slime_ball": S("slime", 0x7EBF6E), "bone": S("bone", 0xF0F0E0), "bone_meal": S("dust", 0xF0F0F0, use="BONE_MEAL"),
    "sugar": S("dust", 0xFAFAFA), "glowstone_dust": S("dust", 0xF8D070), "blaze_powder": S("dust", 0xF8A020), "blaze_rod": S("rod", 0xF8C020),
    "breeze_rod": S("rod", 0xA0B8F0), "ghast_tear": S("shard", 0xE0F0F8), "magma_cream": S("orb", 0xE0702A), "fermented_spider_eye": S("orb", 0xA04A3A),
    "ink_sac": S("dye", 0x2A2A3A), "glow_ink_sac": S("dye", 0x5AE0C0), "prismarine_shard": S("shard", 0x63A39A), "prismarine_crystals": S("gem", 0xA8D8C8),
    "rabbit_foot": S("rabbit_foot", 0xC8A070), "phantom_membrane": S("leather", 0xC8C0A8), "nautilus_shell": S("generic", 0xE8D8C8),
    "heart_of_the_sea": S("orb", 0x3A7AD8), "nether_star": S("star", 0xF0F0F8), "shulker_shell": S("generic", 0x9A6A9A), "turtle_scute": S("generic", 0x47A036),
    "armadillo_scute": S("generic", 0xA06A5A), "echo_shard": S("shard", 0x1A4A5A), "disc_fragment_5": S("shard", 0x3A8A8A), "honeycomb": S("generic", 0xE5941E),
    "popped_chorus_fruit": S("fruit", 0xC8A0C8), "dragon_breath": S("bottle", 0x8A7AE8), "glistering_melon_slice": S("fruit", 0xF8D840),
    "firework_star": S("orb", 0x8A8A8A), "resin_clump": S("lump", 0xD9631B), "glass_bottle": S("bottle", 0xCFE8EC),
    "bowl": S("bowl", 0x8A6A3A), "bucket": S("bucket", 0x0, "TOOLS", use="BUCKET"),
    "bow": S("bow", 0x8A6A3A, "COMBAT", use="BOW", dur=384, stack=1, cat="WEAPON"),
    "crossbow": S("crossbow", 0x6A4A2A, "COMBAT", use="CROSSBOW", dur=465, stack=1, cat="WEAPON"),
    "trident": S("trident", 0x5AB0A0, "COMBAT", use="TRIDENT", dur=250, dmg=9, aspd=1.1, stack=1, cat="WEAPON"),
    "mace": S("mace", 0x6A6A6A, "COMBAT", use="MACE", dur=500, dmg=6, aspd=0.6, stack=1, cat="WEAPON"),
    "shield": S("shield", 0x8A6A3A, "COMBAT", use="SHIELD", dur=336, stack=1, cat="WEAPON"),
    "arrow": S("arrow", 0x8A8A8A, "COMBAT", cat="WEAPON"), "spectral_arrow": S("arrow", 0xF8D070, "COMBAT", cat="WEAPON"),
    "tipped_arrow": S("arrow", 0x7A3AC0, "COMBAT", cat="WEAPON"),
    "flint_and_steel": S("flint_and_steel", 0x8A8A8A, "TOOLS", use="FLINT_STEEL", dur=64, stack=1),
    "fire_charge": S("charge", 0xE0602A, "COMBAT", use="FIRE_CHARGE"), "wind_charge": S("charge", 0xB0C8F0, "COMBAT", use="THROW", arg_proj="WIND_CHARGE"),
    "shears": S("shears", 0xC8C8C8, "TOOLS", use="SHEARS", dur=238, tool="SHEARS", speed=5, stack=1),
    "fishing_rod": S("fishing_rod", 0x8A6A3A, "TOOLS", use="FISH", dur=64, stack=1), "brush": S("brush", 0xC0804A, "TOOLS", use="BRUSH", dur=64, stack=1),
    "spyglass": S("spyglass", 0xD8875F, "TOOLS", use="SPYGLASS", stack=1), "compass": S("compass", 0x8A8A8A, "TOOLS", use="COMPASS"),
    "recovery_compass": S("compass", 0x2A5A6A, "TOOLS", use="COMPASS", arg=1), "clock": S("clock", 0xF8D840, "TOOLS", use="CLOCK"),
    "map": S("map", 0xE8DCB8, "TOOLS"), "filled_map": S("map", 0xE8DCB8, "TOOLS", use="MAP"),
    "snowball": S("orb", 0xF8F8FF, "COMBAT", use="THROW", arg_proj="SNOWBALL"), "egg": S("egg", 0xE8D8C0, "COMBAT", use="THROW", arg_proj="EGG"),
    "blue_egg": S("egg", 0x8AB0D8, "COMBAT", use="THROW", arg_proj="EGG"), "brown_egg": S("egg", 0xB08050, "COMBAT", use="THROW", arg_proj="EGG"),
    "ender_pearl": S("pearl", 0x5A3AB8, "COMBAT", use="THROW", arg_proj="ENDER_PEARL"),
    "ender_eye": S("pearl", 0xE8C850, "INGREDIENTS", use="ENDER_EYE"),
    "experience_bottle": S("bottle", 0x7AE04A, "INGREDIENTS", use="THROW", arg_proj="XP_BOTTLE"),
    "potion": S("potion", 0x385DC6, "FOOD", use="POTION", stack=1), "splash_potion": S("splash", 0x385DC6, "COMBAT", use="THROW", arg_proj="POTION", stack=1),
    "lingering_potion": S("splash", 0x6A3DC6, "COMBAT", use="THROW", arg_proj="POTION", stack=1),
    "totem_of_undying": S("totem", 0xE8C840, "COMBAT", use="TOTEM", stack=1), "end_crystal": S("end_crystal", 0xD8A0D8, "COMBAT", use="END_CRYSTAL"),
    "goat_horn": S("goat_horn", 0xC8B8A0, "TOOLS", use="GOAT_HORN", stack=1), "firework_rocket": S("generic", 0xC03030, "TOOLS", use="FIREWORK"),
    "lead": S("lead", 0xC0A070, "TOOLS"), "name_tag": S("name_tag", 0xE8D8B0, "TOOLS"), "saddle": S("leather", 0x8A4A2A, "TOOLS"),
    "wolf_armor": S("horse_armor", 0xA06A5A, "COMBAT", dur=64, stack=1), "painting": S("painting", 0x8A6A3A, "FUNCTIONAL", use="DECOR"),
    "item_frame": S("item_frame", 0x8A6A3A, "FUNCTIONAL", use="DECOR"), "glow_item_frame": S("item_frame", 0x5AE0C0, "FUNCTIONAL", use="DECOR"),
    "armor_stand": S("armor_stand", 0x8A6A3A, "FUNCTIONAL", use="DECOR"), "writable_book": S("book", 0x5A3A2A, "TOOLS"), "written_book": S("book", 0x5A3A2A, "TOOLS"),
    "enchanted_book": S("book", 0x8A3AA0, "TOOLS"), "knowledge_book": S("book", 0x3A7AC0, "OPERATOR"), "debug_stick": S("stick", 0x9A4AA0, "OPERATOR"),
    "command_block_minecart": S("minecart", 0xC08A5A, "OPERATOR"), "trial_key": S("key", 0xD8875F, "INGREDIENTS"), "ominous_trial_key": S("key", 0x4A6A8A, "INGREDIENTS"),
    "ominous_bottle": S("bottle", 0x3A5A7A, "FOOD"),
    "ectoplasm": S("slime", 0xB8F0E4), "stardust": S("dust", 0xB8A8F8),
    "delvari_coin": S("orb", 0xD8B060), "soul_essence": S("orb", 0xCDEFFF), "brimstone_heart": S("orb", 0xF86A20),
    "holy_water": S("splash", 0xE8F4FF, "COMBAT", use="THROW", arg_proj="HOLY_WATER", stack=16),
    "ghost_sight_tonic": S("potion", 0x9AE8D8, "FOOD", use="INSTANT", stack=16), "phasing_pearl": S("pearl", 0xB8F0E4, "COMBAT", use="INSTANT", stack=16), "divine_ingot": S("ingot", 0xF6E7A8), "void_sigil": S("star", 0x7A4AF0, "TOOLS", stack=1),
    "oxygen_canister": S("bottle", 0x8AD8F8, "FOOD", use="OXYGEN", stack=16), "sift_key": S("key", 0xF8DC60, "TOOLS", stack=1),
}

def potion_names():
    return [p[1] for p in POTIONS]
