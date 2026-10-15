"""Procedural 16x16 pixel-art painters used for every block tile and item icon.

Everything here is drawn from scratch (no external art) so the look is consistent
across the ~1500 textures the content generator produces.  Each painter returns a
16x16 list-of-rows of RGBA tuples; `to_image` turns that into a PIL image.
"""
import random, math, hashlib
from PIL import Image

def clamp(v): return max(0, min(255, int(v)))
def rgb(h, a=255):
    if isinstance(h, tuple): return h if len(h) == 4 else h + (a,)
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255, a)
def shade(c, f): return (clamp(c[0] * f), clamp(c[1] * f), clamp(c[2] * f), c[3])
def mix(a, b, t): return (clamp(a[0] * (1 - t) + b[0] * t), clamp(a[1] * (1 - t) + b[1] * t), clamp(a[2] * (1 - t) + b[2] * t), clamp(a[3] * (1 - t) + b[3] * t))
def lum(c): return (c[0] * 0.3 + c[1] * 0.59 + c[2] * 0.11) / 255.0
CLEAR = (0, 0, 0, 0)

def seed_of(*parts):
    return int(hashlib.md5("|".join(str(p) for p in parts).encode()).hexdigest()[:8], 16)

class Canvas:
    def __init__(self, fill=CLEAR, seed=0):
        self.p = [[fill] * 16 for _ in range(16)]
        self.r = random.Random(seed)
    def set(self, x, y, c):
        if 0 <= x < 16 and 0 <= y < 16: self.p[y][x] = c
    def get(self, x, y): return self.p[y][x] if 0 <= x < 16 and 0 <= y < 16 else CLEAR
    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): self.set(x, y, c)
    def frame(self, x0, y0, x1, y1, c):
        for x in range(x0, x1 + 1): self.set(x, y0, c); self.set(x, y1, c)
        for y in range(y0, y1 + 1): self.set(x0, y, c); self.set(x1, y, c)
    def noise(self, base, var=0.1, x0=0, y0=0, x1=15, y1=15):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): self.set(x, y, shade(base, 1 + self.r.uniform(-var, var)))
    def speckle(self, c, n, size=1):
        for _ in range(n):
            x, y = self.r.randint(0, 15), self.r.randint(0, 15)
            for dy in range(size):
                for dx in range(size): self.set(x + dx, y + dy, shade(c, self.r.uniform(0.85, 1.15)))
    def blobs(self, c, n, rmin=1, rmax=2):
        for _ in range(n):
            cx, cy, rr = self.r.randint(0, 15), self.r.randint(0, 15), self.r.uniform(rmin, rmax)
            for y in range(16):
                for x in range(16):
                    if (x - cx) ** 2 + (y - cy) ** 2 <= rr * rr: self.set(x, y, shade(c, self.r.uniform(0.9, 1.1)))
    def outline(self, c):
        """Dark outline around every opaque pixel (for item icons)."""
        src = [row[:] for row in self.p]
        for y in range(16):
            for x in range(16):
                if src[y][x][3] == 0:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        xx, yy = x + dx, y + dy
                        if 0 <= xx < 16 and 0 <= yy < 16 and src[yy][xx][3] > 0 and src[yy][xx] != c:
                            self.p[y][x] = c; break
    def map(self, f):
        for y in range(16):
            for x in range(16): self.p[y][x] = f(x, y, self.p[y][x])
    def draw_mask(self, rows, pal, ox=0, oy=0):
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch in pal: self.set(x + ox, y + oy, pal[ch])
    def image(self):
        im = Image.new("RGBA", (16, 16))
        im.putdata([c for row in self.p for c in row])
        return im

# ------------------------------------------------------------------------------------------------
# Block tile painters.  Each takes (canvas, *params).
# ------------------------------------------------------------------------------------------------
def p_flat(c, col, var=0.06): c.noise(rgb(col), var)
def p_stone(c, col, var=0.1):
    base = rgb(col)
    c.noise(base, var * 0.6)
    c.blobs(shade(base, 0.86), 6, 0.6, 1.8)
    c.blobs(shade(base, 1.1), 4, 0.5, 1.2)
def p_cobble(c, col):
    base = rgb(col)
    c.noise(shade(base, 0.7), 0.05)
    for _ in range(9):
        cx, cy = c.r.randint(0, 15), c.r.randint(0, 15)
        w, h = c.r.randint(2, 4), c.r.randint(2, 3)
        f = c.r.uniform(0.9, 1.2)
        for y in range(cy, cy + h):
            for x in range(cx, cx + w):
                c.set(x % 16, y % 16, shade(base, f * c.r.uniform(0.95, 1.05)))
def p_ore(c, stone_col, ore_col, deep=False):
    (p_deepslate if deep else p_stone)(c, stone_col)
    oc = rgb(ore_col)
    for _ in range(5):
        cx, cy = c.r.randint(2, 13), c.r.randint(2, 13)
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (-1, 0), (0, -1)):
            if c.r.random() < 0.75: c.set(cx + dx, cy + dy, shade(oc, c.r.uniform(0.8, 1.2)))
        c.set(cx + 1, cy + 1, shade(oc, 0.6))
def p_deepslate(c, col):
    base = rgb(col)
    for y in range(16):
        band = 1.0 + 0.08 * math.sin(y * 1.3)
        for x in range(16): c.set(x, y, shade(base, band * c.r.uniform(0.9, 1.08)))
    for _ in range(6):
        y, x = c.r.randint(0, 15), c.r.randint(0, 12)
        for k in range(c.r.randint(2, 5)): c.set(x + k, y, shade(base, 0.75))
def p_dirt(c, col):
    base = rgb(col)
    c.noise(base, 0.12)
    c.speckle(shade(base, 0.7), 14); c.speckle(shade(base, 1.25), 8)
def p_sand(c, col):
    base = rgb(col)
    c.noise(base, 0.05); c.speckle(shade(base, 0.85), 18); c.speckle(shade(base, 1.08), 10)
def p_gravel(c, col):
    base = rgb(col)
    c.noise(base, 0.1)
    for _ in range(12):
        f = c.r.choice((0.6, 0.75, 1.2, 1.35))
        x, y = c.r.randint(0, 14), c.r.randint(0, 14)
        c.rect(x, y, x + 1, y + c.r.randint(0, 1), shade(base, f))
def p_grass_top(c, col=0x9a9a9a):
    base = rgb(col)
    c.noise(base, 0.14); c.speckle(shade(base, 0.8), 20); c.speckle(shade(base, 1.15), 12)
def p_grass_side(c, dirt, top, tinted=True):
    p_dirt(c, dirt)
    t = rgb(top)
    for x in range(16):
        h = 3 + (c.r.random() < 0.4) + (c.r.random() < 0.2)
        for y in range(h): c.set(x, y, shade(t, c.r.uniform(0.85, 1.12)))
def p_grass_overlay(c, col=0x9a9a9a):
    t = rgb(col)
    for x in range(16):
        h = 3 + (c.r.random() < 0.4) + (c.r.random() < 0.2)
        for y in range(h): c.set(x, y, shade(t, c.r.uniform(0.85, 1.12)))
def p_planks(c, col):
    base = rgb(col)
    c.noise(base, 0.05)
    for y in (3, 7, 11, 15):
        for x in range(16): c.set(x, y, shade(base, 0.68))
    for row, off in enumerate((5, 12, 2, 9)):
        y0 = row * 4
        for y in range(y0, y0 + 3): c.set(off, y, shade(base, 0.72))
    for _ in range(10):
        x, y = c.r.randint(0, 15), c.r.randint(0, 15)
        if y % 4 != 3: c.set(x, y, shade(base, 0.88))
def p_log_side(c, col, stripe=None, stripped=False):
    base = rgb(col)
    for x in range(16):
        f = 1.0 + (0.12 if x % 4 == 0 else 0) - (0.15 if x % 5 == 2 else 0)
        for y in range(16): c.set(x, y, shade(base, f * c.r.uniform(0.92, 1.06)))
    if stripe is not None:   # birch-like dark marks
        s = rgb(stripe)
        for _ in range(7):
            x, y = c.r.randint(0, 13), c.r.randint(0, 15)
            for k in range(c.r.randint(2, 4)): c.set(x + k, y, s)
    if stripped:
        c.frame(0, 0, 15, 15, shade(base, 0.85))
def p_log_top(c, bark, inner):
    b, i = rgb(bark), rgb(inner)
    for y in range(16):
        for x in range(16):
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d > 6.5: col = shade(b, c.r.uniform(0.9, 1.05))
            else: col = shade(i, (0.86 if int(d) % 2 == 0 else 1.0) * c.r.uniform(0.95, 1.04))
            c.set(x, y, col)
def p_leaves(c, col, flowers=None, density=0.78):
    base = rgb(col)
    for y in range(16):
        for x in range(16):
            if c.r.random() < density: c.set(x, y, shade(base, c.r.uniform(0.65, 1.2)))
    if flowers is not None:
        for _ in range(7):
            c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(rgb(flowers), c.r.uniform(0.9, 1.1)))
