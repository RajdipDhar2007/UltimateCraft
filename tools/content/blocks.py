"""Block registry.

Every block item from tools/content/items_list.csv is classified by id rules into a
"kind" with one or more *states* (orientation, half, open, age ...).  A state is a
dict whose keys mirror the C BlockDef struct.  Extra blocks that have no item of
their own (crops, wall torches, fluids, portals, fire ...) are appended at the end.
"""
from colors import *

# ------------------------------------------------------------------ enums mirrored in C
MESH = ["NONE", "CUBE", "CROSS", "LIQUID", "TORCH", "CACTUS", "BOXES", "CONNECT", "CROP", "FLAT", "WIRE", "RAIL"]
LAYER = ["OPAQUE", "CUTOUT", "TRANSLUCENT"]
TOOLS = ["NONE", "PICKAXE", "AXE", "SHOVEL", "SWORD", "HOE", "SHEARS"]
SOUNDS = ["STONE", "WOOD", "GRASS", "GRAVEL", "SAND", "GLASS", "WOOL", "SNOW", "METAL"]
SUPPORT = ["NONE", "SOLID", "SOIL", "SAND", "FARMLAND", "WATER", "SOUL", "NYLIUM", "ATTACH", "CEILING", "STACK", "ANY"]
PLACE = ["SIMPLE", "AXIS", "AXIS2", "FACING4", "FACING6", "SLAB", "STAIRS", "DOOR", "TRAPDOOR", "GATE", "ATTACH", "SIGN",
         "DOUBLE", "BED", "LAYER", "HANGING", "WALLONLY", "RAIL", "PISTON", "HOPPER"]
TINT = ["NONE", "GRASS", "FOLIAGE", "WATER", "SPRUCE", "BIRCH", "LILY"]
BE = ["NONE", "CHEST", "FURNACE", "BLAST", "SMOKER", "BARREL", "ENDER", "SHULKER", "HOPPER", "DISPENSER", "BREWING", "JUKEBOX", "SPAWNER", "COMPARATOR", "BANNER", "LECTERN"]
UI = ["NONE", "CRAFTING", "FURNACE", "CHEST", "STONECUTTER", "SMITHING", "BREWING", "ENCHANT", "ANVIL", "LOOM",
      "GRINDSTONE", "CARTOGRAPHY", "TOGGLE", "BED", "JUKEBOX", "NOTE", "CAKE", "RESPAWN", "COMPOSTER", "CRAFTER", "BELL", "FRAME_EYE", "LECTERN"]
# flag bits
F_LEAVES, F_FLAMMABLE, F_CLIMBABLE, F_WATERLOGGED, F_FENCELIKE, F_TALL, F_NOCOLLIDE_MOB, F_CREATIVE, F_ROT90 = 1, 2, 4, 8, 16, 32, 64, 128, 256

FACES = ["W", "E", "D", "U", "N", "S"]
FACING4 = ["N", "E", "S", "W"]                  # placement order
FACE_OF = {"W": 0, "E": 1, "D": 2, "U": 3, "N": 4, "S": 5}

def ALL(t): return [t] * 6
def TSB(top, side, bottom): return [side, side, bottom, top, side, side]
def FRONT(top, side, bottom, front, facing="S"):
    t = [side, side, bottom, top, side, side]
    t[FACE_OF[facing]] = front
    return t

# ------------------------------------------------------------------ shapes (boxes in 1/16 units, facing north)
def rot_box(b, k):
    x0, y0, z0, x1, y1, z1 = b
    for _ in range(k):  # 90 degrees clockwise seen from above: N -> E -> S -> W
        x0, z0, x1, z1 = 16 - z1, x0, 16 - z0, x1
    return (min(x0, x1), y0, min(z0, z1), max(x0, x1), y1, max(z0, z1))
def rot(boxes, k): return [rot_box(b, k) for b in boxes]
def face6(boxes, f):
    """boxes modelled pointing north (-Z, front at z=0) -> pointing toward face f (0 W,1 E,2 D,3 U,4 N,5 S)."""
    if f == 4: return list(boxes)
    if f in (1, 5, 0): return rot(boxes, {1: 1, 5: 2, 0: 3}[f])
    out = []
    for b in boxes:   # rotate about X: north (-Z) -> up (+Y) or down (-Y)
        x0, y0, z0, x1, y1, z1 = b
        if f == 3: out.append((x0, 16 - z1, y0, x1, 16 - z0, y1))
        else: out.append((x0, z0, 16 - y1, x1, z1, 16 - y0))
    return out
def flip_y(boxes): return [(b[0], 16 - b[4], b[2], b[3], 16 - b[1], b[5]) for b in boxes]
def rot_tex(tex, k):
    """rotate the 4 horizontal face textures of a north-facing tex list."""
    order = [4, 1, 5, 0]   # N E S W face indices
    t = list(tex)
    for _ in range(k):
        n = [t[i] for i in order]
        n = [n[-1]] + n[:-1]
        for j, i in enumerate(order): t[i] = n[j]
    return t

SLAB_B = [(0, 0, 0, 16, 8, 16)]
STAIRS_N = [(0, 0, 0, 16, 8, 16), (0, 8, 0, 16, 16, 8)]
DOOR_N = [(0, 0, 0, 16, 16, 3)]
TRAP_B = [(0, 0, 0, 16, 3, 16)]
TRAP_OPEN_N = [(0, 0, 13, 16, 16, 16)]
CARPET = [(0, 0, 0, 16, 1, 16)]
PLATE = [(1, 0, 1, 15, 1, 15)]
BUTTON_WALL_N = [(5, 6, 14, 11, 10, 16)]
BUTTON_PRESSED_N = [(5, 6, 15, 11, 10, 16)]
LADDER_N = [(0, 0, 14, 16, 16, 16)]
CHEST = [(1, 0, 1, 15, 14, 15)]
BED_HEAD_N = [(0, 3, 0, 16, 9, 16), (0, 0, 0, 3, 3, 3), (13, 0, 0, 16, 3, 3)]
BED_FOOT_N = [(0, 3, 0, 16, 9, 16), (0, 0, 13, 3, 3, 16), (13, 0, 13, 16, 3, 16)]
LANTERN = [(5, 0, 5, 11, 7, 11), (6, 7, 6, 10, 9, 10)]
LANTERN_H = [(5, 1, 5, 11, 8, 11), (6, 8, 6, 10, 10, 10), (7, 10, 7, 9, 16, 9)]
CHAIN = [(7, 0, 7, 9, 16, 9)]
ROD = [(6, 0, 6, 10, 16, 10)]
CANDLE = [(7, 0, 7, 9, 6, 9)]
HEAD = [(4, 0, 4, 12, 8, 12)]
POT = [(5, 0, 5, 11, 6, 11)]
CAKE = [(1, 0, 1, 15, 8, 15)]
ENCH = [(0, 0, 0, 16, 12, 16)]
CUTTER = [(0, 0, 0, 16, 9, 16)]
FARMLAND = [(0, 0, 0, 16, 15, 16)]
CAMPFIRE = [(0, 0, 0, 16, 7, 16)]
FRAME = [(0, 0, 0, 16, 13, 16)]
DAYLIGHT = [(0, 0, 0, 16, 6, 16)]
SIGN_N = [(0, 7, 7, 16, 16, 9), (7, 0, 7, 9, 7, 9)]
SIGN_WALL_N = [(0, 4, 14, 16, 12, 16)]
HSIGN_N = [(1, 0, 7, 15, 10, 9), (2, 10, 8, 3, 16, 8), (13, 10, 8, 14, 16, 8)]
BANNER = [(7, 0, 7, 9, 16, 9), (2, 2, 9, 14, 16, 10)]
BUD = {"small": [(4, 0, 4, 12, 3, 12)], "medium": [(3, 0, 3, 13, 4, 13)], "large": [(3, 0, 3, 13, 5, 13)], "cluster": [(3, 0, 3, 13, 7, 13)]}
ANVIL = [(2, 0, 2, 14, 4, 14), (4, 4, 3, 12, 5, 13), (6, 5, 4, 10, 10, 12), (3, 10, 0, 13, 16, 16)]
HOPPER = [(0, 10, 0, 16, 16, 16), (4, 4, 4, 12, 10, 12), (6, 0, 6, 10, 4, 10)]
CAULDRON = [(0, 3, 0, 16, 16, 2), (0, 3, 14, 16, 16, 16), (0, 3, 0, 2, 16, 16), (14, 3, 0, 16, 16, 16), (0, 3, 0, 16, 4, 16), (0, 0, 0, 4, 3, 4), (12, 0, 12, 16, 3, 16)]
BREWING = [(7, 0, 7, 9, 14, 9), (1, 0, 1, 15, 2, 15)]
LECTERN = [(0, 0, 0, 16, 2, 16), (4, 2, 4, 12, 13, 12), (0, 13, 0, 16, 16, 16)]
GRINDSTONE = [(4, 4, 2, 12, 16, 14), (2, 0, 6, 4, 13, 10), (12, 0, 6, 14, 13, 10)]
BELL = [(4, 4, 4, 12, 13, 12), (3, 4, 3, 13, 6, 13), (7, 13, 7, 9, 16, 9)]
COMPOSTER = [(0, 0, 0, 16, 2, 16), (0, 0, 0, 2, 16, 16), (14, 0, 0, 16, 16, 16), (0, 0, 0, 16, 16, 2), (0, 0, 14, 16, 16, 16)]
DRAGON_EGG = [(3, 0, 3, 13, 12, 13), (5, 12, 5, 11, 15, 11)]
PICKLE = [(6, 0, 6, 10, 6, 10)]
EGG = [(5, 0, 5, 11, 7, 11)]
SNIFFER_EGG = [(1, 0, 2, 15, 16, 14)]
POINTED = [(5, 0, 5, 11, 16, 11)]
DECORATED_POT = [(1, 0, 1, 15, 16, 15)]
CONDUIT = [(5, 5, 5, 11, 11, 11)]
BEACON = [(0, 0, 0, 16, 16, 16)]
SCAFFOLD = [(0, 14, 0, 16, 16, 16), (0, 0, 0, 2, 14, 2), (14, 0, 0, 16, 14, 2), (0, 0, 14, 2, 14, 16), (14, 0, 14, 16, 14, 16)]
STATUE = [(3, 0, 3, 13, 10, 13), (4, 10, 4, 12, 16, 12)]
PISTON_HEAD = [(0, 0, 0, 16, 16, 4), (6, 6, 4, 10, 10, 16)]
SHELF_N = [(0, 0, 11, 16, 16, 16), (0, 0, 5, 16, 1, 16), (0, 15, 5, 16, 16, 16)]
REPEATER = [(0, 0, 0, 16, 2, 16), (7, 2, 2, 9, 7, 4), (7, 2, 10, 9, 7, 12)]
def repeater_shape(delay, powered):
    # front (output) is north: fixed torch near z=2, the movable one slides back with the delay
    z = 6 + delay * 2
    return [(0, 0, 0, 16, 2, 16), (7, 2, 2, 9, 7 if powered else 6, 4), (7, 2, z, 9, 7 if powered else 6, z + 2)]
def comparator_shape(mode, powered):
    return [(0, 0, 0, 16, 2, 16), (7, 2, 2, 9, 5 if mode else 4, 4), (3, 2, 11, 5, 6 if powered else 5, 13), (11, 2, 11, 13, 6 if powered else 5, 13)]
WALL_TORCH_N = [(7, 3, 11, 9, 13, 13)]
END_PORTAL_FRAME = [(0, 0, 0, 16, 13, 16)]
END_PORTAL_FRAME_EYE = [(0, 0, 0, 16, 13, 16), (4, 13, 4, 12, 16, 12)]
PORTAL_X = [(0, 0, 6, 16, 16, 10)]
END_PORTAL = [(0, 0, 0, 16, 12, 16)]
CHORUS = [(3, 0, 3, 13, 16, 13)]
TRIPWIRE = [(0, 1, 7, 16, 2, 9)]
HONEY = [(1, 0, 1, 15, 15, 15)]
CACTUS_FLOWER = [(3, 0, 3, 13, 4, 13)]
SNOW_LAYERS = [[(0, 0, 0, 16, 2 * i, 16)] for i in range(1, 9)]
STATUE_SHAPES = STATUE

# ------------------------------------------------------------------ registry
class Kind:
    def __init__(self, key, name, states, place="SIMPLE", item=True):
        self.key, self.name, self.states, self.place, self.item = key, name, states, place, item

KINDS = []
KIND_BY_KEY = {}

DEFAULT = dict(mesh="CUBE", layer="OPAQUE", solid=True, opaque=True, opacity=15, emit=(0, 0, 0), hardness=1.5,
               tool="NONE", tier=0, drop=None, drop_min=1, drop_max=1, sound="STONE", replaceable=False,
               support="NONE", falls=False, liquid=False, animated=False, blast=None, touch=0, xp=(0, 0),
               shape=None, tint="NONE", tint_faces=0, flags=0, be="NONE", ui="NONE", collide=None, alt=None)

def st(**kw):
    d = dict(DEFAULT)
    d.update(kw)
    return d

def add(key, name, states, place="SIMPLE", item=True):
    if isinstance(states, dict): states = [states]
    if len(states) == 1 and states[0].get("flags", 0) & F_LEAVES:
        states = [states[0], dict(states[0])]   # state 0 = natural leaves (they decay), state 1 = persistent (placed by a player, never decays)
    for s in states:
        if s.get("shape") is not None and s["mesh"] == "CUBE":
            s["mesh"] = "BOXES"
        if s["mesh"] in ("BOXES", "CROSS", "TORCH", "CROP", "FLAT", "CONNECT", "WIRE", "RAIL"):
            full = s.get("shape") == [(0, 0, 0, 16, 16, 16)]
            if not full:
                s["opaque"] = False
                if s["opacity"] == 15 and not s.get("keep_opacity"): s["opacity"] = 0 if s["mesh"] != "BOXES" else 1
                if s["layer"] == "OPAQUE" and s["mesh"] in ("CROSS", "TORCH", "CROP", "FLAT", "WIRE", "RAIL"): s["layer"] = "CUTOUT"
    k = Kind(key, rename(name), states, place, item)
    KINDS.append(k)
    KIND_BY_KEY[key] = k
    return k

