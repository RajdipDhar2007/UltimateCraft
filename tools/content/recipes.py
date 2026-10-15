"""Vanilla recipe database, generated mostly from material families.

Ingredient notation: "item_id", "#tag", or a list of alternatives.
"""
from colors import *
import blocks as B

IDS = B.IDSET
def have(*ks): return all((k[1:] in TAGS) if k.startswith("#") else (k in IDS) for k in ks)

WOODS_ALL = [w for w in WOODS if w != "bamboo"]
STEM = {"crimson", "warped"}
def logs_of(w):
    if w == "bamboo": return ["bamboo_block", "stripped_bamboo_block"]
    a, b = ("stem", "hyphae") if w in STEM else ("log", "wood")
    return [f"{w}_{a}", f"{w}_{b}", f"stripped_{w}_{a}", f"stripped_{w}_{b}"]

TAGS = {
    "planks": [f"{w}_planks" for w in WOODS],
    "logs": sum((logs_of(w) for w in WOODS_ALL), []),
    "logs_that_burn": sum((logs_of(w) for w in WOODS_ALL if w not in STEM), []),
    "wool": [f"{d}_wool" for d in DYES],
    "wool_carpets": [f"{d}_carpet" for d in DYES],
    "wooden_slabs": [f"{w}_slab" for w in WOODS],
    "stone_tool_materials": ["cobblestone", "blackstone", "cobbled_deepslate"],
    "stone_crafting_materials": ["cobblestone", "blackstone", "cobbled_deepslate"],
    "coals": ["coal", "charcoal"],
    "soul_fire_base_blocks": ["soul_sand", "soul_soil"],
    "sand": ["sand", "red_sand"],
    "candles": ["candle"] + [f"{d}_candle" for d in DYES],
    "beds": [f"{d}_bed" for d in DYES],
    "shulker_boxes": ["shulker_box"] + [f"{d}_shulker_box" for d in DYES],
    "bundles": ["bundle"] + [f"{d}_bundle" for d in DYES],
    "terracotta": ["terracotta"] + [f"{d}_terracotta" for d in DYES],
    "stained_glass": ["glass"] + [f"{d}_stained_glass" for d in DYES],
    "glass_panes": ["glass_pane"] + [f"{d}_stained_glass_pane" for d in DYES],
    "harnesses": [f"{d}_harness" for d in DYES],
    "banners": [f"{d}_banner" for d in DYES],
    "eggs": ["egg", "blue_egg", "brown_egg"],
    "smelts_to_glass": ["sand", "red_sand"],
    "copper_blocks": ["copper_block", "waxed_copper_block"],
    "chests": ["chest", "trapped_chest"],
    "small_flowers": ["dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
                      "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose", "torchflower", "open_eyeblossom", "closed_eyeblossom"],
    "leaves": [f"{w}_leaves" for w in WOODS if w not in ("bamboo", "crimson", "warped")] + ["azalea_leaves", "flowering_azalea_leaves"],
    "saplings": [f"{w}_sapling" for w in WOODS if w not in ("bamboo", "crimson", "warped", "mangrove")] + ["mangrove_propagule"],
}
for w in WOODS:
    TAGS[f"{w}_logs"] = logs_of(w)
TAGS = {k: [i for i in v if i in IDS] for k, v in TAGS.items()}

CRAFT = []   # (result, count, shaped: rows or None, key dict / ingredient list)
def shaped(res, n, rows, key):
    if res not in IDS: return
    for v in key.values():
        for x in (v if isinstance(v, list) else [v]):
            if not have(x): return
    CRAFT.append((res, n, rows, key))
def shapeless(res, n, *ings):
    if res not in IDS: return
    for v in ings:
        for x in (v if isinstance(v, list) else [v]):
            if not have(x): return
    CRAFT.append((res, n, None, list(ings)))

SMELT = []   # (input, result, xp, kinds)  kinds: f=furnace b=blast s=smoker c=campfire
def smelt(inp, res, xp=0.1, kinds="f"):
    if (inp.startswith("#") or inp in IDS) and res in IDS: SMELT.append((inp, res, xp, kinds))
CUT = []     # (input, result, count)
def cut(inp, res, n=1):
    if inp in IDS and res in IDS and (inp, res) not in [(a, b) for a, b, _ in CUT]: CUT.append((inp, res, n))
SMITH = []   # (template, base, addition, result)

def build():
    CRAFT.clear(); SMELT.clear(); CUT.clear(); SMITH.clear()
    wood_family(); stone_families(); copper_family(); metals(); colors(); tools_armor(); misc(); smelting(); smithing(); ultimate()
    return CRAFT, SMELT, CUT, SMITH, TAGS