def p_bricks(c, col, mortar=0xa09a92, bw=8, bh=4):
    base, m = rgb(col), rgb(mortar)
    c.noise(m, 0.05)
    for row in range(16 // bh):
        off = (bw // 2) * (row % 2)
        for bx in range(-1, 16 // bw + 1):
            x0 = bx * bw + off
            f = c.r.uniform(0.88, 1.1)
            for y in range(row * bh, row * bh + bh - 1):
                for x in range(x0, x0 + bw - 1): c.set(x, y, shade(base, f * c.r.uniform(0.95, 1.05)))
def p_tiles(c, col):
    base = rgb(col)
    c.noise(shade(base, 0.6), 0.04)
    for ty in range(4):
        for tx in range(4):
            f = c.r.uniform(0.9, 1.1)
            c.rect(tx * 4, ty * 4, tx * 4 + 2, ty * 4 + 2, shade(base, f))
def p_polished(c, col):
    base = rgb(col)
    c.noise(base, 0.04)
    c.frame(0, 0, 15, 15, shade(base, 0.75))
    c.frame(1, 1, 14, 14, shade(base, 1.12))
def p_chiseled(c, col):
    p_polished(c, col)
    base = rgb(col)
    c.frame(4, 4, 11, 11, shade(base, 0.7)); c.rect(6, 6, 9, 9, shade(base, 0.85)); c.rect(7, 7, 8, 8, shade(base, 1.15))
def p_cracked(c, col, inner):
    (PAINTERS_BY_NAME[inner] if isinstance(inner, str) else inner)(c, col)
    base = rgb(col)
    x, y = c.r.randint(3, 6), 0
    while y < 16:
        c.set(x, y, shade(base, 0.45)); y += 1; x += c.r.choice((-1, 0, 1))
def p_mossy(c, col, inner, moss=0x5f7f2f):
    (PAINTERS_BY_NAME[inner] if isinstance(inner, str) else inner)(c, col)
    m = rgb(moss)
    c.blobs(m, 5, 1, 2.3)
def p_pillar_side(c, col):
    base = rgb(col)
    c.noise(base, 0.05)
    for x in (0, 15): c.rect(x, 0, x, 15, shade(base, 0.78))
    for x in (4, 11): c.rect(x, 0, x, 15, shade(base, 0.92))
def p_metal(c, col):
    base = rgb(col)
    c.noise(base, 0.03)
    c.frame(0, 0, 15, 15, shade(base, 0.7))
    c.rect(1, 1, 14, 1, shade(base, 1.25)); c.rect(1, 1, 1, 14, shade(base, 1.18))
    c.rect(2, 14, 14, 14, shade(base, 0.85)); c.rect(14, 2, 14, 14, shade(base, 0.85))
    for _ in range(5): c.set(c.r.randint(3, 12), c.r.randint(3, 12), shade(base, 1.2))
def p_gemblock(c, col):
    base = rgb(col)
    c.noise(base, 0.05)
    c.frame(0, 0, 15, 15, shade(base, 0.6))
    for i in range(4):
        c.rect(2 + i * 3, 2 + (i % 2) * 6, 3 + i * 3, 5 + (i % 2) * 6, shade(base, 1.3))
def p_wool(c, col):
    base = rgb(col)
    c.noise(base, 0.07)
    for y in range(16):
        for x in range(16):
            if (x + y * 3) % 5 == 0: c.set(x, y, shade(base, 0.9))
def p_concrete(c, col): c.noise(rgb(col), 0.025)
def p_powder(c, col):
    base = rgb(col)
    c.noise(base, 0.12); c.speckle(shade(base, 0.8), 25); c.speckle(shade(base, 1.2), 15)
def p_terracotta(c, col):
    base = rgb(col)
    c.noise(base, 0.06); c.speckle(shade(base, 0.9), 12)
def p_glazed(c, col):
    base = rgb(col)
    c.noise(base, 0.04)
    hi, lo = shade(base, 1.35), shade(base, 0.6)
    for i in range(16):
        c.set(i, i, lo); c.set(15 - i, i, hi)
    c.frame(0, 0, 15, 15, lo); c.rect(6, 6, 9, 9, hi); c.rect(7, 7, 8, 8, rgb(0xffffff))
def p_glass(c, col=0xcfe8ec, alpha=70, frame=0xe8f4f6):
    base = rgb(col, alpha)
    for y in range(16):
        for x in range(16): c.set(x, y, base)
    f = rgb(frame, 230)
    c.frame(0, 0, 15, 15, f)
    for i in range(3): c.set(3 + i, 5 - i, rgb(0xffffff, 200)); c.set(10 + i, 12 - i, rgb(0xffffff, 160))
def p_ice(c, col=0x91b7f7, alpha=170):
    base = rgb(col, alpha)
    for y in range(16):
        for x in range(16): c.set(x, y, shade(base, c.r.uniform(0.92, 1.08)))
    for i in range(4): c.set(4 + i, 8 - i, rgb(0xe8f2ff, alpha)); c.set(9 + i, 13 - i, rgb(0xe8f2ff, alpha))
# ------------------------------------------------------------------------------------------------
# v6 painters: tileable fractal noise gives natural clumping, grain and weathering instead of flat
# per-pixel static.  These definitions replace the simple ones above (PAINTERS is built after them).
# ------------------------------------------------------------------------------------------------
def tnoise(c, period=4, octaves=3, persist=0.5):
    """Tileable fractal value noise field, 16x16 floats in [0,1] (wraps, so tiles join seamlessly)."""
    field = [[0.0] * 16 for _ in range(16)]
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        n = min(16, max(1, period * (2 ** o)))
        grid = [[c.r.random() for _ in range(n)] for _ in range(n)]
        s = 16.0 / n
        for y in range(16):
            for x in range(16):
                gx, gy = x / s, y / s
                x0, y0 = int(gx) % n, int(gy) % n
                x1, y1 = (x0 + 1) % n, (y0 + 1) % n
                fx, fy = gx - int(gx), gy - int(gy)
                fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
                a = grid[y0][x0] * (1 - fx) + grid[y0][x1] * fx
                b = grid[y1][x0] * (1 - fx) + grid[y1][x1] * fx
                field[y][x] += (a * (1 - fy) + b * fy) * amp
        tot += amp
        amp *= persist
    return [[v / tot for v in row] for row in field]

def _tone(c, base, field, spread, jitter=0.03):
    for y in range(16):
        for x in range(16):
            c.set(x, y, shade(base, 1 + (field[y][x] - 0.5) * 2 * spread + c.r.uniform(-jitter, jitter)))

def p_stone(c, col, var=0.1):
    base = rgb(col)
    _tone(c, base, tnoise(c, 3, 3), 0.20, 0.025)
    for _ in range(5):   # hairline fissures
        x, y = c.r.randint(0, 15), c.r.randint(0, 15)
        for _k in range(c.r.randint(2, 4)):
            c.set(x % 16, y % 16, shade(base, 0.78))
            x += c.r.choice((-1, 0, 1, 1)); y += c.r.choice((0, 1))
    c.speckle(shade(base, 1.18), 10)
def p_cobble(c, col):
    base = rgb(col)
    cells = [(c.r.uniform(0, 16), c.r.uniform(0, 16)) for _ in range(9)]
    tone = [c.r.uniform(0.9, 1.22) for _ in cells]
    for y in range(16):
        for x in range(16):
            d1, d2, k = 1e9, 1e9, 0
            for i, (cx, cy) in enumerate(cells):
                for ox in (-16, 0, 16):
                    for oy in (-16, 0, 16):
                        d = math.hypot(x - cx - ox, y - cy - oy)
                        if d < d1: d2, d1, k = d1, d, i
                        elif d < d2: d2 = d
            edge = d2 - d1
            f = tone[k] * (0.55 if edge < 0.9 else (0.88 if edge < 1.7 else 1.04))
            c.set(x, y, shade(base, f * c.r.uniform(0.96, 1.04)))
def p_dirt(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 3), 0.17, 0.05)
    c.speckle(shade(base, 0.62), 10); c.speckle(shade(base, 1.28), 7)
    for _ in range(3):   # small pebbles
        x, y = c.r.randint(1, 13), c.r.randint(1, 14)
        c.rect(x, y, x + 1, y, rgb(0x9a8f84)); c.set(x, y + 1, shade(base, 0.55))
def p_sand(c, col):
    base = rgb(col)
    f = tnoise(c, 4, 2)
    for y in range(16):
        for x in range(16):
            ripple = 0.025 * math.sin((x * 0.6 + y * 1.6) + f[y][x] * 4)
            c.set(x, y, shade(base, 1 + (f[y][x] - 0.5) * 0.12 + ripple + c.r.uniform(-0.04, 0.04)))
    c.speckle(shade(base, 0.82), 12); c.speckle(shade(base, 1.1), 8)
def p_gravel(c, col):
    base = rgb(col)
    c.noise(shade(base, 0.7), 0.06)
    for _ in range(15):
        x, y = c.r.randint(0, 15), c.r.randint(0, 15)
        w, h = c.r.randint(2, 4), c.r.randint(2, 3)
        f = c.r.choice((0.75, 0.9, 1.1, 1.25, 1.4))
        for yy in range(h):
            for xx in range(w):
                edge = (xx in (0, w - 1)) and (yy in (0, h - 1))
                if not edge:
                    shd = 0.82 if yy == h - 1 else (1.12 if yy == 0 else 1.0)
                    c.set((x + xx) % 16, (y + yy) % 16, shade(base, f * shd * c.r.uniform(0.96, 1.04)))
def p_grass_top(c, col=0x9a9a9a):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 3), 0.17, 0.06)
    for _ in range(26):   # blades
        x, y = c.r.randint(0, 15), c.r.randint(0, 14)
        c.set(x, y, shade(base, 1.25)); c.set(x, y + 1, shade(base, 0.72))
def _grass_edge(c, t):
    for x in range(16):
        h = 3 + (c.r.random() < 0.45) + (c.r.random() < 0.25)
        for y in range(h):
            c.set(x, y, shade(t, c.r.uniform(0.9, 1.12) * (0.8 if y == h - 1 else 1)))
        if c.r.random() < 0.35: c.set(x, h, shade(t, 0.7))   # drip into the dirt
def p_grass_side(c, dirt, top, tinted=True):
    p_dirt(c, dirt); _grass_edge(c, rgb(top))
def p_grass_overlay(c, col=0x9a9a9a):
    _grass_edge(c, rgb(col))
def p_planks(c, col):
    base = rgb(col)
    for y in range(16):
        board = y // 4
        for x in range(16):
            grain = 0.06 * math.sin(x * 0.9 + board * 2.3 + math.sin(y * 0.8 + board) * 1.2)
            c.set(x, y, shade(base, 1 + grain + c.r.uniform(-0.035, 0.035) + (board % 2) * 0.03))
    for y in (3, 7, 11, 15):
        for x in range(16): c.set(x, y, shade(base, 0.62))
    for y in (0, 4, 8, 12):
        for x in range(16): c.set(x, y, shade(c.get(x, y), 1.1))
    for row, off in enumerate((5, 12, 2, 9)):
        for y in range(row * 4, row * 4 + 3): c.set(off, y, shade(base, 0.7))
    for _ in range(2):   # knots
        x, y = c.r.randint(1, 14), c.r.randint(0, 3) * 4 + 1
        c.set(x, y, shade(base, 0.6)); c.set(x + 1, y, shade(base, 0.74))
def p_log_side(c, col, stripe=None, stripped=False):
    base = rgb(col)
    f = tnoise(c, 4, 2)
    for x in range(16):
        ridge = 0.14 * math.sin(x * 1.7 + 0.6) + (0.1 if x % 5 == 0 else 0) - (0.12 if x % 7 == 3 else 0)
        for y in range(16):
            v = 1 + ridge + (f[y][x] - 0.5) * 0.18 + c.r.uniform(-0.03, 0.03)
            c.set(x, y, shade(base, v if not stripped else 1 + (v - 1) * 0.45))
    if stripe is not None:
        s = rgb(stripe)
        for _ in range(8):
            x, y = c.r.randint(0, 12), c.r.randint(0, 15)
            for k in range(c.r.randint(2, 5)): c.set(x + k, y, shade(s, c.r.uniform(0.9, 1.1)))
    if stripped: c.frame(0, 0, 15, 15, shade(base, 0.88))
def p_log_top(c, bark, inner):
    b, i = rgb(bark), rgb(inner)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 7.5) * 0.92 + max(abs(x - 7.5), abs(y - 7.5)) * 0.1
            if max(abs(x - 7.5), abs(y - 7.5)) > 6.6: col = shade(b, c.r.uniform(0.85, 1.05) * (0.9 + 0.1 * math.sin(x + y)))
            else: col = shade(i, (0.88 + 0.12 * math.cos(d * 2.1)) * c.r.uniform(0.97, 1.03))
            c.set(x, y, col)
    c.set(7, 7, shade(i, 0.7)); c.set(8, 8, shade(i, 0.75))
def p_leaves(c, col, flowers=None, density=0.78):
    base = rgb(col)
    f = tnoise(c, 4, 2)
    for y in range(16):
        for x in range(16):
            if c.r.random() < density + 0.08:
                c.set(x, y, shade(base, 0.72 + f[y][x] * 0.55 + c.r.uniform(-0.1, 0.1)))
    for y in range(16):          # light-catching rim on leaf clumps
        for x in range(16):
            if c.get(x, y)[3] and not c.get(x, y - 1)[3]: c.set(x, y, shade(c.get(x, y), 1.22))
    if flowers is not None:
        for _ in range(7):
            c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(rgb(flowers), c.r.uniform(0.9, 1.1)))
