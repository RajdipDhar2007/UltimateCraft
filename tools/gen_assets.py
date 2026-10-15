#!/usr/bin/env python3
"""
UltimateCraft asset generator.

Every texture the game ships is drawn by code - there is no third-party art in this repository:
  * this script paints the HUD pieces (gui.png, font.png, particles, sun/moon/stars/clouds),
  * tools/gen_content.py (called at the end) paints the block atlas, the item icons and every mob skin
    from the rule tables in tools/content/ and writes the C tables under src/.

Run from the project root:  python3 tools/gen_assets.py   (requires Pillow)
"""
import os, random, math
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "res", "textures")
os.makedirs(os.path.join(OUT, "entity"), exist_ok=True)

def clamp(v): return max(0, min(255, int(v)))
def shade(c, f): return (clamp(c[0]*f), clamp(c[1]*f), clamp(c[2]*f), c[3] if len(c) > 3 else 255)
def rgba(h, a=255): return ((h >> 16) & 255, (h >> 8) & 255, h & 255, a)
def mix(a, b, t): return tuple(clamp(a[i]*(1-t)+b[i]*t) for i in range(3)) + (255,)

# --------------------------------------------------------------------------------------------
# Block atlas
# --------------------------------------------------------------------------------------------
# Scratch canvases for the legacy tile helpers below; the real block atlas is painted by gen_content.py.
atlas = Image.new("RGBA", (256, 256), (125, 125, 125, 255))
pyg = Image.new("RGBA", (256, 256), (160, 80, 60, 255))

def tile_img(f, seed):
    rnd = random.Random(seed)
    im = Image.new("RGBA", (16, 16))
    for y in range(16):
        for x in range(16):
            im.putpixel((x, y), f(x, y, rnd))
    return im

def put(col, row, im):
    atlas.paste(im, (col*16, row*16))

def noisy(base, var=0.12):
    return lambda x, y, r: shade(base, 1 + r.uniform(-var, var))

stone = rgba(0x7d7d7d)
def ore(color, seed, darker=None):
    rnd = random.Random(seed)
    base = atlas.crop((3*16, 0, 4*16, 16)).copy()
    for _ in range(5):
        cx, cy = rnd.randint(2, 13), rnd.randint(2, 13)
        for dx, dy in ((0,0),(1,0),(0,1),(1,1),(-1,0),(0,-1)):
            if rnd.random() < 0.8:
                c = shade(color, rnd.uniform(0.8, 1.2))
                base.putpixel((max(0,min(15,cx+dx)), max(0,min(15,cy+dy))), c)
    return base

put(7, 0, tile_img(lambda x, y, r: shade(rgba(0x3a3a3a), r.choice([0.5, 0.8, 1.0, 1.3, 1.6])), 7))   # bedrock
put(8, 0, ore(rgba(0xd8af93), 8))     # iron ore
put(9, 0, ore(rgba(0xfcee4b), 9))     # gold ore
put(10, 0, ore(rgba(0x5decf5), 10))   # diamond ore
put(11, 0, tile_img(lambda x, y, r: shade(rgba(0x1b1428), r.uniform(0.7, 1.5)) if r.random() > 0.15 else rgba(0x3b2754), 11))  # obsidian
put(12, 0, pyg.crop((0, 192, 64, 256)).resize((16, 16), Image.NEAREST))  # bricks from the pyglet project
def sandstone_side(x, y, r):
    base = rgba(0xd8cb9b)
    if y in (3, 11): return shade(base, 0.82)
    if y < 3: return shade(base, 1.06 + r.uniform(-0.04, 0.04))
    return shade(base, 1 + r.uniform(-0.06, 0.06))
put(13, 0, tile_img(sandstone_side, 13))
put(14, 0, tile_img(noisy(rgba(0xddd19f), 0.06), 14))
def mossy(x, y, r):
    c = atlas.getpixel((2*16 + x, 2*16 + y))
    return mix(c, rgba(0x4f7a2c), 0.55) if r.random() < 0.35 else c
put(15, 0, tile_img(mossy, 15))

plank = lambda x, y: atlas.getpixel((6*16 + x, 16 + y))
def ct_top(x, y, r):
    if x in (0, 15) or y in (0, 15): return rgba(0x6b4a22)
    if x in (1, 14) or y in (1, 14): return rgba(0xa77b45)
    if 3 <= x <= 12 and 3 <= y <= 12 and (x in (3, 12) or y in (3, 12) or x == 7 or y == 7): return rgba(0x5a3d1c)
    return shade(rgba(0xb98f55), 1 + r.uniform(-0.08, 0.08))
put(7, 1, tile_img(ct_top, 71))
def ct_side(front):
    def f(x, y, r):
        if y < 3: return shade(rgba(0x7a5a30), 1 + r.uniform(-0.1, 0.1))
        c = plank(x, y)
        if front:
            # saw + hammer silhouettes
            if 4 <= y <= 12 and x in (3, 4) and y > 6: return rgba(0x5a3d1c)
            if y == 5 and 2 <= x <= 6: return rgba(0x9a9a9a)
            if 6 <= y <= 12 and x == 11: return rgba(0x5a3d1c)
            if y in (5, 6) and 9 <= x <= 13: return rgba(0x8a8a8a)
        else:
            if 5 <= y <= 12 and x in (4, 11) : return rgba(0x5a3d1c)
            if y == 8 and 4 <= x <= 11: return rgba(0x5a3d1c)
        return c
    return f
put(8, 1, tile_img(ct_side(False), 81))
put(9, 1, tile_img(ct_side(True), 91))
cobble = lambda x, y: atlas.getpixel((2*16 + x, 2*16 + y))
def furnace_front(lit):
    def f(x, y, r):
        if 4 <= x <= 11 and 7 <= y <= 12:
            if x in (4, 11) or y in (7, 12): return rgba(0x3b3b3b)
            if lit: return shade(rgba(0xff9a2a), r.uniform(0.7, 1.25)) if y >= 9 else rgba(0x1c1206)
            return rgba(0x161616)
        if 3 <= x <= 12 and y == 3: return rgba(0x4a4a4a)
        return cobble(x, y)
    return f
put(10, 1, tile_img(furnace_front(False), 101))
put(11, 1, tile_img(furnace_front(True), 111))
put(12, 1, tile_img(lambda x, y, r: cobble(x, y), 121))
put(13, 1, tile_img(lambda x, y, r: shade(cobble(x, y), 0.92) if 2 <= x <= 13 and 2 <= y <= 13 else cobble(x, y), 131))
wood_c = rgba(0xa06d34)
def chest(kind):
    def f(x, y, r):
        edge = x in (0, 15) or y in (0, 15)
        if edge: return rgba(0x3f2a10)
        c = shade(wood_c, 1 + r.uniform(-0.07, 0.07))
        if kind != "top" and y == 5: return rgba(0x3f2a10)
        if kind == "front" and 6 <= x <= 9 and 4 <= y <= 7:
            return rgba(0xd8d8d8) if (x in (6, 9) or y in (4, 7)) else rgba(0x9a9a9a)
        if x in (1,) or y in (1,): return shade(c, 1.15)
        return c
    return f