# ------------------------------------------------------------------ wood
def wood_family():
    for w in WOODS:
        P = f"{w}_planks"
        if w == "bamboo":
            shapeless(P, 2, "#bamboo_logs")
            shaped("bamboo_block", 1, ["###", "###", "###"], {"#": "bamboo"})
            shaped("bamboo_mosaic", 1, ["#", "#"], {"#": "bamboo_slab"})
            shaped("bamboo_raft", 1, ["# #", "###"], {"#": P})
            shapeless("bamboo_chest_raft", 1, "bamboo_raft", "chest")
            shaped("stick", 1, ["#", "#"], {"#": "bamboo"})
        else:
            shapeless(P, 4, f"#{w}_logs")
            a, b = ("stem", "hyphae") if w in STEM else ("log", "wood")
            shaped(f"{w}_{b}", 3, ["##", "##"], {"#": f"{w}_{a}"})
            shaped(f"stripped_{w}_{b}", 3, ["##", "##"], {"#": f"stripped_{w}_{a}"})
            if w not in STEM:
                shaped(f"{w}_boat", 1, ["# #", "###"], {"#": P})
                shapeless(f"{w}_chest_boat", 1, f"{w}_boat", "chest")
        shaped(f"{w}_slab", 6, ["###"], {"#": P})
        shaped(f"{w}_stairs", 4, ["#  ", "## ", "###"], {"#": P})
        shaped(f"{w}_fence", 3, ["W#W", "W#W"], {"W": P, "#": "stick"})
        shaped(f"{w}_fence_gate", 1, ["#W#", "#W#"], {"W": P, "#": "stick"})
        shaped(f"{w}_door", 3, ["##", "##", "##"], {"#": P})
        shaped(f"{w}_trapdoor", 2, ["###", "###"], {"#": P})
        shapeless(f"{w}_button", 1, P)
        shaped(f"{w}_pressure_plate", 1, ["##"], {"#": P})
        shaped(f"{w}_sign", 3, ["###", "###", " X "], {"#": P, "X": "stick"})
        stripped = "stripped_bamboo_block" if w == "bamboo" else (f"stripped_{w}_stem" if w in STEM else f"stripped_{w}_log")
        shaped(f"{w}_hanging_sign", 6, ["X X", "###", "###"], {"#": stripped, "X": "iron_chain"})
        shaped(f"{w}_shelf", 6, ["###", "   ", "###"], {"#": stripped})
    shaped("stick", 4, ["#", "#"], {"#": "#planks"})
    shaped("crafting_table", 1, ["##", "##"], {"#": "#planks"})
    shaped("chest", 1, ["###", "# #", "###"], {"#": "#planks"})
    shaped("barrel", 1, ["PSP", "P P", "PSP"], {"P": "#planks", "S": "#wooden_slabs"})
    shaped("ladder", 3, ["S S", "SSS", "S S"], {"S": "stick"})
    shaped("bowl", 4, ["# #", " # "], {"#": "#planks"})
    shaped("bookshelf", 1, ["PPP", "BBB", "PPP"], {"P": "#planks", "B": "book"})
    shaped("chiseled_bookshelf", 1, ["PPP", "SSS", "PPP"], {"P": "#planks", "S": "#wooden_slabs"})
    shaped("jukebox", 1, ["###", "#D#", "###"], {"#": "#planks", "D": "diamond"})
    shaped("note_block", 1, ["###", "#R#", "###"], {"#": "#planks", "R": "redstone"})
    shaped("composter", 1, ["S S", "S S", "SSS"], {"S": "#wooden_slabs"})
    shaped("lectern", 1, ["SSS", " B ", " S "], {"S": "#wooden_slabs", "B": "bookshelf"})
    shaped("loom", 1, ["SS", "PP"], {"S": "string", "P": "#planks"})
    shaped("cartography_table", 1, ["pp", "PP", "PP"], {"p": "paper", "P": "#planks"})
    shaped("fletching_table", 1, ["ff", "PP", "PP"], {"f": "flint", "P": "#planks"})
    shaped("smithing_table", 1, ["II", "PP", "PP"], {"I": "iron_ingot", "P": "#planks"})
    shaped("grindstone", 1, ["S-S", "P P"], {"S": "stick", "-": "stone_slab", "P": "#planks"})
    shaped("beehive", 1, ["PPP", "HHH", "PPP"], {"P": "#planks", "H": "honeycomb"})
    shaped("daylight_detector", 1, ["GGG", "QQQ", "SSS"], {"G": "glass", "Q": "quartz", "S": "#wooden_slabs"})
    shaped("piston", 1, ["PPP", "CIC", "CRC"], {"P": "#planks", "C": "#stone_crafting_materials", "I": "iron_ingot", "R": "redstone"})
    shaped("tripwire_hook", 2, ["I", "S", "P"], {"I": "iron_ingot", "S": "stick", "P": "#planks"})
    shaped("campfire", 1, [" S ", "SCS", "LLL"], {"S": "stick", "C": "#coals", "L": "#logs"})
    shaped("soul_campfire", 1, [" S ", "SCS", "LLL"], {"S": "stick", "C": "#soul_fire_base_blocks", "L": "#logs"})
    shaped("item_frame", 1, ["SSS", "SLS", "SSS"], {"S": "stick", "L": "leather"})
    shaped("painting", 1, ["SSS", "SWS", "SSS"], {"S": "stick", "W": "#wool"})
    shaped("armor_stand", 1, ["SSS", " S ", "S-S"], {"S": "stick", "-": "smooth_stone_slab"})
    shaped("bow", 1, [" #S", "# S", " #S"], {"#": "stick", "S": "string"})
    shaped("arrow", 4, ["F", "S", "E"], {"F": "flint", "S": "stick", "E": "feather"})
    shaped("fishing_rod", 1, ["  #", " #S", "# S"], {"#": "stick", "S": "string"})
    shaped("carrot_on_a_stick", 1, ["# ", " C"], {"#": "fishing_rod", "C": "carrot"})
    shaped("warped_fungus_on_a_stick", 1, ["# ", " F"], {"#": "fishing_rod", "F": "warped_fungus"})
    shaped("shield", 1, ["WIW", "WWW", " W "], {"W": "#planks", "I": "iron_ingot"})
    shaped("scaffolding", 6, ["I~I", "I I", "I I"], {"I": "bamboo", "~": "string"})
    shaped("torch", 4, ["C", "S"], {"C": "#coals", "S": "stick"})
    shaped("soul_torch", 4, ["C", "S", "B"], {"C": "#coals", "S": "stick", "B": "#soul_fire_base_blocks"})
    shaped("copper_torch", 4, ["N", "C", "S"], {"N": "copper_nugget", "C": "#coals", "S": "stick"})
    shaped("redstone_torch", 1, ["R", "S"], {"R": "redstone", "S": "stick"})
    shaped("lever", 1, ["S", "C"], {"S": "stick", "C": "cobblestone"})
    shaped("crossbow", 1, ["#I#", "~T~", " # "], {"#": "stick", "I": "iron_ingot", "~": "string", "T": "tripwire_hook"})

