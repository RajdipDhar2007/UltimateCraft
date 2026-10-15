"""Every Minecraft biome (Overworld, Nether, End) with climate, colours, surface and features.
The C world generator (src/world/worldgen.c) picks biomes with a multi-noise climate model
modelled on Minecraft's OverworldBiomeBuilder and decorates them from these flags."""

TREES = ["NONE", "OAK", "FANCY_OAK", "BIRCH", "TALL_BIRCH", "SPRUCE", "PINE", "MEGA_SPRUCE", "MEGA_PINE", "JUNGLE", "MEGA_JUNGLE",
         "JUNGLE_BUSH", "ACACIA", "DARK_OAK", "PALE_OAK", "MANGROVE", "CHERRY", "AZALEA", "SWAMP_OAK", "BUSH", "HUGE_RED_MUSHROOM",
         "HUGE_BROWN_MUSHROOM", "CRIMSON_FUNGUS", "WARPED_FUNGUS", "CHORUS", "FALLEN_OAK", "FALLEN_BIRCH", "FALLEN_SPRUCE", "CRYSTAL"]
FEATS = ["CACTUS", "DEAD_BUSH", "SUGAR_CANE", "PUMPKIN", "MELON", "BERRIES", "BAMBOO", "LILY", "MUSHROOMS", "SEAGRASS", "KELP", "CORAL",
         "ICE_SPIKES", "BOULDERS", "PODZOL", "COARSE", "TERRACOTTA", "PEAKS_SNOW", "POWDER_SNOW", "CALCITE", "EMERALD", "DRIPSTONE",
         "LUSH", "SCULK", "FIREFLY", "LEAF_LITTER", "WILDFLOWERS", "PETALS", "EYEBLOSSOM", "PALE_MOSS", "DRY_GRASS", "BUSHES",
         "ICEBERG", "GRAVEL_SHORE", "STONE_SHORE", "MUD", "FREEZE", "VINES", "COCOA", "GLOW_LICHEN", "SEA_PICKLE", "MOSS"]
DIMS = ["OVERWORLD", "NETHER", "END", "SIFT"]   # NETHER is shown as Hell, END as Space

def B(key, name, temp, down, precip="rain", top="grass_block", filler="dirt", under="sand", trees=(), tree_density=0.0,
      grass=0.0, flowers=(), flower_density=0.0, feats=(), grass_col=None, foliage_col=None, water=0x3F76E4, sky=None, fog=0xC0D8FF,
      dim="OVERWORLD", cat="land", deep=None):
    return dict(key=key, name=name, temp=temp, down=down, precip=precip, top=top, filler=filler, under=under, trees=list(trees),
                tree_density=tree_density, grass=grass, flowers=list(flowers), flower_density=flower_density, feats=list(feats),
                grass_col=grass_col, foliage_col=foliage_col, water=water, sky=sky, fog=fog, dim=dim, cat=cat)