# common presets ------------------------------------------------------------------------------
def stone(col, h=1.5, tier=1, **kw): return {**dict(hardness=h, tool="PICKAXE", tier=tier, sound="STONE"), **kw}
def wood(h=2.0, **kw): return {**dict(hardness=h, tool="AXE", sound="WOOD", flags=F_FLAMMABLE), **kw}
def soil(h=0.5, **kw): return {**dict(hardness=h, tool="SHOVEL", sound="GRAVEL"), **kw}
def plant(**kw):
    d = dict(mesh="CROSS", layer="CUTOUT", solid=False, opaque=False, opacity=0, hardness=0, sound="GRASS", support="SOIL", flags=F_FLAMMABLE)
    d.update(kw)
    return d

def material_painter(key):
    """Texture spec for the full block of a material key (used by its stairs/slabs/walls)."""
    col = STONE_RGB.get(key)
    if key.startswith("chiseled"): return ("chiseled", col)
    if key == "terracotta": return ("terracotta", col)
    if key in WOODS_PLANKS: return ("planks", PLANKS_RGB[WOODS_PLANKS[key]])
    if key.endswith("bricks") or key in ("bricks",):
        if "deepslate" in key: return ("bricks", col, 0x2a2a2a, 8, 4)
        if "nether" in key: return ("bricks", col, 0x1a0a0a, 8, 4)
        if key in ("bricks",): return ("bricks", col, 0xb8aca0, 8, 4)
        if "mossy" in key: return ("mossy", col, "bricks")
        return ("bricks", col, 0x5a5a5a if "stone" in key or "tuff" in key or "blackstone" in key else 0x4a3a2a, 8, 4)
    if key.endswith("tiles"): return ("tiles", col)
    if key.startswith("polished") or key.startswith("smooth") or key.startswith("cut_") or key in ("quartz_block", "purpur_block", "smooth_quartz"):
        return ("polished", col)
    if key in ("cobblestone", "cobbled_deepslate"): return ("cobble", col)
    if key == "mossy_cobblestone": return ("mossy", col, "cobble")
    if key in ("deepslate",): return ("deepslate", col)
    if key in ("sandstone", "red_sandstone"): return ("sand", col)
    if key in ("prismarine",): return ("stone", col, 0.18)
    if key == "netherrack": return ("netherrack", col)
    if col is None: raise KeyError(key)
    return ("stone", col)

def _p(f, *a): return (f,) + a
# painters referenced by name in paint.py; "mossy"/"cracked" need wrappers:
WOODS_PLANKS = {f"{w}_planks": w for w in WOODS}
WOODS_PLANKS["bamboo_mosaic"] = "bamboo"

# ------------------------------------------------------------------ helpers for families
def cube(key, name, tex, **kw):
    return add(key, name, st(tex=tex if isinstance(tex, list) else ALL(tex), **kw))

def axis_states(tex_side, tex_end, **kw):
    """three states: y, x, z axis"""
    out = []
    for ax in ("y", "x", "z"):
        if ax == "y": t = TSB(tex_end, tex_side, tex_end)
        elif ax == "x": t = [tex_end, tex_end, tex_side, tex_side, tex_side, tex_side]
        else: t = [tex_side, tex_side, tex_side, tex_side, tex_end, tex_end]
        out.append(st(tex=t, **kw))
    return out

def facing_states(front, side, top, bottom=None, nsub=1, sub_front=None, **kw):
    """4 facings (N,E,S,W: front faces that way); optional sub states (e.g. lit) interleaved."""
    out = []
    bottom = bottom or top
    for k, f in enumerate(FACING4):
        for s in range(nsub):
            fr = front if s == 0 or sub_front is None else sub_front
            t = [side, side, bottom, top, side, side]
            t[FACE_OF[f]] = fr
            extra = dict(kw)
            if s == 1 and "emit_lit" in kw: extra["emit"] = kw["emit_lit"]
            extra.pop("emit_lit", None)
            out.append(st(tex=t, **extra))
    return out

def slab_states(tex, double_drop_key=None, **kw):
    t = tex if isinstance(tex, list) else ALL(tex)
    return [st(tex=t, shape=SLAB_B, **kw), st(tex=t, shape=flip_y(SLAB_B), **kw), st(tex=t, drop_mul=2, **kw)]

def stairs_states(tex, **kw):
    t = tex if isinstance(tex, list) else ALL(tex)
    out = []
    for k in range(4):
        out.append(st(tex=t, shape=rot(STAIRS_N, k), **kw))
        out.append(st(tex=t, shape=flip_y(rot(STAIRS_N, k)), **kw))
    return out

def door_states(tex_lower, tex_upper, **kw):
    out = []
    for k in range(4):
        for half in (0, 1):
            for op in (0, 1):
                shp = rot(DOOR_N, (k + (1 if op else 0)) % 4)
                t = ALL(tex_upper if half else tex_lower)
                out.append(st(tex=t, shape=shp, layer="CUTOUT", **kw))
    return out

def trapdoor_states(tex, **kw):
    out = []
    for k in range(4):
        for top in (0, 1):
            for op in (0, 1):
                if op: shp = rot(TRAP_OPEN_N, k)
                else: shp = TRAP_B if not top else flip_y(TRAP_B)
                out.append(st(tex=ALL(tex), shape=shp, layer="CUTOUT", **kw))
    return out

def gate_states(tex, **kw):
    out = []
    for k in range(4):
        for op in (0, 1):
            if not op:
                shp = rot([(0, 5, 7, 2, 16, 9), (14, 5, 7, 16, 16, 9), (2, 6, 7, 14, 9, 9), (2, 12, 7, 14, 15, 9)], k)
            else:
                shp = rot([(0, 5, 7, 2, 16, 9), (14, 5, 7, 16, 16, 9), (0, 6, 9, 2, 15, 16), (14, 6, 9, 16, 15, 16)], k)
            out.append(st(tex=ALL(tex), shape=shp, collide=None if not op else "none", **kw))
    return out

def plate_states(tex, **kw):
    return [st(tex=tex, shape=PLATE, solid=False, support="SOLID", **kw), st(tex=tex, shape=[(1, 0, 1, 15, 0.5, 15)], solid=False, support="SOLID", **kw)]

def powered_attach(tex, floor_shape, wall_shape_n, pressed_floor, pressed_wall_n, **kw):
    """12 states: var = powered * 6 + support face."""
    return attach_states(tex, floor_shape, wall_shape_n, **kw) + attach_states(tex, pressed_floor, pressed_wall_n, **kw)

def attach_states(tex, floor_shape, wall_shape_n, ceil_shape=None, floor=True, ceiling=False, **kw):
    """var = direction of the supporting block as a face index 0..5 (W,E,D,U,N,S).
    wall_shape_n is the shape when the support is on the north (-Z) side."""
    out = []
    for f in range(6):
        if f == 2: shp = floor_shape
        elif f == 3: shp = ceil_shape if ceil_shape is not None else flip_y(floor_shape)
        else: shp = rot(wall_shape_n, {4: 0, 1: 1, 5: 2, 0: 3}[f])
        out.append(st(tex=tex if isinstance(tex, list) else ALL(tex), shape=shp, **kw))
    return out

# wall_shape_n boxes touch z=16 in the definitions above (attached to the block to the SOUTH). Normalise:
def wall_n(boxes):
    """convert boxes attached to +Z wall into boxes attached to -Z wall (support north)."""
    return [(b[0], b[1], 16 - b[5], b[3], b[4], 16 - b[2]) for b in boxes]

# ------------------------------------------------------------------ the item list
import csv, os
HERE = os.path.dirname(os.path.abspath(__file__))
ITEM_ROWS = []
# UltimateCraft's own names for the re-themed dimensions: the Nether is Hell, the End is Space.
# Only display names change; ids stay the same because saves and commands use them.
RENAMES = [("Netherite", "Infernium"), ("Netherrack", "Hellrack"), ("Nether ", "Hell "), ("End Stone", "Asteroid Stone"),
           ("End Rod", "Star Rod"), ("End Crystal", "Star Crystal"), ("End Portal", "Space Portal"), ("End Gateway", "Star Gateway"),
           # v5 re-theme: the old "Ender" family belongs to the void now
           ("Enderman", "Voidwalker"), ("Endermite", "Void Mite"), ("Ender Dragon", "Void Wyrm"), ("Eye of Ender", "Star Eye"),
           ("Ender Pearl", "Void Pearl"), ("Ender Chest", "Void Chest"), ("Dragon's Breath", "Wyrm's Breath"), ("Dragon Egg", "Wyrm Egg"),
           ("Dragon Head", "Wyrm Head"), ("Shulker Shell", "Starshell Plate"), ("Shulker", "Starshell"), ("Purpur", "Nebulite"),
           ("Popped Chorus Fruit", "Toasted Starbloom Fruit"), ("Chorus", "Starbloom")]
def rename(n):
    for a, b in RENAMES: n = n.replace(a, b)
    return n
with open(os.path.join(HERE, "items_list.csv")) as f:
    for r in csv.DictReader(f):
        ITEM_ROWS.append((r["id"], rename(r["name"]), int(r["stack"]), r["obtainable"] == "YES"))
NAME = {i: n for i, n, _, _ in ITEM_ROWS}
ALL_IDS = [i for i, _, _, _ in ITEM_ROWS]
IDSET = set(ALL_IDS)

def nm(key): return NAME.get(key) or key.replace("_", " ").title()

# ------------------------------------------------------------------ texture shorthands
def T(*spec): return spec
GRASS_TOP = T("grass_top", 0x9a9a9a)
DIRT = T("dirt", 0x866043)
STONE_T = T("stone", 0x7D7D7D)

def tex_for_material(key):
    if key == "stone": return STONE_T
    p = material_painter(key)
    if p[0] == "mossy": return T("mossy", p[1], p[2])
    return p

def mat_of(prefix):
    """map a stairs/slab/wall prefix to the full-block material key."""
    special = {"brick": "bricks", "stone_brick": "stone_bricks", "mossy_stone_brick": "mossy_stone_bricks", "mud_brick": "mud_bricks",
               "nether_brick": "nether_bricks", "red_nether_brick": "red_nether_bricks", "end_stone_brick": "end_stone_bricks",
               "deepslate_brick": "deepslate_bricks", "deepslate_tile": "deepslate_tiles", "tuff_brick": "tuff_bricks",
               "polished_blackstone_brick": "polished_blackstone_bricks", "resin_brick": "resin_bricks", "prismarine_brick": "prismarine_bricks",
               "quartz": "quartz_block", "purpur": "purpur_block", "petrified_oak": "oak_planks", "smooth_quartz": "smooth_quartz"}
    if prefix in special: return special[prefix]
    if prefix in WOODS: return f"{prefix}_planks"
    if prefix == "bamboo_mosaic": return "bamboo_mosaic"
    for st_ in COPPER_STAGES:
        for wx in ("", "waxed_"):
            if prefix == f"{wx}{st_}cut_copper": return f"{wx}{st_}cut_copper"
    return prefix

def copper_stage(key):
    k = key.replace("waxed_", "")
    for s in ("exposed_", "weathered_", "oxidized_"):
        if k.startswith(s): return s
    return ""

def mat_tex(mkey):
    if "cut_copper" in mkey: return T("bricks", COPPER_RGB[copper_stage(mkey)], shade_hex(COPPER_RGB[copper_stage(mkey)], 0.7), 8, 8)
    if mkey in WOODS_PLANKS:
        if mkey == "bamboo_mosaic": return T("tiles", PLANKS_RGB["bamboo"])
        return T("planks", PLANKS_RGB[WOODS_PLANKS[mkey]])
    return tex_for_material(mkey)

def shade_hex(h, f):
    r, g, b = (h >> 16) & 255, (h >> 8) & 255, h & 255
    return (min(255, int(r * f)) << 16) | (min(255, int(g * f)) << 8) | min(255, int(b * f))

def mat_props(mkey):
    if mkey in WOODS_PLANKS:
        p = wood()
        if mkey.startswith("crimson") or mkey.startswith("warped"): p["flags"] = 0
        return p
    if "copper" in mkey: return dict(stone(0, 3.0, 1), sound="METAL")
    h = {"cobblestone": 2.0, "mossy_cobblestone": 2.0, "bricks": 2.0, "deepslate": 3.0, "cobbled_deepslate": 3.5,
         "polished_deepslate": 3.5, "deepslate_bricks": 3.5, "deepslate_tiles": 3.5, "end_stone_bricks": 3.0,
         "sandstone": 0.8, "red_sandstone": 0.8, "smooth_sandstone": 2.0, "smooth_red_sandstone": 2.0,
         "quartz_block": 0.8, "smooth_quartz": 2.0, "nether_bricks": 2.0, "red_nether_bricks": 2.0, "mud_bricks": 1.5,
         "smooth_stone": 2.0, "polished_blackstone_bricks": 1.5, "resin_bricks": 1.5}.get(mkey, 1.5)
    return stone(0, h)

# ================================================================== classification
def build():
    KINDS.clear(); KIND_BY_KEY.clear()
    add("air", "Air", st(mesh="NONE", solid=False, opaque=False, opacity=0, replaceable=True, hardness=-1, drop="nothing"), item=False)
    for key, name, stack, obt in ITEM_ROWS:
        if is_item_only(key): continue
        make_block(key, name, obt)
    extra_blocks()
    return KINDS

ITEM_SUFFIXES = ("_sword", "_shovel", "_pickaxe", "_axe", "_hoe", "_helmet", "_chestplate", "_leggings", "_boots",
                 "_spawn_egg", "_bucket", "_pottery_sherd", "_smithing_template", "_banner_pattern", "_dye", "_harness",
                 "_boat", "_raft", "_bundle", "_horse_armor", "_minecart", "_ingot", "_nugget", "_on_a_stick")