def p_bricks(c, col, mortar=0xa09a92, bw=8, bh=4):
    base, m = rgb(col), rgb(mortar)
    c.noise(shade(m, 0.92), 0.05)
    for row in range(16 // bh):
        off = (bw // 2) * (row % 2)
        for bx in range(-1, 16 // bw + 1):
            x0 = bx * bw + off
            f = c.r.uniform(0.88, 1.1)
            for y in range(row * bh, row * bh + bh - 1):
                for x in range(x0, x0 + bw - 1):
                    top = 1.14 if y == row * bh else (0.86 if y == row * bh + bh - 2 else 1.0)
                    c.set(x, y, shade(base, f * top * c.r.uniform(0.94, 1.06)))
def p_deepslate(c, col):
    base = rgb(col)
    f = tnoise(c, 3, 2)
    for y in range(16):
        band = 1.0 + 0.09 * math.sin(y * 1.1 + f[y][3] * 3)
        for x in range(16): c.set(x, y, shade(base, band * (0.93 + f[y][x] * 0.14) * c.r.uniform(0.95, 1.05)))
    for _ in range(7):
        y, x = c.r.randint(0, 15), c.r.randint(0, 12)
        for k in range(c.r.randint(2, 6)): c.set(x + k, y, shade(base, 0.7))
def p_ore(c, stone_col, ore_col, deep=False):
    (p_deepslate if deep else p_stone)(c, stone_col)
    oc = rgb(ore_col)
    for _ in range(4):
        cx, cy = c.r.randint(2, 12), c.r.randint(2, 12)
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (-1, 0), (0, -1), (2, 1)):
            if c.r.random() < 0.8:
                c.set(cx + dx, cy + dy, shade(oc, c.r.uniform(0.9, 1.15)))
        c.set(cx, cy, shade(oc, 1.4)); c.set(cx + 1, cy + 1, shade(oc, 0.55))
def p_wool(c, col):
    base = rgb(col)
    f = tnoise(c, 8, 1)
    for y in range(16):
        for x in range(16):
            weave = 0.05 if (x + (y // 2) * 2) % 4 < 2 else -0.04
            c.set(x, y, shade(base, 1 + weave + (f[y][x] - 0.5) * 0.12 + c.r.uniform(-0.03, 0.03)))
def _fixed_noise(period, octaves, seed):
    """Noise field that is identical in every animation frame (so only the waves move, nothing flickers)."""
    return tnoise(Canvas(seed=seed), period, octaves)
def p_liquid(c, col, frame=0, alpha=255):
    base = rgb(col, alpha)
    ph = frame * 2 * math.pi / 16
    f1, f2 = _fixed_noise(2, 3, 4242), _fixed_noise(4, 2, 77)
    for y in range(16):
        for x in range(16):   # every term tiles in 16 px and loops over the 16-frame animation
            warp = (f1[y][x] - 0.5) * 0.35
            w = (math.sin(2 * math.pi * (x / 16 * 2 + y / 16 + warp) + ph) + 0.6 * math.sin(2 * math.pi * (y / 16 * 3 - x / 16 + warp) - ph * 2) +
                 0.3 * math.sin(2 * math.pi * (x / 16 + y / 16 * 2) + ph * 3))
            glint = 0.10 if (w > 1.25 and f2[y][x] > 0.55) else 0.0
            c.set(x, y, shade(base, 1 + w * 0.055 + (f2[y][x] - 0.5) * 0.10 + glint + c.r.uniform(-0.012, 0.012)))
def p_lava(c, frame=0):
    ph = frame * 2 * math.pi / 16
    f1 = _fixed_noise(2, 3, 911)
    for y in range(16):
        for x in range(16):
            warp = (f1[y][x] - 0.5) * 0.45
            w = (math.sin(2 * math.pi * (x / 16 * 2 + y / 16 + warp) + ph) + 0.8 * math.cos(2 * math.pi * (y / 16 * 2 - x / 16 + warp) - ph) +
                 0.4 * math.sin(2 * math.pi * (x / 16 * 3 + y / 16 * 2) + ph * 2))
            t = max(0.0, min(1.0, 0.60 + w * 0.15 + (f1[y][x] - 0.5) * 0.55 + c.r.uniform(-0.03, 0.03)))
            c.set(x, y, mix(mix(rgb(0xb8300a), rgb(0xff8010), min(1.0, t * 1.5)), rgb(0xffe27a), max(0.0, t - 0.62) * 2.4))

# ---- more block painters: machines, containers, plants, nether blocks, portals ----
# ------------------------------------------------------------------------------------------------
# Machines, containers and redstone devices
# ------------------------------------------------------------------------------------------------
def p_crafting_top(c):
    p_planks(c, 0xb8935a)
    dark = rgb(0x5a3a1a)
    c.frame(0, 0, 15, 15, dark); c.frame(1, 1, 14, 14, shade(rgb(0x8a6a3a), 0.9))
    for i in (5, 10):
        for k in range(1, 15): c.set(i, k, dark); c.set(k, i, dark)
    for gx in (1, 6, 11):
        for gy in (1, 6, 11): c.set(gx + 1, gy + 1, shade(rgb(0xd8b878), 1.05))
def p_crafting_side(c, front=False):
    p_planks(c, 0xb8935a)
    c.rect(0, 0, 15, 2, rgb(0x6a4a2a)); c.rect(0, 3, 15, 3, rgb(0x4a2f18))   # overhanging top slab
    for x in range(16): c.set(x, 0, shade(rgb(0x6a4a2a), 1.2))
    if front:   # saw and hammer hanging on the board
        c.rect(3, 5, 5, 12, rgb(0x9a9a9a)); c.rect(3, 5, 3, 12, rgb(0xc8c8c8)); c.rect(5, 5, 5, 12, rgb(0x707070))
        for y in range(6, 12, 2): c.set(4, y, rgb(0x5a5a5a))
        c.rect(8, 7, 9, 13, rgb(0x6a4a2a)); c.rect(8, 7, 10, 8, rgb(0x707070))
    else:
        c.rect(3, 5, 5, 11, rgb(0x9a9a9a)); c.rect(3, 5, 3, 11, rgb(0xc8c8c8))
        c.rect(9, 6, 12, 8, rgb(0x6a4a2a)); c.set(10, 7, rgb(0x4a2f18))
def p_furnace_front(c, col=0x6f6f6f, lit=False, glow=0xff9020):
    p_cobble(c, col)
    c.rect(3, 3, 12, 12, shade(rgb(col), 0.38)); c.frame(3, 3, 12, 12, shade(rgb(col), 0.28))
    c.rect(4, 4, 11, 5, rgb(0x141414)); c.rect(4, 4, 11, 4, rgb(0x0c0c0c))   # vent slot
    c.rect(4, 7, 11, 11, rgb(0x1c1c1c))
    if lit:
        g = rgb(glow)
        c.rect(5, 9, 10, 11, mix(g, rgb(0xffd860), 0.3)); c.rect(6, 8, 9, 8, shade(g, 0.9))
        for x in range(5, 11): c.set(x, 11, shade(g, 1.2));
        for x in (6, 8, 9): c.set(x, 10, rgb(0xffe890))
def p_furnace_top(c, col=0x6f6f6f):
    base = rgb(col)
    c.noise(base, 0.05); c.frame(0, 0, 15, 15, shade(base, 0.62)); c.frame(1, 1, 14, 14, shade(base, 1.14))
    c.rect(4, 4, 11, 11, shade(base, 0.8)); c.frame(4, 4, 11, 11, shade(base, 0.6))
def p_chest(c, col, face="side", latch=0x9a9a9a):
    base = rgb(col)
    p_planks(c, col)
    dk, lt = shade(base, 0.55), shade(base, 1.15)
    c.frame(0, 0, 15, 15, dk)
    if face == "top":
        c.frame(1, 1, 14, 14, shade(base, 0.8))
        for y in (2, 13):
            for x in range(2, 14): c.set(x, y, lt if y == 2 else shade(base, 0.85))
        return
    c.rect(1, 1, 14, 1, lt)
    for x in range(16): c.set(x, 5, dk)               # lid seam
    for x in range(1, 15): c.set(x, 6, shade(base, 0.85))
    if face == "front":
        c.rect(7, 4, 8, 7, rgb(latch)); c.rect(7, 4, 8, 4, shade(rgb(latch), 1.3)); c.set(7, 7, shade(rgb(latch), 0.6)); c.set(8, 7, shade(rgb(latch), 0.6))
def p_tnt(c, face):
    red = rgb(0xd83a1e)
    c.noise(red, 0.05)
    for y in range(16):
        for x in range(16):
            if (x + y * 2) % 7 == 0: c.set(x, y, shade(red, 0.9))
    if face == "side":
        c.rect(0, 5, 15, 10, rgb(0xe8e0d0)); c.rect(0, 5, 15, 5, shade(rgb(0xe8e0d0), 0.85)); c.rect(0, 10, 15, 10, shade(rgb(0xe8e0d0), 0.8))
        for i, x in enumerate((2, 5, 8, 11)):
            c.rect(x, 6, x + 1, 9, rgb(0x1e1e1e))   # "TNT" bars
        c.rect(0, 0, 15, 0, shade(red, 1.15)); c.rect(0, 15, 15, 15, shade(red, 0.82))
    else:
        c.rect(5, 5, 10, 10, rgb(0xe8e0d0)); c.rect(7, 7, 8, 8, rgb(0x1e1e1e))
        c.frame(0, 0, 15, 15, shade(red, 0.85))
def p_bookshelf(c):
    p_planks(c, 0xa88852)
    c.rect(0, 0, 15, 0, rgb(0x5a3a1a)); c.rect(0, 7, 15, 8, rgb(0x5a3a1a)); c.rect(0, 15, 15, 15, rgb(0x5a3a1a))
    cols = (0x8a2a2a, 0x2a5a8a, 0x3a7a3a, 0x7a5a2a, 0x5a3a7a, 0xa89a3a, 0x2a6a6a)
    for row, (y0, y1) in enumerate(((1, 6), (9, 14))):
        x = 1
        while x < 15:
            w = c.r.choice((1, 2, 2, 3))
            col = rgb(cols[(x * 3 + row * 5 + c.r.randint(0, 6)) % len(cols)])
            h0 = y0 + (c.r.random() < 0.3)
            for xx in range(x, min(15, x + w)):
                for yy in range(h0, y1 + 1): c.set(xx, yy, shade(col, 0.85 + 0.2 * ((xx - x) == 0)))
            x += w
def p_jukebox(c, top=False):
    p_planks(c, 0x6a3a2a)
    c.frame(0, 0, 15, 15, rgb(0x3a1f14))
    if top:
        c.rect(5, 6, 10, 6, rgb(0x1a1210)); c.rect(5, 7, 10, 7, rgb(0x2a1a12))
def p_note_block(c):
    p_planks(c, 0x6a4a2a)
    c.frame(0, 0, 15, 15, rgb(0x3a2412)); c.rect(5, 5, 10, 10, rgb(0x3a2412)); c.frame(5, 5, 10, 10, rgb(0x241408))
def p_composter(c):
    p_planks(c, 0x8a6a3a)
    c.frame(0, 0, 15, 15, rgb(0x5a3e1e)); c.rect(2, 2, 13, 13, rgb(0x5a3a1a)); c.frame(2, 2, 13, 13, rgb(0x3e2810))
    c.speckle(rgb(0x7a5a2a), 14)
def p_piston_top(c, sticky=False):
    p_planks(c, 0xb8935a)
    c.frame(0, 0, 15, 15, rgb(0x6a4a2a))
    if sticky:
        c.rect(4, 4, 11, 11, rgb(0x7cc86a)); c.rect(4, 4, 11, 4, rgb(0xa0e090)); c.rect(4, 11, 11, 11, rgb(0x58a048)); c.speckle(rgb(0x9ad888), 8)
def p_piston_side(c):
    p_planks(c, 0xb8935a); c.rect(0, 4, 15, 15, rgb(0x6a6a6a)); c.noise(rgb(0x6a6a6a), 0.08, 0, 4, 15, 15)
    c.rect(0, 0, 15, 3, shade(rgb(0xb8935a), 1.0)); c.rect(0, 4, 15, 4, rgb(0x4a4a4a))
    c.speckle(rgb(0x8a8a8a), 10)
def p_observer_face(c):
    base = rgb(0x6f6f6f); p_cobble(c, 0x777777); c.frame(0, 0, 15, 15, shade(base, 0.6))
    c.rect(3, 5, 12, 10, rgb(0x2a2a2a)); c.rect(4, 6, 5, 9, rgb(0xb8b8b8)); c.rect(10, 6, 11, 9, rgb(0xb8b8b8))
def p_observer_back(c, on=False):
    base = rgb(0x6f6f6f); p_cobble(c, 0x777777); c.frame(0, 0, 15, 15, shade(base, 0.6))
    c.rect(6, 6, 9, 9, rgb(0xff2a1a) if on else rgb(0x501410)); c.frame(6, 6, 9, 9, rgb(0x3a3a3a))
    if on: c.rect(7, 7, 8, 8, rgb(0xff8a6a))
def p_target(c):
    c.noise(rgb(0xe8dcc8), 0.04)
    for y in range(16):
        for x in range(16):
            d = max(abs(x - 7.5), abs(y - 7.5))
            col = rgb(0xd8282a) if int(d) % 4 in (1, 2) else rgb(0xeee4d0)
            if int(d) == 0 or d < 1.5: col = rgb(0xd8282a)
            c.set(x, y, shade(col, c.r.uniform(0.96, 1.04)))
    c.frame(0, 0, 15, 15, rgb(0xb8a890))

# ------------------------------------------------------------------------------------------------
# Redstone parts, doors, beds and other thin or transparent blocks
# ------------------------------------------------------------------------------------------------
def p_dust(c, col=0xd8d8d8):
    base = rgb(col)
    for y in range(16):
        for x in (6, 7, 8, 9):
            c.set(x, y, shade(base, c.r.uniform(0.85, 1.1) * (0.9 if x in (6, 9) else 1.0)))
    for _ in range(6): c.set(c.r.choice((5, 10)), c.r.randint(0, 15), shade(base, 0.9))
def p_repeater_top(c, col=0xa8a8a8, comparator=False):
    base = rgb(col)
    c.noise(base, 0.05); c.frame(0, 0, 15, 15, shade(base, 0.72)); c.frame(1, 1, 14, 14, shade(base, 1.1))
    c.rect(7, 2, 8, 13, shade(base, 0.55)); c.rect(7, 2, 7, 13, shade(base, 0.45))   # signal track
    c.rect(7, 1, 8, 2, rgb(0xb02a1a))                                                 # output torch
    if comparator:
        c.rect(3, 9, 4, 10, rgb(0xb02a1a)); c.rect(11, 9, 12, 10, rgb(0xb02a1a)); c.rect(7, 5, 8, 6, rgb(0x6a1a10))
    else:
        c.rect(7, 11, 8, 13, shade(base, 0.35)); c.rect(7, 11, 8, 11, rgb(0xd83a2a))   # delay slider
        c.rect(3, 12, 4, 13, rgb(0x8a1a10)); c.rect(11, 12, 12, 13, rgb(0x8a1a10))
def p_redstone_lamp(c, lit=False):
    on = bool(lit) and lit != "off"
    base = rgb(0xd8a050) if on else rgb(0x6a4222)
    c.noise(base, 0.05); c.frame(0, 0, 15, 15, shade(base, 0.62))
    cross = shade(base, 0.45) if not on else rgb(0x7a5020)
    c.rect(7, 1, 8, 14, cross); c.rect(1, 7, 14, 8, cross)
    for i in range(16):
        if on and i % 3 == 0: c.set(i % 14 + 1, (i * 5) % 14 + 1, rgb(0xffe8a0))
def p_bed(c, col, part):
    base = rgb(col)
    wood = rgb(0x8a6234)
    if part == "side":
        c.noise(base, 0.05)
        for y in range(10, 16): c.rect(0, y, 15, y, shade(wood, 0.9 if y % 2 else 1.05))
        c.rect(0, 9, 15, 9, shade(base, 0.8)); c.rect(0, 0, 15, 0, shade(base, 1.15))
        c.rect(0, 10, 2, 15, shade(wood, 0.7)); c.rect(13, 10, 15, 15, shade(wood, 0.7))
    else:
        c.noise(base, 0.05)
        for y in range(16):
            for x in range(16):
                if (x + y) % 5 == 0: c.set(x, y, shade(base, 0.93))
        c.frame(0, 0, 15, 15, shade(base, 0.82))
        if part == "head_top":
            c.rect(2, 1, 13, 5, rgb(0xf2f2f2)); c.rect(2, 5, 13, 5, rgb(0xd0d0d0)); c.rect(2, 1, 13, 1, rgb(0xffffff))
def p_door(c, col, half, style="wood"):
    base = rgb(col)
    if style == "iron":
        c.noise(base, 0.04); c.frame(0, 0, 15, 15, shade(base, 0.7)); c.frame(1, 1, 14, 14, shade(base, 1.12))
        if half == "upper":
            c.rect(3, 3, 12, 8, shade(base, 0.45)); c.frame(3, 3, 12, 8, shade(base, 0.62)); c.rect(4, 4, 11, 4, shade(base, 0.55))
        else:
            c.rect(3, 4, 5, 6, shade(base, 0.9)); c.rect(11, 8, 12, 9, rgb(0x303030))   # handle
            c.speckle(shade(base, 1.2), 5)
        return
    p_planks(c, col)
    c.frame(0, 0, 15, 15, shade(base, 0.55))
    for x in (4, 11):
        c.rect(x, 1, x, 14, shade(base, 0.62))
    if half == "upper":
        for (x0, x1) in ((2, 6), (9, 13)):
            for y in range(3, 8):
                for x in range(x0, x1 + 1): c.set(x, y, CLEAR)
            c.frame(x0 - 1, 2, x1 + 1, 8, shade(base, 0.55))
            for x in range(x0 - 1, x1 + 2): c.set(x, 8, shade(base, 0.45)); c.set(x, 2, shade(base, 1.1))
    else:
        c.rect(11, 8, 12, 9, rgb(0x303030)); c.set(11, 8, rgb(0x6a6a6a))
        c.rect(2, 9, 6, 9, shade(base, 0.58)); c.rect(9, 9, 13, 9, shade(base, 0.58))
def p_trapdoor(c, col, style="wood"):
    base = rgb(col)
    if style == "iron":
        c.noise(base, 0.05); c.frame(0, 0, 15, 15, shade(base, 0.62)); c.frame(1, 1, 14, 14, shade(base, 1.12))
        for (x0, y0) in ((3, 3), (9, 3), (3, 9), (9, 9)):
            c.rect(x0, y0, x0 + 3, y0 + 3, shade(base, 0.42)); c.frame(x0, y0, x0 + 3, y0 + 3, shade(base, 0.55))
        return
    p_planks(c, col)
    c.frame(0, 0, 15, 15, shade(base, 0.52)); c.frame(1, 1, 14, 14, shade(base, 0.8))
    for (x0, y0) in ((3, 3), (9, 3), (3, 9), (9, 9)):
        for y in range(y0, y0 + 4):
            for x in range(x0, x0 + 4): c.set(x, y, CLEAR)
        c.frame(x0 - 1, y0 - 1, x0 + 4, y0 + 4, shade(base, 0.5))
def p_bars(c, col):
    base = rgb(col)
    for x in (2, 5, 8, 11, 14):
        for y in range(16):
            c.set(x, y, shade(base, 1.2 if y % 5 == 0 else 1.0))
            if x < 15: c.set(x + 1, y, shade(base, 0.72))
    for y in (0, 15):
        for x in range(16): c.set(x, y, shade(base, 0.85))
def p_chain(c, col):
    base = rgb(col)
    for k, y in enumerate((1, 5, 9, 13)):
        wide = k % 2 == 0
        c.rect(7, y, 8, y + 2, shade(base, 1.0))
        if wide: c.set(6, y + 1, shade(base, 0.7)); c.set(9, y + 1, shade(base, 0.7))
        c.set(7, y, shade(base, 1.3)); c.set(8, y + 2, shade(base, 0.6))
def p_ladder(c, col):
    base = rgb(col)
    for x in (1, 2, 13, 14):
        for y in range(16): c.set(x, y, shade(base, 1.1 if x in (1, 13) else 0.85))
    for y in (2, 7, 12):
        for x in range(3, 13): c.set(x, y, shade(base, 1.0)); c.set(x, y + 1, shade(base, 0.8))
def p_rail(c, kind="rail", curve=False, on=None):
    wood = rgb(0x7a5a30)
    metal = {"rail": 0x9a9a9a, "powered_rail": 0xd8b030, "detector_rail": 0x9a9a9a, "activator_rail": 0x9a9a9a}.get(kind, 0x9a9a9a)
    lit = on in (True, "on")
    if kind == "powered_rail" and not lit: metal = 0x8a7430
    if kind == "activator_rail": metal = 0xc84030 if lit else 0x8a2a20
    if curve:   # a quarter turn around the bottom-right corner: ties fan out, two rails follow the arc
        for k in range(7):
            a = k / 6 * math.pi / 2
            for t in range(5, 12):
                x, y = int(round(15 - t * math.cos(a))), int(round(15 - t * math.sin(a)))
                if k % 2 == 0: c.set(x, y, shade(wood, 1.0 if t % 2 else 0.78))
        for i in range(30):
            a = i / 29 * math.pi / 2
            for rr in (4.5, 12.0):
                x, y = int(round(15 - rr * math.cos(a))), int(round(15 - rr * math.sin(a)))
                c.set(x, y, shade(rgb(metal), 1.12))
        return
    for y in (2, 6, 10, 14):
        c.rect(2, y, 13, y + 1, shade(wood, 1.0)); c.rect(2, y + 1, 13, y + 1, shade(wood, 0.78))
    for x in (3, 12):
        for y in range(16): c.set(x, y, shade(rgb(metal), 1.15 if y % 4 == 0 else 1.0)); c.set(x + (1 if x == 3 else -1), y, shade(rgb(metal), 0.7))
    if kind == "detector_rail":
        c.rect(6, 5, 9, 10, shade(rgb(0x7a7a7a), 1.0)); c.rect(6, 5, 9, 5, rgb(0x9a9a9a))
    if kind in ("powered_rail", "activator_rail"):
        for y in (3, 11): c.rect(7, y, 8, y + 1, rgb(0xff3a2a) if lit else rgb(0x5a2a1a))
def p_torch(c, stick=0x6b4f2a, flame=0xffd060, head=None):
    wood = rgb(stick)
    for y in range(6, 15):
        c.set(7, y, shade(wood, 1.15)); c.set(8, y, shade(wood, 0.8))
    f = rgb(head if head else flame)
    c.set(7, 4, shade(f, 1.15)); c.set(8, 4, f); c.set(7, 5, f); c.set(8, 5, shade(f, 0.85))
    c.set(7, 3, mix(f, rgb(0xffffff), 0.5))
def p_lantern_like(c, col, glow):
    frame, g = rgb(col), rgb(glow)
    c.rect(5, 2, 10, 3, shade(frame, 1.1)); c.rect(7, 0, 8, 2, shade(frame, 0.8))
    c.rect(4, 4, 11, 13, shade(frame, 0.8))
    c.rect(5, 5, 10, 12, g); c.rect(6, 6, 9, 10, mix(g, rgb(0xffffff), 0.45))
    for y in range(5, 13): c.set(5, y, shade(g, 0.85)); c.set(10, y, shade(g, 0.8))
    c.rect(4, 13, 11, 14, shade(frame, 1.05))
def p_scaffold(c):
    c.frame(0, 0, 15, 15, rgb(0xc8b070)); c.frame(1, 1, 14, 14, rgb(0xa89050)); c.frame(2, 2, 13, 13, shade(rgb(0xc8b070), 0.8))
    for i in range(2, 14, 4): c.set(i, 0, shade(rgb(0xc8b070), 1.2)); c.set(0, i, shade(rgb(0xc8b070), 1.2))
def p_barrier(c):
    red = rgb(0xe02020)
    c.frame(0, 0, 15, 15, red); c.frame(1, 1, 14, 14, shade(red, 0.8))
    for i in range(16):
        c.set(i, i, red)
        if i < 15: c.set(i + 1, i, red)
def p_command_block(c, col):
    base = rgb(col)
    c.noise(base, 0.06); c.frame(0, 0, 15, 15, shade(base, 0.6)); c.frame(1, 1, 14, 14, shade(base, 1.12))
    c.rect(6, 6, 9, 9, rgb(0xf0f0f0)); c.frame(5, 5, 10, 10, shade(base, 0.5)); c.rect(7, 7, 8, 8, shade(base, 0.7))
    for (x, y) in ((3, 3), (12, 3), (3, 12), (12, 12)): c.set(x, y, shade(base, 1.35))

# ------------------------------------------------------------------------------------------------
# Natural and nether blocks, food blocks, portals, fire and the spawner cage
# ------------------------------------------------------------------------------------------------
def p_basalt_side(c, col=0x505055):
    base = rgb(col)
    f = tnoise(c, 4, 2)
    for x in range(16):
        streak = 0.1 * math.sin(x * 1.9 + 1.0) + (0.08 if x % 4 == 1 else -0.06 if x % 4 == 3 else 0)
        for y in range(16): c.set(x, y, shade(base, 1 + streak + (f[y][x] - 0.5) * 0.15 + c.r.uniform(-0.04, 0.04)))
def p_netherrack(c, col=0x6f2f2f):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 3), 0.22, 0.05)
    for _ in range(14): c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(base, 0.62)); c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(base, 1.3))