# ------------------------------------------------------------------ stone families
FAMILIES = {  # base: (stairs, slab, wall)
    "stone": ("stone_stairs", "stone_slab", None), "cobblestone": ("cobblestone_stairs", "cobblestone_slab", "cobblestone_wall"),
    "mossy_cobblestone": ("mossy_cobblestone_stairs", "mossy_cobblestone_slab", "mossy_cobblestone_wall"),
    "smooth_stone": (None, "smooth_stone_slab", None),
    "stone_bricks": ("stone_brick_stairs", "stone_brick_slab", "stone_brick_wall"),
    "mossy_stone_bricks": ("mossy_stone_brick_stairs", "mossy_stone_brick_slab", "mossy_stone_brick_wall"),
    "granite": ("granite_stairs", "granite_slab", "granite_wall"), "polished_granite": ("polished_granite_stairs", "polished_granite_slab", None),
    "diorite": ("diorite_stairs", "diorite_slab", "diorite_wall"), "polished_diorite": ("polished_diorite_stairs", "polished_diorite_slab", None),
    "andesite": ("andesite_stairs", "andesite_slab", "andesite_wall"), "polished_andesite": ("polished_andesite_stairs", "polished_andesite_slab", None),
    "cobbled_deepslate": ("cobbled_deepslate_stairs", "cobbled_deepslate_slab", "cobbled_deepslate_wall"),
    "polished_deepslate": ("polished_deepslate_stairs", "polished_deepslate_slab", "polished_deepslate_wall"),
    "deepslate_bricks": ("deepslate_brick_stairs", "deepslate_brick_slab", "deepslate_brick_wall"),
    "deepslate_tiles": ("deepslate_tile_stairs", "deepslate_tile_slab", "deepslate_tile_wall"),
    "tuff": ("tuff_stairs", "tuff_slab", "tuff_wall"), "polished_tuff": ("polished_tuff_stairs", "polished_tuff_slab", "polished_tuff_wall"),
    "tuff_bricks": ("tuff_brick_stairs", "tuff_brick_slab", "tuff_brick_wall"),
    "bricks": ("brick_stairs", "brick_slab", "brick_wall"), "mud_bricks": ("mud_brick_stairs", "mud_brick_slab", "mud_brick_wall"),
    "sandstone": ("sandstone_stairs", "sandstone_slab", "sandstone_wall"), "smooth_sandstone": ("smooth_sandstone_stairs", "smooth_sandstone_slab", None),
    "cut_sandstone": (None, "cut_sandstone_slab", None),
    "red_sandstone": ("red_sandstone_stairs", "red_sandstone_slab", "red_sandstone_wall"),
    "smooth_red_sandstone": ("smooth_red_sandstone_stairs", "smooth_red_sandstone_slab", None), "cut_red_sandstone": (None, "cut_red_sandstone_slab", None),
    "prismarine": ("prismarine_stairs", "prismarine_slab", "prismarine_wall"), "prismarine_bricks": ("prismarine_brick_stairs", "prismarine_brick_slab", None),
    "dark_prismarine": ("dark_prismarine_stairs", "dark_prismarine_slab", None),
    "nether_bricks": ("nether_brick_stairs", "nether_brick_slab", "nether_brick_wall"),
    "red_nether_bricks": ("red_nether_brick_stairs", "red_nether_brick_slab", "red_nether_brick_wall"),
    "quartz_block": ("quartz_stairs", "quartz_slab", None), "smooth_quartz": ("smooth_quartz_stairs", "smooth_quartz_slab", None),
    "purpur_block": ("purpur_stairs", "purpur_slab", None), "end_stone_bricks": ("end_stone_brick_stairs", "end_stone_brick_slab", "end_stone_brick_wall"),
    "blackstone": ("blackstone_stairs", "blackstone_slab", "blackstone_wall"),
    "polished_blackstone": ("polished_blackstone_stairs", "polished_blackstone_slab", "polished_blackstone_wall"),
    "polished_blackstone_bricks": ("polished_blackstone_brick_stairs", "polished_blackstone_brick_slab", "polished_blackstone_brick_wall"),
    "resin_bricks": ("resin_brick_stairs", "resin_brick_slab", "resin_brick_wall"),
}
# stonecutter parent graph: child <- parent (each child also inherits the parent's own parents)
CUT_PARENT = {
    "stone_bricks": "stone", "chiseled_stone_bricks": "stone_bricks", "polished_granite": "granite", "polished_diorite": "diorite",
    "polished_andesite": "andesite", "polished_deepslate": "cobbled_deepslate", "deepslate_bricks": "polished_deepslate",
    "deepslate_tiles": "deepslate_bricks", "chiseled_deepslate": "cobbled_deepslate", "polished_tuff": "tuff", "tuff_bricks": "polished_tuff",
    "chiseled_tuff": "tuff", "chiseled_tuff_bricks": "tuff_bricks", "cut_sandstone": "sandstone", "chiseled_sandstone": "sandstone",
    "cut_red_sandstone": "red_sandstone", "chiseled_red_sandstone": "red_sandstone", "chiseled_quartz_block": "quartz_block",
    "quartz_pillar": "quartz_block", "quartz_bricks": "quartz_block", "purpur_pillar": "purpur_block", "end_stone_bricks": "end_stone",
    "polished_blackstone": "blackstone", "polished_blackstone_bricks": "polished_blackstone", "chiseled_polished_blackstone": "polished_blackstone",
    "chiseled_nether_bricks": "nether_bricks", "polished_basalt": "basalt", "chiseled_resin_bricks": "resin_bricks", "smooth_stone": None,
}