ITEM_ONLY = set("""saddle minecart phantom_membrane elytra turtle_helmet turtle_scute armadillo_scute wolf_armor flint_and_steel bowl
apple bow arrow coal charcoal diamond emerald lapis_lazuli quartz amethyst_shard raw_iron raw_copper raw_gold netherite_scrap stick
mushroom_stew string feather gunpowder wheat_seeds wheat bread flint porkchop cooked_porkchop painting golden_apple enchanted_golden_apple
bucket snowball leather brick clay_ball paper book slime_ball egg blue_egg brown_egg compass recovery_compass bundle fishing_rod clock
spyglass glowstone_dust cod salmon tropical_fish pufferfish cooked_cod cooked_salmon ink_sac glow_ink_sac cocoa_beans bone_meal bone
sugar cookie filled_map shears melon_slice dried_kelp pumpkin_seeds melon_seeds beef cooked_beef chicken cooked_chicken rotten_flesh
ender_pearl blaze_rod ghast_tear nether_wart_item glass_bottle potion spider_eye fermented_spider_eye blaze_powder magma_cream ender_eye
glistering_melon_slice experience_bottle fire_charge wind_charge writable_book written_book breeze_rod mace item_frame glow_item_frame
carrot potato baked_potato poisonous_potato map golden_carrot nether_star pumpkin_pie firework_rocket firework_star enchanted_book
nether_brick resin_brick prismarine_shard prismarine_crystals rabbit cooked_rabbit rabbit_stew rabbit_foot rabbit_hide armor_stand
lead name_tag mutton cooked_mutton end_crystal chorus_fruit popped_chorus_fruit torchflower_seeds pitcher_pod beetroot beetroot_seeds
beetroot_soup dragon_breath splash_potion spectral_arrow tipped_arrow lingering_potion shield totem_of_undying shulker_shell
disc_fragment_5 trident nautilus_shell heart_of_the_sea crossbow suspicious_stew goat_horn sweet_berries glow_berries honeycomb
honey_bottle echo_shard brush nether_wart trial_key ominous_trial_key ominous_bottle command_block_minecart knowledge_book debug_stick
redstone resin_clump ectoplasm sift_key tyrant_crown stardust oxygen_canister delvari_coin umbral_blade void_sigil soul_essence divine_ingot divine_sword divine_pickaxe divine_axe divine_shovel divine_hoe divine_helmet divine_chestplate divine_leggings divine_boots brimstone_heart holy_water blessed_sword ghost_sight_tonic phasing_pearl""".split())

def is_item_only(key):
    if key in ITEM_ONLY: return True
    if key.startswith("music_disc_"): return True
    if key.endswith(ITEM_SUFFIXES) and key not in ("bamboo_raft",): return True
    if key == "bamboo_raft" or key == "bamboo_chest_raft": return True
    return False

def make_block(key, name, obt):
    creative = 0 if obt else F_CREATIVE
    # ---------------- wood families
    for w in WOODS:
        stem = w in ("crimson", "warped")
        fl = 0 if stem else F_FLAMMABLE
        if key == f"{w}_planks": return add(key, name, st(tex=ALL(T("planks", PLANKS_RGB[w])), **dict(wood(), flags=fl)))
        logn = "stem" if stem else "log"
        woodn = "hyphae" if stem else "wood"
        if key in (f"{w}_{logn}", f"stripped_{w}_{logn}") or (w == "bamboo" and key in ("bamboo_block", "stripped_bamboo_block")):
            strip = key.startswith("stripped")
            col = STRIPPED_RGB[w] if strip else BARK_RGB[w]
            side = T("log_side", col, 0x2a2a2a if w == "birch" and not strip else None, strip)
            end = T("log_top", col, STRIPPED_RGB[w])
            return add(key, name, axis_states(side, end, **dict(wood(), flags=fl, sound="WOOD")), "AXIS")
        if key in (f"{w}_{woodn}", f"stripped_{w}_{woodn}"):
            strip = key.startswith("stripped")
            col = STRIPPED_RGB[w] if strip else BARK_RGB[w]
            side = T("log_side", col, 0x2a2a2a if w == "birch" and not strip else None, strip)
            return add(key, name, axis_states(side, side, **dict(wood(), flags=fl)), "AXIS")
        if key == f"{w}_leaves":
            tint = LEAVES_TINT.get(w)
            tt = {"spruce": "SPRUCE", "birch": "BIRCH"}.get(w, "FOLIAGE" if tint else "NONE")
            return add(key, name, st(tex=ALL(T("leaves", LEAVES_RGB[w])), layer="CUTOUT", opaque=False, opacity=1, hardness=0.2,
                                     tool="HOE", sound="GRASS", drop="nothing", tint=tt, tint_faces=63, flags=F_LEAVES | F_FLAMMABLE))
        if key == f"{w}_sapling" or (w == "mangrove" and key == "mangrove_propagule"):
            return add(key, name, st(**plant(tex=ALL(T("cross_plant", "sapling", LEAVES_RGB.get(w, 0x4a8a2a) if w not in LEAVES_TINT else 0x48A52A)))))
        if key == f"{w}_slab": return add(key, name, slab_states(T("planks", PLANKS_RGB[w]), **dict(wood(), flags=fl)), "SLAB")
        if key == f"{w}_stairs": return add(key, name, stairs_states(T("planks", PLANKS_RGB[w]), **dict(wood(), flags=fl)), "STAIRS")
        if key == f"{w}_fence":
            return add(key, name, st(tex=ALL(T("planks", PLANKS_RGB[w])), mesh="CONNECT", shape=[(6, 0, 6, 10, 16, 10), (7, 12, 0, 9, 15, 6), (7, 6, 0, 9, 9, 6)],
                                     opaque=False, opacity=0, flags=F_FENCELIKE | fl, **{k: v for k, v in wood().items() if k != "flags"}))
        if key == f"{w}_fence_gate": return add(key, name, gate_states(T("planks", PLANKS_RGB[w]), **dict(wood(), flags=fl, ui="TOGGLE")), "GATE")
        if key == f"{w}_door":
            return add(key, name, door_states(T("door", PLANKS_RGB[w], "lower"), T("door", PLANKS_RGB[w], "upper"), **dict(wood(3.0), flags=fl, ui="TOGGLE")), "DOOR")
        if key == f"{w}_trapdoor": return add(key, name, trapdoor_states(T("trapdoor", PLANKS_RGB[w]), **dict(wood(3.0), flags=fl, ui="TOGGLE")), "TRAPDOOR")
        if key == f"{w}_button":
            return add(key, name, powered_attach(T("planks", PLANKS_RGB[w]), [(5, 0, 6, 11, 2, 10)], wall_n(BUTTON_WALL_N), [(5, 0, 6, 11, 1, 10)], wall_n(BUTTON_PRESSED_N), solid=False, hardness=0.5, tool="AXE", sound="WOOD", ceiling=True), "ATTACH")
        if key == f"{w}_pressure_plate":
            return add(key, name, plate_states(ALL(T("planks", PLANKS_RGB[w])), hardness=0.5, tool="AXE", sound="WOOD"))
        if key == f"{w}_sign": return add(key, name, sign_states(T("planks", PLANKS_RGB[w]), **dict(wood(1.0), solid=False)), "SIGN")
        if key == f"{w}_hanging_sign":
            col = STRIPPED_RGB[w]
            return add(key, name, [st(tex=ALL(T("planks", col)), shape=rot(HSIGN_N, k), solid=False, **{k2: v for k2, v in wood(1.0).items()}) for k in range(4)], "FACING4")
        if key == f"{w}_shelf":
            return add(key, name, [st(tex=ALL(T("shelf", PLANKS_RGB[w])), shape=rot(SHELF_N, (k + 2) % 4), **wood(2.0)) for k in range(4)], "FACING4")
    if key == "bamboo_mosaic": return add(key, name, st(tex=ALL(T("tiles", PLANKS_RGB["bamboo"])), **wood()))
    if key in ("azalea_leaves", "flowering_azalea_leaves"):
        return add(key, name, st(tex=ALL(T("leaves", LEAVES_RGB["azalea"], 0xd070b0 if key.startswith("flowering") else None)), layer="CUTOUT",
                                 opaque=False, opacity=1, hardness=0.2, tool="HOE", sound="GRASS", drop="nothing", flags=F_LEAVES | F_FLAMMABLE))
    # ---------------- stairs / slabs / walls (stone, copper, misc)
    if key.endswith("_stairs"):
        m = mat_of(key[:-7]); return add(key, name, stairs_states(mat_tex(m), **mat_props(m)), "STAIRS")
    if key.endswith("_slab"):
        m = mat_of(key[:-5])
        if key == "smooth_stone_slab": return add(key, name, slab_states([T("polished", 0x9E9E9E)] * 6, **stone(0, 2.0)), "SLAB")
        return add(key, name, slab_states(mat_tex(m), **mat_props(m)), "SLAB")
    if key.endswith("_wall"):
        m = mat_of(key[:-5])
        return add(key, name, st(tex=ALL(mat_tex(m)), mesh="CONNECT", shape=[(4, 0, 4, 12, 16, 12), (5, 0, 0, 11, 14, 8)], opaque=False, opacity=0,
                                 flags=F_FENCELIKE, **mat_props(m)))
    # ---------------- colours
    for d in DYES:
        if key == f"{d}_wool": return cube(key, name, T("wool", DYE_RGB[d]), hardness=0.8, tool="SHEARS", sound="WOOL", flags=F_FLAMMABLE)
        if key == f"{d}_carpet": return add(key, name, st(tex=ALL(T("wool", DYE_RGB[d])), shape=CARPET, hardness=0.1, sound="WOOL", support="ANY", solid=True))
        if key == f"{d}_concrete": return cube(key, name, T("concrete", DYE_RGB[d]), **stone(0, 1.8))
        if key == f"{d}_concrete_powder": return cube(key, name, T("powder", DYE_RGB[d]), hardness=0.5, tool="SHOVEL", sound="SAND", falls=True)
        if key == f"{d}_terracotta": return cube(key, name, T("terracotta", TERRACOTTA_RGB[d]), **stone(0, 1.25))
        if key == f"{d}_glazed_terracotta":
            return add(key, name, facing_states(T("glazed", DYE_RGB[d]), T("glazed", DYE_RGB[d]), T("glazed", DYE_RGB[d]), **stone(0, 1.4)), "FACING4")
        if key == f"{d}_stained_glass": return cube(key, name, T("glass", DYE_RGB[d], 120, shade_hex(DYE_RGB[d], 1.2)), layer="TRANSLUCENT", opaque=False, opacity=0, hardness=0.3, sound="GLASS", drop="nothing")
        if key == f"{d}_stained_glass_pane":
            return add(key, name, st(tex=ALL(T("glass", DYE_RGB[d], 120, shade_hex(DYE_RGB[d], 1.2))), mesh="CONNECT", shape=[(7, 0, 7, 9, 16, 9), (7, 0, 0, 9, 16, 7)],
                                     layer="TRANSLUCENT", opaque=False, opacity=0, hardness=0.3, sound="GLASS", drop="nothing", flags=F_FENCELIKE))
        if key == f"{d}_shulker_box": return add(key, name, st(tex=ALL(T("chest", DYE_RGB[d], "top")), hardness=2, tool="PICKAXE", be="SHULKER", ui="CHEST", opaque=False, opacity=0))
        if key == f"{d}_bed": return add(key, name, bed_states(DYE_RGB[d]), "BED")
        if key == f"{d}_candle": return add(key, name, st(tex=ALL(T("wool", DYE_RGB[d])), shape=CANDLE, hardness=0.1, sound="WOOL", support="SOLID", emit=(3, 3, 2)))
        if key == f"{d}_banner": return add(key, name, [st(tex=ALL(T("wool", DYE_RGB[d])), shape=rot(BANNER, k), solid=False, hardness=1, tool="AXE", sound="WOOD", be="BANNER", drop="nothing") for k in range(4)], "FACING4")
    # ---------------- copper
    if ("copper" in key or "lightning_rod" in key) and not key.endswith("_ore") and key not in ("raw_copper_block",):
        r = copper_block(key, name)
        if r: return r
    # ---------------- explicit table
    f = SPECIAL.get(key)
    if f: return f(key, name)
    # ---------------- plain cubes by material table
    if key in STONE_RGB:
        m = mat_tex(key)
        props = mat_props(key)
        if key.endswith("pillar") or key in ("basalt", "polished_basalt", "quartz_pillar", "purpur_pillar"):
            side = T("pillar_side", STONE_RGB[key]) if key != "basalt" else T("basalt_side", STONE_RGB[key])
            return add(key, name, axis_states(side, T("polished", STONE_RGB[key]), **props), "AXIS")
        if key.startswith("cracked_"): m = T("cracked", STONE_RGB[key], "bricks" if "bricks" in key else "tiles")
        return cube(key, name, m, **props)
    if key.endswith("_ore") or key == "ancient_debris": return ore_block(key, name)
    if key.endswith("_coral_block"):
        col = CORAL_RGB[key.replace("dead_", "").split("_coral")[0]]
        if key.startswith("dead_"): col = 0x847C77
        return cube(key, name, T("coral_block", col), **stone(0, 1.5, 1, drop=key if key.startswith("dead_") else "dead_" + key))
    if key.endswith("_coral") or key.endswith("_coral_fan"):
        col = CORAL_RGB[key.replace("dead_", "").split("_coral")[0]]
        if key.startswith("dead_"): col = 0x847C77
        return add(key, name, st(**plant(tex=ALL(T("cross_plant", "fan" if key.endswith("fan") else "coral", col)), support="SOLID", drop="nothing", flags=0)))
    if key.startswith("infested_"):
        base = key[len("infested_"):]
        t = STONE_T if base == "stone" else mat_tex(base if base != "deepslate" else "deepslate")
        return cube(key, name, t, hardness=0.75, tool="PICKAXE", drop="nothing", flags=creative)
    if key.endswith("_head") or key.endswith("_skull"):
        cols = {"skeleton_skull": (0xC8C8C0, 0x303030), "wither_skeleton_skull": (0x353535, 0x101010), "zombie_head": (0x4A7A3A, 0x1a2a1a),
                "creeper_head": (0x5AB04A, 0x101010), "dragon_head": (0x1A1A1A, 0x8a2aaa), "piglin_head": (0xE8A090, 0x502020), "player_head": (0x9A6A4A, 0x3a2a7a)}[key]
        t = ALL(T("head", cols[0], None)); t[5] = T("head", cols[0], cols[1])
        return add(key, name, [st(tex=t, shape=HEAD, hardness=1, sound="STONE")], "SIMPLE")
    # ---------------- fallback: generic cube
    print("  [blocks] generic cube for", key)
    return cube(key, name, T("stone", 0x8a6aaa), hardness=1.0, tool="PICKAXE", flags=creative)