def p_nylium_top(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 3), 0.2, 0.06)
    for _ in range(26):
        x, y = c.r.randint(0, 15), c.r.randint(0, 14)
        c.set(x, y, shade(base, 1.3)); c.set(x, y + 1, shade(base, 0.7))
def p_soul_sand(c, col=0x51402f):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 2), 0.12, 0.04)
    for (x, y) in ((3, 4), (10, 3), (6, 10), (12, 11), (2, 12)):   # faces pressed into the sand
        c.set(x, y, shade(base, 0.45)); c.set(x + 1, y, shade(base, 0.45)); c.set(x, y + 1, shade(base, 0.5)); c.set(x + 1, y + 2, shade(base, 0.5)); c.set(x - 1, y + 1, shade(base, 0.7))
def p_glowstone(c, col=0xd0a050):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 2), 0.2, 0.05)
    for _ in range(7):
        x, y = c.r.randint(0, 13), c.r.randint(0, 13)
        c.rect(x, y, x + c.r.randint(0, 2), y + c.r.randint(0, 2), mix(rgb(0xfff0a0), base, 0.15))
    c.speckle(shade(base, 1.35), 8)
def p_froglight(c, col):
    base = rgb(col)
    c.noise(base, 0.04); c.frame(0, 0, 15, 15, shade(base, 0.82)); c.frame(1, 1, 14, 14, shade(base, 1.12))
    c.rect(3, 3, 12, 12, shade(base, 0.95)); c.frame(3, 3, 12, 12, shade(base, 1.18)); c.speckle(shade(base, 1.2), 8)