def stone_families():
    for base, (stairs, slab, wall) in FAMILIES.items():
        if stairs: shaped(stairs, 4, ["#  ", "## ", "###"], {"#": base})
        if slab: shaped(slab, 6, ["###"], {"#": base})
        if wall: shaped(wall, 6, ["###", "###"], {"#": base})
    # stonecutter: a block -> its family members and every descendant's family members
    def chain(b):
        out = [b]
        while CUT_PARENT.get(b): b = CUT_PARENT[b]; out.append(b)
        return out
    all_children = set(FAMILIES) | set(CUT_PARENT)
    for child in sorted(all_children):
        anc = chain(child)
        for a in anc:
            if a != child: cut(a, child)
            fam = FAMILIES.get(child)
            if fam:
                st_, sl, wa = fam
                if st_: cut(a, st_)
                if sl: cut(a, sl, 2)
                if wa: cut(a, wa)
    cut("stone", "stone_button"); cut("stone", "stone_pressure_plate")
    # 2x2 polishing / bricks
    for res, src in (("stone_bricks", "stone"), ("polished_granite", "granite"), ("polished_diorite", "diorite"), ("polished_andesite", "andesite"),
                     ("polished_deepslate", "cobbled_deepslate"), ("deepslate_bricks", "polished_deepslate"), ("deepslate_tiles", "deepslate_bricks"),
                     ("polished_tuff", "tuff"), ("tuff_bricks", "polished_tuff"), ("mud_bricks", "packed_mud"), ("cut_sandstone", "sandstone"),
                     ("cut_red_sandstone", "red_sandstone"), ("quartz_bricks", "quartz_block"), ("end_stone_bricks", "end_stone"),
                     ("polished_blackstone", "blackstone"), ("polished_blackstone_bricks", "polished_blackstone"), ("polished_basalt", "basalt")):
        shaped(res, 4, ["##", "##"], {"#": src})
    for res, src in (("chiseled_stone_bricks", "stone_brick_slab"), ("chiseled_deepslate", "cobbled_deepslate_slab"), ("chiseled_tuff", "tuff_slab"),
                     ("chiseled_tuff_bricks", "tuff_brick_slab"), ("chiseled_sandstone", "sandstone_slab"), ("chiseled_red_sandstone", "red_sandstone_slab"),
                     ("chiseled_quartz_block", "quartz_slab"), ("purpur_pillar", "purpur_slab"), ("chiseled_nether_bricks", "nether_brick_slab"),
                     ("chiseled_polished_blackstone", "polished_blackstone_slab"), ("chiseled_resin_bricks", "resin_brick_slab"), ("chiseled_bookshelf", None)):
        if src: shaped(res, 1, ["#", "#"], {"#": src})
    shaped("quartz_pillar", 2, ["#", "#"], {"#": "quartz_block"})
    shaped("bricks", 1, ["##", "##"], {"#": "brick"})
    shaped("nether_bricks", 1, ["##", "##"], {"#": "nether_brick"})
    shaped("red_nether_bricks", 1, ["NW", "WN"], {"N": "nether_brick", "W": "nether_wart"})
    shaped("nether_brick_fence", 6, ["#-#", "#-#"], {"#": "nether_bricks", "-": "nether_brick"})
    shaped("sandstone", 1, ["##", "##"], {"#": "sand"})
    shaped("red_sandstone", 1, ["##", "##"], {"#": "red_sand"})
    shaped("quartz_block", 1, ["##", "##"], {"#": "quartz"})
    shaped("purpur_block", 4, ["##", "##"], {"#": "popped_chorus_fruit"})
    shaped("prismarine", 1, ["##", "##"], {"#": "prismarine_shard"})
    shaped("prismarine_bricks", 1, ["###", "###", "###"], {"#": "prismarine_shard"})
    shaped("dark_prismarine", 1, ["###", "#I#", "###"], {"#": "prismarine_shard", "I": "ink_sac"})
    shaped("sea_lantern", 1, ["SCS", "CCC", "SCS"], {"S": "prismarine_shard", "C": "prismarine_crystals"})
    shaped("diorite", 2, ["CQ", "QC"], {"C": "cobblestone", "Q": "quartz"})
    shapeless("granite", 1, "diorite", "quartz")
    shapeless("andesite", 2, "diorite", "cobblestone")
    shapeless("mossy_cobblestone", 1, "cobblestone", "vine"); shapeless("mossy_cobblestone", 1, "cobblestone", "moss_block")
    shapeless("mossy_stone_bricks", 1, "stone_bricks", "vine"); shapeless("mossy_stone_bricks", 1, "stone_bricks", "moss_block")
    shapeless("packed_mud", 1, "mud", "wheat")
    shapeless("muddy_mangrove_roots", 1, "mud", "mangrove_roots")
    shaped("coarse_dirt", 4, ["DG", "GD"], {"D": "dirt", "G": "gravel"})
    shaped("dripstone_block", 1, ["##", "##"], {"#": "pointed_dripstone"})
    shaped("moss_carpet", 3, ["##"], {"#": "moss_block"})
    shaped("pale_moss_carpet", 3, ["##"], {"#": "pale_moss_block"})
    shaped("resin_block", 1, ["###", "###", "###"], {"#": "resin_clump"}); shapeless("resin_clump", 9, "resin_block")
    shaped("resin_bricks", 1, ["##", "##"], {"#": "resin_brick"})
    shaped("furnace", 1, ["###", "# #", "###"], {"#": "#stone_crafting_materials"})
    shaped("stonecutter", 1, [" I ", "###"], {"I": "iron_ingot", "#": "stone"})
    shaped("blast_furnace", 1, ["III", "IFI", "SSS"], {"I": "iron_ingot", "F": "furnace", "S": "smooth_stone"})
    shaped("smoker", 1, [" L ", "LFL", " L "], {"L": "#logs", "F": "furnace"})
    shaped("dispenser", 1, ["###", "#B#", "#R#"], {"#": "cobblestone", "B": "bow", "R": "redstone"})
    shaped("dropper", 1, ["###", "# #", "#R#"], {"#": "cobblestone", "R": "redstone"})
    shaped("observer", 1, ["###", "RRQ", "###"], {"#": "cobblestone", "R": "redstone", "Q": "quartz"})
    shaped("stone_button", 1, ["#"], {"#": "stone"})
    shaped("stone_pressure_plate", 1, ["##"], {"#": "stone"})
    shaped("polished_blackstone_button", 1, ["#"], {"#": "polished_blackstone"})
    shaped("polished_blackstone_pressure_plate", 1, ["##"], {"#": "polished_blackstone"})
    shaped("repeater", 1, ["TRT", "SSS"], {"T": "redstone_torch", "R": "redstone", "S": "stone"})
    shaped("comparator", 1, [" T ", "TQT", "SSS"], {"T": "redstone_torch", "Q": "quartz", "S": "stone"})
    shaped("brewing_stand", 1, [" B ", "###"], {"B": "blaze_rod", "#": "#stone_crafting_materials"})
    shaped("lodestone", 1, ["SSS", "SNS", "SSS"], {"S": "chiseled_stone_bricks", "N": "netherite_ingot"})
    shaped("flower_pot", 1, ["# #", " # "], {"#": "brick"})
    shaped("decorated_pot", 1, [" # ", "# #", " # "], {"#": "brick"})

