#!/usr/bin/env python3
"""UltimateCraft content generator.

Reads tools/content/items_list.csv (the full Minecraft item list) plus the rule
tables in tools/content/*.py and writes:
  src/gen/content.h        enums (B_*, I_*, MOB_*, BIOME_*), counts, table declarations
  src/gen/content_blocks.c BLOCKS[] (every block state) + SHAPES[]
  src/gen/content_items.c  ITEMS[] + potion table
  src/gen/content_recipes.c crafting / smelting / stonecutting / smithing tables
  src/gen/content_world.c  biomes + mobs (see content/biomes.py, content/mobs.py)
  res/textures/blocks.png  1024x1024 block atlas (64x64 tiles)
  res/textures/items.png   1024x1024 item icon atlas
Run from anywhere:  python3 tools/gen_content.py   (needs Pillow)
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "content"))
from PIL import Image
import paint, colors
import blocks as B
import items as I
import recipes as R

ROOT = os.path.dirname(HERE)
GEN = os.path.join(ROOT, "src", "gen")
TEX = os.path.join(ROOT, "res", "textures")
os.makedirs(GEN, exist_ok=True)

def cname(key): return key.upper().replace("-", "_")
def cstr(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

# ============================================================== blocks
kinds = B.build()
state_id, canon = {}, {}
states = []   # (kind, var, state)
for k in kinds:
    canon[k.key] = len(states)
    for v, s in enumerate(k.states):
        states.append((k, v, s))
NSTATES = len(states)
ITEM_BASE = ((NSTATES + 255) // 256) * 256
print(f"blocks: {len(kinds)} kinds, {NSTATES} states, ITEM_BASE {ITEM_BASE}")

# ---------------- textures
ATLAS_TILES = 64
WATER_ROW, LAVA_ROW, CRACK_ROW = 63, 62, 61
tile_of = {}
atlas = Image.new("RGBA", (1024, 1024), (255, 0, 255, 255))
next_tile = [0]
def tile(spec):
    spec = tuple(spec)
    if spec in tile_of: return tile_of[spec]
    if spec[0] == "liquid": t = WATER_ROW * 64
    elif spec[0] == "lava": t = LAVA_ROW * 64
    else:
        t = next_tile[0]; next_tile[0] += 1
        if t >= CRACK_ROW * 64: raise RuntimeError("block atlas full")
        im = paint.paint(spec)
        atlas.paste(im, ((t % 64) * 16, (t // 64) * 16))
    tile_of[spec] = t
    return t
for f in range(16):
    atlas.paste(paint.paint(("liquid", 0xB0B0B0, f, 200)), (f * 16, WATER_ROW * 16))
    atlas.paste(paint.paint(("lava", f)), (f * 16, LAVA_ROW * 16))
for s in range(10):
    atlas.paste(paint.paint(("crack", s)), (s * 16, CRACK_ROW * 16))
# a few special tiles the engine references directly
SPECIAL_TILES = {"dirt": tile(B.DIRT), "grass_top": tile(B.GRASS_TOP), "stone": tile(B.STONE_T), "missing": tile(("barrier",))}

# ---------------- shapes
shape_ids = {(): 0}
shape_list = [()]
def shape_id(boxes):
    if not boxes: return 0
    key = tuple(tuple(int(v) for v in b) for b in boxes)
    if key not in shape_ids:
        shape_ids[key] = len(shape_list); shape_list.append(key)
    return shape_ids[key]

# ---------------- item ids
items = I.build(kinds)
item_id = {}
pure_n = 0
for it in items:
    if it.block: item_id[it.key] = canon[it.block]
    else: item_id[it.key] = ITEM_BASE + pure_n; pure_n += 1
ITEM_MAX = ITEM_BASE + pure_n
print(f"items: {len(items)} ({pure_n} pure), ITEM_MAX {ITEM_MAX}")

def drop_id(s, kind):
    d = s["drop"]
    if d is None: return canon[kind.key] if kind.item else 0xFFFF
    if d == "nothing": return 0xFFFF
    if d in item_id: return item_id[d]
    if d in canon: return canon[d]
    raise KeyError(f"drop {d} of {kind.key}")

FLAG_EXTRA = {"rot90": B.F_ROT90}
block_rows = []
for sid, (k, v, s) in enumerate(states):
    tex = [tile(t) for t in s["tex"]] if "tex" in s else [SPECIAL_TILES["missing"]] * 6
    emit = s["emit"]
    flags = s["flags"] | (B.F_ROT90 if s.get("rot90") else 0)
    if s.get("collide") == "none": flags |= B.F_NOCOLLIDE_MOB
    hard = s["hardness"]
    blast = s["blast"] if s["blast"] is not None else (3600000.0 if hard < 0 else max(hard * 3.0, 0.0))
    dmin, dmax = s["drop_min"], s["drop_max"]
    if s.get("drop_mul"): dmin = dmax = 2
    block_rows.append("    {%s, %s, MESH_%s, LAYER_%s, %d, %d, %d, %d, %d, %d, {%s}, %.3ff, %.1ff, TOOL_%s, %d, %d, %d, %d, SG_%s, SUP_%s, PL_%s, TINT_%s, %d, BE_%s, UI_%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}," % (
        cstr(k.name), cstr(k.key), s["mesh"], s["layer"], int(s["solid"]), int(s["opaque"]), s["opacity"], emit[0], emit[1], emit[2],
        ", ".join(str(t) for t in tex), hard, blast, s["tool"], s["tier"], drop_id(s, k), dmin, dmax, s["sound"], s["support"], k.place,
        s["tint"], s["tint_faces"], s["be"], s["ui"], int(s["replaceable"]), int(s["falls"]), int(s["liquid"]), int(s["animated"]),
        s["touch"], s["xp"][0], s["xp"][1], shape_id(s["shape"]), canon[k.key], flags, v, len(k.states)))

# ============================================================== item icons
icons = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
icon_of = {}
nicon = [0]
def put_icon(im):
    i = nicon[0]; nicon[0] += 1
    icons.paste(im, ((i % 64) * 16, (i // 64) * 16))
    return i
TOOL_PAL_HANDLE = 0x6b4f2a
def make_icon(spec, key):
    kind = spec[0]
    if kind == "tool":
        _, tk, tier = spec
        col = colors.tier_rgb(tier)
        return paint.icon(tk, paint.material_pal(col), paint.seed_of(key))
    if kind == "armor":
        _, sl, mat = spec
        return paint.icon(sl, paint.material_pal(colors.tier_rgb(mat)), paint.seed_of(key))
    if kind == "bucket":
        pal = paint.material_pal(0xC8C8C8, accent=paint.rgb(spec[1]) if spec[1] else paint.rgb(0x5a5a5a))
        return paint.icon("bucket", pal, 1)
    if kind == "egg":
        import mobs as M
        a, b = M.EGG_COLORS.get(spec[1], (0x8a8a8a, 0x4a4a4a))
        pal = paint.material_pal(a, accent=paint.rgb(b))
        return paint.icon("spawn_egg", pal, paint.seed_of(key))
    if kind == "mask":
        _, tmpl, col = spec
        pal = paint.material_pal(col)
        if tmpl in ("bottle", "potion", "splash"):
            pal = paint.material_pal(0xD8E8F0, accent=paint.rgb(col)); pal["d"] = paint.rgb(0x8a6a3a); pal["h"] = paint.rgb(0xD8E8F0)
        if tmpl in ("bowl",): pal["x"] = paint.shade(paint.rgb(col), 1.1); pal["y"] = paint.rgb(col); pal["b"] = paint.rgb(0x8A6A3A); pal["c"] = paint.rgb(0x5A4A2A)
        if tmpl in ("sign", "hanging_sign"): pal["x"] = paint.shade(paint.rgb(col), 0.6)
        if tmpl == "bucket": pal = paint.material_pal(0xC8C8C8, accent=paint.rgb(0x5a5a5a))
        return paint.icon(tmpl, pal, paint.seed_of(key))
    raise ValueError(spec)

potion_icons = {}
def potion_icon_base(tmpl):
    if tmpl in potion_icons: return potion_icons[tmpl]
    base = None
    for (pk, pname, eff, amp, sec, col) in I.POTIONS:
        pal = paint.material_pal(0xD8E8F0, accent=paint.rgb(col)); pal["d"] = paint.rgb(0x8a6a3a); pal["h"] = paint.rgb(0xD8E8F0)
        if tmpl == "arrow": pal = paint.material_pal(0x8a8a8a, accent=paint.rgb(col), handle=0x8a6a3a)
        i = put_icon(paint.icon(tmpl, pal, 3))
        if base is None: base = i
    potion_icons[tmpl] = base
    return base

def flat_icon_for_block(kind_key):
    k = B.KIND_BY_KEY[kind_key]
    s = k.states[0]
    key = kind_key
    if key.endswith("_door"):
        col = colors.PLANKS_RGB.get(key[:-5], colors.COPPER_RGB.get(B.copper_stage(key), 0xC8C8C8)) if "copper" in key or key[:-5] in colors.PLANKS_RGB else 0xC8C8C8
        return put_icon(paint.icon("door", paint.material_pal(col, accent=paint.rgb(0x2a2a2a)), 2))
    if key.endswith("_hanging_sign"): return put_icon(paint.icon("hanging_sign", paint.material_pal(colors.STRIPPED_RGB[key[:-13]], handle=0x5a5a5a), 2))
    if key.endswith("_sign"): return put_icon(paint.icon("sign", paint.material_pal(colors.PLANKS_RGB[key[:-5]], handle=0x6b4f2a, accent=paint.rgb(0x3a2a1a)), 2))
    if key.endswith("_bed"): return put_icon(paint.icon("bed", paint.material_pal(colors.DYE_RGB[key[:-4]], accent=paint.rgb(colors.DYE_RGB[key[:-4]]), accent2=paint.rgb(0xF0F0F0), handle=0x8A6A40), 2))
    if key.endswith("_banner"): return put_icon(paint.icon("banner", paint.material_pal(colors.DYE_RGB[key[:-7]], handle=0x8A6A3A), 2))
    if key in ("lantern", "soul_lantern") or key.endswith("copper_lantern"):
        return -2
    return None

item_rows = []
for it in items:
    iid = item_id[it.key]
    icon = -1
    variants = 0
    if it.block:
        fi = flat_icon_for_block(it.block)
        if fi is not None: icon = fi
        elif it.flat: icon = -2
    elif it.key in ("potion", "splash_potion", "lingering_potion", "tipped_arrow"):
        icon = potion_icon_base({"potion": "potion", "splash_potion": "splash", "lingering_potion": "splash", "tipped_arrow": "arrow"}[it.key])
        variants = len(I.POTIONS)
    else:
        icon = put_icon(make_icon(it.icon, it.key))
    block = canon[it.block] if it.block else (canon[getattr(it, "arg_block")] if getattr(it, "arg_block", None) else 0)
    arg = it.arg
    if getattr(it, "arg_mob", None): arg = I.MOBS.index(it.arg_mob)
    if getattr(it, "arg_proj", None): arg = I.PROJ.index(it.arg_proj)
    if getattr(it, "block_place", None): block = canon[it.block_place]
    rem = item_id[it.remainder] if it.remainder else 0
    fx = it.fx[:4]
    fxs = ", ".join("{%d, %d, %d, %d}" % (I.EFFECTS.index(e), a, s, c) for e, a, s, c in fx) or "{0}"
    item_rows.append((iid, "    [%d] = {%s, %s, CAT_%s, TAB_%s, %d, %d, %d, TOOL_%s, %d, %.1ff, %d, %.1ff, %.2ff, %d, %d, %d, %d, %d, %d, %d, USE_%s, %d, %d, %d, {%s}, %d, %d}," % (
        iid, cstr(it.name), cstr(it.key), it.cat, it.tab, it.stack, icon, block, it.tool, it.tier, it.speed, it.dur, it.dmg, it.aspd,
        it.food, it.sat10, it.slot, it.armor, it.tough10, it.kbres10, it.fuel, it.use, arg, rem, len(fx), fxs, variants, int(it.obt))))
print(f"icons: {nicon[0]}, block tiles: {next_tile[0]}")

# ============================================================== recipes
craft, smelts, cuts, smiths, tags = R.build()
def iid(k):
    if k in item_id: return item_id[k]
    raise KeyError(k)
ing_index, ing_lists = {}, []
def ing(x):
    if isinstance(x, list): lst = []; [lst.extend(tags[i[1:]] if i.startswith("#") else [i]) for i in x]
    elif x.startswith("#"): lst = tags[x[1:]]
    else: lst = [x]
    t = tuple(sorted(set(iid(i) for i in lst)))
    if t not in ing_index: ing_index[t] = len(ing_lists); ing_lists.append(t)
    return ing_index[t] + 1
recipe_rows, seen = [], set()
for res, n, rows, key in craft:
    if rows is not None:
        w, h = max(len(r) for r in rows), len(rows)
        cells = []
        for y in range(h):
            for x in range(w):
                ch = rows[y][x] if x < len(rows[y]) else " "
                cells.append(0 if ch == " " else ing(key[ch]))
        sig = (res, w, h, tuple(cells))
        shapeless = 0
    else:
        cells = sorted(ing(x) for x in key)
        w, h = len(cells), 1
        sig = (res, "sl", tuple(cells))
        shapeless = 1
    if sig in seen or len(cells) > 9: continue
    seen.add(sig)
    cells += [0] * (9 - len(cells))
    recipe_rows.append("    {%d, %d, %d, %d, %d, {%s}}," % (iid(res), n, w, h, shapeless, ", ".join(map(str, cells))))
smelt_rows = []
for inp, res, xp, kinds_ in smelts:
    k = sum({"f": 1, "b": 2, "s": 4, "c": 8}[c] for c in kinds_)
    smelt_rows.append("    {%d, %d, %d, %d}," % (ing(inp), iid(res), k, int(round(xp * 100))))
cut_rows = ["    {%d, %d, %d}," % (iid(a), iid(b), n) for a, b, n in cuts]
smith_rows = ["    {%d, %d, %d, %d}," % (iid(t), iid(b), iid(a), iid(r)) for t, b, a, r in smiths if all(x in item_id for x in (t, b, a, r))]
print(f"recipes: {len(recipe_rows)} crafting, {len(smelt_rows)} smelting, {len(cut_rows)} stonecutting, {len(smith_rows)} smithing, {len(ing_lists)} ingredients")

# ============================================================== world content (biomes, mobs)
import biomes as BI
import mobs as M
world_h = BI.header() + M.header()
world_c = "// AUTO-GENERATED by tools/gen_content.py - do not edit by hand.\n" + M.emit_c(canon, item_id) + BI.emit(canon, item_id)

# ============================================================== write C
def enum_lines(prefix, names_ids):
    return "\n".join(f"    {prefix}{cname(n)} = {i}," for n, i in names_ids)
hdr = []
hdr.append("// AUTO-GENERATED by tools/gen_content.py - do not edit by hand.\n#ifndef CONTENT_GEN_H\n#define CONTENT_GEN_H\n")
hdr.append("extern const unsigned short UC_ITEMS[]; extern const int UC_ITEM_COUNT;   // UltimateCraft's own items (creative tab)")
hdr.append(f"#define BLOCK_STATES {NSTATES}\n#define ITEM_BASE {ITEM_BASE}\n#define ITEM_MAX {ITEM_MAX}\n#define SHAPE_COUNT {len(shape_list)}")
hdr.append(f"#define RECIPE_COUNT {len(recipe_rows)}\n#define SMELT_COUNT {len(smelt_rows)}\n#define CUT_COUNT {len(cut_rows)}\n#define SMITH_COUNT {len(smith_rows)}\n#define ING_COUNT {len(ing_lists)}")
hdr.append(f"#define ATLAS_TILES {ATLAS_TILES}\n#define TILE_WATER {WATER_ROW * 64}\n#define TILE_LAVA {LAVA_ROW * 64}\n#define TILE_CRACK {CRACK_ROW * 64}")
hdr.append(f"#define TILE_DIRT {SPECIAL_TILES['dirt']}\n#define TILE_GRASS_TOP {SPECIAL_TILES['grass_top']}\n#define TILE_STONE {SPECIAL_TILES['stone']}")
hdr.append(f"#define POTION_COUNT {len(I.POTIONS)}\n#define EFFECT_COUNT {len(I.EFFECTS)}\n#define DISC_COUNT {len(I.DISCS)}")
hdr.append("typedef enum {\n" + enum_lines("B_", [(k.key, canon[k.key]) for k in kinds]) + "\n} BlockKindId;")
hdr.append("enum {\n" + enum_lines("I_", [(it.key, item_id[it.key]) for it in items]) + "\n};")
hdr.append("typedef enum {\n" + "\n".join(f"    MESH_{m}," for m in B.MESH) + "\n} MeshType;")
hdr.append("typedef enum {\n" + "\n".join(f"    LAYER_{m}," for m in B.LAYER) + "\n} RenderLayer;")
hdr.append("typedef enum {\n" + "\n".join(f"    TOOL_{m}," for m in B.TOOLS) + "\n} ToolType;")
hdr.append("typedef enum {\n" + "\n".join(f"    SG_{m}," for m in B.SOUNDS) + "\n    SG_COUNT\n} SoundGroup;")
hdr.append("typedef enum {\n" + "\n".join(f"    SUP_{m}," for m in B.SUPPORT) + "\n} SupportType;")
hdr.append("typedef enum {\n" + "\n".join(f"    PL_{m}," for m in B.PLACE) + "\n} PlaceType;")
hdr.append("typedef enum {\n" + "\n".join(f"    TINT_{m}," for m in B.TINT) + "\n} TintType;")
hdr.append("typedef enum {\n" + "\n".join(f"    BE_{m}," for m in B.BE) + "\n} BlockEntityType;")
hdr.append("typedef enum {\n" + "\n".join(f"    UI_{m}," for m in B.UI) + "\n} BlockUI;")
hdr.append("enum { BF_LEAVES = 1, BF_FLAMMABLE = 2, BF_CLIMBABLE = 4, BF_WATERLOGGED = 8, BF_FENCELIKE = 16, BF_TALL = 32, BF_NOCOLLIDE = 64, BF_CREATIVE = 128, BF_ROT90 = 256 };")
hdr.append("typedef enum {\n" + "\n".join(f"    CAT_{m}," for m in I.CATS) + "\n} ItemCategory;")
hdr.append("typedef enum {\n" + "\n".join(f"    TAB_{m}," for m in I.TABS) + "\n    TAB_COUNT\n} CreativeTab;")
hdr.append("typedef enum {\n" + "\n".join(f"    USE_{m}," for m in I.USES) + "\n} ItemUse;")
hdr.append("typedef enum {\n" + "\n".join(f"    PROJ_{m}," for m in I.PROJ) + "\n    PROJ_COUNT\n} ProjectileType;")
hdr.append("typedef enum {\n" + "\n".join(f"    EFF_{e.upper()}," for e in I.EFFECTS) + "\n} EffectId;")
hdr.append("typedef enum {\n" + "\n".join(f"    POT_{p[0].upper()}," for p in I.POTIONS) + "\n} PotionId;")
hdr.append(world_h)
hdr.append("#endif")
open(os.path.join(GEN, "content.h"), "w").write("\n".join(hdr) + "\n")

with open(os.path.join(GEN, "content_blocks.c"), "w") as f:
    f.write("// AUTO-GENERATED by tools/gen_content.py - do not edit by hand.\n#include \"../world/block.h\"\n\n")
    f.write("const BlockShape SHAPES[SHAPE_COUNT] = {\n")
    for sh in shape_list:
        f.write("    {%d, {%s}},\n" % (len(sh), ", ".join("{%s}" % ", ".join(map(str, b)) for b in sh) or "{0}"))
    f.write("};\n\nBlockDef BLOCKS[BLOCK_STATES] = {\n" + "\n".join(block_rows) + "\n};\n")
with open(os.path.join(GEN, "content_items.c"), "w") as f:
    f.write("// AUTO-GENERATED by tools/gen_content.py - do not edit by hand.\n#include \"../item/item.h\"\n\n")
    f.write("ItemDef ITEMS[ITEM_MAX] = {\n" + "\n".join(r for _, r in sorted(item_rows)) + "\n};\n\n")
    f.write("const PotionDef POTIONS[POTION_COUNT] = {\n")
    for pk, pname, eff, amp, sec, col in I.POTIONS:
        f.write("    {%s, %s, EFF_%s, %d, %d, 0x%06X},\n" % (cstr(pname), cstr(pk), eff.upper(), amp, sec, col))
    def nice(e): return " ".join(w if (i and w in ("of", "the")) else w.capitalize() for i, w in enumerate(e.split("_")))
    f.write("};\nconst char *EFFECT_NAMES[EFFECT_COUNT] = {%s};\n" % ", ".join(cstr(nice(e)) for e in I.EFFECTS))
    uc_keys = [r[0] for r in B.ITEM_ROWS[[r[0] for r in B.ITEM_ROWS].index("cloud_block"):]]
    uc_ids = [item_id[k] for k in uc_keys if k in item_id]
    f.write("const unsigned short UC_ITEMS[] = {%s};\nconst int UC_ITEM_COUNT = %d;\n" % (", ".join(map(str, uc_ids)), len(uc_ids)))
    f.write("const char *DISC_NAMES[DISC_COUNT] = {%s};\n" % ", ".join(cstr(d) for d in I.DISCS))
with open(os.path.join(GEN, "content_recipes.c"), "w") as f:
    f.write("// AUTO-GENERATED by tools/gen_content.py - do not edit by hand.\n#include \"../item/item.h\"\n\n")
    flat, offs = [], []
    for t in ing_lists: offs.append((len(flat), len(t))); flat.extend(t)
    f.write("const u16 ING_ITEMS[] = {%s};\n" % ", ".join(map(str, flat)))
    f.write("const IngredientDef INGREDIENTS[ING_COUNT + 1] = {{0, 0},\n%s\n};\n" % "\n".join("    {%d, %d}," % o for o in offs))
    f.write("const RecipeDef RECIPES[RECIPE_COUNT] = {\n" + "\n".join(recipe_rows) + "\n};\n")
    f.write("const SmeltDef SMELTS[SMELT_COUNT] = {\n" + "\n".join(smelt_rows) + "\n};\n")
    f.write("const CutDef CUTS[CUT_COUNT] = {\n" + "\n".join(cut_rows) + "\n};\n")
    f.write("const SmithDef SMITHS[SMITH_COUNT] = {\n" + "\n".join(smith_rows) + "\n};\n")
with open(os.path.join(GEN, "content_world.c"), "w") as f:
    f.write(world_c)

atlas.save(os.path.join(TEX, "blocks.png"))
icons.save(os.path.join(TEX, "items.png"))
M.write_assets(TEX, ROOT)
print("done")