CORAL_RGB = {"tube": 0x3253D3, "brain": 0xCF5BA0, "bubble": 0xA118A0, "fire": 0xA3232E, "horn": 0xD8C842}

def sign_states(tex, **kw):
    out = []
    for k in range(4): out.append(st(tex=ALL(tex), shape=rot(SIGN_N, k), **kw))
    for k in range(4): out.append(st(tex=ALL(tex), shape=rot(wall_n(SIGN_WALL_N), k), **kw))
    return out

def bed_states(col):
    out = []
    for k in range(4):
        for head in (0, 1):
            top = T("bed", col, "head_top" if head else "foot_top")
            side = T("bed", col, "side")
            t = [side, side, T("planks", 0x8a6a40), top, side, side]
            out.append(st(tex=t, shape=rot(BED_HEAD_N if head else BED_FOOT_N, k), hardness=0.2, sound="WOOL", ui="BED", drop=None if not head else "nothing",
                          flags=F_FLAMMABLE))
    return out

ORE_INFO = {  # key: (ore colour, tier, drop, min, max, xp)
    "coal": (0x2a2a2a, 1, "coal", 1, 1, (0, 2)), "iron": (0xD8AF93, 2, "raw_iron", 1, 1, (0, 0)), "copper": (0xE0734F, 2, "raw_copper", 2, 5, (0, 0)),
    "gold": (0xFCEE4B, 3, "raw_gold", 1, 1, (0, 0)), "redstone": (0xFF0000, 3, "redstone", 4, 5, (1, 5)), "emerald": (0x17DD62, 3, "emerald", 1, 1, (3, 7)),
    "lapis": (0x1D47A5, 2, "lapis_lazuli", 4, 9, (2, 5)), "diamond": (0x5DECF5, 3, "diamond", 1, 1, (3, 7)),
}
def ore_block(key, name):
    if key == "ancient_debris":
        return add(key, name, st(tex=TSB(T("polished", 0x654740), T("basalt_side", 0x5E4136), T("polished", 0x654740)), hardness=30, tool="PICKAXE", tier=4, blast=1200, sound="METAL"))
    if key == "nether_gold_ore":
        return cube(key, name, T("ore", 0x6F3535, 0xFCEE4B), hardness=3, tool="PICKAXE", tier=1, drop="gold_nugget", drop_min=2, drop_max=6, xp=(0, 1))
    if key == "nether_quartz_ore":
        return cube(key, name, T("ore", 0x6F3535, 0xEAE4DA), hardness=3, tool="PICKAXE", tier=1, drop="quartz", xp=(2, 5))
    deep = key.startswith("deepslate_")
    o = key.replace("deepslate_", "").replace("_ore", "")
    col, tier, drop, mn, mx, xp = ORE_INFO[o]
    emit = (9, 0, 0) if o == "redstone" else (0, 0, 0)
    return cube(key, name, T("ore", 0x505052 if deep else 0x7D7D7D, col, deep), hardness=4.5 if deep else 3, tool="PICKAXE", tier=tier,
                drop=drop, drop_min=mn, drop_max=mx, xp=xp)

def copper_block(key, name):
    stg = copper_stage(key)
    col = COPPER_RGB[stg]
    base = key.replace("waxed_", "").replace(stg, "")
    props = dict(hardness=3, tool="PICKAXE", tier=2, sound="METAL")
    if base in ("copper_block", "copper"): return cube(key, name, T("metal", col), **props)
    if base == "chiseled_copper": return cube(key, name, T("chiseled", col), **props)
    if base == "cut_copper": return cube(key, name, mat_tex(key), **props)
    if base == "copper_grate": return cube(key, name, T("bars", col), layer="CUTOUT", opaque=False, opacity=0, **props)
    if base == "copper_bulb": return cube(key, name, T("froglight", shade_hex(col, 1.2)), emit={"": (15, 13, 9), "exposed_": (12, 10, 7), "weathered_": (8, 7, 5), "oxidized_": (4, 4, 3)}[stg], **props)
    if base == "copper_door": return add(key, name, door_states(T("door", col, "lower", "iron"), T("door", col, "upper", "iron"), ui="TOGGLE", **props), "DOOR")
    if base == "copper_trapdoor": return add(key, name, trapdoor_states(T("trapdoor", col, "iron"), ui="TOGGLE", **props), "TRAPDOOR")
    if base == "copper_bars":
        return add(key, name, st(tex=ALL(T("bars", col)), mesh="CONNECT", shape=[(7, 0, 7, 9, 16, 9), (7, 0, 0, 9, 16, 7)], layer="CUTOUT", opaque=False, opacity=0, flags=F_FENCELIKE, **props))
    if base == "copper_chain": return add(key, name, axis_chain(T("chain", col), props), "AXIS")
    if base == "copper_lantern": return add(key, name, lantern_states(col, 0x9AF07A, (8, 15, 10)), "HANGING")
    if base == "lightning_rod": return add(key, name, rod_states(T("metal", col), props), "FACING6")
    if base == "copper_chest": return add(key, name, chest_states(col, props, "CHEST"), "FACING4")
    if base == "copper_golem_statue": return add(key, name, [st(tex=ALL(T("metal", col)), shape=rot(STATUE, k), **props) for k in range(4)], "FACING4")
    if base == "copper_torch": return None
    return None

def axis_chain(tex, props):
    out = []
    for ax in range(3):
        if ax == 0: shp = CHAIN
        elif ax == 1: shp = [(0, 7, 7, 16, 9, 9)]
        else: shp = [(7, 7, 0, 9, 9, 16)]
        out.append(st(tex=ALL(tex), shape=shp, layer="CUTOUT", **props))
    return out

def rod_states(tex, props, shape=ROD):
    out = []
    for f in range(6):  # W E D U N S : direction the rod points
        if f in (2, 3): shp = shape
        elif f in (0, 1): shp = [(0, shape[0][0], shape[0][2], 16, shape[0][3], shape[0][5])]
        else: shp = [(shape[0][0], shape[0][2], 0, shape[0][3], shape[0][5], 16)]
        out.append(st(tex=ALL(tex), shape=shp, **props))
    return out

def lantern_states(frame_col, glow, emit):
    t = ALL(T("lantern_like", 0x404048, glow))
    return [st(tex=t, shape=LANTERN, hardness=3.5, tool="PICKAXE", sound="METAL", emit=emit, support="ANY", layer="CUTOUT"),
            st(tex=t, shape=LANTERN_H, hardness=3.5, tool="PICKAXE", sound="METAL", emit=emit, support="CEILING", layer="CUTOUT")]

def six_tex(front, back, side, f):
    t = [side] * 6
    t[f] = front; t[[1, 0, 3, 2, 5, 4][f]] = back
    return t
PISTON_BASE_N = [(0, 0, 4, 16, 16, 16)]
def piston_states(sticky):
    """12 states: var = extended * 6 + facing (face index the head points to)."""
    out = []
    for ext in range(2):
        for f in range(6):
            top = T("piston_top", sticky)
            t = six_tex(top if not ext else T("polished", 0x6a6a6a), T("cobble", 0x6f6f6f), T("piston_side"), f)
            kw = dict(hardness=1.5, tool="PICKAXE", drop="sticky_piston" if sticky else "piston")
            if ext: out.append(st(tex=t, shape=face6(PISTON_BASE_N, f), **kw))
            else: out.append(st(tex=t, **kw))
    return out
def chest_states(col, props, be, ui="CHEST", tex=None):
    out = []
    for k, f in enumerate(FACING4):
        t = [T("chest", col, "side")] * 6
        t = list(t); t[3] = t[2] = T("chest", col, "top"); t[FACE_OF[f]] = T("chest", col, "front")
        out.append(st(tex=t, shape=CHEST, be=be, ui=ui, **props))
    return out

def furnace_like(key, name, front, front_lit, side, top, be, emit_lit, props):
    return add(key, name, facing_states(front, side, top, nsub=2, sub_front=front_lit, be=be, ui="FURNACE", emit_lit=emit_lit, **props), "FACING4")

def simple(tex, **kw): return lambda k, n: cube(k, n, tex, **kw)
def P(fn): return fn

SPECIAL = {}
# --------------------------------------------------------------- Sift (heaven) blocks
HOLY = 0xE6E1CC
SIFT_BLOCKS = {
    "cloud_block": lambda k, n: cube(k, n, T("powder", 0xF6F9FF), hardness=0.3, tool="SHOVEL", sound="WOOL"),
    "holy_stone": lambda k, n: cube(k, n, T("stone", HOLY, 0.12), **stone(0, 1.5)),
    "holy_stone_bricks": lambda k, n: cube(k, n, T("bricks", 0xEEE8D2, 0xB8AE90, 8, 4), **stone(0, 2.0)),
    "golden_grass_block": lambda k, n: cube(k, n, TSB(T("grass_top", 0xE8C440), T("grass_side", HOLY, 0xE8C440, False), T("stone", HOLY, 0.12)),
                                            **soil(0.6, sound="GRASS")),
    "crystal_log": lambda k, n: add(k, n, axis_states(T("log_side", 0xA8DCEE), T("log_top", 0xA8DCEE, 0xE0F6FF), **wood(emit=(3, 5, 6))), "AXIS"),
    "crystal_planks": lambda k, n: cube(k, n, T("planks", 0xC8ECF6), **wood()),
    "crystal_leaves": lambda k, n: add(k, n, st(tex=ALL(T("leaves", 0x9AD8F0, 0xF0FAFF)), layer="CUTOUT", opaque=False, opacity=1, hardness=0.2,
                                               tool="HOE", sound="GLASS", drop="nothing", emit=(4, 8, 10), flags=F_LEAVES)),
    "radiant_crystal": lambda k, n: cube(k, n, T("glowstone", 0xB8ECFF), hardness=0.3, sound="GLASS", emit=(12, 14, 15)),
    # Underground City (the Delvari): dark blue-grey stone with amber lamps and glowing runes
    "delvari_bricks": lambda k, n: cube(k, n, T("bricks", 0x343B52, 0x1E2232, 8, 4), **stone(0, 3.0)),
    "delvari_tiles": lambda k, n: cube(k, n, T("bricks", 0x42495E, 0x2A2F3E, 4, 4), **stone(0, 3.0)),
    "runic_delvari_bricks": lambda k, n: cube(k, n, T("bricks", 0x343B52, 0x5AD0F0, 8, 4), **stone(0, 3.0, emit=(2, 6, 8))),
    "delvari_pillar": lambda k, n: cube(k, n, T("stone", 0x4A526A, 0.08), **stone(0, 3.0)),
    "umbral_lamp": lambda k, n: cube(k, n, T("glowstone", 0xF0B860), hardness=0.3, sound="GLASS", emit=(15, 14, 10)),
    # Space: a pale bloom that grows on asteroids and breathes out oxygen (refills air nearby)
    "oxygen_bloom": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "flower", 0x8AE8F8)), support="SOLID", emit=(4, 9, 12), flags=0))),
}
def sp(*keys):
    def deco(fn):
        for k in keys: SPECIAL[k] = fn
        return fn
    return deco