put(14, 1, tile_img(chest("top"), 141))
put(15, 1, tile_img(chest("side"), 151))
put(7, 2, tile_img(chest("front"), 72))
def cactus(kind):
    def f(x, y, r):
        if kind == "side":
            if x in (0, 15): return (0, 0, 0, 0)
            if x in (1, 14): return rgba(0x0e5a14)
            if (x + y * 3) % 7 == 0 and x in (3, 7, 11): return rgba(0xd9e8b0)
            return shade(rgba(0x1f8a2a), 1 + r.uniform(-0.12, 0.12))
        if x in (0, 15) or y in (0, 15): return (0, 0, 0, 0)
        if x in (1, 14) or y in (1, 14): return rgba(0x0e5a14)
        return shade(rgba(0x3faa45) if kind == "top" else rgba(0x7a9a4a), 1 + r.uniform(-0.1, 0.1))
    return f
put(8, 2, tile_img(cactus("side"), 82))
put(9, 2, tile_img(cactus("top"), 92))
put(10, 2, tile_img(cactus("bottom"), 102))
put(11, 2, tile_img(lambda x, y, r: shade(rgba(0xffd36a), r.uniform(0.6, 1.25)) if r.random() > 0.2 else rgba(0x9a6a2a), 112))  # glowstone
put(12, 2, tile_img(lambda x, y, r: (shade(rgba(0x9cc4ff), 1 + r.uniform(-0.05, 0.05))[:3] + (200,)) if (x - y) % 9 else (210, 230, 255, 220), 122))  # ice
put(13, 2, tile_img(lambda x, y, r: shade(rgba(0xeaeaea), 1 + r.uniform(-0.06, 0.06) - (0.08 if (x + y) % 4 == 0 else 0)), 132))  # wool
def birch_side(x, y, r):
    if r.random() < 0.10 or ((y * 5 + x // 4 * 3) % 11 == 0 and x % 7 < 3): return rgba(0x2b2b2b)
    return shade(rgba(0xe8e4d6), 1 + r.uniform(-0.07, 0.04))
put(14, 2, tile_img(birch_side, 142))
def log_top_birch(x, y, r):
    d = max(abs(x - 7.5), abs(y - 7.5))
    if d > 6.5: return rgba(0xe8e4d6)
    return shade(rgba(0xcfb27a), 1 - 0.08 * (int(d) % 2))
put(15, 2, tile_img(log_top_birch, 152))
def birch_leaves(x, y, r):
    if r.random() < 0.18: return (0, 0, 0, 0)
    return shade(rgba(0x80a755), r.uniform(0.7, 1.15))
put(7, 3, tile_img(birch_leaves, 73))
def sapling(x, y, r):
    if x in (7, 8) and y >= 9: return rgba(0x6b4a22)
    d = math.hypot(x - 7.5, y - 6)
    if d < 5 and r.random() < 0.8: return shade(rgba(0x3f8f2a), r.uniform(0.75, 1.2))
    return (0, 0, 0, 0)
put(8, 3, tile_img(sapling, 83))
def snowy_side(x, y, r):
    c = atlas.getpixel((2*16 + x, y))
    lim = 3 + (1 if (x * 7) % 5 == 0 else 0)
    return shade(rgba(0xf4f8ff), 1 + r.uniform(-0.04, 0.02)) if y < lim else c
put(9, 3, tile_img(snowy_side, 93))
def stone_bricks(x, y, r):
    if y in (0, 8) or (y < 8 and x in (0,)) or (y >= 8 and x == 8): return rgba(0x5a5a5a)
    return shade(rgba(0x8a8a8a), 1 + r.uniform(-0.06, 0.06))
put(10, 3, tile_img(stone_bricks, 103))
def tnt(kind):
    def f(x, y, r):
        if kind == "top":
            if 6 <= x <= 9 and 6 <= y <= 9: return rgba(0x333333)
            return shade(rgba(0xb33a22), 1 + r.uniform(-0.1, 0.1))
        if 5 <= y <= 10:
            if 6 <= y <= 9 and 3 <= x <= 12 and ((x in (3,4,5) and (y == 6 or x == 4)) or (x in (7,8) and (y == 6 or x == 7 or x == 8)) or (x in (10,11,12) and (y == 6 or x == 11))):
                return rgba(0x202020)
            return rgba(0xeaeaea)
        return shade(rgba(0xc0402a), 1 + r.uniform(-0.1, 0.1) - (0.25 if x % 4 == 0 else 0))
    return f
put(11, 3, tile_img(tnt("side"), 113))
put(12, 3, tile_img(tnt("top"), 123))

# Break-progress cracks (row 13, cols 0..9): transparent overlays.
for stage in range(10):
    rnd = random.Random(900 + 0)  # same seed => cracks grow progressively
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    paths = []
    for _ in range(9):
        x, y = rnd.randint(4, 11), rnd.randint(4, 11)
        pts = [(x, y)]
        for _ in range(12):
            x = max(0, min(15, x + rnd.choice((-1, 0, 1))))
            y = max(0, min(15, y + rnd.choice((-1, 0, 1))))
            pts.append((x, y))
        paths.append(pts)
    n = int((stage + 1) / 10 * sum(len(p) for p in paths))
    k = 0
    for p in paths:
        for (x, y) in p:
            if k < n:
                im.putpixel((x, y), (20, 20, 20, 200))
            k += 1
    put(stage, 13, im)
atlas.save(os.path.join(OUT, "blocks.png"))

# --------------------------------------------------------------------------------------------
# Item icons (items.png 256x256, 16x16 icons)
# --------------------------------------------------------------------------------------------
items = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
def icon(index, rows, pal, seed=0):
    rnd = random.Random(seed + index * 31)
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in pal:
                c = pal[ch]
                im.putpixel((x, y), shade(c, 1 + rnd.uniform(-0.05, 0.05)))
    # automatic dark outline for readability
    out = im.copy()
    for y in range(16):
        for x in range(16):
            if im.getpixel((x, y))[3] == 0:
                for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
                    nx, ny = x+dx, y+dy
                    if 0 <= nx < 16 and 0 <= ny < 16 and im.getpixel((nx, ny))[3] > 0:
                        out.putpixel((x, y), (30, 22, 14, 255) if index < 400 else (0,0,0,255))
                        break
    items.paste(out, ((index % 16) * 16, (index // 16) * 16))

E = "................"
def grid(*rows): return [r.ljust(16, ".") for r in rows] + [E] * (16 - len(rows))

STICK = grid(E, E, E, E, E, E, E, E, ".........h", "........hd", ".......hd", "......hd", ".....hd", "....hd", "...hd", "..h")
icon(0, grid(E,E,E,"..........hd",".........hd","........hd",".......hd","......hd",".....hd","....hd","...hd","..hd",".hd"), {"h": rgba(0x8a6436), "d": rgba(0x5c3f1e)})
LUMP = grid(E, E, E, ".....aab", "....aaaab", "...aaaaabb", "...aaaabbb", "..aaaaabbbb", "..aabbbbbbb", "...abbbbbb", "....bbbbb", ".....bbb")
icon(1, LUMP, {"a": rgba(0x4a4a4a), "b": rgba(0x1c1c1c)})
INGOT = grid(E, E, E, E, E, ".......aaaa", ".....aaaaaab", "...aaaaaaabbc", ".aaaaaaabbbcc", ".abbbbbbbccc", ".bbbbbbbccc", "..cccccccc")
icon(2, INGOT, {"a": rgba(0xffffff), "b": rgba(0xd8d8d8), "c": rgba(0x9a9a9a)})
icon(3, INGOT, {"a": rgba(0xfffbb0), "b": rgba(0xf5d94a), "c": rgba(0xc79a1a)})
GEM = grid(E, E, "....aaaaaa", "...abbbbbba", "..abbbccbbba", ".abbbccccbbba", ".bbbccccccbbb", "..bbccccccbb", "...bbccccbb", "....bbccbb", ".....bbbb", "......bb")
icon(4, GEM, {"a": rgba(0xd8ffff), "b": rgba(0x4ae0e8), "c": rgba(0x1fa8b0)})
icon(5, grid(E, E, ".....aa", "....aabb", "...aabbbb", "..aabbbbbb", "..abbbbbbb", "..bbbbbbbb", "..bbbbbbb", "...bbbbbb", "....bbbb"), {"a": rgba(0x9a9a9a), "b": rgba(0x3c3c3c)})
icon(6, grid(E, E, "..........aa", "........aa.a", "......aa...a", ".....a.....a", "....a.....a", "...a....aa", "..a...aa", ".a..aa", ".aaa", ".a"), {"a": rgba(0xf0f0f0)})
icon(7, grid(E, "...........aa", "..........aab", ".........aabb", "........aabb", ".......aabb", "......aabb", ".....aabb", "....aabb", "...aabb", "..ab", ".a"), {"a": rgba(0xffffff), "b": rgba(0xc8c8c8)})
icon(8, grid(E, E, E, E, ".....aab.a", "...aaabbbb.", "..aabbbbbbb", "..abbbbbcbb", "..bbbcbbbbb", "...bbbbbcb", "....bbbbb", "......b"), {"a": rgba(0x8a8a8a), "b": rgba(0x4c4c4c), "c": rgba(0x2a2a2a)})
icon(9, grid(E, "...........aa", "..........aaab", "...........abb", "..........ab", ".........ab", "........ab", ".......ab", "......ab", ".....ab", "...aab", "..aaab", "..aab", "...b"), {"a": rgba(0xffffff), "b": rgba(0xc8c4b0)})
icon(10, grid(E, E, "...aaaaaaaa", "..abbbbbbbba", "..abbbbbbbba", "...abbbbbba", "...abbbbbba", "..abbbbbbbba", "..abbbbbbbba", "...aaaaaaaa"), {"a": rgba(0x7a4a22), "b": rgba(0xa66a35)})
MEAT = grid(E, E, "....aaaa", "...abbbbaa", "..abbcbbbba", "..abbbbcbbba", "..abcbbbbbbba", "...abbbbcbbba", "....abbbbbba", ".....aabbba", ".......aaa")
icon(11, MEAT, {"a": rgba(0x5a6a2a), "b": rgba(0x7a8a3a), "c": rgba(0xa04a3a)})
icon(12, MEAT, {"a": rgba(0x9a2020), "b": rgba(0xd84a4a), "c": rgba(0xf0e0d0)})
icon(13, MEAT, {"a": rgba(0x4a2a12), "b": rgba(0x7a4a22), "c": rgba(0xc89a6a)})
icon(14, MEAT, {"a": rgba(0xc06a6a), "b": rgba(0xf09a9a), "c": rgba(0xfff0f0)})
icon(15, MEAT, {"a": rgba(0x8a5a2a), "b": rgba(0xc8905a), "c": rgba(0xf0d0a0)})
LEG = grid(E, E, ".......aaaa", "......abbbba", ".....abbbbbba", ".....abbbbbba", ".....abbbbbba", "......abbbba", ".....cabbba", "....cc.aaa", "...cc", "..ccc", "..cc")
icon(16, LEG, {"a": rgba(0xd8a0a0), "b": rgba(0xf8d0c8), "c": rgba(0xf0f0f0)})
icon(17, LEG, {"a": rgba(0x8a5a2a), "b": rgba(0xc8905a), "c": rgba(0xf0f0f0)})
icon(18, MEAT, {"a": rgba(0x9a2a2a), "b": rgba(0xc85050), "c": rgba(0xf0f0f0)})
icon(19, MEAT, {"a": rgba(0x6a3a1a), "b": rgba(0x9a6030), "c": rgba(0xf0e0c0)})
icon(20, grid(E, "........a", ".......a", "....bbb.bbb", "...bbbbbbbbb", "..bbcbbbbbbbb", "..bcbbbbbbbbb", "..bbbbbbbbbbb", "..bbbbbbbbbbb", "...bbbbbbbbb", "...bbbbbbbbb", "....bbbbbbb", ".....bb.bb"), {"a": rgba(0x5a3a1a), "b": rgba(0xd82a2a), "c": rgba(0xffa0a0)})
icon(21, grid(E, "...........ccc", "............cc", "...........h.c", "..........h", ".........h", "........h", ".......h", "......h", ".....h", "..f.h", "..ffh", "..fff", ".ff"), {"h": rgba(0x8a6436), "c": rgba(0xbcbcbc), "f": rgba(0xf0f0f0)})
icon(22, grid(E, "........hhh", ".......h...s", "......h....s", ".....h.....s", "....h......s", "....h.....s", "...h.....s", "...h....s", "...h...s", "...h..s", "....hs", "....hs", ".....h"), {"h": rgba(0x8a6436), "s": rgba(0xe8e8e8)})
icon(23, grid(E, "....aaaaaa", "...abbbbbba", "...abbbbbba", "....abbbba", "....abbbba", "...abbbbbba", "..abbbbbbbba", "..abbbbbbbba", "..abbbbbbbba", "...aaaaaaaa"), {"a": rgba(0x8a4a2a), "b": rgba(0xc06040)})  # brick item (clay)
icon(24, grid(E, E, E, ".....aaaaa", "....abbbbba", "...abbccbbba", "...abccccbba", "...abbccbbba", "....abbbbba", ".....aaaaa"), {"a": rgba(0xc8b878), "b": rgba(0xf0e8a8), "c": rgba(0xfffff0)})  # bread
icon(25, grid(E, E, ".......a", "......aba", ".....abbba", "....abbcbba", "...abbbbbbba", "...abbbbbbba", "....abbbbba", ".....aaaaa"), {"a": rgba(0x3a7a2a), "b": rgba(0x6ab04a), "c": rgba(0xc0ff90)})  # slimeball-ish xp bottle stand-in (unused)

# Tools: tiers wood, stone, iron, gold, diamond
TIERS = [
    {"a": rgba(0xc8a064), "b": rgba(0x8a6436), "c": rgba(0x5c3f1e)},
    {"a": rgba(0xb0b0b0), "b": rgba(0x7a7a7a), "c": rgba(0x4a4a4a)},
    {"a": rgba(0xffffff), "b": rgba(0xd0d0d0), "c": rgba(0x8a8a8a)},
    {"a": rgba(0xfffbb0), "b": rgba(0xf5d94a), "c": rgba(0xb08a1a)},
    {"a": rgba(0xd8ffff), "b": rgba(0x4ae0e8), "c": rgba(0x168a90)},
]
PICK = grid(E, "...aaaaaa", "..abbbbbbac", "...cc..ccbbc", "......hh.cbc", ".....hh...cbc", "....hh.....cc", "...hh", "..hh", ".hh", "hh")
AXE = grid(E, "......aaa", ".....abbbc", "....abbbbbc", "....abbbhbc", ".....cchbbc", ".......hhcc", "......hh", ".....hh", "....hh", "...hh", "..hh", ".hh")
SHOVEL = grid(E, "...........aac", "..........abbbc", ".........abbbc", "..........bbc", ".........hcc", "........hh", ".......hh", "......hh", ".....hh", "....hh", "...hh", "..hh")
SWORD = grid(E, "............aa", "...........abc", "..........abc", ".........abc", "........abc", ".......abc", "..c...abc", "...cabc", "...hcc", "..hh.c", ".hh", "hh")
for t, pal in enumerate(TIERS):
    p = dict(pal); p["h"] = rgba(0x6b4a22)
    for k, shape in enumerate((PICK, AXE, SHOVEL, SWORD)):
        icon(32 + t * 4 + k, shape, p)
ARM = {"a": rgba(0xffffff), "b": rgba(0xd0d0d0), "c": rgba(0x8a8a8a)}
icon(52, grid(E, E, "...aaaaaaaa", "..abbbbbbbbc", "..abbbbbbbbc", "..ab......bc", "..ab......bc", "..cc......cc"), ARM)
icon(53, grid(E, "..aaa....aaa", ".abbba..abbbc", ".abbbbaabbbbc", ".abbbbbbbbbbc", "..cbbbbbbbbc", "...abbbbbbc", "...abbbbbbc", "...abbbbbbc", "...abbbbbbc", "...ccccccc"), ARM)
icon(54, grid(E, "...aaaaaaaa", "...abbbbbbc", "...abbbbbbc", "...abbccbbc", "...abc..abc", "...abc..abc", "...abc..abc", "...abc..abc", "...abc..abc", "...ccc..ccc"), ARM)
icon(55, grid(E, E, E, E, E, "...ab....bc", "...ab....bc", "...ab....bc", "..abb....bbc", "..abb....bbc", "..ccc....ccc"), ARM)
items.save(os.path.join(OUT, "items.png"))

# --------------------------------------------------------------------------------------------
# GUI icons (gui.png 256x256; each icon sits at the top-left of a 16x16 cell)
# --------------------------------------------------------------------------------------------
gui = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
HEART = [".xxx.xxx.", "xxxxxxxxx", "xxxxxxxxx", "xxxxxxxxx", ".xxxxxxx.", "..xxxxx..", "...xxx...", "....x....", "........."]
FOOD = ["....xxx..", "...xxxxx.", "..xxxxxxx", "..xxxxxxx", "..xxxxxx.", ".xxxxx...", "xx.x.....", "x.xx.....", "........."]
ARMOR = [".xx...xx.", "xxxx.xxxx", "xxxxxxxxx", ".xxxxxxx.", ".xxxxxxx.", ".xxxxxxx.", ".xxxxxxx.", ".xxxxxxx.", "........."]
BUBBLE = ["..xxxxx..", ".xxxxxxx.", "xxxxxxxxx", "xxxxxxxxx", "xxxxxxxxx", "xxxxxxxxx", "xxxxxxxxx", ".xxxxxxx.", "..xxxxx.."]
def mask_icon(cell, mask, fill, outline, interior=None, half=False, highlight=None, bone=False):
    ox, oy = (cell % 16) * 16, (cell // 16) * 16
    for y in range(9):
        for x in range(9):
            if mask[y][x] != "x": continue
            edge = any(not (0 <= x+dx < 9 and 0 <= y+dy < 9) or mask[y+dy][x+dx] != "x" for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)))
            if edge:
                c = outline
            elif fill is None or (half and x > 4):
                c = interior
            else:
                c = fill
                if highlight and (x + y) <= 3 and y <= 2: c = highlight
                if bone and x <= 2 and y >= 5: c = rgba(0xf0e8d8)
            if c is not None:
                gui.putpixel((ox + x, oy + y), c)
dark = rgba(0x1a0a0a)
mask_icon(0, HEART, None, dark, interior=rgba(0x3a1010))
mask_icon(1, HEART, rgba(0xe01818), dark, highlight=rgba(0xff9a9a))
mask_icon(2, HEART, rgba(0xe01818), dark, interior=None, half=True, highlight=rgba(0xff9a9a))
mask_icon(3, HEART, None, rgba(0xffffff), interior=rgba(0x3a1010))
mask_icon(4, FOOD, None, rgba(0x2a1a0a), interior=rgba(0x3a2a1a))
mask_icon(5, FOOD, rgba(0xa8582a), rgba(0x2a1a0a), highlight=rgba(0xe09a5a), bone=True)
mask_icon(6, FOOD, rgba(0xa8582a), rgba(0x2a1a0a), interior=None, half=True, highlight=rgba(0xe09a5a), bone=True)
mask_icon(7, ARMOR, None, rgba(0x202020), interior=rgba(0x404040))
mask_icon(8, ARMOR, rgba(0xd8d8d8), rgba(0x202020), highlight=rgba(0xffffff))
mask_icon(9, ARMOR, rgba(0xd8d8d8), rgba(0x202020), interior=None, half=True)
mask_icon(10, BUBBLE, rgba(0x3a7aff), rgba(0x0a2a8a), highlight=rgba(0xd0e8ff))
mask_icon(11, BUBBLE, None, rgba(0x6a9aff), interior=None)
gui.save(os.path.join(OUT, "gui.png"))

# --------------------------------------------------------------------------------------------
# Pixel font (font.png 128x128, 8x8 cells, ASCII). Glyph widths are measured at load time.
# Rows are separated by '/', empty rows are empty strings.
# --------------------------------------------------------------------------------------------
G = {
 'A': ".###./#...#/#...#/#####/#...#/#...#/#...#", 'B': "####./#...#/####./#...#/#...#/#...#/####.",
 'C': ".###./#...#/#..../#..../#..../#...#/.###.", 'D': "####./#...#/#...#/#...#/#...#/#...#/####.",
 'E': "#####/#..../###../#..../#..../#..../#####", 'F': "#####/#..../###../#..../#..../#..../#....",
 'G': ".####/#..../#..##/#...#/#...#/#...#/.###.", 'H': "#...#/#...#/#####/#...#/#...#/#...#/#...#",
 'I': "###/.#./.#./.#./.#./.#./###", 'J': "....#/....#/....#/....#/#...#/#...#/.###.",
 'K': "#...#/#..#./###../#..#./#...#/#...#/#...#", 'L': "#..../#..../#..../#..../#..../#..../#####",
 'M': "#...#/##.##/#.#.#/#...#/#...#/#...#/#...#", 'N': "#...#/##..#/#.#.#/#..##/#...#/#...#/#...#",
 'O': ".###./#...#/#...#/#...#/#...#/#...#/.###.", 'P': "####./#...#/####./#..../#..../#..../#....",
 'Q': ".###./#...#/#...#/#...#/#...#/#..#./.##.#", 'R': "####./#...#/####./#...#/#...#/#...#/#...#",
 'S': ".####/#..../.###./....#/....#/#...#/.###.", 'T': "#####/..#../..#../..#../..#../..#../..#..",
 'U': "#...#/#...#/#...#/#...#/#...#/#...#/.###.", 'V': "#...#/#...#/#...#/#...#/.#.#./.#.#./..#..",
 'W': "#...#/#...#/#...#/#...#/#.#.#/##.##/#...#", 'X': "#...#/.#.#./..#../.#.#./#...#/#...#/#...#",
 'Y': "#...#/.#.#./..#../..#../..#../..#../..#..", 'Z': "#####/....#/...#./..#../.#.../#..../#####",
 'a': "//.###./....#/.####/#...#/.####", 'b': "#..../#..../#.##./##..#/#...#/#...#/####.",
 'c': "//.###./#...#/#..../#...#/.###.", 'd': "....#/....#/.##.#/#..##/#...#/#...#/.####",
 'e': "//.###./#...#/#####/#..../.####", 'f': "..##/.#../####/.#../.#../.#../.#..",
 'g': "//.####/#...#/#...#/.####/....#/####.", 'h': "#..../#..../#.##./##..#/#...#/#...#/#...#",
 'i': "#//#/#/#/#/#", 'j': "....#//....#/....#/....#/#...#/.###.",
 'k': "#.../#.../#..#/#.#./##../#.#./#..#", 'l': "#./#./#./#./#./#./.#",
 'm': "//##.#./#.#.#/#.#.#/#...#/#...#", 'n': "//####./#...#/#...#/#...#/#...#",
 'o': "//.###./#...#/#...#/#...#/.###.", 'p': "//#.##./##..#/#...#/####./#..../#....",
 'q': "//.##.#/#..##/#...#/.####/....#/....#", 'r': "//#.##./##..#/#..../#..../#....",
 's': "//.####/#..../.###./....#/####.", 't': ".#../.#../####/.#../.#../.#../..##",
 'u': "//#...#/#...#/#...#/#...#/.####", 'v': "//#...#/#...#/#...#/.#.#./..#..",
 'w': "//#...#/#...#/#.#.#/#.#.#/.####", 'x': "//#...#/.#.#./..#../.#.#./#...#",
 'y': "//#...#/#...#/#...#/.####/....#/####.", 'z': "//#####/...#./..#../.#.../#####",
 '0': ".###./#...#/#..##/#.#.#/##..#/#...#/.###.", '1': "..#../.##../..#../..#../..#../..#../#####",
 '2': ".###./#...#/....#/..##./.#.../#...#/#####", '3': ".###./#...#/....#/..##./....#/#...#/.###.",
 '4': "...##/..#.#/.#..#/#...#/#####/....#/....#", '5': "#####/#..../####./....#/....#/#...#/.###.",
 '6': "..##./.#.../#..../####./#...#/#...#/.###.", '7': "#####/#...#/....#/...#./..#../..#../..#..",
 '8': ".###./#...#/#...#/.###./#...#/#...#/.###.", '9': ".###./#...#/#...#/.####/....#/...#./.##..",
 '!': "#/#/#/#/#//#", '"': "#.#/#.#", '#': ".#.#./#####/.#.#./.#.#./#####/.#.#.",
 '$': "..#../.####/#..../.###./....#/####./..#..", '%': "#...#/#..#./...#./..#../.#.../.#..#/#...#",
 '&': "..#../.#.#./..#../.##.#/#..#./#..#./.##.#", "'": "#/#", '(': "..#/.#./#../#../#../.#./..#",
 ')': "#../.#./..#/..#/..#/.#./#..", '*': "//#..#/.##./#..#", '+': "/..#../..#../#####/..#../..#..",
 ',': "/////#/#/#", '-': "///#####", '.': "/////#/#", '/': "....#/...#./...#./..#../.#.../.#.../#....",
 ':': "/#/#///#/#", ';': "/#/#///#/#/#", '<': "...#/..#./.#../#.../.#../..#./...#",
 '=': "//#####///#####", '>': "#.../.#../..#./...#/..#./.#../#...", '?': ".###./#...#/....#/...#./..#..//..#..",
 '@': ".####./#....#/#.##.#/#.#..#/#.####/#...../.####.", '[': "###/#../#../#../#../#../###",
 ']': "###/..#/..#/..#/..#/..#/###", '\\': "#..../.#.../.#.../..#../...#./...#./....#",
 '^': "..#../.#.#./#...#", '_': "///////#####", '`': "#./.#", '{': "..##/.#../.#../#.../.#../.#../..##",
 '|': "#/#/#/#/#/#/#/#", '}': "##../..#./..#./...#/..#./..#./##..", '~': "///.##..#/#..##.",
}
font = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
for ch, enc in G.items():
    code = ord(ch)
    ox, oy = (code % 16) * 8, (code // 16) * 8
    for y, row in enumerate(enc.split("/")):
        for x, c in enumerate(row):
            if c == "#" and y < 8 and x < 8:
                font.putpixel((ox + x, oy + y), (255, 255, 255, 255))
# Latin-1 accented letters (0xC0-0xFF) built from the base letters plus an accent drawn above (or a cedilla below):
# enough for German, French, Spanish, Italian, Portuguese and the Nordic languages. Capitals lose one middle row
# to make room for the accent.
ACC = {"grave": ["#.", ".#"], "acute": [".#", "#."], "circ": [".#.", "#.#"], "uml": ["#.#"], "tilde": [".#.#", "#.#."], "ring": [".#.", "#.#", ".#."]}
LAT = {0xC0: ("A", "grave"), 0xC1: ("A", "acute"), 0xC2: ("A", "circ"), 0xC3: ("A", "tilde"), 0xC4: ("A", "uml"), 0xC5: ("A", "ring"),
       0xC7: ("C", "cedilla"), 0xC8: ("E", "grave"), 0xC9: ("E", "acute"), 0xCA: ("E", "circ"), 0xCB: ("E", "uml"),
       0xCC: ("I", "grave"), 0xCD: ("I", "acute"), 0xCE: ("I", "circ"), 0xCF: ("I", "uml"), 0xD1: ("N", "tilde"),
       0xD2: ("O", "grave"), 0xD3: ("O", "acute"), 0xD4: ("O", "circ"), 0xD5: ("O", "tilde"), 0xD6: ("O", "uml"),
       0xD9: ("U", "grave"), 0xDA: ("U", "acute"), 0xDB: ("U", "circ"), 0xDC: ("U", "uml"), 0xDD: ("Y", "acute")}
for k, (b, a) in list(LAT.items()): LAT[k + 0x20] = (b.lower(), a)
LAT[0xFF] = ("y", "uml")
def glyph_rows(ch):
    rows = [list(r) for r in G[ch].split("/")]
    while len(rows) < 7: rows.append([])
    return rows
def put_glyph(code, rows):
    ox, oy = (code % 16) * 8, (code // 16) * 8
    for y, row in enumerate(rows[:8]):
        for x, c in enumerate(row[:8]):
            if c == "#": font.putpixel((ox + x, oy + y), (255, 255, 255, 255))
for code, (base, acc) in LAT.items():
    rows = glyph_rows(base)
    w = max((len(r) for r in rows), default=1)
    if acc == "cedilla":
        rows = rows[:7] + [list(("." * (w // 2)) + "#")]
        put_glyph(code, rows); continue
    mark = ACC[acc]
    if base.isupper() or base in "bdfhklt":   # tall letter: drop one middle row, the accent goes on top
        body = rows[:3] + rows[4:7]
        top = len(mark)
        out = [[] for _ in range(8)]
        for i, m in enumerate(mark[:2] if top > 2 else mark):
            pad = max(0, (w - len(m)) // 2); out[i] = list("." * pad + m)
        for i, r in enumerate(body): 
            if top + i < 8: out[top + i] = r
        put_glyph(code, out)
    else:   # short letter: rows 0-1 are free above it
        out = [list(r) for r in rows]
        if base == "i": out[0] = []   # dotless i under the accent
        while len(out) < 8: out.append([])
        m = mark[-2:] if len(mark) > 2 else mark
        for i, mr in enumerate(m):
            pad = max(0, (w - len(mr)) // 2)
            out[i] = list("." * pad + mr) if not any(c == "#" for c in out[i]) else out[i]
        put_glyph(code, out)
put_glyph(0xDF, [list(r) for r in ".##./#..#/#.#./#..#/#..#/#.#./#...".split("/")])   # sharp s
put_glyph(0xB0, [list(r) for r in ".#./#.#/.#.".split("/")])                           # degree
put_glyph(0xAB, [list(r) for r in "//.#.#/#.#./.#.#".split("/")])                     # guillemets
put_glyph(0xBB, [list(r) for r in "//#.#./.#.#/#.#.".split("/")])
put_glyph(0xBF, [list(r) for r in "..#..//..#../.#.../#...#/#...#/.###.".split("/")]) # inverted ?
put_glyph(0xA1, [list(r) for r in "#//#/#/#/#/#".split("/")])                         # inverted !
# special glyph 1 (\x01) = heart for chat, 2 = cursor block
for y, row in enumerate(HEART[:8]):
    for x, c in enumerate(row[:7]):
        if c == "x": font.putpixel((8 + x, y), (255, 255, 255, 255))
font.save(os.path.join(OUT, "font.png"))

# --------------------------------------------------------------------------------------------
# Particles (particles.png 64x64, 8x8 cells)
# --------------------------------------------------------------------------------------------
parts = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
def pcell(i, f):
    rnd = random.Random(i * 77)
    ox, oy = (i % 8) * 8, (i // 8) * 8
    for y in range(8):
        for x in range(8):
            c = f(x, y, rnd)
            if c: parts.putpixel((ox + x, oy + y), c)
for s in range(8):   # smoke/puff, radius grows with index
    r = 1.2 + s * 0.42
    pcell(s, lambda x, y, rnd, r=r: (255, 255, 255, 255) if math.hypot(x - 3.5, y - 3.5) <= r and rnd.random() > 0.12 else None)
pcell(8, lambda x, y, r: (255, 255, 255, 255) if (x == 3 or y == 3 or x == y or x + y == 7) and math.hypot(x-3.5, y-3.5) < 4 else None)  # crit
pcell(9, lambda x, y, r: rgba(0xff3030) if y < 7 and HEART[y][x] == "x" else None)  # heart
pcell(10, lambda x, y, r: (255, clamp(220 - y * 18 - abs(x - 3.5) * 25), 40, 255) if abs(x - 3.5) < (y * 0.45 + 0.5) and y > 0 else None)  # flame
pcell(11, lambda x, y, r: ((200, 230, 255, 255) if math.hypot(x-3.5, y-3.5) > 1.8 else (120, 170, 255, 120)) if math.hypot(x - 3.5, y - 3.5) <= 3 else None)  # bubble
pcell(12, lambda x, y, r: (90, 140, 255, 220) if math.hypot(x - 3.5, y - 4.5) <= 2.2 else None)  # splash drop
def xp(x, y, r):
    d = math.hypot(x - 3.5, y - 3.5)
    if d > 3.6: return None
    if d > 2.6: return (40, 90, 0, 255)
    return mix(rgba(0xfff86a), rgba(0x8fe028), d / 2.6)
pcell(13, xp)
pcell(14, lambda x, y, r: rgba(0x401010) if y < 7 and HEART[y][x] == "x" else None)  # damage indicator heart
pcell(15, lambda x, y, r: (255, 255, 255, 255) if math.hypot(x - 3.5, y - 3.5) <= 3.6 and r.random() > 0.35 else None)  # explosion
parts.save(os.path.join(OUT, "particles.png"))

# --------------------------------------------------------------------------------------------
# Mob models + skins.  Model space: y up (pixels, feet at 0), mob faces +Z, its right side is -X.
# Box UV layout (identical to Minecraft's):
#   top (u+d, v)  bottom (u+d+w, v)  right(-X) (u, v+d)  front(+Z) (u+d, v+d)  left(+X) (u+d+w, v+d)  back (u+2d+w, v+d)
# --------------------------------------------------------------------------------------------
# role ids must match enum PartRole in src/entity/model.h
R = {"BODY": 0, "HEAD": 1, "ARM_R": 2, "ARM_L": 3, "LEG_R": 4, "LEG_L": 5, "LEG_FR": 6, "LEG_FL": 7,
     "LEG_BR": 8, "LEG_BL": 9, "WING_R": 10, "WING_L": 11, "SPIDER_LEG": 12}

def P(role, uv, box, size, pivot, rot=(0, 0, 0), idx=0):
    return dict(role=R[role], u=uv[0], v=uv[1], box=box, size=size, pivot=pivot, rot=rot, idx=idx)

def humanoid(limb):
    a = limb
    return [
        P("HEAD", (0, 0), (-4, 0, -4), (8, 8, 8), (0, 24, 0)),
        P("BODY", (16, 16), (-4, 0, -2), (8, 12, 4), (0, 12, 0)),
        P("ARM_R", (40, 16), (-a + 1 if a == 2 else -3, -10, -a / 2), (a, 12, a), (-5, 22, 0)),
        P("ARM_L", (40, 16), (-1, -10, -a / 2), (a, 12, a), (5, 22, 0)),
        P("LEG_R", (0, 16), (-a / 2, -12, -a / 2), (a, 12, a), (-2, 12, 0)),
        P("LEG_L", (0, 16), (-a / 2, -12, -a / 2), (a, 12, a), (2, 12, 0)),
    ]

MODELS = {
    "humanoid": (64, 32, humanoid(4)),
    "skeleton": (64, 32, humanoid(2)),
    "creeper": (64, 32, [
        P("HEAD", (0, 0), (-4, 0, -4), (8, 8, 8), (0, 18, 0)),
        P("BODY", (16, 16), (-4, 0, -2), (8, 12, 4), (0, 6, 0)),
        P("LEG_FL", (0, 16), (-2, -6, -2), (4, 6, 4), (2, 6, 4)),
        P("LEG_FR", (0, 16), (-2, -6, -2), (4, 6, 4), (-2, 6, 4)),
        P("LEG_BL", (0, 16), (-2, -6, -2), (4, 6, 4), (2, 6, -4)),
        P("LEG_BR", (0, 16), (-2, -6, -2), (4, 6, 4), (-2, 6, -4)),
    ]),
    "cow": (64, 64, [
        P("HEAD", (0, 0), (-4, -4, 0), (8, 8, 6), (0, 20, 9)),
        P("HEAD", (44, 0), (-5, 3, 1), (1, 3, 1), (0, 20, 9)),
        P("HEAD", (44, 0), (4, 3, 1), (1, 3, 1), (0, 20, 9)),
        P("BODY", (0, 32), (-6, 12, -9), (12, 10, 18), (0, 0, 0)),
        P("LEG_FL", (28, 0), (-2, -12, -2), (4, 12, 4), (4, 12, 6)),
        P("LEG_FR", (28, 0), (-2, -12, -2), (4, 12, 4), (-4, 12, 6)),
        P("LEG_BL", (28, 0), (-2, -12, -2), (4, 12, 4), (4, 12, -7)),
        P("LEG_BR", (28, 0), (-2, -12, -2), (4, 12, 4), (-4, 12, -7)),
    ]),
    "pig": (64, 64, [
        P("HEAD", (0, 0), (-4, -4, 0), (8, 8, 8), (0, 12, 7)),
        P("HEAD", (32, 0), (-2, -4, 8), (4, 3, 1), (0, 12, 7)),
        P("BODY", (0, 32), (-5, 6, -8), (10, 8, 16), (0, 0, 0)),
        P("LEG_FL", (40, 0), (-2, -6, -2), (4, 6, 4), (3, 6, 5)),
        P("LEG_FR", (40, 0), (-2, -6, -2), (4, 6, 4), (-3, 6, 5)),
        P("LEG_BL", (40, 0), (-2, -6, -2), (4, 6, 4), (3, 6, -6)),
        P("LEG_BR", (40, 0), (-2, -6, -2), (4, 6, 4), (-3, 6, -6)),
    ]),
    "sheep": (64, 64, [
        P("HEAD", (0, 0), (-3, -3, 0), (6, 6, 8), (0, 18, 7)),
        P("BODY", (0, 32), (-5, 11, -8), (10, 10, 16), (0, 0, 0)),
        P("LEG_FL", (40, 0), (-2, -12, -2), (4, 12, 4), (3, 12, 5)),
        P("LEG_FR", (40, 0), (-2, -12, -2), (4, 12, 4), (-3, 12, 5)),
        P("LEG_BL", (40, 0), (-2, -12, -2), (4, 12, 4), (3, 12, -6)),
        P("LEG_BR", (40, 0), (-2, -12, -2), (4, 12, 4), (-3, 12, -6)),
    ]),
    "chicken": (64, 32, [
        P("HEAD", (0, 0), (-2, 0, -1), (4, 6, 3), (0, 9, 4)),
        P("HEAD", (14, 0), (-2, 2, 2), (4, 2, 2), (0, 9, 4)),
        P("HEAD", (14, 4), (-1, 0, 2), (2, 2, 2), (0, 9, 4)),
        P("BODY", (0, 9), (-3, 5, -4), (6, 6, 8), (0, 0, 0)),
        P("LEG_R", (26, 0), (-1, -5, -1), (3, 5, 3), (-2, 5, 0)),
        P("LEG_L", (26, 0), (-1, -5, -1), (3, 5, 3), (2, 5, 0)),
        P("WING_R", (28, 9), (-1, -4, -3), (1, 4, 6), (-3, 10, 0)),
        P("WING_L", (28, 9), (0, -4, -3), (1, 4, 6), (3, 10, 0)),
    ]),
    "spider": (64, 64, [
        P("HEAD", (32, 4), (-4, -4, 0), (8, 8, 8), (0, 9, 3)),
        P("BODY", (0, 0), (-3, -3, -3), (6, 6, 6), (0, 9, 0)),
        P("BODY", (0, 32), (-5, -4, -12), (10, 8, 12), (0, 9, -3)),
    ] + [P("SPIDER_LEG", (24, 0), (-1, -1, -1) if s > 0 else (-15, -1, -1), (16, 2, 2), (4 * s, 9, z),
           (0, s * yaw, -s * 35), idx=i)
         for i, (s, z, yaw) in enumerate([(1, 2, 45), (-1, 2, -45), (1, 1, 20), (-1, 1, -20),
                                           (1, 0, -20), (-1, 0, 20), (1, -1, -45), (-1, -1, 45)])]),
    "arrow": (32, 32, [
        P("BODY", (0, 0), (-0.5, -0.5, -7), (1, 1, 14), (0, 0, 0)),
        P("BODY", (0, 16), (-1.5, -1.5, -7), (3, 3, 2), (0, 0, 0)),
    ]),
}

# ---------- skin painters ----------
def paint_box(img, part, painter, rnd):
    u, v = part["u"], part["v"]
    w, h, d = [int(round(s)) for s in part["size"]]
    w, h, d = max(w, 1), max(h, 1), max(d, 1)
    faces = {"top": (u + d, v, w, d), "bottom": (u + d + w, v, w, d), "right": (u, v + d, d, h),
             "front": (u + d, v + d, w, h), "left": (u + d + w, v + d, d, h), "back": (u + 2 * d + w, v + d, w, h)}
    for name, (fx, fy, fw, fh) in faces.items():
        for y in range(fh):
            for x in range(fw):
                c = painter(part, name, x, y, fw, fh, rnd)
                if c is not None and 0 <= fx + x < img.size[0] and 0 <= fy + y < img.size[1]:
                    img.putpixel((fx + x, fy + y), c)

def skin(model_name, painter, out_name, seed=1):
    tw, th, plist = MODELS[model_name]
    img = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    rnd = random.Random(seed)
    for p in plist:
        paint_box(img, p, painter, rnd)
    img.save(os.path.join(OUT, "entity", out_name))

def N(c, rnd, v=0.1): return shade(c, 1 + rnd.uniform(-v, v))

def skeleton_p(p, f, x, y, w, h, r):
    bone = rgba(0xc8c8c4)
    if p["role"] == R["HEAD"] and f == "front":
        if y in (3, 4) and x in (1, 2, 5, 6): return rgba(0x1a1a1a)
        if y == 5 and x in (3, 4): return rgba(0x3a3a3a)
        if y == 6 and 1 <= x <= 6: return rgba(0x2a2a2a) if x % 2 else rgba(0x9a9a96)
    if p["role"] == R["BODY"] and f in ("front", "back"):
        if y % 3 == 1 and y < 9 and 1 <= x <= 6: return rgba(0x3a3a3a)
        if x in (3, 4) and y < 12: return N(bone, r, 0.05)
        if y >= 9: return N(bone, r, 0.08)
        return rgba(0x2a2a2a) if y % 3 != 1 else N(bone, r)
    return N(bone, r, 0.08)

def creeper_p(p, f, x, y, w, h, r):
    g = r.choice([rgba(0x0da70b), rgba(0x5ec95a), rgba(0x46b443), rgba(0x8fdc8a), rgba(0x2f8a2d)])
    if p["role"] == R["HEAD"] and f == "front":
        face = ["........", "........", ".##..##.", ".##..##.", "...##...", "..####..", "..####..", "..#..#.."]
        if face[y][x] == "#": return rgba(0x101010)
    if p["role"] in (R["LEG_FL"], R["LEG_FR"], R["LEG_BL"], R["LEG_BR"]) and f == "bottom": return rgba(0x2a2a2a)
    return g

def cow_p(p, f, x, y, w, h, r):
    brown, white = rgba(0x4a3018), rgba(0xe8e8e8)
    if p["size"] == (1, 3, 1): return N(rgba(0xd0d0c8), r)
    if p["role"] == R["HEAD"]:
        if f == "front":
            if y >= 5 and 1 <= x <= 6: return N(rgba(0xd8a8a0), r, 0.05) if not (y == 6 and x in (2, 5)) else rgba(0x3a2a2a)
            if y == 2 and x in (1, 6): return rgba(0x101010)
            if y == 2 and x in (2, 5): return white
            if 2 <= x <= 5: return N(white, r, 0.05)
        return N(brown, r)
    if p["role"] != R["BODY"]:
        if f in ("bottom",) or y >= h - 2: return rgba(0x2a2a2a)
        return N(white if (x + y) % 5 == 0 else brown, r)
    n = math.sin(x * 0.7 + p["u"]) + math.cos(y * 0.6 + hash(f) % 5)
    return N(white if n > 0.6 else brown, r, 0.08)

def pig_p(p, f, x, y, w, h, r):
    pink = rgba(0xf0a0a0)
    if p["size"] == (4, 3, 1):
        if f == "front": return rgba(0x8a3a3a) if y == 1 and x in (0, 3) else N(rgba(0xe08080), r, 0.04)
        return N(rgba(0xe08080), r, 0.04)
    if p["role"] == R["HEAD"] and f == "front":
        if y == 2 and x in (1, 6): return rgba(0xffffff)
        if y == 2 and x in (2, 5): return rgba(0x101010)
    if p["role"] not in (R["HEAD"], R["BODY"]) and (y >= h - 1 or f == "bottom"): return rgba(0x6a4a3a)
    return N(pink, r, 0.06)

def sheep_p(p, f, x, y, w, h, r):
    wool = rgba(0xeeeeee)
    if p["role"] == R["HEAD"]:
        if f == "front":
            if y >= 1 and 1 <= x <= 4:
                if y == 2 and x in (1, 4): return rgba(0x101010)
                return N(rgba(0xd8bca8), r, 0.05)
        return N(wool, r, 0.07)
    if p["role"] == R["BODY"]:
        return N(wool, r, 0.07) if r.random() > 0.08 else rgba(0xd8d8d8)
    if y < 5: return N(wool, r, 0.06)
    return rgba(0x3a2a2a) if y >= h - 1 else N(rgba(0xd8bca8), r, 0.05)

def chicken_p(p, f, x, y, w, h, r):
    if p["u"] == 14 and p["v"] == 0: return N(rgba(0xf0a020), r, 0.05)
    if p["u"] == 14 and p["v"] == 4: return N(rgba(0xd02020), r, 0.05)
    if p["role"] in (R["LEG_R"], R["LEG_L"]): return N(rgba(0xe8a020), r, 0.05)
    if p["role"] == R["HEAD"] and f in ("left", "right") and y == 1 and x == 1: return rgba(0x101010)
    return N(rgba(0xf4f4f4), r, 0.04)

def spider_p(p, f, x, y, w, h, r):
    dark = rgba(0x2c2420)
    if p["role"] == R["HEAD"] and f == "front":
        eyes = {(1, 2), (2, 2), (5, 2), (6, 2), (2, 4), (5, 4), (3, 3), (4, 3)}
        if (x, y) in eyes: return rgba(0xc81010)
    if p["role"] == R["BODY"] and p["v"] == 32 and f == "top":
        if abs(x - w / 2 + 0.5) < 1.5 and y % 3 == 0: return rgba(0x6a1a1a)
    return N(dark, r, 0.25)

def player_p(p, f, x, y, w, h, r):
    sk = rgba(0xc69680)
    if p["role"] == R["HEAD"]:
        if f == "top": return N(rgba(0x3a2210), r)
        if f == "front":
            if y < 2: return N(rgba(0x3a2210), r)
            if y == 4 and x in (1, 6): return rgba(0xffffff)
            if y == 4 and x in (2, 5): return rgba(0x3a3aa0)
            if y == 6 and 2 <= x <= 5: return rgba(0x8a5a4a)
            return N(sk, r, 0.04)
        if y < 3 or f == "back": return N(rgba(0x3a2210), r)
        return N(sk, r, 0.04)
    if p["role"] == R["BODY"]: return N(rgba(0x10a8a8), r, 0.06)
    if p["role"] in (R["ARM_R"], R["ARM_L"]): return N(rgba(0x10a8a8), r, 0.06) if y < 4 else N(sk, r, 0.04)
    return N(rgba(0x3a3aa0), r, 0.06) if y < h - 1 else rgba(0x4a4a4a)

def arrow_p(p, f, x, y, w, h, r):
    if p["v"] == 16: return N(rgba(0xf0f0f0), r, 0.05)
    if f in ("front",): return rgba(0x9a9a9a)
    return N(rgba(0x8a6436), r, 0.08)

skin("skeleton", skeleton_p, "skeleton.png", 2)
skin("creeper", creeper_p, "creeper.png", 3)
skin("cow", cow_p, "cow.png", 4)
skin("pig", pig_p, "pig.png", 5)
skin("sheep", sheep_p, "sheep.png", 6)
skin("chicken", chicken_p, "chicken.png", 7)
skin("spider", spider_p, "spider.png", 8)
skin("humanoid", player_p, "player.png", 9)
skin("arrow", arrow_p, "arrow.png", 10)

# ---------- blocks, items, mobs and models now come from gen_content.py ----------
import subprocess, sys as _sys
subprocess.check_call([_sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_content.py")])
raise SystemExit(0)
# ---------- (legacy) emit C model table ----------
hdr = ["// AUTO-GENERATED by tools/gen_assets.py - do not edit by hand.",
       "// Box geometry for every mob model; the same table drives skin painting.",
       "#ifndef MODEL_DEFS_H", "#define MODEL_DEFS_H", '#include "model.h"', ""]
order = []
for name, (tw, th, plist) in MODELS.items():
    cname = "MODEL_PARTS_" + name.upper()
    hdr.append(f"static const ModelPartDef {cname}[] = {{")
    for p in plist:
        fl = lambda vals: ", ".join("%.4ff" % float(v) for v in vals)
        hdr.append("    {%d, %d, %d, {%s}, {%s}, {%s}, {%s}, %d}," % (
            p["role"], p["u"], p["v"], fl(p["box"]), fl(p["size"]), fl(p["pivot"]), fl(p["rot"]), p["idx"]))
    hdr.append("};")
    order.append((name, tw, th, cname, len(plist)))
hdr.append("")
hdr.append("// indexed by enum ModelId (model.h)")
hdr.append("static const ModelDef MODEL_DEFS[] = {")
for (name, tw, th, cname, n) in order:
    hdr.append(f'    {{"{name}", {tw}, {th}, {cname}, {n}}},')
hdr.append("};")
hdr.append("#endif")
with open(os.path.join(ROOT, "src", "entity", "model_defs.h"), "w") as fh:
    fh.write("\n".join(hdr) + "\n")
print("assets generated:", ", ".join(sorted(os.listdir(OUT))), "| models:", ", ".join(n for n, *_ in order))