# ------------------------------------------------------------------ copper
def copper_family():
    for stg in COPPER_STAGES:
        blk = "copper_block" if stg == "" else f"{stg}copper"
        for wx in ("", "waxed_"):
            b = f"{wx}{blk}" if not (wx and stg == "") else "waxed_copper_block"
            cutc = f"{wx}{stg}cut_copper"
            shaped(cutc, 4, ["##", "##"], {"#": b})
            cut(b, cutc, 4); cut(b, f"{wx}{stg}cut_copper_stairs", 4); cut(b, f"{wx}{stg}cut_copper_slab", 8)
            cut(cutc, f"{wx}{stg}cut_copper_stairs"); cut(cutc, f"{wx}{stg}cut_copper_slab", 2); cut(b, f"{wx}{stg}chiseled_copper", 4)
            cut(cutc, f"{wx}{stg}chiseled_copper")
            shaped(f"{wx}{stg}cut_copper_stairs", 4, ["#  ", "## ", "###"], {"#": cutc})
            shaped(f"{wx}{stg}cut_copper_slab", 6, ["###"], {"#": cutc})
            shaped(f"{wx}{stg}chiseled_copper", 1, ["#", "#"], {"#": f"{wx}{stg}cut_copper_slab"})
            shaped(f"{wx}{stg}copper_grate", 4, [" # ", "# #", " # "], {"#": b})
            cut(b, f"{wx}{stg}copper_grate", 4)
            shaped(f"{wx}{stg}copper_bulb", 4, [" C ", "CBC", " R "], {"C": b, "B": "blaze_rod", "R": "redstone"})
        # waxing (honeycomb) for every waxable copper block
        for base in (blk, f"{stg}cut_copper", f"{stg}cut_copper_stairs", f"{stg}cut_copper_slab", f"{stg}chiseled_copper", f"{stg}copper_grate",
                     f"{stg}copper_bulb", f"{stg}copper_door", f"{stg}copper_trapdoor", f"{stg}copper_bars", f"{stg}copper_chain", f"{stg}copper_lantern",
                     f"{stg}lightning_rod", f"{stg}copper_chest", f"{stg}copper_golem_statue"):
            w = "waxed_copper_block" if base == "copper_block" else "waxed_" + base
            shapeless(w, 1, base, "honeycomb")
    shaped("copper_block", 1, ["###", "###", "###"], {"#": "copper_ingot"})
    shapeless("copper_ingot", 9, "copper_block"); shapeless("copper_ingot", 9, "waxed_copper_block")
    shaped("copper_ingot", 1, ["###", "###", "###"], {"#": "copper_nugget"}); shapeless("copper_nugget", 9, "copper_ingot")
    shaped("copper_door", 3, ["##", "##", "##"], {"#": "copper_ingot"})
    shaped("copper_trapdoor", 2, ["##", "##"], {"#": "copper_ingot"})
    shaped("lightning_rod", 1, ["#", "#", "#"], {"#": "copper_ingot"})
    shaped("copper_bars", 16, ["###", "###"], {"#": "copper_ingot"})
    shaped("copper_chain", 1, ["N", "I", "N"], {"N": "copper_nugget", "I": "copper_ingot"})
    shaped("copper_lantern", 1, ["NNN", "NTN", "NNN"], {"N": "copper_nugget", "T": "copper_torch"})
    shaped("copper_chest", 1, ["###", "#C#", "###"], {"#": "copper_ingot", "C": "chest"})
    shaped("spyglass", 1, ["A", "C", "C"], {"A": "amethyst_shard", "C": "copper_ingot"})
    shaped("brush", 1, ["F", "C", "S"], {"F": "feather", "C": "copper_ingot", "S": "stick"})

# ------------------------------------------------------------------ metals & gems
def metals():
    for blk, ing, nug in (("iron_block", "iron_ingot", "iron_nugget"), ("gold_block", "gold_ingot", "gold_nugget"), ("diamond_block", "diamond", None),
                          ("emerald_block", "emerald", None), ("lapis_block", "lapis_lazuli", None), ("coal_block", "coal", None),
                          ("redstone_block", "redstone", None), ("netherite_block", "netherite_ingot", None), ("raw_iron_block", "raw_iron", None),
                          ("raw_copper_block", "raw_copper", None), ("raw_gold_block", "raw_gold", None), ("slime_block", "slime_ball", None),
                          ("bone_block", "bone_meal", None), ("hay_block", "wheat", None), ("dried_kelp_block", "dried_kelp", None),
                          ("nether_wart_block", "nether_wart", None), ("melon", "melon_slice", None)):
        shaped(blk, 1, ["###", "###", "###"], {"#": ing})
        if blk not in ("nether_wart_block", "melon"): shapeless(ing, 9, blk)
        if nug:
            shaped(ing, 1, ["###", "###", "###"], {"#": nug}); shapeless(nug, 9, ing)
    shapeless("netherite_ingot", 1, "netherite_scrap", "netherite_scrap", "netherite_scrap", "netherite_scrap", "gold_ingot", "gold_ingot", "gold_ingot", "gold_ingot")
    shaped("iron_bars", 16, ["###", "###"], {"#": "iron_ingot"})
    shaped("iron_door", 3, ["##", "##", "##"], {"#": "iron_ingot"})
    shaped("iron_trapdoor", 1, ["##", "##"], {"#": "iron_ingot"})
    shaped("iron_chain", 1, ["N", "I", "N"], {"N": "iron_nugget", "I": "iron_ingot"})
    shaped("bucket", 1, ["# #", " # "], {"#": "iron_ingot"})
    shaped("shears", 1, [" #", "# "], {"#": "iron_ingot"})
    shaped("anvil", 1, ["###", " i ", "iii"], {"#": "iron_block", "i": "iron_ingot"})
    shaped("cauldron", 1, ["# #", "# #", "###"], {"#": "iron_ingot"})
    shaped("hopper", 1, ["I I", "ICI", " I "], {"I": "iron_ingot", "C": "chest"})
    shaped("compass", 1, [" # ", "#R#", " # "], {"#": "iron_ingot", "R": "redstone"})
    shaped("clock", 1, [" # ", "#R#", " # "], {"#": "gold_ingot", "R": "redstone"})
    shaped("minecart", 1, ["# #", "###"], {"#": "iron_ingot"})
    for k, v in (("chest_minecart", "chest"), ("furnace_minecart", "furnace"), ("tnt_minecart", "tnt"), ("hopper_minecart", "hopper")):
        shapeless(k, 1, "minecart", v)
    shaped("rail", 16, ["# #", "#S#", "# #"], {"#": "iron_ingot", "S": "stick"})
    shaped("powered_rail", 6, ["G G", "GSG", "GRG"], {"G": "gold_ingot", "S": "stick", "R": "redstone"})
    shaped("detector_rail", 6, ["I I", "IPI", "IRI"], {"I": "iron_ingot", "P": "stone_pressure_plate", "R": "redstone"})
    shaped("activator_rail", 6, ["ISI", "ITI", "ISI"], {"I": "iron_ingot", "S": "stick", "T": "redstone_torch"})
    shaped("heavy_weighted_pressure_plate", 1, ["##"], {"#": "iron_ingot"})
    shaped("light_weighted_pressure_plate", 1, ["##"], {"#": "gold_ingot"})
    shapeless("flint_and_steel", 1, "iron_ingot", "flint")
    shaped("lantern", 1, ["NNN", "NTN", "NNN"], {"N": "iron_nugget", "T": "torch"})
    shaped("soul_lantern", 1, ["NNN", "NTN", "NNN"], {"N": "iron_nugget", "T": "soul_torch"})
    shaped("golden_apple", 1, ["###", "#A#", "###"], {"#": "gold_ingot", "A": "apple"})
    shaped("golden_carrot", 1, ["###", "#C#", "###"], {"#": "gold_nugget", "C": "carrot"})
    shaped("glistering_melon_slice", 1, ["###", "#M#", "###"], {"#": "gold_nugget", "M": "melon_slice"})
    shaped("amethyst_block", 1, ["##", "##"], {"#": "amethyst_shard"})
    shaped("tinted_glass", 2, [" A ", "AGA", " A "], {"A": "amethyst_shard", "G": "glass"})
    shaped("recovery_compass", 1, ["EEE", "ECE", "EEE"], {"E": "echo_shard", "C": "compass"})
    shaped("enchanting_table", 1, [" B ", "D#D", "###"], {"B": "book", "D": "diamond", "#": "obsidian"})
    shaped("ender_chest", 1, ["###", "#E#", "###"], {"#": "obsidian", "E": "ender_eye"})
    shaped("beacon", 1, ["GGG", "GSG", "OOO"], {"G": "glass", "S": "nether_star", "O": "obsidian"})
    shaped("conduit", 1, ["NNN", "NHN", "NNN"], {"N": "nautilus_shell", "H": "heart_of_the_sea"})
    shaped("respawn_anchor", 1, ["CCC", "GGG", "CCC"], {"C": "crying_obsidian", "G": "glowstone"})
    shaped("end_crystal", 1, ["GGG", "GEG", "GTG"], {"G": "glass", "E": "ender_eye", "T": "ghast_tear"})
    shaped("end_rod", 4, ["B", "P"], {"B": "blaze_rod", "P": "popped_chorus_fruit"})
    shaped("wind_charge", 4, ["#"], {"#": "breeze_rod"})
    shaped("mace", 1, ["H", "R"], {"H": "heavy_core", "R": "breeze_rod"})
    shaped("crafter", 1, ["III", "ICI", "RDR"], {"I": "iron_ingot", "C": "crafting_table", "R": "redstone", "D": "dropper"})
    shaped("target", 1, [" R ", "RHR", " R "], {"R": "redstone", "H": "hay_block"})
    shaped("redstone_lamp", 1, [" R ", "RGR", " R "], {"R": "redstone", "G": "glowstone"})
    shapeless("sticky_piston", 1, "slime_ball", "piston")