SPECIAL.update(SIFT_BLOCKS)
# --------------------------------------------------------------- natural
SPECIAL.update({
    "grass_block": lambda k, n: add(k, n, [st(tex=TSB(GRASS_TOP, T("grass_side", 0x866043, 0x9a9a9a), DIRT), **soil(0.6, drop="dirt", sound="GRASS"), tint="GRASS", tint_faces=0b111011),
                                           st(tex=TSB(T("flat", 0xF4FAFB, 0.03), T("snow_side"), DIRT), **soil(0.6, drop="dirt", sound="SNOW"))]),
    "dirt": simple(DIRT, **soil()),
    "coarse_dirt": simple(T("gravel", 0x77553B), **soil()),
    "podzol": lambda k, n: cube(k, n, TSB(T("dirt", 0x5A3D1A), T("grass_side", 0x866043, 0x6A4A20), DIRT), **soil(drop="dirt")),
    "rooted_dirt": simple(T("dirt", 0x90684A), **soil()),
    "mud": simple(T("dirt", 0x3C3A3D), **soil()),
    "mycelium": lambda k, n: cube(k, n, TSB(T("grass_top", 0x6F6265), T("grass_side", 0x866043, 0x6F6265), DIRT), **soil(0.6, drop="dirt", sound="GRASS")),
    "dirt_path": lambda k, n: add(k, n, st(tex=TSB(T("dirt", 0x94793F), T("grass_side", 0x866043, 0x94793F), DIRT), shape=FARMLAND, keep_opacity=True, **soil(0.65, drop="dirt"))),
    "farmland": lambda k, n: add(k, n, [st(tex=TSB(T("dirt", 0x5A3A20), DIRT, DIRT), shape=FARMLAND, keep_opacity=True, **soil(0.6, drop="dirt")),
                                        st(tex=TSB(T("dirt", 0x3A2410), DIRT, DIRT), shape=FARMLAND, keep_opacity=True, **soil(0.6, drop="dirt"))]),
    "crimson_nylium": lambda k, n: cube(k, n, TSB(T("nylium_top", 0x8A1A1A), T("grass_side", 0x6F3535, 0x8A1A1A), T("netherrack", 0x6F3535)), **stone(0, 0.4, 1, drop="netherrack")),
    "warped_nylium": lambda k, n: cube(k, n, TSB(T("nylium_top", 0x2A7A6A), T("grass_side", 0x6F3535, 0x2A7A6A), T("netherrack", 0x6F3535)), **stone(0, 0.4, 1, drop="netherrack")),
    "netherrack": simple(T("netherrack", 0x6F3535), **stone(0, 0.4)),
    "sand": simple(T("sand", 0xDBCFA3), hardness=0.5, tool="SHOVEL", sound="SAND", falls=True),
    "red_sand": simple(T("sand", 0xBE6621), hardness=0.5, tool="SHOVEL", sound="SAND", falls=True),
    "suspicious_sand": simple(T("sand", 0xD3C49A), hardness=0.25, tool="SHOVEL", sound="SAND", falls=True, drop="nothing"),
    "gravel": simple(T("gravel", 0x837F7E), hardness=0.6, tool="SHOVEL", sound="GRAVEL", falls=True),
    "suspicious_gravel": simple(T("gravel", 0x7F7A79), hardness=0.25, tool="SHOVEL", sound="GRAVEL", falls=True, drop="nothing"),
    "clay": simple(T("flat", 0xA1A7B2, 0.05), hardness=0.6, tool="SHOVEL", sound="GRAVEL", drop="clay_ball", drop_min=4, drop_max=4),
    "snow_block": simple(T("flat", 0xF4FAFB, 0.03), hardness=0.2, tool="SHOVEL", sound="SNOW", drop="snowball", drop_min=4, drop_max=4),
    "ice": simple(T("ice"), layer="TRANSLUCENT", opaque=False, opacity=2, hardness=0.5, tool="PICKAXE", sound="GLASS", drop="nothing"),
    "packed_ice": simple(T("ice", 0x8DB4FA, 255), hardness=0.5, tool="PICKAXE", sound="GLASS", drop="nothing"),
    "blue_ice": simple(T("ice", 0x74A8FD, 255), hardness=2.8, tool="PICKAXE", sound="GLASS", drop="nothing"),
    "bedrock": simple(T("stone", 0x555555, 0.35), hardness=-1, flags=F_CREATIVE),
    "obsidian": simple(T("stone", 0x0F0B19, 0.4), hardness=50, tool="PICKAXE", tier=4, blast=1200),
    "crying_obsidian": simple(T("stone", 0x200A3A, 0.5), hardness=50, tool="PICKAXE", tier=4, blast=1200, emit=(7, 2, 10)),
    "soul_sand": simple(T("soul_sand"), hardness=0.5, tool="SHOVEL", sound="SAND"),
    "soul_soil": simple(T("dirt", 0x4B3A2D), hardness=0.5, tool="SHOVEL", sound="SAND"),
    "magma_block": simple(T("lava"), hardness=0.5, tool="PICKAXE", tier=1, emit=(3, 1, 0), touch=1),
    "glowstone": simple(T("glowstone"), hardness=0.3, sound="GLASS", emit=(15, 13, 9), drop="glowstone_dust", drop_min=2, drop_max=4),
    "shroomlight": simple(T("glowstone", 0xF09A4A), hardness=1, tool="HOE", sound="WOOD", emit=(15, 11, 6)),
    "sea_lantern": simple(T("froglight", 0xC8E0D8), hardness=0.3, sound="GLASS", emit=(14, 15, 15), drop="prismarine_crystals", drop_min=2, drop_max=3),
    "ochre_froglight": lambda k, n: add(k, n, axis_states(T("froglight", 0xF5E3A3), T("froglight", 0xF8EEC0), hardness=0.3, sound="WOOD", emit=(15, 14, 9)), "AXIS"),
    "verdant_froglight": lambda k, n: add(k, n, axis_states(T("froglight", 0xD3EFC3), T("froglight", 0xE2F4D8), hardness=0.3, sound="WOOD", emit=(12, 15, 11)), "AXIS"),
    "pearlescent_froglight": lambda k, n: add(k, n, axis_states(T("froglight", 0xF2D9EC), T("froglight", 0xF6E6F1), hardness=0.3, sound="WOOD", emit=(15, 12, 15)), "AXIS"),
    "sponge": simple(T("sponge", 0xC7C34E), hardness=0.6, tool="HOE", sound="GRASS"),
    "wet_sponge": simple(T("sponge", 0xA5A136), hardness=0.6, tool="HOE", sound="GRASS"),
    "moss_block": simple(T("grass_top", 0x596E2D), hardness=0.1, tool="HOE", sound="GRASS"),
    "pale_moss_block": simple(T("grass_top", 0x8A918A), hardness=0.1, tool="HOE", sound="GRASS"),
    "moss_carpet": lambda k, n: add(k, n, st(tex=ALL(T("grass_top", 0x596E2D)), shape=CARPET, hardness=0.1, sound="GRASS", support="ANY")),
    "pale_moss_carpet": lambda k, n: add(k, n, st(tex=ALL(T("grass_top", 0x8A918A)), shape=CARPET, hardness=0.1, sound="GRASS", support="ANY")),
    "packed_mud": simple(T("dirt", 0x8E6B50), hardness=1.0, tool="PICKAXE", sound="GRAVEL"),
    "dried_kelp_block": simple(T("grass_top", 0x3A4A2A), hardness=0.5, tool="HOE", sound="GRASS", flags=F_FLAMMABLE),
    "hay_block": lambda k, n: add(k, n, axis_states(T("hay", True), T("hay", False), hardness=0.5, tool="HOE", sound="GRASS", flags=F_FLAMMABLE), "AXIS"),
    "bone_block": lambda k, n: add(k, n, axis_states(T("pillar_side", 0xE5E1CF), T("polished", 0xD8D4C0), hardness=2, tool="PICKAXE", tier=1), "AXIS"),
    "amethyst_block": simple(T("stone", 0x8562C1, 0.2), hardness=1.5, tool="PICKAXE", sound="GLASS"),
    "budding_amethyst": simple(T("stone", 0x9A72D1, 0.25), hardness=1.5, tool="PICKAXE", sound="GLASS", drop="nothing"),
    "honeycomb_block": simple(T("honeycomb", 0xE5941E), hardness=0.6, sound="WOOL"),
    "honey_block": lambda k, n: add(k, n, st(tex=ALL(T("glass", 0xF5B020, 180, 0xF8C040)), shape=HONEY, layer="TRANSLUCENT", hardness=0, sound="WOOL")),
    "slime_block": simple(T("glass", 0x7EBF6E, 200, 0x8ed07e), layer="TRANSLUCENT", opaque=False, opacity=1, hardness=0, sound="WOOL"),
    "sculk": simple(T("sculk"), hardness=0.2, tool="HOE", sound="WOOL", xp=(1, 1)),
    "sculk_catalyst": simple(T("sculk"), hardness=3, tool="HOE", emit=(4, 6, 6), xp=(5, 5)),
    # var 0: placed by a player; var 1: natural, can summon a warden (sculk.c)
    "sculk_shrieker": lambda k, n: add(k, n, [st(tex=ALL(T("sculk")), shape=[(0, 0, 0, 16, 8, 16)], hardness=3, tool="HOE", xp=(5, 5), drop="sculk_shrieker") for _ in range(2)]),
    # var = redstone output power (0 = listening); active sensors glow brighter
    "sculk_sensor": lambda k, n: add(k, n, [st(tex=ALL(T("sculk")), shape=[(0, 0, 0, 16, 8, 16)], hardness=1.5, tool="HOE", emit=(1, 1, 1) if i == 0 else (3, 9, 10), xp=(1, 1), drop="sculk_sensor") for i in range(16)]),
    "calibrated_sculk_sensor": lambda k, n: add(k, n, [st(tex=ALL(T("sculk")), shape=[(0, 0, 0, 16, 8, 16)], hardness=1.5, tool="HOE", emit=(1, 1, 1) if i == 0 else (3, 9, 10), xp=(1, 1), drop="calibrated_sculk_sensor") for i in range(16)]),
    "sculk_vein": lambda k, n: add(k, n, st(tex=ALL(T("sculk")), shape=CARPET, hardness=0.2, tool="HOE", solid=False, replaceable=True, support="ANY")),
    "glow_lichen": lambda k, n: add(k, n, st(tex=ALL(T("cross_plant", "vine", 0x7F9A8A)), shape=CARPET, solid=False, replaceable=True, hardness=0.2, sound="GRASS", emit=(7, 7, 6), support="ANY", layer="CUTOUT")),
    "resin_clump": lambda k, n: add(k, n, st(tex=ALL(T("flat", 0xD9631B)), shape=CARPET, solid=False, hardness=0, sound="GRASS")),
    "melon": lambda k, n: cube(k, n, TSB(T("melon", False), T("melon", True), T("melon", False)), hardness=1, tool="AXE", sound="WOOD", drop="melon_slice", drop_min=3, drop_max=7),
    "pumpkin": lambda k, n: cube(k, n, TSB(T("pumpkin", "top"), T("pumpkin", "side"), T("pumpkin", "top")), hardness=1, tool="AXE", sound="WOOD"),
    "carved_pumpkin": lambda k, n: add(k, n, facing_states(T("pumpkin", "carved"), T("pumpkin", "side"), T("pumpkin", "top"), hardness=1, tool="AXE", sound="WOOD"), "FACING4"),
    "jack_o_lantern": lambda k, n: add(k, n, facing_states(T("pumpkin", "lit"), T("pumpkin", "side"), T("pumpkin", "top"), hardness=1, tool="AXE", sound="WOOD", emit=(15, 12, 6)), "FACING4"),
    "cactus": lambda k, n: add(k, n, st(mesh="CACTUS", layer="CUTOUT", tex=TSB(T("cactus", "top"), T("cactus", "side"), T("cactus", "top")), opaque=False, opacity=0, hardness=0.4, sound="WOOL", support="SAND", touch=1)),
    "cobweb": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "vine", 0xE8E8E8)), support="NONE", tool="SWORD", hardness=4, drop="string", solid=False, flags=0))),
    "brown_mushroom_block": simple(T("mushroom_block", 0x957051), hardness=0.2, tool="AXE", sound="WOOD", drop="brown_mushroom", drop_min=0, drop_max=2),
    "red_mushroom_block": simple(T("mushroom_block", 0xC52C2A, 0xE8E0D0), hardness=0.2, tool="AXE", sound="WOOD", drop="red_mushroom", drop_min=0, drop_max=2),
    "mushroom_stem": simple(T("mushroom_stem"), hardness=0.2, tool="AXE", sound="WOOD", drop="nothing"),
    "nether_wart_block": simple(T("mushroom_block", 0x7A0A0A), hardness=1, tool="HOE", sound="WOOD"),
    "warped_wart_block": simple(T("mushroom_block", 0x167E86), hardness=1, tool="HOE", sound="WOOD"),
    "end_stone": simple(T("sand", 0xDBDE9E), hardness=3, tool="PICKAXE", tier=1, blast=9),
    "end_portal_frame": lambda k, n: add(k, n, [st(tex=TSB(T("chiseled", 0x3A6A5A), T("polished", 0xD8DCA0), T("sand", 0xDBDE9E)), shape=END_PORTAL_FRAME, hardness=-1, ui="FRAME_EYE", emit=(0, 0, 0), flags=F_CREATIVE),
                                                st(tex=TSB(T("chiseled", 0x2A8A5A), T("polished", 0xD8DCA0), T("sand", 0xDBDE9E)), shape=END_PORTAL_FRAME_EYE, hardness=-1, emit=(1, 1, 1), flags=F_CREATIVE)]),
    "dragon_egg": lambda k, n: add(k, n, st(tex=ALL(T("dragon_egg")), shape=DRAGON_EGG, hardness=3, emit=(1, 0, 1), falls=True)),
    "chorus_plant": lambda k, n: add(k, n, st(tex=ALL(T("stone", 0x5E3A5E, 0.2)), shape=CHORUS, hardness=0.4, tool="AXE", sound="WOOD", drop="chorus_fruit", drop_min=0, drop_max=1)),
    "chorus_flower": lambda k, n: add(k, n, st(tex=ALL(T("stone", 0x8A6A9A, 0.2)), shape=CHORUS, hardness=0.4, tool="AXE", sound="WOOD")),
    "spawner": simple(T("spawner"), layer="CUTOUT", opaque=False, opacity=1, hardness=5, tool="PICKAXE", drop="nothing", xp=(15, 43), flags=F_CREATIVE, be="SPAWNER"),
    "trial_spawner": simple(T("spawner"), layer="CUTOUT", opaque=False, opacity=1, hardness=50, tool="PICKAXE", drop="nothing", flags=F_CREATIVE, be="SPAWNER"),
    "vault": simple(T("spawner"), layer="CUTOUT", opaque=False, opacity=1, hardness=50, tool="PICKAXE", drop="nothing", flags=F_CREATIVE),
    "reinforced_deepslate": simple(T("deepslate", 0x505353), hardness=55, blast=1200, drop="nothing", flags=F_CREATIVE),
    "barrier": simple(T("barrier"), layer="CUTOUT", opaque=False, opacity=0, hardness=-1, drop="nothing", flags=F_CREATIVE),
    "light": lambda k, n: add(k, n, st(mesh="NONE", solid=False, opaque=False, opacity=0, replaceable=True, emit=(15, 15, 15), hardness=-1, flags=F_CREATIVE)),
    "structure_void": lambda k, n: add(k, n, st(mesh="NONE", solid=False, opaque=False, opacity=0, replaceable=True, hardness=-1, flags=F_CREATIVE)),
    "command_block": simple(T("command_block", 0xC08A5A), hardness=-1, flags=F_CREATIVE),
    "repeating_command_block": simple(T("command_block", 0x7A5AC0), hardness=-1, flags=F_CREATIVE),
    "chain_command_block": simple(T("command_block", 0x5AC08A), hardness=-1, flags=F_CREATIVE),
    "structure_block": simple(T("command_block", 0x5A4A6A), hardness=-1, flags=F_CREATIVE),
    "jigsaw": simple(T("command_block", 0x6A5A7A), hardness=-1, flags=F_CREATIVE),
    "test_block": simple(T("command_block", 0x4A6A9A), hardness=-1, flags=F_CREATIVE),
    "test_instance_block": simple(T("command_block", 0x6A9A4A), hardness=-1, flags=F_CREATIVE),
    "heavy_core": lambda k, n: add(k, n, st(tex=ALL(T("metal", 0x4A4A55)), shape=[(4, 0, 4, 12, 8, 12)], hardness=10, tool="PICKAXE", blast=1200)),
    "creaking_heart": lambda k, n: add(k, n, axis_states(T("log_side", 0x6F6560), T("glowstone", 0xE07A2A), **wood(10)), "AXIS"),
    "dried_ghast": lambda k, n: add(k, n, st(tex=ALL(T("flat", 0xC8C0B8)), shape=[(3, 0, 3, 13, 10, 13)], hardness=0, sound="WOOL")),
    "frogspawn": lambda k, n: add(k, n, st(tex=ALL(T("glass", 0x3A3A3A, 160, 0x5a5a5a)), shape=[(0, 0, 0, 16, 1, 16)], solid=False, hardness=0, drop="nothing", support="WATER", flags=F_CREATIVE)),
    "turtle_egg": lambda k, n: add(k, n, st(tex=ALL(T("flat", 0xE8E4C8)), shape=EGG, hardness=0.5, drop="nothing", support="SAND")),
    "mangrove_roots": simple(T("log_side", 0x4A3A26), layer="CUTOUT", opaque=False, opacity=1, **wood(0.7)),
    "muddy_mangrove_roots": lambda k, n: add(k, n, axis_states(T("dirt", 0x3C3A3D), T("log_side", 0x4A3A26), hardness=0.7, tool="SHOVEL", sound="GRAVEL"), "AXIS"),
    "sniffer_egg": lambda k, n: add(k, n, st(tex=ALL(T("terracotta", 0xA03A2A)), shape=SNIFFER_EGG, hardness=0.5)),
})