def p_sponge(c, col):
    base = rgb(col)
    c.noise(base, 0.06)
    for _ in range(9):
        x, y = c.r.randint(0, 13), c.r.randint(0, 14)
        w = c.r.randint(1, 3)
        for k in range(w): c.set(x + k, y, shade(base, 0.62)); c.set(x + k, y + 1, shade(base, 0.8))
def p_coral_block(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 3), 0.15, 0.05)
    for _ in range(10): c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(base, 0.55))
def p_dragon_egg(c):
    c.noise(rgb(0x1a0a20), 0.2)
    for _ in range(14): c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(rgb(0x6a2a7a), c.r.uniform(0.8, 1.2)))
def p_end_portal(c):
    c.noise(rgb(0x06101a), 0.3)
    for _ in range(18): c.set(c.r.randint(0, 15), c.r.randint(0, 15), rgb(c.r.choice((0x58e8e8, 0x2a58c8, 0x3a9ad8))))
def p_sculk(c):
    base = rgb(0x0e2a30)
    _tone(c, base, tnoise(c, 4, 3), 0.25, 0.04)
    for _ in range(9): c.set(c.r.randint(0, 15), c.r.randint(0, 15), rgb(0x39d6e0));
    for _ in range(14): c.set(c.r.randint(0, 15), c.r.randint(0, 15), shade(base, 0.5))
def p_spawner(c):
    c.noise(rgb(0x12202e), 0.08)
    for i in (0, 4, 8, 12, 15):
        for k in range(16): c.set(i, k, rgb(0x2a4a6a)); c.set(k, i, rgb(0x2a4a6a))
    for i in (1, 5, 9, 13):
        for k in range(16): c.set(i, k, shade(rgb(0x1a3048), 0.85)); c.set(k, i, shade(rgb(0x1a3048), 0.85))
def p_portal(c, col=0x7a2adb, frame=0):
    base = rgb(col)
    f = tnoise(c, 2, 3)
    for y in range(16):
        for x in range(16):
            w = 0.5 + 0.5 * math.sin((x + f[y][x] * 6) * 0.9 + y * 0.45 + frame)
            c.set(x, y, shade(base, 0.7 + 0.6 * w + c.r.uniform(-0.03, 0.03)))
def p_fire(c, col=0xff8a1a, frame=0):
    base = rgb(col)
    for x in range(16):
        h = int(7 + 6 * (0.5 + 0.5 * math.sin(x * 1.1 + frame * 0.8 + c.r.random())))
        for y in range(16 - h, 16):
            t = (y - (16 - h)) / max(1, h - 1)   # 0 at the tip, 1 at the base
            col2 = mix(mix(rgb(0xffe27a), base, min(1.0, t * 1.4)), rgb(0xd83a10), max(0.0, t - 0.7) * 2.5)
            c.set(x, y, col2)
def p_snow_side(c):
    p_dirt(c, 0x8a6a4a)
    for x in range(16):
        h = 2 + (c.r.random() < 0.4) + (c.r.random() < 0.15)
        for y in range(h): c.set(x, y, shade(rgb(0xf6fafa), c.r.uniform(0.94, 1.02) * (0.9 if y == h - 1 else 1.0)))
def p_cactus(c, face):
    g = rgb(0x4a8a22)
    if face == "side":
        for x in range(16):
            for y in range(16): c.set(x, y, shade(g, (1.12 if x % 4 == 1 else 0.85 if x % 4 == 3 else 1.0) * c.r.uniform(0.94, 1.06)))
        for (x, y) in ((0, 3), (15, 8), (0, 11), (15, 14), (8, 1), (4, 7), (11, 12)): c.set(x, y, rgb(0xe8f0d8))
        c.rect(0, 0, 0, 15, shade(g, 0.72)); c.rect(15, 0, 15, 15, shade(g, 0.72))
    else:
        c.rect(1, 1, 14, 14, shade(g, 1.0)); c.frame(1, 1, 14, 14, shade(g, 0.7)); c.rect(4, 4, 11, 11, shade(g, 1.15))
        c.speckle(shade(g, 0.8), 8)
def p_cake(c, face):
    cream = rgb(0xf6f0e4); sponge = rgb(0xc88a5a)
    if face == "side":
        c.noise(sponge, 0.06); c.rect(0, 0, 15, 4, cream); c.rect(0, 4, 15, 4, shade(cream, 0.85))
        for x in range(0, 16, 3): c.set(x, 5, shade(cream, 0.9))
    else:
        c.noise(cream, 0.03)
        for (x, y) in ((2, 3), (7, 2), (12, 4), (4, 9), (10, 10), (13, 13), (2, 13)): c.set(x, y, rgb(0xd83030))