# ------------------------------------------------------------------ colours
DYE_SOURCES = {"white": ["bone_meal", "lily_of_the_valley"], "orange": ["orange_tulip", "torchflower"], "magenta": ["allium"],
               "light_blue": ["blue_orchid"], "yellow": ["dandelion", "wildflowers"], "lime": [], "pink": ["pink_tulip", "pink_petals", "cactus_flower"],
               "gray": ["closed_eyeblossom"], "light_gray": ["azure_bluet", "oxeye_daisy", "white_tulip"], "cyan": ["pitcher_plant"], "purple": [],
               "blue": ["lapis_lazuli", "cornflower"], "brown": ["cocoa_beans"], "green": [], "red": ["poppy", "red_tulip", "beetroot"],
               "black": ["ink_sac", "wither_rose"]}
DOUBLE_DYE = {"rose_bush": "red", "sunflower": "yellow", "lilac": "magenta", "peony": "pink", "open_eyeblossom": "orange"}
MIXES = [("orange", ["red", "yellow"], 2), ("magenta", ["purple", "pink"], 2), ("magenta", ["blue", "red", "pink"], 3), ("magenta", ["blue", "red", "red", "white"], 4),
         ("light_blue", ["blue", "white"], 2), ("lime", ["green", "white"], 2), ("pink", ["red", "white"], 2), ("gray", ["black", "white"], 2),
         ("light_gray", ["gray", "white"], 2), ("light_gray", ["black", "white", "white"], 3), ("cyan", ["blue", "green"], 2), ("purple", ["blue", "red"], 2)]

def colors():
    for d, srcs in DYE_SOURCES.items():
        for s in srcs: shapeless(f"{d}_dye", 1, s)
    for s, d in DOUBLE_DYE.items(): shapeless(f"{d}_dye", 2, s)
    for d, parts, n in MIXES: shapeless(f"{d}_dye", n, *[f"{p}_dye" for p in parts])
    shaped("white_wool", 1, ["##", "##"], {"#": "string"})
    for d in DYES:
        dye = f"{d}_dye"
        others = lambda tag, me: [x for x in TAGS[tag] if x != me]
        shapeless(f"{d}_wool", 1, dye, others("wool", f"{d}_wool"))
        shaped(f"{d}_carpet", 3, ["##"], {"#": f"{d}_wool"})
        shapeless(f"{d}_carpet", 1, dye, others("wool_carpets", f"{d}_carpet"))
        shaped(f"{d}_stained_glass", 8, ["###", "#D#", "###"], {"#": "glass", "D": dye})
        shaped(f"{d}_stained_glass_pane", 16, ["###", "###"], {"#": f"{d}_stained_glass"})
        shaped(f"{d}_stained_glass_pane", 8, ["###", "#D#", "###"], {"#": "glass_pane", "D": dye})
        shaped(f"{d}_terracotta", 8, ["###", "#D#", "###"], {"#": "terracotta", "D": dye})
        shapeless(f"{d}_concrete_powder", 8, dye, "sand", "sand", "sand", "sand", "gravel", "gravel", "gravel", "gravel")
        shaped(f"{d}_bed", 1, ["WWW", "PPP"], {"W": f"{d}_wool", "P": "#planks"})
        shapeless(f"{d}_bed", 1, dye, others("beds", f"{d}_bed"))
        shaped(f"{d}_banner", 1, ["WWW", "WWW", " S "], {"W": f"{d}_wool", "S": "stick"})
        shapeless(f"{d}_candle", 1, "candle", dye)
        shapeless(f"{d}_shulker_box", 1, dye, others("shulker_boxes", f"{d}_shulker_box"))
        shapeless(f"{d}_bundle", 1, dye, others("bundles", f"{d}_bundle"))
        shaped(f"{d}_harness", 1, ["LLL", "GWG"], {"L": "leather", "G": "glass", "W": f"{d}_wool"})
        smelt(f"{d}_terracotta", f"{d}_glazed_terracotta", 0.1)
    shaped("glass_pane", 16, ["###", "###"], {"#": "glass"})
    shaped("candle", 1, ["S", "H"], {"S": "string", "H": "honeycomb"})
    shaped("shulker_box", 1, ["S", "C", "S"], {"S": "shulker_shell", "C": "chest"})
    shaped("bundle", 1, ["S", "L"], {"S": "string", "L": "leather"})