# --------------------------------------------------------------- plants
def flower(col, kind="flower"): return lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", kind, col)))))
for fk, col in {"dandelion": 0xF5D020, "poppy": 0xD02020, "blue_orchid": 0x30A0E0, "allium": 0xB060E0, "azure_bluet": 0xE8E8F0,
                "oxeye_daisy": 0xF0F0F0, "cornflower": 0x4060E0, "lily_of_the_valley": 0xFAFAFA, "wither_rose": 0x2A2A2A,
                "torchflower": 0xF08A20, "open_eyeblossom": 0xF0A030, "closed_eyeblossom": 0x8A8A8A, "cactus_flower": 0xF07AA0}.items():
    SPECIAL[fk] = flower(col)
for fk, col in {"red_tulip": 0xD03030, "orange_tulip": 0xF08030, "white_tulip": 0xF0F0F0, "pink_tulip": 0xF0A0C0}.items():
    SPECIAL[fk] = flower(col, "tulip")
SPECIAL.update({
    "short_grass": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0x9a9a9a)), replaceable=True, drop="nothing", tint="GRASS", tint_faces=63))),
    "fern": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "fern", 0x9a9a9a)), replaceable=True, drop="nothing", tint="GRASS", tint_faces=63))),
    "bush": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "bush", 0x9a9a9a)), replaceable=True, tint="GRASS", tint_faces=63))),
    "firefly_bush": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "bush", 0x5A7A3A, 0xF0E060)), emit=(2, 2, 1)))),
    "short_dry_grass": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0xC8A86A)), replaceable=True, support="SAND", drop="nothing"))),
    "tall_dry_grass": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0xB8985A)), replaceable=True, support="SAND", drop="nothing"))),
    "dead_bush": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "dead", 0x8A6A3A)), replaceable=True, support="SAND", drop="stick", drop_min=0, drop_max=2))),
    "azalea": lambda k, n: add(k, n, st(tex=TSB(T("leaves", 0x5E7C2A), T("leaves", 0x5E7C2A), T("planks", 0x6A4A2A)), shape=[(0, 8, 0, 16, 16, 16), (6, 0, 6, 10, 8, 10)], layer="CUTOUT", hardness=0, sound="GRASS", support="SOIL")),
    "flowering_azalea": lambda k, n: add(k, n, st(tex=TSB(T("leaves", 0x5E7C2A, 0xd070b0), T("leaves", 0x5E7C2A, 0xd070b0), T("planks", 0x6A4A2A)), shape=[(0, 8, 0, 16, 16, 16), (6, 0, 6, 10, 8, 10)], layer="CUTOUT", hardness=0, sound="GRASS", support="SOIL")),
    "seagrass": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0x3A8A3A)), support="SOLID", flags=F_WATERLOGGED, drop="nothing"))),
    "sea_pickle": lambda k, n: add(k, n, st(tex=ALL(T("flat", 0x6A8A2A)), shape=PICKLE, hardness=0, sound="WOOL", emit=(6, 7, 4), support="SOLID")),
    "kelp": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "stalk", 0x4A8A2A)), support="STACK", flags=F_WATERLOGGED))),
    "sugar_cane": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "stalk", 0x9a9a9a)), support="STACK", tint="GRASS", tint_faces=63))),
    "bamboo": lambda k, n: add(k, n, st(tex=ALL(T("log_side", 0x5A8A2A)), shape=[(6, 0, 6, 10, 16, 10)], hardness=1, tool="AXE", sound="WOOD", support="STACK", flags=F_FLAMMABLE)),
    "brown_mushroom": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "mushroom", 0x957051)), support="SOLID", emit=(1, 1, 1)))),
    "red_mushroom": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "mushroom", 0xD02A28)), support="SOLID"))),
    "crimson_fungus": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "mushroom", 0xA52A2A, 0xE0A040)), support="NYLIUM"))),
    "warped_fungus": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "mushroom", 0x2AA595, 0xE07A2A)), support="NYLIUM"))),
    "crimson_roots": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0xA52A3A)), support="NYLIUM", replaceable=True))),
    "warped_roots": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "grass", 0x2AA595)), support="NYLIUM", replaceable=True))),
    "nether_sprouts": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "sprouts", 0x2AA595)), support="NYLIUM", replaceable=True, drop="nothing"))),
    "weeping_vines": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "hanging", 0xA52A2A)), support="CEILING", flags=F_CLIMBABLE))),
    "twisting_vines": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "hanging", 0x2AA595)), support="STACK", flags=F_CLIMBABLE))),
    "hanging_roots": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "hanging", 0xA88A6A)), support="CEILING"))),
    "pale_hanging_moss": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "hanging", 0x8A918A)), support="CEILING"))),
    "spore_blossom": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "flower", 0xE070B0)), support="CEILING"))),
    "pink_petals": lambda k, n: add(k, n, st(tex=ALL(T("cross_plant", "vine", 0xF0A0C0)), shape=CARPET, solid=False, hardness=0, sound="GRASS", support="SOIL", layer="CUTOUT", replaceable=True)),
    "wildflowers": lambda k, n: add(k, n, st(tex=ALL(T("cross_plant", "vine", 0xF0E060)), shape=CARPET, solid=False, hardness=0, sound="GRASS", support="SOIL", layer="CUTOUT", replaceable=True)),
    "leaf_litter": lambda k, n: add(k, n, st(tex=ALL(T("cross_plant", "vine", 0x9A7A3A)), shape=CARPET, solid=False, hardness=0, sound="GRASS", support="SOLID", layer="CUTOUT", replaceable=True)),
    "lily_pad": lambda k, n: add(k, n, st(mesh="FLAT", tex=ALL(T("cross_plant", "vine", 0x9a9a9a)), solid=True, hardness=0, sound="GRASS", support="WATER", tint="LILY", tint_faces=63, shape=[(0, 0, 0, 16, 1, 16)])),
    "vine": lambda k, n: add(k, n, [st(tex=ALL(T("cross_plant", "vine", 0x9a9a9a)), shape=rot(wall_n(LADDER_N), k), solid=False, layer="CUTOUT", hardness=0.2, tool="SHEARS", sound="GRASS",
                                       replaceable=True, tint="FOLIAGE", tint_faces=63, flags=F_CLIMBABLE | F_FLAMMABLE, drop="nothing") for k in range(4)], "WALLONLY"),
    "ladder": lambda k, n: add(k, n, [st(tex=ALL(T("ladder", 0x8A6A3A)), shape=rot(wall_n(LADDER_N), k), solid=False, layer="CUTOUT", hardness=0.4, tool="AXE", sound="WOOD", flags=F_CLIMBABLE) for k in range(4)], "WALLONLY"),
    "big_dripleaf": lambda k, n: add(k, n, st(tex=ALL(T("leaves", 0x5A9A2A, None, 1.0)), shape=[(0, 15, 0, 16, 16, 16), (7, 0, 7, 9, 15, 9)], hardness=0.1, tool="AXE", sound="GRASS", support="SOIL")),
    "small_dripleaf": lambda k, n: add(k, n, st(**plant(tex=ALL(T("cross_plant", "fern", 0x5A9A2A)), tool="SHEARS"))),
    "pointed_dripstone": lambda k, n: add(k, n, [st(tex=ALL(T("stone", 0x866B5C)), shape=[(5, 0, 5, 11, 16, 11)], hardness=1.5, tool="PICKAXE", support="SOLID"),
                                                st(tex=ALL(T("stone", 0x866B5C)), shape=[(5, 0, 5, 11, 16, 11)], hardness=1.5, tool="PICKAXE", support="CEILING")], "HANGING"),
    "small_amethyst_bud": lambda k, n: add(k, n, rod_states(T("cross_plant", "cluster", 0xA87ADA), dict(hardness=1.5, tool="PICKAXE", sound="GLASS", emit=(1, 1, 1), drop="nothing"), BUD["small"]), "FACING6"),
    "medium_amethyst_bud": lambda k, n: add(k, n, rod_states(T("cross_plant", "cluster", 0xA87ADA), dict(hardness=1.5, tool="PICKAXE", sound="GLASS", emit=(2, 1, 2), drop="nothing"), BUD["medium"]), "FACING6"),
    "large_amethyst_bud": lambda k, n: add(k, n, rod_states(T("cross_plant", "cluster", 0xA87ADA), dict(hardness=1.5, tool="PICKAXE", sound="GLASS", emit=(4, 2, 4), drop="nothing"), BUD["large"]), "FACING6"),
    "amethyst_cluster": lambda k, n: add(k, n, rod_states(T("cross_plant", "cluster", 0xB88AEA), dict(hardness=1.5, tool="PICKAXE", sound="GLASS", emit=(5, 3, 5), drop="amethyst_shard", drop_min=4, drop_max=4), BUD["cluster"]), "FACING6"),
    "end_rod": lambda k, n: add(k, n, rod_states(T("flat", 0xF4F0F8), dict(hardness=0, sound="WOOD", emit=(14, 14, 15)), [(7, 0, 7, 9, 16, 9)]), "FACING6"),
})
def tall(col, kind="flower", tint=None):
    def f(k, n):
        ex = dict(tint=tint, tint_faces=63) if tint else {}
        return add(k, n, [st(**plant(tex=ALL(T("cross_plant", kind, col)), flags=F_TALL | F_FLAMMABLE, **ex)),
                          st(**plant(tex=ALL(T("cross_plant", kind, col)), support="STACK", flags=F_TALL | F_FLAMMABLE, **ex))], "DOUBLE")
    return f
SPECIAL.update({"sunflower": tall(0xF5D020), "lilac": tall(0xC890D0), "rose_bush": tall(0xD02020, "bush"), "peony": tall(0xF0A8C8),
                "tall_grass": tall(0x9a9a9a, "grass", "GRASS"), "large_fern": tall(0x9a9a9a, "fern", "GRASS"), "pitcher_plant": tall(0x6A8AD0, "tulip"),
                "tall_seagrass": tall(0x3A8A3A, "grass")})

# --------------------------------------------------------------- building / functional
def chest_kind(col, be, ui="CHEST"):
    return lambda k, n: add(k, n, chest_states(col, dict(hardness=2.5, tool="AXE", sound="WOOD", flags=F_FLAMMABLE), be, ui), "FACING4")