def p_hay(c, side=True):
    base = rgb(0xd8aa30)
    for x in range(16):
        for y in range(16):
            c.set(x, y, shade(base, (1.1 if (x + y // 3) % 4 == 0 else 0.9 if (x * 2 + y) % 5 == 0 else 1.0) * c.r.uniform(0.93, 1.07)))
    if side:
        c.rect(0, 5, 15, 6, rgb(0x8a4a1a)); c.rect(0, 10, 15, 11, rgb(0x8a4a1a))
        for x in range(0, 16, 2): c.set(x, 5, rgb(0x6a3410))
def p_melon(c, side=True):
    g1, g2 = rgb(0x7ac020), rgb(0x4a9a18)
    for x in range(16):
        for y in range(16):
            c.set(x, y, shade(g1 if (x // 2) % 2 == 0 else g2, c.r.uniform(0.92, 1.08)))
    if not side: c.rect(6, 6, 8, 8, rgb(0x8a6a2a)); c.set(7, 7, rgb(0x5a3a14))
def p_pumpkin(c, face="side"):
    o1, o2 = rgb(0xe88a1a), rgb(0xc86a10)
    for x in range(16):
        for y in range(16): c.set(x, y, shade(o1 if (x // 2) % 2 == 0 else o2, c.r.uniform(0.94, 1.06)))
    if face == "top":
        c.rect(7, 7, 8, 8, rgb(0x3a7a18)); c.set(7, 7, rgb(0x5a9a28))
    elif face in ("carved", "lit"):
        glow = face == "lit"
        eye = rgb(0xffd860) if glow else rgb(0x3a2410)
        c.rect(3, 4, 5, 6, eye); c.rect(10, 4, 12, 6, eye)
        for x in range(3, 13): c.set(x, 10, eye)
        c.rect(5, 11, 10, 11, eye); c.set(3, 9, eye); c.set(12, 9, eye)
        c.rect(7, 7, 8, 8, eye)
def p_mushroom_block(c, col, spots=None):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 2), 0.1, 0.04)
    if spots is not None:
        sp = rgb(spots)
        for (x, y, w) in ((2, 2, 3), (9, 1, 2), (11, 8, 3), (3, 9, 2), (7, 13, 2)):
            c.rect(x, y, x + w - 1, y + w - 1, sp); c.set(x, y, shade(sp, 1.1)); c.set(x + w - 1, y + w - 1, shade(sp, 0.85))
def p_mushroom_stem(c):
    base = rgb(0xd8d2c0)
    _tone(c, base, tnoise(c, 4, 2), 0.07, 0.03)
    for x in (3, 8, 12):
        for y in range(16): c.set(x, y, shade(base, 0.88))
def p_honeycomb(c, col):
    base = rgb(col)
    c.noise(base, 0.05)
    for y in range(1, 16, 4):
        for x in range(1 + (y // 4 % 2) * 2, 16, 4): c.set(x, y, shade(base, 0.7)); c.set(x + 1, y, shade(base, 0.8)); c.set(x, y + 1, shade(base, 0.85))
def p_beehive(c, front=False):
    p_planks(c, 0xc8a050)
    c.frame(0, 0, 15, 15, rgb(0x8a6a2a))
    for y in (4, 9):
        for x in range(1, 15): c.set(x, y, rgb(0x8a6a2a))
    if front: c.rect(6, 6, 9, 8, rgb(0x2a1a0a)); c.rect(6, 6, 9, 6, rgb(0x1a0e04))
def p_shelf(c, col):
    p_planks(c, col)
    base = rgb(col)
    c.rect(1, 5, 14, 10, shade(base, 0.38)); c.frame(1, 5, 14, 10, shade(base, 0.28)); c.rect(1, 5, 14, 5, shade(base, 0.2))
def p_head(c, col, face_col=None):
    base = rgb(col)
    _tone(c, base, tnoise(c, 4, 2), 0.1, 0.04)
    if face_col is not None:
        e = rgb(face_col)
        c.rect(3, 6, 5, 8, e); c.rect(10, 6, 12, 8, e)
def p_map(c):
    c.rect(1, 1, 14, 14, rgb(0xe8dcb8)); c.frame(0, 0, 15, 15, rgb(0x8a6a38)); c.frame(1, 1, 14, 14, shade(rgb(0x8a6a38), 1.2))
    c.rect(3, 3, 6, 7, rgb(0x6a9a40)); c.rect(4, 8, 5, 9, rgb(0x4a80c8)); c.rect(9, 4, 11, 6, rgb(0x6a9a40)); c.rect(7, 10, 12, 11, rgb(0xc8b888))
def p_bell(c):
    c.rect(4, 4, 11, 13, rgb(0xd8b030)); c.rect(3, 13, 12, 14, rgb(0xb89020)); c.rect(4, 4, 5, 12, rgb(0xf0d060))
def p_flower_pot(c):
    c.rect(4, 9, 11, 14, rgb(0xa8583a)); c.rect(3, 9, 12, 10, rgb(0xc06a48)); c.rect(4, 14, 11, 14, rgb(0x7a3e28))

# ------------------------------------------------------------------------------------------------
# Plants (cross-shaped sprites, crops) and the block-breaking crack overlay
# ------------------------------------------------------------------------------------------------
def _stem(c, x, y0, y1, col=0x3a7a2a):
    g = rgb(col)
    for y in range(y0, y1 + 1): c.set(x, y, shade(g, 1.1 if y % 2 else 0.9))
def _leaf(c, x, y, left=True, col=0x3a7a2a):
    g = rgb(col)
    c.set(x + (-1 if left else 1), y, shade(g, 1.15)); c.set(x + (-2 if left else 2), y - 1, g)
def p_cross_plant(c, kind, col, col2=None):
    main = rgb(col)
    acc = rgb(col2) if col2 is not None else None
    r = c.r
    if kind == "sapling":
        _stem(c, 7, 9, 15, 0x6a4a2a); c.set(8, 11, shade(rgb(0x6a4a2a), 0.8))
        for (cx, cy) in ((7, 6), (4, 8), (10, 8), (7, 10), (5, 5), (9, 4)):
            for dx, dy in ((0, 0), (1, 0), (0, 1), (-1, 0), (0, -1), (1, 1)):
                if r.random() < 0.85: c.set(cx + dx, cy + dy, shade(main, r.uniform(0.75, 1.2)))
    elif kind in ("flower", "tulip"):
        _stem(c, 7, 8, 15); _leaf(c, 7, 12, True); _leaf(c, 8, 10, False)
        if kind == "flower":
            for (dx, dy) in ((0, -1), (-1, 0), (1, 0), (0, 1)): c.set(7 + dx, 5 + dy, shade(main, 1.05))
            for (dx, dy) in ((-1, -1), (1, -1), (-1, 1), (1, 1)): c.set(7 + dx, 5 + dy, shade(main, 0.88))
            c.set(7, 5, acc if acc else mix(main, rgb(0xfff0a0), 0.6))
        else:
            c.rect(6, 3, 8, 6, main); c.set(6, 2, shade(main, 1.1)); c.set(8, 2, shade(main, 1.1)); c.set(7, 4, shade(main, 0.8)); c.set(6, 6, shade(main, 0.85))
    elif kind == "mushroom":
        c.rect(7, 9, 8, 14, rgb(0xe8e0d0)); c.rect(8, 9, 8, 14, shade(rgb(0xe8e0d0), 0.85))
        c.rect(4, 5, 11, 8, main); c.rect(5, 4, 10, 4, shade(main, 1.1)); c.rect(4, 8, 11, 8, shade(main, 0.8))
        if acc: c.set(6, 5, acc); c.set(9, 6, acc); c.set(5, 7, acc)
    elif kind in ("grass", "fern"):
        for k, x in enumerate((2, 4, 6, 8, 10, 12, 13)):
            h = r.randint(6, 12) if kind == "grass" else r.randint(7, 12)
            lean = r.choice((-1, 0, 1))
            for y in range(h):
                xx = x + (lean if y > h // 2 else 0) + (1 if kind == "fern" and y > h - 3 and k % 2 else 0)
                c.set(xx, 15 - y, shade(main, 0.75 + 0.5 * y / h + r.uniform(-0.05, 0.05)))
    elif kind == "bush":
        for _ in range(70):
            x, y = int(r.gauss(7.5, 3.2)), int(r.gauss(9.5, 2.8))
            if 0 <= x < 16 and 5 <= y < 16: c.set(x, y, shade(main, r.uniform(0.7, 1.2)))
        if acc:
            for _ in range(6): c.set(r.randint(3, 12), r.randint(7, 13), acc)
        for y in range(13, 16): c.set(7, y, rgb(0x5a3a1a))
    elif kind == "dead":
        wood = main
        _stem(c, 7, 8, 15, col); c.set(6, 10, wood); c.set(5, 9, wood); c.set(8, 9, wood); c.set(9, 8, wood); c.set(8, 12, wood); c.set(9, 13, wood); c.set(6, 13, wood)
    elif kind == "vine":
        for y in range(16):
            for x in range(16):
                if r.random() < 0.42: c.set(x, y, shade(main, r.uniform(0.6, 1.25)))
    elif kind == "hanging":
        for x in range(1, 16, 3):
            ln = r.randint(8, 16)
            for y in range(ln):
                c.set(x, y, shade(main, r.uniform(0.8, 1.15)))
                if y % 4 == 2: c.set(x + 1, y, shade(main, 0.8))
            if acc: c.set(x, ln - 1, acc); c.set(x + 1 if x < 15 else x, ln - 2, acc)
    elif kind == "stalk":
        for x in (3, 8, 13):
            for y in range(16):
                c.set(x, y, shade(main, 1.1 if y % 5 else 0.8)); c.set(x + 1, y, shade(main, 0.85 if y % 5 else 0.65))
            if acc: c.set(x, 0, acc); c.set(x + 1, 1, acc)
    elif kind == "sprouts":
        for x in (2, 5, 8, 11, 13):
            h = r.randint(3, 6)
            for y in range(h): c.set(x, 15 - y, shade(main, 0.8 + 0.3 * y / h)); c.set(x + 1 if y > 1 else x, 15 - y, shade(main, 0.7))
    elif kind == "coral":
        for (x0, x1) in ((3, 6), (9, 12)):
            for y in range(3, 15):
                c.set(x0 + (y // 4) % 2, y, shade(main, 1.0 - 0.01 * y)); c.set(x1 - (y // 4) % 2, y, shade(main, 0.85))
            for x in range(x0, x1 + 1): c.set(x, 3, shade(main, 1.2)); c.set(x, 9, shade(main, 0.9))
        for y in range(12, 16):
            for x in range(1, 15):
                if r.random() < 0.6: c.set(x, y, shade(main, 0.8 + 0.1 * (x % 3)))
    elif kind == "fan":
        for y in range(5, 15):
            half = int((15 - y) * 0.9) if y > 8 else 7 - (8 - y)
            for x in range(8 - max(1, half), 8 + max(1, half)):
                if 0 <= x < 16 and r.random() < 0.9: c.set(x, y, shade(main, 0.75 + 0.4 * ((x + y) % 3) / 2))
        for y in range(12, 16): c.set(7, y, shade(main, 0.6)); c.set(8, y, shade(main, 0.6))
    elif kind == "cluster":
        for (x, h) in ((3, 6), (6, 12), (9, 8), (11, 5)):
            for y in range(h):
                w = max(0, 1 - (y * 1 // max(1, h - 1)) + (1 if y < h - 2 else 0))
                for dx in range(-1 if y < h - 2 else 0, 2 if y < h - 2 else 1):
                    c.set(x + dx, 15 - y, shade(main, 0.8 + 0.4 * ((dx + 1) / 2)))
    else:   # unknown kind: a small leafy tuft keeps new content visible
        _stem(c, 7, 8, 15); c.rect(5, 5, 9, 8, main)
def p_crop(c, col, stage, maxstage, ripe=None):
    g = rgb(col)
    frac = stage / max(1, maxstage)
    h = 2 + int(round(frac * 11))
    for k, x in enumerate((2, 6, 10, 14)):
        top = 16 - h - (k % 2)
        for y in range(max(0, top), 16):
            c.set(x, y, shade(g, 1.15 if y % 2 else 0.85))
            if frac >= 0.4 and (y - top) % 4 == 2:
                c.set(x - 1, y, shade(g, 0.9)); c.set(x + 1, y - 1, shade(g, 1.05))
    if ripe is not None and stage >= maxstage:
        rr = rgb(ripe)
        for x in (2, 10):
            c.rect(x, 0, x + 3, 2, rr); c.rect(x, 0, x + 3, 0, shade(rr, 1.2)); c.rect(x, 2, x + 3, 2, shade(rr, 0.8))
def p_crack(c, stage):
    dark = (0, 0, 0, 175)
    rr = random.Random(7)   # the same cracks grow stage by stage
    pts = [(7, 7)]
    paths = []
    for arm in range(5):
        x, y = 7 + rr.randint(-1, 1), 7 + rr.randint(-1, 1)
        path = []
        a = rr.uniform(0, 2 * math.pi)
        for step in range(9):
            a += rr.uniform(-0.6, 0.6)
            x += math.cos(a) * 1.2; y += math.sin(a) * 1.2
            path.append((int(round(x)), int(round(y))))
        paths.append(path)
    n = int((stage + 1) / 10 * 9) + 1
    for path in paths[: 1 + stage // 2]:
        for (x, y) in path[: max(1, n)]:
            c.set(x, y, dark)
    for (x, y) in pts: c.set(x, y, dark)


# ------------------------------------------------------------------------------------------------
# Generic building-block surfaces (each is reused by dozens of blocks): bevels, mottling, brushed grain and facets
# ------------------------------------------------------------------------------------------------
def _bevel(c, base, light=1.16, dark=0.78, inset=0):
    """Light top/left edge and dark bottom/right edge: gives a flat tile real thickness."""
    a, b = inset, 15 - inset
    for i in range(a, b + 1):
        c.set(i, a, shade(c.get(i, a), light)); c.set(a, i, shade(c.get(a, i), light))
        c.set(i, b, shade(c.get(i, b), dark)); c.set(b, i, shade(c.get(b, i), dark))
def p_polished(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 3, 3), 0.075, 0.015)
    for _ in range(5):   # fine polishing scratches
        x, y = c.r.randint(0, 11), c.r.randint(1, 14)
        for k in range(c.r.randint(3, 6)): c.set(x + k, y, shade(c.get(x + k, y), 1.07))
    c.frame(0, 0, 15, 15, shade(base, 0.7)); _bevel(c, base, 1.14, 0.84, 1)
def p_chiseled(c, col):
    p_polished(c, col)
    base = rgb(col)
    for i, (f_hi, f_lo) in enumerate(((1.12, 0.72), (1.1, 0.78), (1.08, 0.82))):   # three nested carved squares
        a = 3 + i * 2
        for k in range(a, 16 - a):
            c.set(k, a, shade(base, f_lo)); c.set(a, k, shade(base, f_lo))
            c.set(k, 15 - a, shade(base, f_hi)); c.set(15 - a, k, shade(base, f_hi))
    c.rect(7, 7, 8, 8, shade(base, 1.18))
def p_tiles(c, col):
    base = rgb(col)
    grout = shade(base, 0.52)
    for y in range(16):
        for x in range(16): c.set(x, y, grout)
    for ty in range(4):
        for tx in range(4):
            f = c.r.uniform(0.93, 1.08)
            for y in range(ty * 4, ty * 4 + 3):
                for x in range(tx * 4, tx * 4 + 3):
                    bev = 1.12 if (y == ty * 4 or x == tx * 4) else (0.88 if (y == ty * 4 + 2 or x == tx * 4 + 2) else 1.0)
                    c.set(x, y, shade(base, f * bev * c.r.uniform(0.97, 1.03)))
def p_metal(c, col):
    base = rgb(col)
    for y in range(16):
        streak = c.r.uniform(-0.035, 0.035)
        for x in range(16): c.set(x, y, shade(base, 1 + streak + 0.03 * math.sin(x * 0.7 + y * 0.2) + c.r.uniform(-0.015, 0.015)))   # brushed
    c.frame(0, 0, 15, 15, shade(base, 0.66)); _bevel(c, base, 1.26, 0.8, 1)
    for (x, y) in ((2, 2), (13, 2), (2, 13), (13, 13)):   # rivets
        c.set(x, y, shade(base, 1.35)); c.set(x + 1, y + 1, shade(base, 0.6))
def p_gemblock(c, col):
    base = rgb(col)
    f = tnoise(c, 4, 2)
    for y in range(16):
        for x in range(16):
            facet = ((x // 4 + y // 4) % 2) * 0.1 + ((x + y) % 8 < 4) * 0.06
            c.set(x, y, shade(base, 0.92 + facet + (f[y][x] - 0.5) * 0.12))
    c.frame(0, 0, 15, 15, shade(base, 0.58)); _bevel(c, base, 1.3, 0.75, 1)
    for (x, y) in ((3, 3), (11, 6), (6, 11), (12, 12)):   # sparkles
        c.set(x, y, shade(base, 1.7)); c.set(x + 1, y, shade(base, 1.4)); c.set(x, y + 1, shade(base, 1.4))
def p_pillar_side(c, col):
    base = rgb(col)
    f = tnoise(c, 3, 2)
    for x in range(16):
        flute = 1.0 + 0.1 * math.cos((x - 7.5) / 7.5 * math.pi * 2)   # rounded column shading
        for y in range(16): c.set(x, y, shade(base, flute + (f[y][x] - 0.5) * 0.08 + c.r.uniform(-0.02, 0.02)))
    for x in (0, 15): c.rect(x, 0, x, 15, shade(base, 0.74))
    for x in (4, 11): c.rect(x, 0, x, 15, shade(base, 0.9))
def p_concrete(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 2, 2), 0.035, 0.012)
    c.speckle(shade(base, 0.93), 6); c.speckle(shade(base, 1.06), 6)
def p_powder(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 6, 2), 0.09, 0.05)
    c.speckle(shade(base, 0.82), 22); c.speckle(shade(base, 1.18), 18)
def p_terracotta(c, col):
    base = rgb(col)
    f = tnoise(c, 3, 3)
    for y in range(16):
        band = 0.015 * math.sin(y * 0.9)
        for x in range(16): c.set(x, y, shade(base, 1 + band + (f[y][x] - 0.5) * 0.12 + c.r.uniform(-0.02, 0.02)))
    c.speckle(shade(base, 0.9), 9)
def p_glazed(c, col):
    base = rgb(col)
    _tone(c, base, tnoise(c, 2, 2), 0.05, 0.015)
    hi, lo = shade(base, 1.38), shade(base, 0.62)
    c.frame(0, 0, 15, 15, lo); c.frame(1, 1, 14, 14, shade(base, 1.12))
    for i in range(2, 14):   # arrow-like pattern of glazed terracotta, a diagonal fold
        c.set(i, i, lo); c.set(15 - i, i, hi)
    c.rect(6, 6, 9, 9, hi); c.rect(7, 7, 8, 8, rgb(0xffffff))
def p_ice(c, col=0x91b7f7, alpha=170):
    base = rgb(col, alpha)
    f = tnoise(c, 3, 3)
    for y in range(16):
        for x in range(16): c.set(x, y, shade(base, 0.92 + f[y][x] * 0.2 + c.r.uniform(-0.02, 0.02)))
    for _ in range(3):   # frozen veins
        x, y = c.r.randint(0, 15), c.r.randint(0, 15)
        for k in range(c.r.randint(4, 8)):
            c.set(x % 16, y % 16, rgb(0xeaf4ff, min(255, alpha + 50))); x += c.r.choice((1, 1, 0)); y += c.r.choice((0, 1, 1))
    for i in range(4): c.set(4 + i, 8 - i, rgb(0xffffff, min(255, alpha + 60)))
def p_flat(c, col, var=0.06):
    base = rgb(col)
    _tone(c, base, tnoise(c, 3, 2), var * 0.8, var * 0.35)

PAINTERS = {k[2:]: v for k, v in globals().items() if k.startswith("p_")}
PAINTERS_BY_NAME = PAINTERS

def paint(spec):
    """spec = (painter_name, *args)  ->  PIL image"""
    name, args = spec[0], spec[1:]
    c = Canvas(seed=seed_of(*spec))
    PAINTERS[name](c, *args)
    return c.image()

# ------------------------------------------------------------------------------------------------
# Item icon templates (masks).  Letters map to palette entries: a=highlight b=main c=shadow
# h=handle d=handle-dark x=accent y=accent2 o=outline
# ------------------------------------------------------------------------------------------------
E = ""
def G(*rows): return list(rows) + [E] * (16 - len(rows))
MASKS = {
 "stick": G(E, E, E, E, E, E, E, E, ".........h", "........hd", ".......hd", "......hd", ".....hd", "....hd", "...hd", "..h"),
 "lump": G(E, E, E, ".....aab", "....aaaab", "...aaaaabb", "...aaaabbb", "..aaaaabbbb", "..aabbbbbbb", "...abbbbbb", "....bbbbb", ".....bbb"),
 "ingot": G(E, E, E, E, E, ".......aaaa", ".....aaaaaab", "...aaaaaaabbc", ".aaaaaaabbbcc", ".abbbbbbbccc", ".bbbbbbbccc", "..cccccccc"),
 "gem": G(E, E, "....aaaaaa", "...abbbbbba", "..abbbccbbba", ".abbbccccbbba", ".bbbccccccbbb", "..bbccccccbb", "...bbccccbb", "....bbccbb", ".....bbbb", "......bb"),
 "nugget": G(E, E, E, E, E, E, ".....aab", "....aabbb", "....abbbc", ".....bbcc", "......cc"),
 "dust": G(E, E, E, E, E, E, E, "......a.b", "....abbab.b", "...abbbbbbb", "..abbbbcbbbc", ".abbbbbbbbccc", ".bbbcbbbbbcc", "..cccccccc"),
 "raw": G(E, E, E, "......aa", "....aabbba", "...abbbcbbb", "..abbcbbbbbb", "..bbbbbbcbbc", "..abbcbbbbcc", "...bbbbbccc", "....cccccc"),
 "shard": G(E, E, "..........aa", ".........abb", "........abbc", ".......abbc", "......abbc", ".....abbc", "....abbc", "...abbc", "..abbc", "..bcc", "..c"),
 "meat": G(E, E, "....aaaa", "...abbbbaa", "..abbcbbbba", "..abbbbcbbba", "..abcbbbbbbba", "...abbbbcbbba", "....abbbbbba", ".....aabbba", ".......aaa"),
 "leg": G(E, E, ".......aaaa", "......abbbba", ".....abbbbbba", ".....abbbbbba", ".....abbbbbba", "......abbbba", ".....xabbba", "....xx.aaa", "...xx", "..xxx", "..xx"),
 "fish": G(E, E, E, E, "..........b.", ".....bbbbbbb", "...bbabbbbbbb.b", "..bxbbbbbbbbbbb", "..bbbbbbbbbbb.b", "...bbbbbbbbb", ".....bbbbbb", E),
 "fruit": G(E, ".......h", "......h", "....aaaaa", "...abbbbbb", "..abbbbbbbb", "..abbbbbbbb", "..bbbbbbbbc", "..bbbbbbbbc", "...bbbbbbc", "....bbbcc", "......cc"),
 "bread": G(E, E, E, E, E, "....aaaaaa", "..aabbabbab", ".abbbbbbbbbb", ".bbbbbbbbbbbc", ".bbbbbbbbbbcc", "..cccccccccc"),
 "bowl": G(E, E, E, E, E, E, E, "..xxxxxxxxxxx", "..yyyyyyyyyyy", "...bbbbbbbbb", "....bbbbbbb", ".....ccccc"),
 "cookie": G(E, E, E, "....aaaaa", "...abbbbba", "..abxbbbxba", "..abbbbbbbb", "..abbxbbbbb", "..bbbbbbxbc", "...bbbbbbc", "....ccccc"),
 "pie": G(E, E, E, E, "....aaaaaaa", "...axaxaxaxa", "..abbbbbbbbbb", "..bbbbbbbbbbb", "..ccccccccccc"),
 "bucket": G(E, E, E, "...aaaaaaaaa", "..axxxxxxxxxa", "..axxxxxxxxxb", "...abbbbbbbb", "...abbbbbbbb", "....abbbbbb", "....abbbbbb", ".....bbbbb", ".....ccccc"),
 "bottle": G(E, ".......cc", ".......bb", "......abba", "......abba", ".....abxxba", "....abxxxxba", "....axxxxxxb", "....axxxxxxb", "....axxxxxxb", ".....bxxxxb", "......bbbb"),
 "disc": G(E, E, "......bbbb", "....bbbbbbbb", "...bbbbbbbbbb", "...bbbxxxxbbb", "..bbbxxyyxxbbb", "..bbbxyyyyxbbb", "..bbbxxyyxxbbb", "...bbbxxxxbbb", "...bbbbbbbbbb", "....bbbbbbbb", "......bbbb"),
 "egg": G(E, E, "......aaa", ".....abbba", "....abxbbba", "....abbbbxb", "...abbxbbbbb", "...abbbbbxbb", "...bxbbbbbbc", "...bbbbbxbbc", "....bbxbbbc", ".....cccc"),
 "dye": G(E, E, E, "......hh", ".....abba", "....abbbba", "...abbbbbba", "...abbbbbbb", "...bbbbbbbb", "....bbbbbbc", ".....cccc"),
 "book": G(E, E, "...bbbbbbbb", "..abbbbbbbbx", "..abbbbbbbbx", "..abbxxxxbbx", "..abbbbbbbbx", "..abbbbbbbbx", "..abbbbbbbbx", "..abbbbbbbbx", "..ayyyyyyyyy", "...yyyyyyyy"),
 "paper": G(E, E, "...aaaaaaaa", "...abbbbbbbb", "...abxxxxbbb", "...abbbbbbbb", "...abxxxxxbb", "...abbbbbbbb", "...abxxxbbbb", "...abbbbbbbb", "...bbbbbbbbc", "....cccccccc"),
 "map": G(E, E, "..aaaaaaaaaa", "..abbxxbbbyb", "..abxxxxbbyb", "..abbxxbyyyb", "..abbbbbyybb", "..abyyyybbbb", "..abyyxxxbbb", "..abbbxxxbbb", "..abbbbbbbbb", "..bbbbbbbbbc"),
 "orb": G(E, E, E, ".....aaaa", "....abbbba", "...abbxxbbb", "...abxxxxbb", "...abxxxxbb", "...abbxxbbc", "....bbbbbc", ".....cccc"),
 "sherd": G(E, E, E, "....aaaaaa", "...abbbbbbba", "...abxxxxbbb", "...abxbbxbbb", "...abxxxxbbc", "...abbbbbbbc", "....bbbbbbc", ".....ccccc"),
 "template": G(E, E, "...aaaaaaaaa", "...abbbbbbbbc", "...abxxxxxxbc", "...abxbbbbxbc", "...abxbxxbxbc", "...abxbbbbxbc", "...abxxxxxxbc", "...abbbbbbbbc", "...accccccccc"),
 "helmet": G(E, E, E, E, "...aaaaaaaaa", "..abbbbbbbbbb", "..abbbbbbbbbc", "..abc.....bbc", "..abc.....bbc", "..acc.....ccc"),
 "chestplate": G(E, E, "..aaa...aaa", "..abba.abbbb", "..abbbbbbbbc", "...abbbbbbc", "...abbbbbbc", "...abbbbbbc", "...abbbbbbc", "...abbbbbbc", "...bbbbbbbc", "...cccccccc"),
 "leggings": G(E, E, "...aaaaaaaa", "...abbbbbbbc", "...abbbbbbbc", "...abbc.abbc", "...abbc.abbc", "...abbc.abbc", "...abbc.abbc", "...abbc.abbc", "...accc.accc"),
 "boots": G(E, E, E, E, E, E, "...aaa..aaa", "...abc..abc", "...abc..abc", "..abbc.abbc", ".abbbcabbbc", ".bbbbcbbbbc", ".ccccccccc"),
 "pickaxe": G(E, ".aaaaaaa", "abbbbbbbac", ".cc...ccbbc", "......hh.cbc", ".....hh...cbc", "....hh.....cc", "...hh", "..hh", ".hh", "hh"),
 "axe": G(E, "......aaa", ".....abbbc", "....abbbbbc", "....abbbhbc", ".....cchbbc", ".......hhcc", "......hh", ".....hh", "....hh", "...hh", "..hh", ".hh"),
 "shovel": G(E, "...........aac", "..........abbbc", ".........abbbc", "..........bbc", ".........hcc", "........hh", ".......hh", "......hh", ".....hh", "....hh", "...hh", "..hh"),
 "sword": G(E, "............aa", "...........abc", "..........abc", ".........abc", "........abc", ".......abc", "..c...abc", "...cabc", "...hcc", "..hh.c", ".hh", "hh"),
 "hoe": G(E, "....aaaaa", "...abbbbbc", "......hcc", ".....hh", "....hh", "...hh", "..hh", ".hh", "hh"),
 "bow": G(E, "..........aax", ".........ab.x", "........ab..x", ".......hb...x", "......hh....x", ".....hh.....x", "....hb......x", "...ab.......x", "..ab........x", ".ab.........x", "aax"),
 "crossbow": G(E, "aa........aa", ".ab......ba", "..ab....ba", "...xhhhhx", "....hhhh", "....hhhh", "...xhhhhx", "..ab....ba", ".ab......ba", "aa........aa"),
 "trident": G(E, "..a.a.a", "..b.b.b", "..bbbbb", "....b", "....h", "....h", "....h", "....h", "....h", "....h", "....h", "....d"),
 "arrow": G(E, ".............aa", "............abc", "...........hc", "..........hc", ".........h", "........h", ".......h", "......h", ".x..h", "..xh", ".xxx", "x.x"),
 "shield": G(E, "..aaaaaaaaa", "..abbbbbbbbc", "..abbxxxbbbc", "..abbxxxbbbc", "..abbbbbbbbc", "..abbbbbbbbc", "...abbbbbbc", "....abbbbc", ".....abbc", "......cc"),
 "boat": G(E, E, E, E, E, E, "..a..........a", ".abbbbbbbbbbbbb", ".abhhhhhhhhhhbc", "..abbbbbbbbbbc", "...ccccccccccc"),
 "minecart": G(E, E, E, E, "..aaaaaaaaaaa", "..abxxxxxxxxbc", "..abxxxxxxxxbc", "..abbbbbbbbbbc", "...ccccccccccc", "...hh.....hh", "...hh.....hh"),
 "sign": G(E, E, "..aaaaaaaaaaa", "..abbbbbbbbbbc", "..abxxxxxxxbbc", "..abbbbbbbbbbc", "..abxxxxxxbbbc", "..abbbbbbbbbbc", "...cccchccccc", ".......h", ".......h", ".......d"),
 "hanging_sign": G(E, "...h......h", "...h......h", "..aaaaaaaaaaa", "..abbbbbbbbbbc", "..abxxxxxxxbbc", "..abbbbbbbbbbc", "..abxxxxxxbbbc", "..abbbbbbbbbbc", "..ccccccccccc"),
 "door": G("....aaaaaaa", "....abbbbbbc", "....abxxbbxc", "....abxxbbxc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....abbbbybc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....abbbbbbc", "....accccccc"),
 "bed": G(E, E, E, E, E, E, "..yyyyxxxxxxxx", ".ayyyyxxxxxxxxx", ".abbbbbbbbbbbbc", ".abbbbbbbbbbbbc", ".hh..........hh", ".hh..........hh"),
 "banner": G("..aaaaaaaaaaa", "..dhhhhhhhhhd", "...bbbbbbbbb", "...bbbbbbbbb", "...bbxxxxxbb", "...bbxbbbxbb", "...bbxxxxxbb", "...bbbbbbbbb", "...bbbbbbbbb", "...bbbbbbbbb", "....bbbbbbb", ".....bbbbb", "......bbb"),
 "harness": G(E, E, "...aaaaaaaaa", "..abbbbbbbbbc", "..abbxxxxxbbc", "..abxbbbbbxbc", "..abxbbbbbxbc", "..abbxxxxxbbc", "..abbbbbbbbbc", "...ccccccccc"),
 "horse_armor": G(E, E, E, "..........aaa", ".........abbbc", "...aaaaaabbbbc", "..abbbbbbbbbc", "..abbbbbbbbc", "..abc.....bc", "..abc.....bc", "..acc.....cc"),
 "bundle": G(E, E, E, "......hh", ".....h..h", "....aaaaaa", "...abbbbbbb", "..abbbxbbbbb", "..abbbbbbbbbc", "..abbbbbbbbbc", "...bbbbbbbbc", "....cccccc"),
 "potion": G(E, ".......dd", ".......hh", "......abba", "......abba", ".....abxxba", "....abxxxxba", "....axxxxxxb", "....axxxxxxb", "....axxxxxxb", ".....bxxxxb", "......bbbb"),
 "splash": G(E, ".......dd", "......abba", ".....abxxba", "....abxxxxba", "...abxxxxxxba", "...axxxxxxxxb", "...axxxxxxxxb", "...axxxxxxxxb", "....bxxxxxxb", ".....bbbbbb"),
 "rod": G(E, "............a", "...........ab", "..........ab", ".........ab", "........ab", ".......ab", "......ab", ".....ab", "....ab", "...ab", "..ab", ".ab"),
 "fishing_rod": G(E, "...........hx", "..........h.x", ".........h..x", "........h...x", ".......h....x", "......h.....x", ".....h......x", "....h.......a", "...h", "..h", ".h", "h"),
 "compass": G(E, E, "....aaaaaa", "...abbbbbbc", "..abbbxbbbbc", "..abbbxbbbbc", "..abbbyybbbc", "..abbbbybbbc", "..abbbbbbbbc", "...abbbbbbc", "....cccccc"),
 "clock": G(E, E, "....aaaaaa", "...axxxxxxc", "..axxxxxxxxc", "..axxxbxxxxc", "..ayyyyyyyyc", "..ayyyyyyyyc", "..ayyyyyyyyc", "...ayyyyyyc", "....cccccc"),
 "spyglass": G(E, E, "...........aa", "..........abbc", ".........abbc", "........hbcc", ".......hh", "......hh", ".....xh", "....xxh", "...xxx", "..xx"),
 "shears": G(E, E, "......aa", ".....abbc", "....abbc", "...abbc", "..bbcc.hh", "..cc..h..h", ".....h..h", "......hh"),
 "flint_and_steel": G(E, E, E, "...aaaa", "..abbbbc", "..ab..bc", "..ab..bc", "...cc.cc", ".........xx", "........xxxx", ".......xxxyy", "........yyy"),
 "lead": G(E, E, "....hhh", "...h...h", "...h...h", "....hhh.h", "........h", ".........h", "..........h", "...........hh", "...........hh"),
 "name_tag": G(E, E, E, "........aaaa", ".......abbbbc", "......abxbbbc", ".....abbbbbc", "....abbbbbc", "...abbbbbc", "..hbbbbbc", "..h.cccc"),
 "star": G(E, E, ".......a", "......abc", "......abc", "..aaaabbbccc", "...abbbbbbc", "....abbbbc", "....abbbbc", "...abbccbbc", "...bcc..cbc", "...c......c"),
 "totem": G(E, ".....aaaaaa", "....abbbbbbc", "....abxbbxbc", "....abbbbbbc", "....abyyyybc", "..aabbbbbbbbcc", "..abbbbbbbbbbc", ".....bbbbbb", ".....bb..bb", ".....cc..cc"),
 "mace": G(E, "........aaaaa", ".......abbbbbc", ".......abxbxbc", ".......abbbbbc", "........cchcc", ".......hh", "......hh", ".....hh", "....hh", "...hh", "..hh"),
 "elytra": G(E, "..aaa....aaa", ".abbba..abbba", ".abbbba.abbbba", ".abbbbaabbbbba", ".abbbbaabbbbbc", "..abbbaabbbbc", "..abbb..bbbbc", "...abb..bbc", "...ab....bc", "....c....c"),
 "goat_horn": G(E, E, E, "..........aa", ".........abc", "........abc", ".......abbc", ".....aabbc", "...aabbbc", "..abbbcc", "..bbcc", "..cc"),
 "brush": G(E, E, "..........aa", ".........abc", "........abc", ".......hbc", "......hh", ".....hh", "...xxh", "..xxxx", ".xxxx", ".xx"),
 "key": G(E, E, "....aaa", "...abbba", "...ab.bb", "...abbbc", "....bcc", ".....bb", ".....bbb", ".....bb", ".....bbb", ".....cc"),
 "charge": G(E, E, E, "......aaaa", ".....abbbbb", "....abxbbxbb", "....abbxbbbb", "....abbbbxbc", "....abxbbbbc", ".....bbbbbc", "......cccc"),
 "pearl": G(E, E, E, E, ".....aaaa", "....abbbbb", "....abxxbb", "....abxxbc", "....abbbbc", ".....bbcc", E),
 "spawn_egg": G(E, E, "......aaa", ".....abbba", "....abxbbba", "....abbbbxb", "...abbxbbbbb", "...abbbbbxbb", "...bxbbbbbbc", "...bbbbbxbbc", "....bbxbbbc", ".....cccc"),
 "item_frame": G(E, "..hhhhhhhhhhhh", "..hddddddddddh", "..hdbbbbbbbbdh", "..hdbbbbbbbbdh", "..hdbbbbbbbbdh", "..hdbbbbbbbbdh", "..hdbbbbbbbbdh", "..hdbbbbbbbbdh", "..hddddddddddh", "..hhhhhhhhhhhh"),
 "painting": G(E, "..hhhhhhhhhhhh", "..hxxxxxxxyyyh", "..hxxxxxxyyyyh", "..hxxbbxxxyyyh", "..hxbbbbxxxxxh", "..hbbbbbbxxxxh", "..hbbbbbbbbxxh", "..hhhhhhhhhhhh"),
 "armor_stand": G(E, ".......aa", ".......hh", "...hhhhhhhhh", ".......hh", ".......hh", ".....hhhhhh", "......h..h", "......h..h", "......h..h", "....dddddddd"),
 "end_crystal": G(E, E, "......aaa", ".....abxba", "....abxxxba", "....axxxxxb", "....abxxxbc", ".....bxxbc", "......bbc", E, "....hhhhhh", "....dddddd"),
 "head": G(E, E, E, "...aaaaaaaaa", "...abbbbbbbbc", "...abxxbbxxbc", "...abxxbbxxbc", "...abbbbbbbbc", "...abbbyybbbc", "...abbbbbbbbc", "...abbbbbbbbc", "...cccccccccc"),
 "generic": G(E, E, E, "....aaaaaa", "...abbbbbbb", "..abbxbbbxbb", "..abbbbbbbbb", "..abbbxxbbbc", "..abbbbbbbbc", "...bbbbbbbc", "....cccccc"),
 "wheat": G(E, "..........a", ".........ab.a", "........ab.ab", ".......ab.ab", "......hab.ab", ".....hh.ab", "....hh.ab", "...hh.ab", "..hh..h", ".hh", "hh"),
 "seeds": G(E, E, E, E, "......b", "....b...b", "..b..b", ".....b.b", "...b....b", "......b", "....b..b", E),
 "carrot": G(E, ".........xx", "........xx", ".......abx", "......abbc", ".....abbc", "....abbc", "...abbc", "..abbc", "..bcc", ".cc"),
 "berries": G(E, E, "......hh", ".....h..h", "....aa..aa", "...abba.abba", "...bbbc.bbbc", "....cc..ccc", ".......aa", "......abba", "......bbbc", ".......cc"),
 "slime": G(E, E, E, "....aaaaa", "...abbbbba", "..abbxbbxbb", "..abbbbbbbb", "..abbbbbbbc", "...bbbbbbc", "....ccccc"),
 "string": G(E, E, ".........aa", "........ab", ".......ab", "......ab", ".....ab", "....ab", "...ab", "..ab", ".ab", ".b"),
 "feather": G(E, "...........aa", "..........abb", ".........abbb", "........abbbc", ".......abbbc", "......abbbc", ".....abbbc", "....abbcc", "...xbcc", "..x", ".x"),
 "bone": G(E, "...........aa", "..........abba", "...........bbc", "..........bc", ".........bc", "........bc", ".......bc", "......bc", ".abb.bc", "abbbbc", ".bcc"),
 "leather": G(E, E, "...aaaaaaaa", "..abbbbbbbbb", "..abbbbbbbbbc", "...abbbbbbbc", "...abbbbbbbc", "..abbbbbbbbbc", "..abbbbbbbbbc", "...bbbcccbbc", "....cc...cc"),
 "rabbit_foot": G(E, E, E, "........aa", ".......abbb", "......abbbc", ".....abbbc", "....abbbc", "...abbbc", "..xbbbc", "..xxxc"),
}

def icon(template, pal, seed=0, outline=True):
    c = Canvas(seed=seed)
    rows = MASKS[template]
    p = {k: rgb(v) for k, v in pal.items()}
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in p:
                col = p[ch]
                if ch in "bac" and col[3] > 0: col = shade(col, c.r.uniform(0.94, 1.06))
                c.set(x, y, col)
    if outline: c.outline(rgb(0x1a1a1a, 255) if outline is True else rgb(outline))
    return c.image()

def material_pal(main, hi=None, lo=None, handle=0x6b4f2a, accent=None, accent2=None):
    m = rgb(main)
    return {"a": hi if hi is not None else shade(m, 1.35), "b": m, "c": lo if lo is not None else shade(m, 0.62),
            "h": rgb(handle), "d": shade(rgb(handle), 0.7), "x": accent if accent is not None else shade(m, 0.8),
            "y": accent2 if accent2 is not None else shade(m, 1.15)}