# ------------------------------------------------------------------ tools & armour
def tools_armor():
    mats = {"wooden": "#planks", "stone": "#stone_tool_materials", "copper": "copper_ingot", "iron": "iron_ingot", "golden": "gold_ingot", "diamond": "diamond"}
    for t, m in mats.items():
        k = {"#": m, "S": "stick"}
        shaped(f"{t}_pickaxe", 1, ["###", " S ", " S "], k)
        shaped(f"{t}_axe", 1, ["##", "#S", " S"], k)
        shaped(f"{t}_shovel", 1, ["#", "S", "S"], k)
        shaped(f"{t}_sword", 1, ["#", "#", "S"], k)
        shaped(f"{t}_hoe", 1, ["##", " S", " S"], k)
    amats = {"leather": "leather", "copper": "copper_ingot", "iron": "iron_ingot", "golden": "gold_ingot", "diamond": "diamond"}
    for a, m in amats.items():
        k = {"#": m}
        shaped(f"{a}_helmet", 1, ["###", "# #"], k)
        shaped(f"{a}_chestplate", 1, ["# #", "###", "###"], k)
        shaped(f"{a}_leggings", 1, ["###", "# #", "# #"], k)
        shaped(f"{a}_boots", 1, ["# #", "# #"], k)
    shaped("turtle_helmet", 1, ["###", "# #"], {"#": "turtle_scute"})
    shaped("wolf_armor", 1, ["#  ", "###", "# #"], {"#": "armadillo_scute"})
    shaped("leather_horse_armor", 1, ["# #", "###", "# #"], {"#": "leather"})
    shaped("saddle", 1, [" # ", "#I#"], {"#": "leather", "I": "iron_ingot"})
    shaped("spectral_arrow", 2, [" G ", "GAG", " G "], {"G": "glowstone_dust", "A": "arrow"})