SPECIAL.update({
    "glass": simple(T("glass"), layer="CUTOUT", opaque=False, opacity=0, hardness=0.3, sound="GLASS", drop="nothing"),
    "tinted_glass": simple(T("glass", 0x2A2430, 220, 0x3a3440), layer="TRANSLUCENT", opaque=False, opacity=15, hardness=0.3, sound="GLASS"),
    "glass_pane": lambda k, n: add(k, n, st(tex=ALL(T("glass")), mesh="CONNECT", shape=[(7, 0, 7, 9, 16, 9), (7, 0, 0, 9, 16, 7)], layer="CUTOUT", opaque=False, opacity=0, hardness=0.3, sound="GLASS", drop="nothing", flags=F_FENCELIKE)),
    "iron_bars": lambda k, n: add(k, n, st(tex=ALL(T("bars", 0x8A8A8A)), mesh="CONNECT", shape=[(7, 0, 7, 9, 16, 9), (7, 0, 0, 9, 16, 7)], layer="CUTOUT", opaque=False, opacity=0, hardness=5, tool="PICKAXE", sound="METAL", flags=F_FENCELIKE)),
    "nether_brick_fence": lambda k, n: add(k, n, st(tex=ALL(mat_tex("nether_bricks")), mesh="CONNECT", shape=[(6, 0, 6, 10, 16, 10), (7, 12, 0, 9, 15, 6), (7, 6, 0, 9, 9, 6)], opaque=False, opacity=0, flags=F_FENCELIKE, **stone(0, 2))),
    "iron_chain": lambda k, n: add(k, n, axis_chain(T("chain", 0x5A5A6A), dict(hardness=5, tool="PICKAXE", sound="METAL")), "AXIS"),
    "torch": lambda k, n: add(k, n, st(mesh="TORCH", tex=ALL(T("torch")), solid=False, hardness=0, sound="WOOD", support="SOLID", emit=(14, 11, 6))),
    "soul_torch": lambda k, n: add(k, n, st(mesh="TORCH", tex=ALL(T("torch", 0x6b4f2a, 0x70E0F0)), solid=False, hardness=0, sound="WOOD", support="SOLID", emit=(4, 10, 12))),
    "copper_torch": lambda k, n: add(k, n, st(mesh="TORCH", tex=ALL(T("torch", 0x6b4f2a, 0x9AF07A)), solid=False, hardness=0, sound="WOOD", support="SOLID", emit=(10, 14, 8))),
    "redstone_torch": lambda k, n: add(k, n, [st(mesh="TORCH", tex=ALL(T("torch", 0x6b4f2a, 0xFF3020)), solid=False, hardness=0, sound="WOOD", support="SOLID", emit=(7, 1, 0)),
                                             st(mesh="TORCH", tex=ALL(T("torch", 0x6b4f2a, 0x5a2a20)), solid=False, hardness=0, sound="WOOD", support="SOLID", drop="redstone_torch")]),
    "lantern": lambda k, n: add(k, n, lantern_states(0x404048, 0xF8D070, (15, 12, 7)), "HANGING"),
    "soul_lantern": lambda k, n: add(k, n, lantern_states(0x404048, 0x70E0F0, (4, 10, 12)), "HANGING"),
    "crafting_table": simple([T("crafting_side", True), T("crafting_side", False), T("planks", 0xA98654), T("crafting_top"), T("crafting_side", True), T("crafting_side", False)], hardness=2.5, tool="AXE", sound="WOOD", ui="CRAFTING", flags=F_FLAMMABLE),
    "crafter": simple(T("command_block", 0x7A7A7A), hardness=1.5, tool="PICKAXE", ui="CRAFTING"),
    "furnace": lambda k, n: furnace_like(k, n, T("furnace_front"), T("furnace_front", 0x6f6f6f, True), T("cobble", 0x6f6f6f), T("furnace_top"), "FURNACE", (13, 9, 4), dict(hardness=3.5, tool="PICKAXE", tier=1)),
    "blast_furnace": lambda k, n: furnace_like(k, n, T("furnace_front", 0x5A5A60), T("furnace_front", 0x5A5A60, True), T("metal", 0x6A6A70), T("polished", 0x5A5A60), "BLAST", (13, 9, 4), dict(hardness=3.5, tool="PICKAXE", tier=1)),
    "smoker": lambda k, n: furnace_like(k, n, T("furnace_front", 0x5A4A3A), T("furnace_front", 0x5A4A3A, True), T("log_side", 0x6A5A4A), T("polished", 0x5A4A3A), "SMOKER", (13, 9, 4), dict(hardness=3.5, tool="PICKAXE", tier=1)),
    "chest": chest_kind(0xA8782E, "CHEST"),
    "trapped_chest": chest_kind(0xA0702A, "CHEST"),
    "barrel": lambda k, n: add(k, n, facing_states(T("chiseled", 0x8A6A3A), T("planks", 0x7A5A30), T("planks", 0x7A5A30), be="BARREL", ui="CHEST", **wood(2.5)), "FACING4"),
    "ender_chest": lambda k, n: add(k, n, chest_states(0x2A1A4A, dict(hardness=22.5, tool="PICKAXE", tier=1, blast=600, emit=(7, 7, 7), drop="obsidian", drop_min=8, drop_max=8), "ENDER"), "FACING4"),
    "shulker_box": lambda k, n: add(k, n, st(tex=ALL(T("chest", 0x8A5A8A, "top")), hardness=2, tool="PICKAXE", be="SHULKER", ui="CHEST", opaque=False, opacity=0)),
    "bookshelf": simple(TSB(T("planks", 0xB08850), T("bookshelf"), T("planks", 0xB08850)), hardness=1.5, tool="AXE", sound="WOOD", drop="book", drop_min=3, drop_max=3, flags=F_FLAMMABLE),
    "chiseled_bookshelf": lambda k, n: add(k, n, facing_states(T("bookshelf"), T("planks", 0xB08850), T("planks", 0xB08850), **wood(1.5)), "FACING4"),
    "jukebox": simple(TSB(T("jukebox", True), T("jukebox", False), T("jukebox", False)), hardness=2, tool="AXE", sound="WOOD", ui="JUKEBOX", be="JUKEBOX"),
    "note_block": lambda k, n: add(k, n, [st(tex=ALL(T("note_block")), hardness=0.8, tool="AXE", sound="WOOD", ui="NOTE"), st(tex=ALL(T("note_block")), hardness=0.8, tool="AXE", sound="WOOD", ui="NOTE", drop="note_block")]),
    "tnt": simple(TSB(T("tnt", "top"), T("tnt", "side"), T("tnt", "top")), hardness=0, sound="GRASS"),
    "enchanting_table": lambda k, n: add(k, n, st(tex=TSB(T("chiseled", 0x8A2A3A), T("stone", 0x1A1020), T("stone", 0x1A1020)), shape=ENCH, hardness=5, tool="PICKAXE", tier=1, emit=(7, 5, 8), ui="ENCHANT", blast=1200)),
    "anvil": lambda k, n: add(k, n, [st(tex=ALL(T("metal", 0x404040)), shape=rot(ANVIL, k), hardness=5, tool="PICKAXE", tier=1, sound="METAL", falls=True, ui="ANVIL", blast=1200) for k in range(4)], "FACING4"),
    "chipped_anvil": lambda k, n: add(k, n, [st(tex=ALL(T("metal", 0x3A3A3A)), shape=rot(ANVIL, k), hardness=5, tool="PICKAXE", tier=1, sound="METAL", falls=True, ui="ANVIL", blast=1200) for k in range(4)], "FACING4"),
    "damaged_anvil": lambda k, n: add(k, n, [st(tex=ALL(T("metal", 0x343434)), shape=rot(ANVIL, k), hardness=5, tool="PICKAXE", tier=1, sound="METAL", falls=True, ui="ANVIL", blast=1200) for k in range(4)], "FACING4"),
    "beacon": lambda k, n: add(k, n, st(tex=ALL(T("glass", 0x7AF0E0, 200, 0xc0fff8)), layer="CUTOUT", opaque=False, opacity=1, hardness=3, emit=(15, 15, 15), sound="GLASS")),
    "conduit": lambda k, n: add(k, n, st(tex=ALL(T("chiseled", 0xA08A6A)), shape=CONDUIT, hardness=3, tool="PICKAXE", emit=(15, 15, 15))),
    "lodestone": simple(TSB(T("chiseled", 0x8A8A8A), T("metal", 0x6A6A6A), T("polished", 0x6A6A6A)), hardness=3.5, tool="PICKAXE", tier=1),
    "respawn_anchor": simple(TSB(T("glowstone", 0x3A1A5A), T("stone", 0x200A3A, 0.4), T("stone", 0x200A3A, 0.4)), hardness=50, tool="PICKAXE", tier=4, blast=1200, ui="RESPAWN"),
    "brewing_stand": lambda k, n: add(k, n, st(tex=ALL(T("metal", 0x5A5A5A)), shape=BREWING, hardness=0.5, tool="PICKAXE", emit=(1, 1, 1), be="BREWING", ui="BREWING", layer="CUTOUT")),
    "cauldron": lambda k, n: add(k, n, st(tex=ALL(T("metal", 0x3A3A3A)), shape=CAULDRON, hardness=2, tool="PICKAXE", tier=1, sound="METAL")),
    "hopper": lambda k, n: add(k, n, [st(tex=ALL(T("metal", 0x4A4A4A)), shape=HOPPER if f < 0 else HOPPER[:2] + rot([(6, 4, 0, 10, 8, 4)], f), hardness=3, tool="PICKAXE", tier=1, sound="METAL", be="HOPPER", ui="CHEST", drop="hopper") for f in (-1, 0, 1, 2, 3)], "HOPPER"),
    "dispenser": lambda k, n: add(k, n, [st(tex=six_tex(T("furnace_front", 0x6f6f6f), T("cobble", 0x6f6f6f), T("cobble", 0x6f6f6f), f), be="DISPENSER", ui="CHEST", drop="dispenser", **stone(0, 3.5)) for f in range(6)], "PISTON"),
    "dropper": lambda k, n: add(k, n, [st(tex=six_tex(T("furnace_front", 0x6a6a6a), T("cobble", 0x6f6f6f), T("cobble", 0x6f6f6f), f), be="DISPENSER", ui="CHEST", drop="dropper", **stone(0, 3.5)) for f in range(6)], "PISTON"),
    "observer": lambda k, n: add(k, n, [st(tex=six_tex(T("observer_face"), T("observer_back", bool(pw)), T("polished", 0x5a5a5a), f), drop="observer", **stone(0, 3)) for pw in range(2) for f in range(6)], "PISTON"),
    "piston": lambda k, n: add(k, n, piston_states(False), "PISTON"),
    "sticky_piston": lambda k, n: add(k, n, piston_states(True), "PISTON"),
    "lectern": lambda k, n: add(k, n, [st(tex=ALL(T("planks", 0xB8945F)), shape=rot(LECTERN, k), ui="LECTERN", be="LECTERN", **wood(2.5)) for k in range(4)], "FACING4"),
    "loom": lambda k, n: add(k, n, facing_states(T("planks", 0xC8A87A), T("planks", 0xB8945F), T("wool", 0xE8E8E8), ui="LOOM", **wood(2.5)), "FACING4"),
    "cartography_table": simple(TSB(T("map"), T("planks", 0x5A4A3A), T("planks", 0x5A4A3A)), ui="CARTOGRAPHY", **wood(2.5)),
    "fletching_table": simple(TSB(T("planks", 0xC8B07A), T("planks", 0xB8A06A), T("planks", 0xB8A06A)), **wood(2.5)),
    "smithing_table": simple(TSB(T("metal", 0x3A3A44), T("planks", 0x4A3A30), T("planks", 0x4A3A30)), ui="SMITHING", **wood(2.5)),
    "stonecutter": lambda k, n: add(k, n, st(tex=TSB(T("metal", 0x8A8A8A), T("polished", 0x7A7A7A), T("polished", 0x7A7A7A)), shape=CUTTER, ui="STONECUTTER", **stone(0, 3.5))),
    "grindstone": lambda k, n: add(k, n, [st(tex=ALL(T("polished", 0x8A8A8A)), shape=rot(GRINDSTONE, k), ui="GRINDSTONE", **stone(0, 2)) for k in range(4)], "FACING4"),
    "composter": lambda k, n: add(k, n, st(tex=ALL(T("composter")), shape=COMPOSTER, ui="COMPOSTER", **wood(0.6))),
    "bell": lambda k, n: add(k, n, st(tex=ALL(T("metal", 0xE8C040)), shape=BELL, hardness=5, tool="PICKAXE", sound="METAL", ui="BELL", support="SOLID")),
    "scaffolding": lambda k, n: add(k, n, st(tex=ALL(T("scaffold")), shape=SCAFFOLD, layer="CUTOUT", hardness=0, sound="WOOD", flags=F_CLIMBABLE)),
    "campfire": lambda k, n: add(k, n, st(tex=TSB(T("fire"), T("log_side", 0x6D5532), T("log_side", 0x6D5532)), shape=CAMPFIRE, layer="CUTOUT", hardness=2, tool="AXE", sound="WOOD", emit=(15, 11, 6), drop="charcoal", drop_min=2, drop_max=2, touch=1)),
    "soul_campfire": lambda k, n: add(k, n, st(tex=TSB(T("fire", 0x40C0E0), T("log_side", 0x6D5532), T("log_side", 0x6D5532)), shape=CAMPFIRE, layer="CUTOUT", hardness=2, tool="AXE", sound="WOOD", emit=(4, 10, 12), drop="soul_soil", touch=2)),
    "beehive": lambda k, n: add(k, n, facing_states(T("beehive", True), T("beehive", False), T("honeycomb", 0xC8A050), **wood(0.6)), "FACING4"),
    "bee_nest": lambda k, n: add(k, n, facing_states(T("beehive", True), T("honeycomb", 0xD8B050), T("honeycomb", 0xC8A050), **wood(0.3)), "FACING4"),
    "flower_pot": lambda k, n: add(k, n, st(tex=ALL(T("terracotta", 0x8A4A30)), shape=POT, hardness=0, support="SOLID")),
    "decorated_pot": lambda k, n: add(k, n, st(tex=ALL(T("terracotta", 0x9A5A3A)), shape=DECORATED_POT, hardness=0)),
    "cake": lambda k, n: add(k, n, st(tex=TSB(T("cake", "top"), T("cake", "side"), T("cake", "side")), shape=CAKE, hardness=0.5, sound="WOOL", drop="nothing", ui="CAKE", support="SOLID")),
    "candle": lambda k, n: add(k, n, st(tex=ALL(T("wool", 0xE8D8B0)), shape=CANDLE, hardness=0.1, sound="WOOL", support="SOLID", emit=(3, 3, 2))),
    "target": lambda k, n: add(k, n, [st(tex=ALL(T("target")), hardness=0.5, tool="HOE", sound="GRASS"), st(tex=ALL(T("target")), hardness=0.5, tool="HOE", sound="GRASS", drop="target")]),
    "redstone_block": simple(T("metal", 0xB01A0A), hardness=5, tool="PICKAXE", tier=1, emit=(0, 0, 0)),
    "redstone_lamp": lambda k, n: add(k, n, [st(tex=ALL(T("redstone_lamp", "off")), hardness=0.3, sound="GLASS"),
                                            st(tex=ALL(T("redstone_lamp", True)), hardness=0.3, sound="GLASS", emit=(15, 12, 7), drop="redstone_lamp")]),
    "repeater": lambda k, n: add(k, n, [st(tex=TSB(T("repeater_top"), T("polished", 0x9a9a9a), T("polished", 0x9a9a9a)), shape=rot(repeater_shape(dl, pw), f), hardness=0, support="SOLID",
                                           emit=(2, 0, 0) if pw else (0, 0, 0), drop="repeater", layer="CUTOUT") for f in range(4) for dl in range(4) for pw in range(2)], "FACING4"),
    "comparator": lambda k, n: add(k, n, [st(tex=TSB(T("repeater_top", 0xA8A8A8, True), T("polished", 0x9a9a9a), T("polished", 0x9a9a9a)), shape=rot(comparator_shape(md, pw), f), hardness=0, support="SOLID",
                                             be="COMPARATOR", drop="comparator", layer="CUTOUT") for f in range(4) for md in range(2) for pw in range(2)], "FACING4"),
    "lever": lambda k, n: add(k, n, powered_attach(T("cobble", 0x7A7A7A), [(5, 0, 4, 11, 3, 12), (7, 3, 5, 9, 10, 7)], wall_n([(5, 4, 13, 11, 12, 16), (7, 9, 6, 9, 11, 13)]),
                                                  [(5, 0, 4, 11, 3, 12), (7, 3, 9, 9, 10, 11)], wall_n([(5, 4, 13, 11, 12, 16), (7, 5, 6, 9, 7, 13)]), solid=False, hardness=0.5, ceiling=True), "ATTACH"),
    # var 0..3: wall direction; +4: powered (a tripped line) - see src/world/tripwire.c
    "tripwire_hook": lambda k, n: add(k, n, [st(tex=ALL(T("planks", 0x8A6A3A)), shape=rot(wall_n([(6, 1, 14, 10, 9, 16), (7, 6, 10, 9, 8, 14)]), k % 4), solid=False, hardness=0, drop="tripwire_hook") for k in range(8)], "WALLONLY"),
    "daylight_detector": lambda k, n: add(k, n, [st(tex=TSB(T("chiseled", 0xD8D0B8), T("planks", 0xB8945F), T("planks", 0xB8945F)), shape=DAYLIGHT, **wood(0.2)),
                                                st(tex=TSB(T("chiseled", 0x5A6A9A), T("planks", 0xB8945F), T("planks", 0xB8945F)), shape=DAYLIGHT, drop="daylight_detector", **wood(0.2))]),
    "stone_button": lambda k, n: add(k, n, powered_attach(STONE_T, [(5, 0, 6, 11, 2, 10)], wall_n(BUTTON_WALL_N), [(5, 0, 6, 11, 1, 10)], wall_n(BUTTON_PRESSED_N), solid=False, hardness=0.5, tool="PICKAXE", ceiling=True), "ATTACH"),
    "polished_blackstone_button": lambda k, n: add(k, n, powered_attach(T("polished", 0x35303A), [(5, 0, 6, 11, 2, 10)], wall_n(BUTTON_WALL_N), [(5, 0, 6, 11, 1, 10)], wall_n(BUTTON_PRESSED_N), solid=False, hardness=0.5, tool="PICKAXE", ceiling=True), "ATTACH"),
    "stone_pressure_plate": lambda k, n: add(k, n, plate_states(ALL(STONE_T), hardness=0.5, tool="PICKAXE")),
    "polished_blackstone_pressure_plate": lambda k, n: add(k, n, plate_states(ALL(T("polished", 0x35303A)), hardness=0.5, tool="PICKAXE")),
    "light_weighted_pressure_plate": lambda k, n: add(k, n, plate_states(ALL(T("metal", 0xF4D33F)), hardness=0.5, tool="PICKAXE", sound="METAL")),
    "heavy_weighted_pressure_plate": lambda k, n: add(k, n, plate_states(ALL(T("metal", 0xD8D8D8)), hardness=0.5, tool="PICKAXE", sound="METAL")),
    "iron_door": lambda k, n: add(k, n, door_states(T("door", 0xC8C8C8, "lower", "iron"), T("door", 0xC8C8C8, "upper", "iron"), hardness=5, tool="PICKAXE", sound="METAL", ui="TOGGLE"), "DOOR"),
    "iron_trapdoor": lambda k, n: add(k, n, trapdoor_states(T("trapdoor", 0xC8C8C8, "iron"), hardness=5, tool="PICKAXE", sound="METAL", ui="TOGGLE"), "TRAPDOOR"),
    "rail": lambda k, n: add(k, n, rail_states("rail", 10, 1), "RAIL"),
    "redstone_wire": None,
})
def rail_states(kind, shapes, powers):
    """var = powered * shapes + shape. Shapes: 0 N-S, 1 E-W, 2 asc E, 3 asc W, 4 asc N, 5 asc S, 6 SE, 7 SW, 8 NW, 9 NE."""
    out = []
    for pw in range(powers):
        for sh in range(shapes):
            tex = T("rail", kind, sh >= 6)
            if kind != "rail" and pw: tex = T("rail", kind, False) + ("on",)
            out.append(st(mesh="RAIL", tex=[tex, tex, tex, tex, T("rail", kind, True), tex], solid=False, layer="CUTOUT", hardness=0.7, tool="PICKAXE", sound="METAL",
                          support="SOLID", shape=[(0, 0, 0, 16, 2, 16)], drop=kind, emit=(0, 0, 0)))
    return out