STD_FLOWERS = ["dandelion", "poppy"]
BIOMES = [
    # ---------------------------------------------------------------- overworld land
    B("plains", "Plains", 0.8, 0.4, grass=0.30, trees=["OAK", "FANCY_OAK"], tree_density=0.0015, flowers=["dandelion", "poppy", "azure_bluet", "oxeye_daisy", "cornflower", "red_tulip", "white_tulip"], flower_density=0.012, feats=["SUGAR_CANE", "PUMPKIN", "BUSHES"]),
    B("sunflower_plains", "Sunflower Plains", 0.8, 0.4, grass=0.30, trees=["OAK"], tree_density=0.001, flowers=["sunflower", "dandelion", "poppy", "oxeye_daisy"], flower_density=0.03, feats=["SUGAR_CANE", "PUMPKIN"]),
    B("snowy_plains", "Snowy Plains", 0.0, 0.5, precip="snow", grass=0.02, trees=["SPRUCE"], tree_density=0.001, feats=["FREEZE"]),
    B("ice_spikes", "Ice Spikes", 0.0, 0.5, precip="snow", top="snow_block", filler="dirt", feats=["ICE_SPIKES", "FREEZE"]),
    B("desert", "Desert", 2.0, 0.0, precip="none", top="sand", filler="sand", under="sand", feats=["CACTUS", "DEAD_BUSH", "DRY_GRASS"]),
    B("swamp", "Swamp", 0.8, 0.9, grass=0.12, trees=["SWAMP_OAK"], tree_density=0.012, flowers=["blue_orchid"], flower_density=0.01, feats=["LILY", "MUSHROOMS", "SUGAR_CANE", "VINES", "FIREFLY", "SEAGRASS"], grass_col=0x6A7039, foliage_col=0x6A7039, water=0x617B64, under="dirt"),
    B("mangrove_swamp", "Mangrove Swamp", 0.8, 0.9, top="mud", filler="mud", under="mud", trees=["MANGROVE"], tree_density=0.03, feats=["LILY", "SEAGRASS", "FIREFLY", "MUD"], foliage_col=0x8DB127, water=0x3A7A6A, grass=0.04),
    B("forest", "Forest", 0.7, 0.8, grass=0.10, trees=["OAK", "OAK", "BIRCH", "FANCY_OAK", "FALLEN_OAK"], tree_density=0.035, flowers=["lilac", "rose_bush", "peony", "lily_of_the_valley"], flower_density=0.006, feats=["LEAF_LITTER", "MUSHROOMS", "BUSHES"]),
    B("flower_forest", "Flower Forest", 0.7, 0.8, grass=0.06, trees=["OAK", "BIRCH", "FANCY_OAK"], tree_density=0.02, flowers=["dandelion", "poppy", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley", "lilac", "rose_bush", "peony"], flower_density=0.12),
    B("birch_forest", "Birch Forest", 0.6, 0.6, grass=0.10, trees=["BIRCH", "BIRCH", "FALLEN_BIRCH"], tree_density=0.035, flowers=["lilac", "rose_bush", "peony"], flower_density=0.006, feats=["WILDFLOWERS", "LEAF_LITTER"]),
    B("dark_forest", "Dark Forest", 0.7, 0.8, grass=0.05, trees=["DARK_OAK", "DARK_OAK", "DARK_OAK", "HUGE_RED_MUSHROOM", "HUGE_BROWN_MUSHROOM", "BIRCH", "OAK"], tree_density=0.10, flowers=["rose_bush", "lilac"], flower_density=0.004, feats=["MUSHROOMS", "LEAF_LITTER"]),
    B("pale_garden", "Pale Garden", 0.7, 0.8, grass=0.04, trees=["PALE_OAK", "PALE_OAK"], tree_density=0.09, feats=["EYEBLOSSOM", "PALE_MOSS"], grass_col=0x778272, foliage_col=0x878D76, water=0x76889D, sky=0xB9B9B9, fog=0x817770),
    B("old_growth_birch_forest", "Old Growth Birch Forest", 0.6, 0.6, grass=0.10, trees=["TALL_BIRCH", "TALL_BIRCH", "FALLEN_BIRCH"], tree_density=0.04, flowers=["lilac", "peony"], flower_density=0.005, feats=["WILDFLOWERS", "LEAF_LITTER"]),
    B("old_growth_pine_taiga", "Old Growth Pine Taiga", 0.3, 0.8, grass=0.10, top="grass_block", trees=["MEGA_PINE", "MEGA_PINE", "SPRUCE", "PINE", "FALLEN_SPRUCE"], tree_density=0.05, feats=["PODZOL", "COARSE", "BOULDERS", "BERRIES", "MUSHROOMS"]),
    B("old_growth_spruce_taiga", "Old Growth Spruce Taiga", 0.25, 0.8, grass=0.10, trees=["MEGA_SPRUCE", "MEGA_SPRUCE", "SPRUCE", "FALLEN_SPRUCE"], tree_density=0.05, feats=["PODZOL", "COARSE", "BOULDERS", "BERRIES", "MUSHROOMS"]),
    B("taiga", "Taiga", 0.25, 0.8, grass=0.12, trees=["SPRUCE", "SPRUCE", "PINE", "FALLEN_SPRUCE"], tree_density=0.035, feats=["BERRIES", "MUSHROOMS"]),
    B("snowy_taiga", "Snowy Taiga", -0.5, 0.4, precip="snow", grass=0.03, trees=["SPRUCE", "PINE"], tree_density=0.03, feats=["BERRIES", "FREEZE"]),
    B("savanna", "Savanna", 2.0, 0.0, precip="none", grass=0.45, trees=["ACACIA", "OAK"], tree_density=0.006, flowers=STD_FLOWERS, flower_density=0.003, feats=["BUSHES"]),
    B("savanna_plateau", "Savanna Plateau", 2.0, 0.0, precip="none", grass=0.40, trees=["ACACIA", "OAK"], tree_density=0.008),
    B("windswept_hills", "Windswept Hills", 0.2, 0.3, grass=0.05, trees=["SPRUCE", "OAK"], tree_density=0.002, feats=["EMERALD"]),
    B("windswept_gravelly_hills", "Windswept Gravelly Hills", 0.2, 0.3, top="gravel", filler="gravel", trees=["SPRUCE", "OAK"], tree_density=0.002, feats=["EMERALD"]),
    B("windswept_forest", "Windswept Forest", 0.2, 0.3, grass=0.06, trees=["SPRUCE", "OAK", "SPRUCE"], tree_density=0.025, feats=["EMERALD"]),
    B("windswept_savanna", "Windswept Savanna", 2.0, 0.0, precip="none", grass=0.3, top="coarse_dirt", trees=["ACACIA"], tree_density=0.004),
    B("jungle", "Jungle", 0.95, 0.9, grass=0.25, trees=["JUNGLE", "MEGA_JUNGLE", "JUNGLE_BUSH", "JUNGLE_BUSH", "FANCY_OAK"], tree_density=0.10, flowers=STD_FLOWERS, flower_density=0.004, feats=["MELON", "VINES", "COCOA", "BAMBOO"]),
    B("sparse_jungle", "Sparse Jungle", 0.95, 0.8, grass=0.25, trees=["JUNGLE", "JUNGLE_BUSH", "OAK"], tree_density=0.015, feats=["MELON", "VINES"]),
    B("bamboo_jungle", "Bamboo Jungle", 0.95, 0.9, grass=0.2, top="podzol", trees=["JUNGLE", "JUNGLE_BUSH", "FANCY_OAK"], tree_density=0.03, feats=["BAMBOO", "BAMBOO", "MELON", "VINES"]),
    B("badlands", "Badlands", 2.0, 0.0, precip="none", top="red_sand", filler="terracotta", under="red_sand", feats=["TERRACOTTA", "CACTUS", "DEAD_BUSH", "DRY_GRASS"], grass_col=0x90814D, foliage_col=0x9E814D, sky=0x6EB1FF),
    B("eroded_badlands", "Eroded Badlands", 2.0, 0.0, precip="none", top="red_sand", filler="terracotta", under="red_sand", feats=["TERRACOTTA", "CACTUS", "DEAD_BUSH"], grass_col=0x90814D, foliage_col=0x9E814D),
    B("wooded_badlands", "Wooded Badlands", 2.0, 0.0, precip="none", top="coarse_dirt", filler="terracotta", under="red_sand", trees=["OAK"], tree_density=0.012, feats=["TERRACOTTA", "DEAD_BUSH"], grass_col=0x90814D, foliage_col=0x9E814D),
    B("meadow", "Meadow", 0.5, 0.8, grass=0.45, trees=["OAK", "BIRCH"], tree_density=0.0006, flowers=["dandelion", "poppy", "allium", "azure_bluet", "oxeye_daisy", "cornflower"], flower_density=0.05, feats=["WILDFLOWERS"], water=0x0E4ECF),
    B("cherry_grove", "Cherry Grove", 0.5, 0.8, grass=0.25, trees=["CHERRY"], tree_density=0.015, feats=["PETALS"], grass_col=0xB6DB61, foliage_col=0xB6DB61, water=0x5DB7EF, fog=0xF5C8E8),
    B("grove", "Grove", -0.2, 0.8, precip="snow", top="snow_block", trees=["SPRUCE", "PINE"], tree_density=0.03, feats=["POWDER_SNOW", "FREEZE"]),
    B("snowy_slopes", "Snowy Slopes", -0.3, 0.9, precip="snow", top="snow_block", filler="snow_block", feats=["POWDER_SNOW", "FREEZE", "PEAKS_SNOW"]),
    B("frozen_peaks", "Frozen Peaks", -0.7, 0.9, precip="snow", top="snow_block", filler="packed_ice", feats=["PEAKS_SNOW", "FREEZE"]),
    B("jagged_peaks", "Jagged Peaks", -0.7, 0.9, precip="snow", top="snow_block", filler="stone", feats=["PEAKS_SNOW", "FREEZE", "EMERALD"]),
    B("stony_peaks", "Stony Peaks", 1.0, 0.3, top="stone", filler="stone", feats=["CALCITE", "EMERALD"]),
    B("mushroom_fields", "Mushroom Fields", 0.9, 1.0, top="mycelium", trees=["HUGE_RED_MUSHROOM", "HUGE_BROWN_MUSHROOM"], tree_density=0.004, feats=["MUSHROOMS"], cat="mushroom"),
    # ---------------------------------------------------------------- water & shores
    B("river", "River", 0.5, 0.5, top="sand", filler="sand", under="sand", feats=["SEAGRASS", "SUGAR_CANE"], cat="river"),
    B("frozen_river", "Frozen River", 0.0, 0.5, precip="snow", top="sand", filler="sand", under="sand", feats=["FREEZE"], water=0x3938C9, cat="river"),
    B("beach", "Beach", 0.8, 0.4, top="sand", filler="sand", under="sand", feats=["SUGAR_CANE"], cat="beach"),
    B("snowy_beach", "Snowy Beach", 0.05, 0.3, precip="snow", top="sand", filler="sand", under="sand", feats=["FREEZE"], water=0x3D57D6, cat="beach"),
    B("stony_shore", "Stony Shore", 0.2, 0.3, top="stone", filler="stone", under="gravel", cat="beach"),
    B("warm_ocean", "Warm Ocean", 0.5, 0.5, top="sand", filler="sand", under="sand", feats=["CORAL", "SEA_PICKLE", "SEAGRASS"], water=0x43D5EE, cat="ocean"),
    B("lukewarm_ocean", "Lukewarm Ocean", 0.5, 0.5, top="sand", filler="sand", under="sand", feats=["SEAGRASS", "KELP"], water=0x45ADF2, cat="ocean"),
    B("deep_lukewarm_ocean", "Deep Lukewarm Ocean", 0.5, 0.5, top="sand", filler="sand", under="sand", feats=["SEAGRASS", "KELP"], water=0x45ADF2, cat="ocean"),
    B("ocean", "Ocean", 0.5, 0.5, top="gravel", filler="gravel", under="gravel", feats=["SEAGRASS", "KELP"], cat="ocean"),
    B("deep_ocean", "Deep Ocean", 0.5, 0.5, top="gravel", filler="gravel", under="gravel", feats=["SEAGRASS", "KELP"], cat="ocean"),
    B("cold_ocean", "Cold Ocean", 0.5, 0.5, top="gravel", filler="gravel", under="gravel", feats=["SEAGRASS", "KELP"], water=0x3D57D6, cat="ocean"),
    B("deep_cold_ocean", "Deep Cold Ocean", 0.5, 0.5, top="gravel", filler="gravel", under="gravel", feats=["SEAGRASS", "KELP"], water=0x3D57D6, cat="ocean"),
    B("frozen_ocean", "Frozen Ocean", 0.0, 0.5, precip="snow", top="gravel", filler="gravel", under="gravel", feats=["ICEBERG", "FREEZE"], water=0x3938C9, cat="ocean"),
    B("deep_frozen_ocean", "Deep Frozen Ocean", 0.5, 0.5, top="gravel", filler="gravel", under="gravel", feats=["ICEBERG", "FREEZE"], water=0x3938C9, cat="ocean"),
    # ---------------------------------------------------------------- caves
    B("dripstone_caves", "Dripstone Caves", 0.8, 0.4, feats=["DRIPSTONE"], cat="cave"),
    B("lush_caves", "Lush Caves", 0.5, 0.5, feats=["LUSH"], cat="cave"),
    B("deep_dark", "Deep Dark", 0.8, 0.4, feats=["SCULK"], cat="cave"),
    # ---------------------------------------------------------------- nether
    B("nether_wastes", "Hell Wastes", 2.0, 0.0, precip="none", top="netherrack", filler="netherrack", dim="NETHER", fog=0x4A1208, sky=0x330808, feats=["MUSHROOMS"]),
    B("soul_sand_valley", "Soul Sand Valley", 2.0, 0.0, precip="none", top="soul_sand", filler="soul_soil", dim="NETHER", fog=0x1E5250, sky=0x1B4745),
    B("crimson_forest", "Crimson Forest", 2.0, 0.0, precip="none", top="crimson_nylium", filler="netherrack", trees=["CRIMSON_FUNGUS"], tree_density=0.03, dim="NETHER", fog=0x4A0A08, sky=0x330303),
    B("warped_forest", "Warped Forest", 2.0, 0.0, precip="none", top="warped_nylium", filler="netherrack", trees=["WARPED_FUNGUS"], tree_density=0.03, dim="NETHER", fog=0x2A0F38, sky=0x1A051A),
    B("basalt_deltas", "Basalt Deltas", 2.0, 0.0, precip="none", top="basalt", filler="blackstone", dim="NETHER", fog=0x5A5060, sky=0x685F70),
    # ---------------------------------------------------------------- end
    B("the_end", "Central Asteroid", 0.5, 0.5, precip="none", top="end_stone", filler="end_stone", dim="END", fog=0x0A080C, sky=0x000000),
    B("end_highlands", "Asteroid Highlands", 0.5, 0.5, precip="none", top="end_stone", filler="end_stone", trees=["CHORUS"], tree_density=0.006, dim="END", fog=0x0A080C, sky=0x000000),
    B("end_midlands", "Asteroid Midlands", 0.5, 0.5, precip="none", top="end_stone", filler="end_stone", dim="END", fog=0x0A080C, sky=0x000000),
    B("small_end_islands", "Asteroid Field", 0.5, 0.5, precip="none", top="end_stone", filler="end_stone", dim="END", fog=0x0A080C, sky=0x000000),
    B("end_barrens", "Asteroid Barrens", 0.5, 0.5, precip="none", top="end_stone", filler="end_stone", dim="END", fog=0x0A080C, sky=0x000000),
    B("the_void", "Deep Space", 0.5, 0.5, precip="none", top="stone", filler="stone", dim="END", fog=0x000000, sky=0x000000),
    # ---------------------------------------------------------------- sift (appended: biome ids are saved per column)
    B("golden_meadows", "Golden Meadows", 0.8, 0.5, precip="none", top="golden_grass_block", filler="holy_stone", under="holy_stone", trees=["CRYSTAL"], tree_density=0.004,
      grass=0.10, flowers=["oxeye_daisy", "lily_of_the_valley", "dandelion"], flower_density=0.03, dim="SIFT", grass_col=0xF0D060, foliage_col=0xC8F0FF, sky=0x78B4FF, fog=0xD6E6FF),
    B("crystal_grove", "Crystal Grove", 0.7, 0.6, precip="none", top="golden_grass_block", filler="holy_stone", under="holy_stone", trees=["CRYSTAL", "CRYSTAL"], tree_density=0.03,
      grass=0.05, dim="SIFT", grass_col=0xE8D070, foliage_col=0xB0E8FF, sky=0xB8E0FF, fog=0xE8F4FF),
    B("cloud_sea", "Cloud Sea", 0.6, 0.6, precip="none", top="cloud_block", filler="cloud_block", under="cloud_block", dim="SIFT", sky=0x80BCFF, fog=0xE0ECFF),
]

def clamp01(v): return max(0.0, min(1.0, v))
def colormap(temp, down, hot_dry, hot_wet, cold):
    t = clamp01(temp); d = clamp01(down) * t
    def ch(c, s): return (c >> s) & 255
    out = 0
    for s in (16, 8, 0):
        v = ch(cold, s) * (1 - t) + (ch(hot_dry, s) * (1 - d / max(t, 1e-6)) + ch(hot_wet, s) * (d / max(t, 1e-6))) * t
        out |= int(max(0, min(255, v))) << s
    return out

def sky_color(temp):
    t = max(-1, min(1, temp / 3.0))
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(0.6222 - t * 0.05, 0.5 + t * 0.1, 1.0)
    return (int(r * 255) << 16) | (int(g * 255) << 8) | int(b * 255)

def cname(k): return k.upper()

def header():
    return "typedef enum {\n" + "\n".join(f"    BIOME_{cname(b['key'])}," for b in BIOMES) + "\n    BIOME_COUNT\n} BiomeId;\n" + \
           "typedef enum {\n" + "\n".join(f"    TREE_{t}," for t in TREES) + "\n    TREE_COUNT\n} TreeKind;\n" + \
           "enum {\n" + "\n".join(f"    FEAT_{f} = {i}," for i, f in enumerate(FEATS)) + "\n};\n" + \
           "typedef enum { DIM_OVERWORLD, DIM_NETHER, DIM_END, DIM_SIFT, DIM_COUNT } Dimension;   // NETHER = Hell, END = Space (indices are saved)\n"

def emit(canon, item_id):
    rows = []
    for b in BIOMES:
        g = b["grass_col"] if b["grass_col"] is not None else colormap(b["temp"], b["down"], 0xBFB755, 0x47CD33, 0x80B497)
        f = b["foliage_col"] if b["foliage_col"] is not None else colormap(b["temp"], b["down"], 0xAEA42A, 0x1ABF00, 0x60A17B)
        sky = b["sky"] if b["sky"] is not None else sky_color(b["temp"])
        feats = 0
        for x in b["feats"]: feats |= 1 << FEATS.index(x)
        trees = (b["trees"] + ["NONE"] * 8)[:8]
        flowers = [canon[x] for x in b["flowers"]][:16] + [0] * (16 - len(b["flowers"][:16]))
        rows.append('    {"%s", "%s", DIM_%s, %.2ff, %.2ff, %d, 0x%06X, 0x%06X, 0x%06X, 0x%06X, 0x%06X, %d, %d, %d, {%s}, %.4ff, %.3ff, %.4ff, %d, {%s}, %dULL, %d},' % (
            b["name"], b["key"], b["dim"], b["temp"], b["down"], {"none": 0, "rain": 1, "snow": 2}[b["precip"]], g, f, b["water"], sky, b["fog"],
            canon[b["top"]], canon[b["filler"]], canon[b["under"]], ", ".join("TREE_" + t for t in trees), b["tree_density"], b["grass"],
            b["flower_density"], len(b["flowers"][:16]), ", ".join(map(str, flowers)), feats, {"land": 0, "ocean": 1, "river": 2, "beach": 3, "cave": 4, "mushroom": 5}[b["cat"]]))
    return "const BiomeDef BIOMES[BIOME_COUNT] = {\n" + "\n".join(rows) + "\n};\n"
