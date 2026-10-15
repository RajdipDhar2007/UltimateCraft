#!/usr/bin/env python3
"""Draws the README artwork as pixel-art SVGs (no fonts or images needed, so it renders identically everywhere):

    python tools/make_banner.py       ->  docs/images/banner.svg, divider.svg, footer.svg, requirements.svg

The lettering is a built-in 5x7 pixel font; the scene (sunset sky, stars, mountains, trees, grass and ore blocks) is
generated with a fixed random seed, so the output is reproducible.
"""
import os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "images")

_G = """
A .###. #...# #...# ##### #...# #...# #...#
B ####. #...# #...# ####. #...# #...# ####.
C .###. #...# #.... #.... #.... #...# .###.
D ####. #...# #...# #...# #...# #...# ####.
E ##### #.... #.... ####. #.... #.... #####
F ##### #.... #.... ####. #.... #.... #....
G .###. #...# #.... #.### #...# #...# .####
H #...# #...# #...# ##### #...# #...# #...#
I ##### ..#.. ..#.. ..#.. ..#.. ..#.. #####
J ..### ...#. ...#. ...#. ...#. #..#. .##..
K #...# #..#. #.#.. ##... #.#.. #..#. #...#
L #.... #.... #.... #.... #.... #.... #####
M #...# ##.## #.#.# #.#.# #...# #...# #...#
N #...# ##..# #.#.# #..## #...# #...# #...#
O .###. #...# #...# #...# #...# #...# .###.
P ####. #...# #...# ####. #.... #.... #....
Q .###. #...# #...# #...# #.#.# #..#. .##.#
R ####. #...# #...# ####. #.#.. #..#. #...#
S .#### #.... #.... .###. ....# ....# ####.
T ##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..
U #...# #...# #...# #...# #...# #...# .###.
V #...# #...# #...# #...# #...# .#.#. ..#..
W #...# #...# #...# #.#.# #.#.# ##.## #...#
X #...# #...# .#.#. ..#.. .#.#. #...# #...#
Y #...# #...# .#.#. ..#.. ..#.. ..#.. ..#..
Z ##### ....# ...#. ..#.. .#... #.... #####
· ..... ..... ..... ..#.. ..... ..... .....
- ..... ..... ..... ###.. ..... ..... .....
0 .###. #...# #..## #.#.# ##..# #...# .###.
1 ..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.
2 .###. #...# ....# ...#. ..#.. .#... #####
3 ####. ....# ....# .###. ....# ....# ####.
4 #...# #...# #...# ##### ....# ....# ....#
5 ##### #.... ####. ....# ....# #...# .###.
6 .###. #.... #.... ####. #...# #...# .###.
7 ##### ....# ...#. ..#.. .#... .#... .#...
8 .###. #...# #...# .###. #...# #...# .###.
9 .###. #...# #...# .#### ....# ....# .###.
+ ..... ..#.. ..#.. ##### ..#.. ..#.. .....
. ..... ..... ..... ..... ..... .##.. .##..
/ ....# ....# ...#. ..#.. .#... #.... #....
> #.... .#... ..#.. ...#. ..#.. .#... #....
! ..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#..
? .###. #...# ....# ...#. ..#.. ..... ..#..
"""
FONT = {line[0]: "".join(line.split()[1:]) for line in _G.strip().splitlines()}
FONT[" "] = "." * 35
assert all(len(v) == 35 for v in FONT.values()), [k for k, v in FONT.items() if len(v) != 35]

def glyph_pixels(ch):
    g = FONT.get(ch.upper(), FONT[" "])
    return [(x, y) for y in range(7) for x in range(5) if g[y * 5 + x] == "#"]

def _runs(pixels):
    """Merge horizontally adjacent pixels of one row into (x, y, length) runs."""
    out, by_row = [], {}
    for (x, y) in pixels: by_row.setdefault(y, []).append(x)
    for y, xs in by_row.items():
        xs.sort(); start = prev = xs[0]
        for x in xs[1:]:
            if x != prev + 1: out.append((start, y, prev - start + 1)); start = x
            prev = x
        out.append((start, y, prev - start + 1))
    return out