# ------------------------------------------------------------------ everything else
def misc():
    shaped("paper", 3, ["###"], {"#": "sugar_cane"})
    shapeless("book", 1, "paper", "paper", "paper", "leather")
    shapeless("writable_book", 1, "book", "ink_sac", "feather")
    shaped("map", 1, ["PPP", "PCP", "PPP"], {"P": "paper", "C": "compass"})
    shapeless("sugar", 1, "sugar_cane"); shapeless("sugar", 3, "honey_bottle")
    shaped("bread", 1, ["###"], {"#": "wheat"})
    shaped("cookie", 8, ["WCW"], {"W": "wheat", "C": "cocoa_beans"})
    shaped("cake", 1, ["MMM", "SES", "WWW"], {"M": "milk_bucket", "S": "sugar", "E": "#eggs", "W": "wheat"})
    shapeless("pumpkin_pie", 1, "pumpkin", "sugar", "#eggs")
    shapeless("mushroom_stew", 1, "brown_mushroom", "red_mushroom", "bowl")
    shapeless("rabbit_stew", 1, "baked_potato", "cooked_rabbit", "bowl", "carrot", "brown_mushroom")
    shaped("beetroot_soup", 1, ["BBB", "BBB", " W "], {"B": "beetroot", "W": "bowl"})
    shapeless("suspicious_stew", 1, "brown_mushroom", "red_mushroom", "bowl", "#small_flowers")
    shapeless("melon_seeds", 1, "melon_slice")
    shapeless("pumpkin_seeds", 4, "pumpkin")
    shapeless("beetroot_seeds", 0, "beetroot") if False else None
    shapeless("jack_o_lantern", 1, "carved_pumpkin", "torch")
    shaped("snow_block", 1, ["##", "##"], {"#": "snowball"})
    shaped("snow", 6, ["###"], {"#": "snow_block"})
    shaped("packed_ice", 1, ["###", "###", "###"], {"#": "ice"})
    shaped("blue_ice", 1, ["###", "###", "###"], {"#": "packed_ice"})
    shaped("clay", 1, ["##", "##"], {"#": "clay_ball"})
    shapeless("bone_meal", 3, "bone")
    shaped("honey_block", 1, ["##", "##"], {"#": "honey_bottle"})
    shapeless("honey_bottle", 4, "honey_block", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle")
    shaped("honeycomb_block", 1, ["##", "##"], {"#": "honeycomb"})
    shapeless("dried_kelp", 9, "dried_kelp_block")
    shaped("glowstone", 1, ["##", "##"], {"#": "glowstone_dust"})
    shaped("magma_block", 1, ["##", "##"], {"#": "magma_cream"})
    shapeless("magma_cream", 1, "blaze_powder", "slime_ball")
    shapeless("blaze_powder", 2, "blaze_rod")
    shapeless("ender_eye", 1, "ender_pearl", "blaze_powder")
    shapeless("fire_charge", 3, "gunpowder", "blaze_powder", "#coals")
    shapeless("fermented_spider_eye", 1, "spider_eye", "brown_mushroom", "sugar")
    shaped("glass_bottle", 3, ["# #", " # "], {"#": "glass"})
    shaped("lead", 2, ["~~ ", "~O ", "  ~"], {"~": "string", "O": "slime_ball"})
    shapeless("glow_item_frame", 1, "item_frame", "glow_ink_sac")
    shapeless("leather", 1, "rabbit_hide", "rabbit_hide", "rabbit_hide", "rabbit_hide") if False else shaped("leather", 1, ["##", "##"], {"#": "rabbit_hide"})
    shaped("tnt", 1, ["GSG", "SGS", "GSG"], {"G": "gunpowder", "S": "#sand"})
    shapeless("firework_rocket", 3, "paper", "gunpowder")
    shapeless("firework_star", 1, "gunpowder", "white_dye")
    shapeless("slime_ball", 9, "slime_block")
    shaped("white_wool", 1, ["##", "##"], {"#": "string"})
    shapeless("flower_banner_pattern", 1, "paper", "oxeye_daisy")
    shapeless("creeper_banner_pattern", 1, "paper", "creeper_head")
    shapeless("skull_banner_pattern", 1, "paper", "wither_skeleton_skull")
    shapeless("mojang_banner_pattern", 1, "paper", "enchanted_golden_apple")
    shapeless("field_masoned_banner_pattern", 1, "paper", "bricks")
    shapeless("bordure_indented_banner_pattern", 1, "paper", "vine")
    shaped("leather", 1, ["##", "##"], {"#": "rabbit_hide"})
    shapeless("bone_meal", 9, "bone_block")
    shapeless("wheat", 9, "hay_block")
    shaped("dried_ghast", 1, ["###", "#T#", "###"], {"#": "soul_sand", "T": "ghast_tear"}) if False else None
    shapeless("mangrove_planks", 0) if False else None

# ------------------------------------------------------------------ smelting
def smelting():
    for o, res, xp in (("iron", "iron_ingot", 0.7), ("gold", "gold_ingot", 1.0), ("copper", "copper_ingot", 0.7), ("diamond", "diamond", 1.0),
                       ("emerald", "emerald", 1.0), ("coal", "coal", 0.1), ("lapis", "lapis_lazuli", 0.2), ("redstone", "redstone", 0.7)):
        smelt(f"{o}_ore", res, xp, "fb"); smelt(f"deepslate_{o}_ore", res, xp, "fb")
    smelt("raw_iron", "iron_ingot", 0.7, "fb"); smelt("raw_gold", "gold_ingot", 1.0, "fb"); smelt("raw_copper", "copper_ingot", 0.7, "fb")
    smelt("nether_gold_ore", "gold_ingot", 1.0, "fb"); smelt("nether_quartz_ore", "quartz", 0.2, "fb"); smelt("ancient_debris", "netherite_scrap", 2.0, "fb")
    smelt("#smelts_to_glass", "glass", 0.1)
    for a, b in (("cobblestone", "stone"), ("stone", "smooth_stone"), ("sandstone", "smooth_sandstone"), ("red_sandstone", "smooth_red_sandstone"),
                 ("quartz_block", "smooth_quartz"), ("clay_ball", "brick"), ("clay", "terracotta"), ("netherrack", "nether_brick"),
                 ("stone_bricks", "cracked_stone_bricks"), ("nether_bricks", "cracked_nether_bricks"), ("deepslate_bricks", "cracked_deepslate_bricks"),
                 ("deepslate_tiles", "cracked_deepslate_tiles"), ("polished_blackstone_bricks", "cracked_polished_blackstone_bricks"),
                 ("cobbled_deepslate", "deepslate"), ("basalt", "smooth_basalt"), ("wet_sponge", "sponge"), ("cactus", "green_dye"),
                 ("sea_pickle", "lime_dye"), ("chorus_fruit", "popped_chorus_fruit"), ("resin_clump", "resin_brick")):
        smelt(a, b, 0.1)
    smelt("#logs_that_burn", "charcoal", 0.15)
    for a, b, xp in (("beef", "cooked_beef", 0.35), ("porkchop", "cooked_porkchop", 0.35), ("chicken", "cooked_chicken", 0.35),
                     ("mutton", "cooked_mutton", 0.35), ("rabbit", "cooked_rabbit", 0.35), ("cod", "cooked_cod", 0.35), ("salmon", "cooked_salmon", 0.35),
                     ("potato", "baked_potato", 0.35), ("kelp", "dried_kelp", 0.1)):
        smelt(a, b, xp, "fsc")
    for t in ("iron", "golden"):
        nug = "iron_nugget" if t == "iron" else "gold_nugget"
        for k in ("sword", "shovel", "pickaxe", "axe", "hoe", "helmet", "chestplate", "leggings", "boots", "horse_armor"):
            smelt(f"{t}_{k}" if k != "horse_armor" else f"{'iron' if t == 'iron' else 'golden'}_horse_armor", nug, 0.1, "fb")
    for k in ("helmet", "chestplate", "leggings", "boots"): smelt(f"chainmail_{k}", "iron_nugget", 0.1, "fb")
    for k in ("sword", "shovel", "pickaxe", "axe", "hoe", "helmet", "chestplate", "leggings", "boots"): smelt(f"copper_{k}", "copper_nugget", 0.1, "fb")

def smithing():
    for k in ("sword", "shovel", "pickaxe", "axe", "hoe", "helmet", "chestplate", "leggings", "boots"):
        SMITH.append(("netherite_upgrade_smithing_template", f"diamond_{k}", "netherite_ingot", f"netherite_{k}"))
    trims = {"netherite_upgrade": "netherrack", "sentry_armor_trim": "cobblestone", "dune_armor_trim": "sandstone", "coast_armor_trim": "cobblestone",
             "wild_armor_trim": "mossy_cobblestone", "ward_armor_trim": "cobbled_deepslate", "eye_armor_trim": "end_stone", "vex_armor_trim": "cobblestone",
             "tide_armor_trim": "prismarine", "snout_armor_trim": "blackstone", "rib_armor_trim": "netherrack", "spire_armor_trim": "purpur_block",
             "wayfinder_armor_trim": "terracotta", "shaper_armor_trim": "terracotta", "silence_armor_trim": "cobbled_deepslate",
             "raiser_armor_trim": "terracotta", "host_armor_trim": "terracotta", "flow_armor_trim": "breeze_rod", "bolt_armor_trim": "copper_block"}
    for t, base in trims.items():
        shaped(f"{t}_smithing_template", 2, ["DTD", "DBD", "DDD"], {"D": "diamond", "T": f"{t}_smithing_template", "B": base})

def ultimate():
    """UltimateCraft's own content (Hell, Space, Sift)."""
    shapeless("crystal_planks", 4, "crystal_log")
    shaped("holy_stone_bricks", 4, ["##", "##"], {"#": "holy_stone"})
    shaped("space_helmet", 1, ["III", "IGI"], {"I": "iron_ingot", "G": "glass"})
    shaped("oxygen_canister", 2, [" I ", "GBG", " I "], {"I": "iron_ingot", "G": "glass", "B": "oxygen_bloom"})
    shaped("umbral_lamp", 1, [" C ", "CGC", " C "], {"C": "delvari_coin", "G": "glowstone"})
    shaped("delvari_tiles", 4, ["BB", "BB"], {"B": "delvari_bricks"})
    shapeless("divine_ingot", 1, "gold_ingot", "gold_ingot", "radiant_crystal", "soul_essence")
    for kind, rows in (("sword", ["D", "D", "S"]), ("pickaxe", ["DDD", " S ", " S "]), ("axe", ["DD", "DS", " S"]), ("shovel", ["D", "S", "S"]), ("hoe", ["DD", " S", " S"])):
        shaped(f"divine_{kind}", 1, rows, {"D": "divine_ingot", "S": "stick"})
    for kind, rows in (("helmet", ["DDD", "D D"]), ("chestplate", ["D D", "DDD", "DDD"]), ("leggings", ["DDD", "D D", "D D"]), ("boots", ["D D", "D D"])):
        shaped(f"divine_{kind}", 1, rows, {"D": "divine_ingot"})
    shapeless("holy_water", 2, "glass_bottle", "glass_bottle", "gold_nugget", "gold_nugget", "glowstone_dust")
    shapeless("holy_water", 8, "brimstone_heart", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle")
    shapeless("blessed_sword", 1, "iron_sword", "gold_ingot", "gold_ingot", "holy_water")
    shapeless("ghost_sight_tonic", 1, "glass_bottle", "ectoplasm", "ectoplasm", "glowstone_dust")
    shapeless("phasing_pearl", 1, "ender_pearl", "ectoplasm", "ectoplasm")
    shaped("radiant_crystal", 1, ["PP", "PP"], {"P": "crystal_leaves"})