for rk in ("powered_rail", "detector_rail", "activator_rail"):
    SPECIAL[rk] = (lambda r: lambda k, n: add(k, n, rail_states(r, 6, 2), "RAIL"))(rk)
del SPECIAL["redstone_wire"]
SPECIAL["snow"] = lambda k, n: add(k, n, [st(tex=ALL(T("flat", 0xF4FAFB, 0.03)), shape=SNOW_LAYERS[i], hardness=0.1, tool="SHOVEL", sound="SNOW", drop="snowball", drop_min=i + 1, drop_max=i + 1,
                                            replaceable=(i == 0), support="SOLID", opacity=0 if i < 7 else 15) for i in range(8)], "LAYER")
# metal / gem blocks
for mk, col, tier, h in (("coal_block", 0x1A1A1A, 1, 5), ("iron_block", 0xDCDCDC, 2, 5), ("gold_block", 0xF8D840, 3, 3), ("diamond_block", 0x62E8DD, 3, 5),
                         ("emerald_block", 0x2AC85A, 3, 5), ("lapis_block", 0x1F43A0, 2, 3), ("netherite_block", 0x433D40, 4, 50), ("raw_iron_block", 0xA6866A, 1, 5),
                         ("raw_copper_block", 0x9A6A4A, 1, 5), ("raw_gold_block", 0xD8A82A, 1, 5)):
    painter = "gemblock" if mk in ("diamond_block", "emerald_block", "lapis_block") else ("stone" if mk.startswith("raw_") or mk == "coal_block" else "metal")
    SPECIAL[mk] = (lambda c, t, h_, p: lambda k, n: cube(k, n, T(p, c), hardness=h_, tool="PICKAXE", tier=t, sound="METAL" if p == "metal" else "STONE",
                                                          blast=1200 if k == "netherite_block" else None, flags=F_FLAMMABLE if k == "coal_block" else 0))(col, tier, h, painter)

# ------------------------------------------------------------------ blocks without an item
def extra_blocks():
    def crop(key, name, ages, col, ripe=None, support="FARMLAND", drop="nothing"):
        add(key, name, [st(mesh="CROP", tex=ALL(T("crop", col, i, ages - 1, ripe)), solid=False, hardness=0, sound="GRASS", support=support, drop=drop, flags=F_FLAMMABLE)
                        for i in range(ages)], "SIMPLE", item=False)
    crop("wheat_crop", "Wheat Crops", 8, 0x5AA02A, 0xD8B850)
    crop("carrots", "Carrots", 4, 0x3AA02A, 0xF08A20)
    crop("potatoes", "Potatoes", 4, 0x4A9A2A, 0xC8A050)
    crop("beetroots", "Beetroots", 4, 0x3A9A3A, 0xA02040)
    crop("nether_wart_crop", "Nether Wart", 4, 0x9A1A2A, 0xC02030, support="SOUL")
    crop("torchflower_crop", "Torchflower Crop", 2, 0x5A9A3A, 0xF08A20)
    crop("pitcher_crop", "Pitcher Crop", 5, 0x5A8A9A, 0x6A8AD0)
    crop("melon_stem", "Melon Stem", 8, 0x8A9A3A, support="FARMLAND")
    crop("pumpkin_stem", "Pumpkin Stem", 8, 0x8A9A3A, support="FARMLAND")
    crop("sweet_berry_bush", "Sweet Berry Bush", 4, 0x2A6A3A, 0xC02030, support="SOIL")
    add("cave_vines", "Cave Vines", [st(**plant(tex=ALL(T("cross_plant", "hanging", 0x5A8A2A)), support="CEILING", flags=F_CLIMBABLE, drop="nothing")),
                                     st(**plant(tex=ALL(T("cross_plant", "bush", 0x5A8A2A, 0xF0B030)), support="CEILING", flags=F_CLIMBABLE, emit=(14, 11, 5), drop="glow_berries"))], item=False)
    add("cocoa", "Cocoa", [st(tex=ALL(T("flat", c)), shape=[(5, 4, 5, 11, 12, 11)], hardness=0.2, tool="AXE", sound="WOOD", drop="nothing") for c in (0x8A9A3A, 0xB08A3A, 0xA05A2A)], item=False)
    # var 0 = source, 1..7 = flowing (level drop), 8..15 = falling
    add("water", "Water", [st(mesh="LIQUID", layer="TRANSLUCENT", tex=ALL(("liquid", 0x3F76E4, 0, 180)), solid=False, opaque=False, opacity=2, hardness=-1, replaceable=True,
                             liquid=True, animated=True, drop="nothing", blast=500, tint="WATER", tint_faces=63) for _ in range(16)], item=False)
    add("lava", "Lava", [st(mesh="LIQUID", layer="OPAQUE", tex=ALL(("lava", 0)), solid=False, opaque=False, opacity=15, emit=(15, 10, 4), hardness=-1, replaceable=True,
                           liquid=True, animated=True, drop="nothing", blast=500) for _ in range(16)], item=False)
    add("fire", "Fire", st(mesh="CROSS", tex=ALL(T("fire")), layer="CUTOUT", solid=False, opaque=False, opacity=0, emit=(15, 11, 6), hardness=0, replaceable=True, drop="nothing", touch=1, support="ANY"), item=False)
    add("soul_fire", "Soul Fire", st(mesh="CROSS", tex=ALL(T("fire", 0x40C0E0)), layer="CUTOUT", solid=False, opaque=False, opacity=0, emit=(4, 10, 12), hardness=0, replaceable=True, drop="nothing", touch=2), item=False)
    add("nether_portal", "Nether Portal", [st(tex=ALL(T("portal")), shape=PORTAL_X, layer="TRANSLUCENT", solid=False, hardness=-1, emit=(8, 3, 11), drop="nothing"),
                                           st(tex=ALL(T("portal")), shape=rot(PORTAL_X, 1), layer="TRANSLUCENT", solid=False, hardness=-1, emit=(8, 3, 11), drop="nothing")], item=False)
    add("end_portal", "End Portal", st(tex=ALL(T("end_portal")), shape=END_PORTAL, solid=False, hardness=-1, emit=(15, 15, 15), drop="nothing"), item=False)
    add("sift_portal", "Sift Portal", [st(tex=ALL(T("portal", 0xF0D860)), shape=PORTAL_X, layer="TRANSLUCENT", solid=False, hardness=-1, emit=(15, 14, 9), drop="nothing"),
                                       st(tex=ALL(T("portal", 0xF0D860)), shape=rot(PORTAL_X, 1), layer="TRANSLUCENT", solid=False, hardness=-1, emit=(15, 14, 9), drop="nothing")], item=False)
    add("end_gateway", "End Gateway", st(tex=ALL(T("end_portal")), hardness=-1, emit=(15, 15, 15), drop="nothing"), item=False)
    add("redstone_wire", "Redstone Wire", [st(mesh="WIRE", tex=ALL(T("dust")), solid=False, layer="CUTOUT", hardness=0, support="SOLID", drop="redstone", shape=[(0, 0, 0, 16, 1, 16)]) for _ in range(16)], item=False)
    add("tripwire", "Tripwire", [st(tex=ALL(T("flat", 0xD8D8D8 if i != 2 else 0x9A9A9A)), shape=TRIPWIRE, solid=False, hardness=0, drop="string", layer="CUTOUT") for i in range(3)], item=False)   # 0 idle, 1 tripped, 2 disarmed
    add("bubble_column", "Bubble Column", st(mesh="LIQUID", layer="TRANSLUCENT", tex=ALL(("liquid", 0x3F76E4, 0, 180)), solid=False, opaque=False, opacity=2, hardness=-1,
                                             replaceable=True, liquid=True, animated=True, drop="nothing", tint="WATER", tint_faces=63), item=False)
    add("powder_snow", "Powder Snow", st(tex=ALL(T("powder", 0xF4FAFB)), solid=False, hardness=0.25, sound="SNOW", drop="nothing"), item=False)
    add("wall_torch", "Wall Torch", [st(tex=ALL(T("torch")), shape=rot(wall_n(WALL_TORCH_N), k), solid=False, hardness=0, sound="WOOD", emit=(14, 11, 6), drop="torch", layer="CUTOUT") for k in range(4)], "WALLONLY", item=False)
    add("soul_wall_torch", "Soul Wall Torch", [st(tex=ALL(T("torch", 0x6b4f2a, 0x70E0F0)), shape=rot(wall_n(WALL_TORCH_N), k), solid=False, hardness=0, sound="WOOD", emit=(4, 10, 12), drop="soul_torch", layer="CUTOUT") for k in range(4)], "WALLONLY", item=False)
    add("copper_wall_torch", "Copper Wall Torch", [st(tex=ALL(T("torch", 0x6b4f2a, 0x9AF07A)), shape=rot(wall_n(WALL_TORCH_N), k), solid=False, hardness=0, sound="WOOD", emit=(10, 14, 8), drop="copper_torch", layer="CUTOUT") for k in range(4)], "WALLONLY", item=False)
    add("redstone_wall_torch", "Redstone Wall Torch", [st(tex=ALL(T("torch", 0x6b4f2a, 0xFF3020 if not off else 0x5a2a20)), shape=rot(wall_n(WALL_TORCH_N), k), solid=False, hardness=0, sound="WOOD",
                                                          emit=(0, 0, 0) if off else (7, 1, 0), drop="redstone_torch", layer="CUTOUT") for off in range(2) for k in range(4)], "WALLONLY", item=False)
    add("frosted_ice", "Frosted Ice", [st(tex=ALL(T("ice", 0x91b7f7 - i * 0x080808, 170)), layer="TRANSLUCENT", opaque=False, opacity=2, hardness=0.5, sound="GLASS", drop="nothing", flags=0) for i in range(4)], item=False)
    add("cave_air", "Cave Air", st(mesh="NONE", solid=False, opaque=False, opacity=0, replaceable=True, hardness=-1, drop="nothing"), item=False)
    add("water_cauldron", "Water Cauldron", st(tex=ALL(T("metal", 0x3A3A3A)), shape=CAULDRON + [(2, 4, 2, 14, 13, 14)], hardness=2, tool="PICKAXE", drop="cauldron"), item=False)
    add("piston_head", "Piston Head", [st(tex=ALL(T("piston_top", bool(sticky))), shape=face6(PISTON_HEAD, f), hardness=1.5, drop="nothing") for sticky in range(2) for f in range(6)], item=False)
    add("tall_seagrass", "Tall Seagrass", [st(**plant(tex=ALL(T("cross_plant", "grass", 0x3A8A3A)), support="SOLID", flags=F_WATERLOGGED | F_TALL, drop="nothing")),
                                          st(**plant(tex=ALL(T("cross_plant", "grass", 0x3A8A3A)), support="STACK", flags=F_WATERLOGGED | F_TALL, drop="nothing"))], "DOUBLE", item=False)
    add("kelp_plant", "Kelp Plant", st(**plant(tex=ALL(T("cross_plant", "stalk", 0x4A8A2A)), support="STACK", flags=F_WATERLOGGED, drop="kelp")), item=False)
    add("ice_spike_snow", "Snow", st(tex=ALL(T("flat", 0xF4FAFB, 0.03)), shape=SNOW_LAYERS[0], hardness=0.1, sound="SNOW", drop="snowball", replaceable=True, support="SOLID", opacity=0), item=False)