def text_rects(text, x0, y0, s, face, shade=None, spacing=1):
    """SVG rects for a text in the pixel font; `face(row)` gives the fill for a pixel row (a simple vertical gradient)."""
    rects, x = [], x0
    hl = max(1, s // 5)
    for ch in text:
        runs = _runs(glyph_pixels(ch))
        if shade:
            for (px, py, n) in runs: rects.append(f'<rect x="{x + px * s + s // 2}" y="{y0 + py * s + s // 2}" width="{n * s}" height="{s}" fill="{shade}"/>')
        for (px, py, n) in runs:
            fx, fy = x + px * s, y0 + py * s
            rects.append(f'<rect x="{fx}" y="{fy}" width="{n * s}" height="{s}" fill="{face(py)}"/><rect x="{fx}" y="{fy}" width="{n * s}" height="{hl}" fill="#fff" fill-opacity=".3"/>'
                         f'<rect x="{fx}" y="{fy + s - hl}" width="{n * s}" height="{hl}" fill="#000" fill-opacity=".22"/>')
        x += (5 + spacing) * s
    return "".join(rects), x - x0 - spacing * s

def lerp_hex(a, b, t):
    a = [int(a[i:i + 2], 16) for i in (1, 3, 5)]; b = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#%02x%02x%02x" % tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))

def banner():
    W, H = 1280, 400
    rnd = random.Random(2026)
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="UltimateCraft - made by RAJDIP DHAR">']
    p.append('<defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">'
             '<stop offset="0" stop-color="#070b1e"/><stop offset="0.45" stop-color="#1d2a66"/><stop offset="0.75" stop-color="#b4527a"/><stop offset="1" stop-color="#ff9a52"/></linearGradient>'
             '<style>@keyframes tw{0%,100%{opacity:.25}50%{opacity:1}}.s{animation:tw 3s ease-in-out infinite}'
             '@keyframes fl{0%,100%{opacity:1}50%{opacity:.55}}.t{animation:fl 1.4s ease-in-out infinite}</style></defs>')
    p.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    for i in range(90):   # stars
        x, y = rnd.randrange(0, W, 4), rnd.randrange(0, 210, 4)
        c = rnd.choice(("#ffffff", "#cfe3ff", "#ffe9b0"))
        p.append(f'<rect class="s" style="animation-delay:{rnd.random() * 3:.2f}s" x="{x}" y="{y}" width="{rnd.choice((4, 4, 8))}" height="4" fill="{c}"/>')
    # a blocky moon with craters
    mx, my = 1090, 48
    p.append(f'<rect x="{mx}" y="{my}" width="72" height="72" fill="#e9edf5"/><rect x="{mx}" y="{my}" width="72" height="8" fill="#ffffff"/>')
    for (cx, cy, cw) in ((10, 14, 16), (40, 30, 12), (22, 46, 14), (52, 8, 8)): p.append(f'<rect x="{mx + cx}" y="{my + cy}" width="{cw}" height="{cw}" fill="#c3cad8"/>')
    # layered stepped mountains
    def ridge(base, amp, col, step=16, seed=1):
        r = random.Random(seed); h = base; pts = []
        for x in range(0, W + step, step):
            h = max(base - amp, min(base + amp // 3, h + r.choice((-2, -1, 0, 0, 1, 2)) * step // 2))
            pts.append((x, h))
        d = f"M0 {H}"
        for x, h in pts: d += f" L{x} {h} L{x + step} {h}"
        return f'<path d="{d} L{W} {H} Z" fill="{col}"/>'
    p.append(ridge(250, 70, "#4a3a78", seed=3)); p.append(ridge(285, 55, "#2e2352", seed=7)); p.append(ridge(318, 40, "#1b1538", seed=11))
    # title
    s = 11
    title = "ULTIMATECRAFT"
    _, tw = text_rects(title, 0, 0, s, lambda r: "#fff")
    tx = (W - tw) // 2 - 8
    face = lambda r: lerp_hex("#fff1b8", "#f2a02a", r / 6)
    svg, _ = text_rects(title, tx, 64, s, face, shade="#2a1636")
    p.append(svg)
    sub = "BUILD  ·  EXPLORE  ·  SURVIVE  ·  ASCEND"
    _, sw = text_rects(sub, 0, 0, 4, lambda r: "#fff")
    svg, _ = text_rects(sub, (W - sw) // 2, 168, 4, lambda r: "#ffe9b0", shade="#2a1636")
    p.append(svg)
    by = "MADE BY RAJDIP DHAR"
    _, bw = text_rects(by, 0, 0, 3, lambda r: "#fff")
    svg, _ = text_rects(by, (W - bw) // 2, 214, 3, lambda r: "#ffffff", shade="#2a1636")
    p.append(svg)
    # trees on the ridge line
    for x in (60, 210, 330, 905, 1010, 1180):
        h = rnd.choice((3, 4, 5)) * 16; y = 330 - h
        p.append(f'<rect x="{x}" y="{y + 32}" width="16" height="{h - 16}" fill="#3b2a1d"/>')
        for (dx, dy, w, hh) in ((-32, 0, 80, 32), (-16, -16, 48, 16), (-16, 32, 48, 16)):
            p.append(f'<rect x="{x + dx}" y="{y + dy}" width="{w}" height="{hh}" fill="#1f4d2b"/><rect x="{x + dx}" y="{y + dy}" width="{w}" height="4" fill="#2f7a40"/>')
    # torches with flickering flames
    for x in (150, 560, 760, 1120):
        p.append(f'<rect x="{x}" y="320" width="6" height="30" fill="#6b4a22"/><rect class="t" x="{x - 2}" y="306" width="10" height="14" fill="#ffd05a"/><rect class="t" x="{x + 1}" y="300" width="4" height="8" fill="#fff2a8"/>')
    # ground: grass, dirt, stone and ore blocks (shading lines span the whole width)
    B = 16
    for gx in range(0, W, B):
        top = rnd.choice(("#4fae3a", "#5bc044", "#46a033"))
        p.append(f'<rect x="{gx}" y="350" width="{B}" height="{B}" fill="{top}"/>')
        for gy in range(366, H, B):
            r = rnd.random(); deep = gy >= 382
            col = ("#7a8088" if r < 0.55 else "#6a7078") if deep else ("#8a5a34" if r < 0.6 else "#7a4e2c")
            p.append(f'<rect x="{gx}" y="{gy}" width="{B}" height="{B}" fill="{col}"/>')
            if deep and rnd.random() < 0.11:
                ore = rnd.choice(("#5ff0ff", "#ffd84a", "#ff5a5a", "#6ad06a"))
                for (ox, oy) in ((3, 3), (8, 6), (5, 9), (10, 11)): p.append(f'<rect x="{gx + ox}" y="{gy + oy}" width="3" height="3" fill="{ore}"/>')
    p.append(f'<rect y="350" width="{W}" height="4" fill="#7ad65b"/>')
    for gy in range(366, H, B):
        p.append(f'<rect y="{gy}" width="{W}" height="2" fill="#fff" fill-opacity=".08"/><rect y="{gy + B - 2}" width="{W}" height="2" fill="#000" fill-opacity=".18"/>')
    for gx in range(0, W, B): p.append(f'<rect x="{gx}" y="350" width="1" height="{H - 350}" fill="#000" fill-opacity=".14"/>')
    p.append("</svg>")
    return "".join(p)

def requirements():
    """Two pixel-art cards: minimum and recommended hardware (keep the numbers in step with README -> System requirements)."""
    W, H = 1280, 420
    rnd = random.Random(5)
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="System requirements: minimum and recommended">'
         '<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0b1026"/><stop offset="1" stop-color="#1d2a66"/></linearGradient>'
         '<style>@keyframes tw{0%,100%{opacity:.25}50%{opacity:1}}.s{animation:tw 3s ease-in-out infinite}</style></defs>'
         f'<rect width="{W}" height="{H}" fill="url(#bg)"/>']
    for i in range(45):
        p.append(f'<rect class="s" style="animation-delay:{rnd.random() * 3:.2f}s" x="{rnd.randrange(0, W, 4)}" y="{rnd.randrange(0, H, 4)}" width="4" height="4" fill="#fff"/>')
    title = "SYSTEM REQUIREMENTS"
    _, tw = text_rects(title, 0, 0, 6, lambda r: "#fff")
    svg, _ = text_rects(title, (W - tw) // 2, 22, 6, lambda r: lerp_hex("#fff1b8", "#f2a02a", r / 6), shade="#0b1026")
    p.append(svg)
    cards = (("MINIMUM", "#4fae3a", "#7ad65b", ("OS   WINDOWS 10 64-BIT", "CPU  2 CORES  2 GHZ", "RAM  2 GB", "GPU  OPENGL 3.3", "DISK 100 MB FREE")),
             ("RECOMMENDED", "#3a7bd5", "#7ab8ff", ("OS   WINDOWS 11 OR LINUX", "CPU  4 CORES  3 GHZ+", "RAM  8 GB", "GPU  ANY DEDICATED GPU", "DISK SSD")))
    cw, cx0, cy0 = 590, 40, 90
    for i, (name, dark, light, rows) in enumerate(cards):
        x = cx0 + i * (cw + 20)
        p.append(f'<rect x="{x}" y="{cy0}" width="{cw}" height="300" fill="#0a0e20"/><rect x="{x}" y="{cy0}" width="{cw}" height="52" fill="{dark}"/><rect x="{x}" y="{cy0}" width="{cw}" height="6" fill="{light}"/>'
                 f'<rect x="{x}" y="{cy0 + 294}" width="{cw}" height="6" fill="#000" fill-opacity=".4"/>')
        svg, _ = text_rects(name, x + 20, cy0 + 12, 4, lambda r: "#fff", shade="#000")
        p.append(svg)
        for j, line in enumerate(rows):
            y = cy0 + 74 + j * 44
            p.append(f'<rect x="{x + 20}" y="{y + 4}" width="16" height="16" fill="{light}"/><rect x="{x + 20}" y="{y + 4}" width="16" height="4" fill="#fff" fill-opacity=".35"/>')
            svg, _ = text_rects(line, x + 52, y, 3, lambda r: "#ffe9b0" if i else "#e8ffe0")
            p.append(svg)
    p.append("</svg>")
    return "".join(p)

STAR = ["....#....", "....#....", "...###...", "#########", ".#######.", "..#####..", ".###.###.", ".##...##.", "#.......#"]

def _star(x, y, s, fill="#ffd84a", hi="#fff6b0", lo="#e0a020"):
    out = []
    for r, row in enumerate(STAR):
        for c, ch in enumerate(row):
            if ch == "#":
                col = hi if (r + c) % 5 == 0 else lo if r > 5 else fill
                out.append(f'<rect x="{x + c * s}" y="{y + r * s}" width="{s}" height="{s}" fill="{col}"/>')
    return "".join(out)

def tagline():
    """An animated one-line hook that cycles through the stages of the game (pure CSS, works inside <img> on GitHub)."""
    W, H = 1280, 64
    phrases = ["PUNCH A TREE.", "SURVIVE THE NIGHT.", "RAID A LOST CITY.", "SLAY A COSMIC TYRANT.", "ASCEND TO THE SIFT."]
    cols = ["#9be36d", "#ffd84a", "#ff9a52", "#ff6b9d", "#7ab8ff"]
    n = len(phrases); T = 3.2
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="Punch a tree. Survive the night. Raid a lost city. Slay a cosmic tyrant. Ascend to the Sift.">'
         f'<style>@keyframes ph{{0%{{opacity:0;transform:translateY(8px)}}3%,17%{{opacity:1;transform:none}}20%,100%{{opacity:0;transform:translateY(-8px)}}}}'
         f'.ph{{animation:ph {n * T}s infinite both}}@keyframes cur{{0%,49%{{opacity:1}}50%,100%{{opacity:0}}}}.cur{{animation:cur .8s infinite}}</style>'
         f'<rect width="{W}" height="{H}" fill="#0b1026"/><rect width="{W}" height="3" fill="#2a3a7a"/><rect y="{H - 3}" width="{W}" height="3" fill="#2a3a7a"/>']
    for i, (t, c) in enumerate(zip(phrases, cols)):
        _, w = text_rects(t, 0, 0, 5, lambda r: "#fff")
        x = (W - w) // 2
        svg, _ = text_rects(t, x, 14, 5, lambda r, c=c: lerp_hex("#ffffff", c, r / 6), shade="#0b1026")
        p.append(f'<g class="ph" style="animation-delay:{i * T}s">{svg}<rect class="cur" x="{x + w + 12}" y="14" width="15" height="35" fill="{c}"/></g>')
    p.append("</svg>")
    return "".join(p)

def star_cta():
    """The call to action: a pulsing pixel star, bouncing arrow and a sparkle field."""
    W, H = 1280, 190
    rnd = random.Random(21)
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="Enjoying UltimateCraft? Star the repository!">'
         '<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1b1538"/><stop offset=".5" stop-color="#3a2670"/><stop offset="1" stop-color="#1b1538"/></linearGradient></defs>'
         '<style>@keyframes pu{0%,100%{transform:scale(1) rotate(-4deg)}50%{transform:scale(1.18) rotate(4deg)}}.st{transform-box:fill-box;transform-origin:center;animation:pu 1.6s ease-in-out infinite}'
         '@keyframes tw{0%,100%{opacity:.2}50%{opacity:1}}.s{animation:tw 2.4s ease-in-out infinite}'
         '@keyframes bo{0%,100%{transform:translate(0,0)}50%{transform:translate(14px,-14px)}}.ar{animation:bo .9s ease-in-out infinite}'
         '@keyframes gl{0%,100%{opacity:1}50%{opacity:.6}}.gl{animation:gl 1.2s infinite}</style>'
         f'<rect width="{W}" height="{H}" fill="url(#g)"/><rect width="{W}" height="6" fill="#ffd84a"/><rect y="{H - 6}" width="{W}" height="6" fill="#ffd84a"/>']
    for i in range(70):
        c = rnd.choice(("#fff", "#ffe9b0", "#ffd84a"))
        p.append(f'<rect class="s" style="animation-delay:{rnd.random() * 2.4:.2f}s" x="{rnd.randrange(0, W, 4)}" y="{rnd.randrange(8, H - 12, 4)}" width="4" height="4" fill="{c}"/>')
    p.append(f'<g class="st">{_star(28, 52, 10)}</g>')
    p.append(f'<g class="st" style="animation-delay:.4s">{_star(1090, 52, 10)}</g>')
    t1 = "ENJOYING ULTIMATECRAFT?"
    _, w1 = text_rects(t1, 0, 0, 6, lambda r: "#fff")
    svg, _ = text_rects(t1, (W - w1) // 2, 34, 6, lambda r: lerp_hex("#fff1b8", "#f2a02a", r / 6), shade="#0b1026"); p.append(svg)
    t2 = "SMASH THE STAR - THE BEST LOOT DROP A DEV CAN GET!"
    _, w2 = text_rects(t2, 0, 0, 3, lambda r: "#fff")
    svg, _ = text_rects(t2, (W - w2) // 2, 104, 3, lambda r: "#ffffff", shade="#0b1026"); p.append(f'<g class="gl">{svg}</g>')
    t3 = "UP THERE  >>>  TOP RIGHT"
    _, w3 = text_rects(t3, 0, 0, 3, lambda r: "#fff")
    svg, _ = text_rects(t3, (W - w3) // 2, 142, 3, lambda r: "#9be36d", shade="#0b1026"); p.append(svg)
    p.append('<g class="ar"><path d="M1200 52 h36 v36 h-10 v-16 l-26 26 -8 -8 26 -26 h-18 z" fill="#ffd84a" transform="translate(-4,-8)"/></g>')
    p.append("</svg>")
    return "".join(p)

def brand():
    """'A RAJDIP DHAR PROJECT' signature with a pixel RD monogram and a shimmering underline."""
    W, H = 640, 96
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="A RAJDIP DHAR project">'
         '<defs><linearGradient id="sh" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#4fae3a"/><stop offset=".5" stop-color="#ffd84a"/><stop offset="1" stop-color="#ff6b9d"/></linearGradient></defs>'
         '<style>@keyframes sw{0%{transform:translateX(-100%)}100%{transform:translateX(100%)}}.sw{animation:sw 2.8s linear infinite}</style>'
         f'<rect width="{W}" height="{H}" fill="#0b1026"/><rect width="{W}" height="4" fill="url(#sh)"/><rect y="{H - 4}" width="{W}" height="4" fill="url(#sh)"/>'
         '<rect x="16" y="16" width="64" height="64" fill="#f2a02a"/><rect x="16" y="16" width="64" height="6" fill="#fff1b8"/><rect x="16" y="74" width="64" height="6" fill="#b86f10"/>']
    for k, ch in enumerate("RD"):
        svg, _ = text_rects(ch, 22 + k * 24, 27, 4, lambda r: "#2a1636"); p.append(svg)
    svg, _ = text_rects("A PROJECT BY", 100, 18, 3, lambda r: "#9fb0ff"); p.append(svg)
    svg, _ = text_rects("RAJDIP DHAR", 100, 42, 5, lambda r: lerp_hex("#fff1b8", "#f2a02a", r / 6), shade="#2a1636"); p.append(svg)
    p.append('<clipPath id="c"><rect x="100" y="84" width="520" height="4"/></clipPath><g clip-path="url(#c)"><rect class="sw" x="100" y="84" width="520" height="4" fill="#fff" fill-opacity=".8"/></g>')
    p.append("</svg>")
    return "".join(p)

def divider():
    W, H = 1280, 28
    rnd = random.Random(7)
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="presentation"><style>@keyframes fl{{0%,100%{{opacity:1}}50%{{opacity:.5}}}}.t{{animation:fl 1.4s infinite}}</style>']
    for gx in range(0, W, 8):
        col = rnd.choice(("#4fae3a", "#5bc044", "#46a033"))
        p.append(f'<rect x="{gx}" y="16" width="8" height="12" fill="#8a5a34"/><rect x="{gx}" y="12" width="8" height="8" fill="{col}"/><rect x="{gx}" y="12" width="8" height="2" fill="#7ad65b"/>')
    for x in sorted(rnd.sample(range(24, W - 24, 8), 30)):   # tufts and flowers
        kind = rnd.random()
        if kind < 0.6: p.append(f'<rect x="{x}" y="6" width="2" height="8" fill="#3d9a30"/><rect x="{x + 2}" y="8" width="2" height="6" fill="#5bc044"/>')
        else:
            c = rnd.choice(("#ff5a7a", "#ffd84a", "#7ab8ff", "#ffffff"))
            p.append(f'<rect x="{x + 1}" y="8" width="2" height="6" fill="#3d9a30"/><rect x="{x - 1}" y="4" width="6" height="4" fill="{c}"/>')
    for x in (160, 640, 1120):
        p.append(f'<rect x="{x}" y="4" width="4" height="10" fill="#6b4a22"/><rect class="t" x="{x - 1}" y="0" width="6" height="6" fill="#ffd05a"/>')
    p.append("</svg>")
    return "".join(p)

def footer():
    W, H = 1280, 150
    rnd = random.Random(11)
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="Made by RAJDIP DHAR"><defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0b1026"/><stop offset="1" stop-color="#26346e"/></linearGradient>'
         '<style>@keyframes tw{0%,100%{opacity:.25}50%{opacity:1}}.s{animation:tw 3s ease-in-out infinite}</style></defs>'
         f'<rect width="{W}" height="{H}" fill="url(#g)"/>']
    for i in range(50):
        p.append(f'<rect class="s" style="animation-delay:{rnd.random() * 3:.2f}s" x="{rnd.randrange(0, W, 4)}" y="{rnd.randrange(0, 90, 4)}" width="4" height="4" fill="#ffffff"/>')
    msg = "MADE BY RAJDIP DHAR"
    _, w = text_rects(msg, 0, 0, 6, lambda r: "#fff")
    svg, _ = text_rects(msg, (W - w) // 2, 34, 6, lambda r: lerp_hex("#fff1b8", "#f2a02a", r / 6), shade="#0b1026")
    p.append(svg)
    for gx in range(0, W, 16):
        p.append(f'<rect x="{gx}" y="118" width="16" height="32" fill="#8a5a34"/><rect x="{gx}" y="118" width="16" height="12" fill="{rnd.choice(("#4fae3a", "#5bc044", "#46a033"))}"/><rect x="{gx}" y="118" width="16" height="3" fill="#7ad65b"/>')
    p.append("</svg>")
    return "".join(p)

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("banner.svg", banner), ("divider.svg", divider), ("footer.svg", footer), ("requirements.svg", requirements), ("tagline.svg", tagline), ("star.svg", star_cta), ("brand.svg", brand)):
        open(os.path.join(OUT, name), "w", encoding="utf-8").write(fn())
        print("wrote", name)
