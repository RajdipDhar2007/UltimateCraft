"""Every Minecraft mob: stats (Java Edition, Normal difficulty), drops, AI type, spawn rules,
box model (auto-packed box UVs) and a procedurally painted skin."""
import os, random, math
from PIL import Image
from items import MOBS
import paint

# --------------------------------------------------------------------------------------------- enums mirrored in C
AI = ["PASSIVE", "NEUTRAL", "HOSTILE", "RANGED", "CREEPER", "SLIME", "FLYER", "FISH", "SQUID", "SHULKER", "GUARDIAN", "ENDERMAN",
      "SPIDER", "PHANTOM", "GOLEM", "VILLAGER", "WARDEN", "CREAKING", "BOSS_WITHER", "BOSS_DRAGON", "EVOKER", "WITCH", "BREEZE", "BLAZE", "GHAST", "SILVERFISH", "GHOST", "DRIFTER", "BOSS_TYRANT", "BOSS_KEEPER", "SOUL", "GOD", "BOSS_AVATAR", "BOSS_REVENANT"]
ROLES = ["BODY", "HEAD", "ARM_R", "ARM_L", "LEG_R", "LEG_L", "LEG_FR", "LEG_FL", "LEG_BR", "LEG_BL", "WING_R", "WING_L", "SPIDER_LEG",
         "TAIL", "FIN", "TENTACLE", "ROD", "STATIC", "JAW"]
F = dict(UNDEAD=1, ARTHROPOD=2, FLYING=4, SWIMS=8, AQUATIC=16, FIRE_IMMUNE=32, BURNS=64, CLIMBS=128, NO_FALL=256, BREEDS=512,
         TAMEABLE=1024, SHEARABLE=2048, MILKABLE=4096, PERSIST=8192, BOSS=16384, NO_KNOCKBACK=32768, HOPS=65536, WALKS_ON_LAVA=131072,
         ILLAGER=262144, BABY_SMALL=524288, FLEES_PLAYER=1048576, GROUP_ANGER=2097152, LAYS_EGG=4194304, RIDES_WATER=8388608, IMMUNE_DROWN=16777216, GLOWS=33554432)
SPAWN_CATS = ["MONSTER", "CREATURE", "WATER", "WATER_AMBIENT", "AMBIENT", "UNDERGROUND_WATER", "AXOLOTL", "LAVA"]

# --------------------------------------------------------------------------------------------- models
def P(role, box, size, pivot, rot=(0, 0, 0), idx=0, mirror=False, uv=None):
    d = dict(role=role, box=list(box), size=list(size), pivot=list(pivot), rot=list(rot), idx=idx)
    if uv: d["u"], d["v"] = uv
    return d

def classic_biped():
    """Minecraft's own 64x32 biped UV layout (the same layout the classic humanoid skins use)."""
    return [P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 24, 0), uv=(0, 0)), P("BODY", (-4, 0, -2), (8, 12, 4), (0, 12, 0), uv=(16, 16)),
            P("ARM_R", (-3, -10, -2), (4, 12, 4), (-5, 22, 0), uv=(40, 16)), P("ARM_L", (-1, -10, -2), (4, 12, 4), (5, 22, 0), uv=(40, 16)),
            P("LEG_R", (-2, -12, -2), (4, 12, 4), (-2, 12, 0), uv=(0, 16)), P("LEG_L", (-2, -12, -2), (4, 12, 4), (2, 12, 0), uv=(0, 16))]

def biped(head=8, body=(8, 12, 4), arm=(4, 12, 4), leg=(4, 12, 4), nose=False, snout=False, ears=False, hat=False, robe=False, crossed=False, height=None):
    bw, bh, bd = body
    lh = leg[1]
    top = lh + bh
    parts = [P("HEAD", (-head / 2, 0, -head / 2), (head, head + (2 if nose and head == 8 and False else 0), head), (0, top, 0)),
             P("BODY", (-bw / 2, 0, -bd / 2), (bw, bh, bd), (0, lh, 0))]
    if nose: parts.append(P("HEAD", (-1, 1, head / 2), (2, 4, 2), (0, top, 0)))
    if snout: parts.append(P("HEAD", (-2.5, 0.5, head / 2), (5, 4, 1), (0, top, 0)))
    if ears:
        parts.append(P("HEAD", (-head / 2 - 1, 2, -1), (1, 5, 4), (0, top, 0), (0, 0, -20)))
        parts.append(P("HEAD", (head / 2, 2, -1), (1, 5, 4), (0, top, 0), (0, 0, 20)))
    if hat:
        parts.append(P("HEAD", (-5, head, -5), (10, 2, 10), (0, top, 0)))
        parts.append(P("HEAD", (-3, head + 2, -3), (6, 4, 6), (0, top, 0)))
        parts.append(P("HEAD", (-1, head + 6, 0), (2, 3, 2), (0, top, 0), (-20, 0, 0)))
    if robe: parts.append(P("STATIC", (-bw / 2 - 0.5, 0, -bd / 2 - 0.5), (bw + 1, bh + 6, bd + 1), (0, lh - 6, 0)))
    aw, ah, ad = arm
    if crossed:
        parts.append(P("STATIC", (-4, -6, -2), (8, 4, 4), (0, top - 2, -2 - bd / 2 + 4)))
    else:
        parts.append(P("ARM_R", (-aw + 1, -ah + 2, -ad / 2), arm, (-bw / 2 - 1 + 0, top - 2, 0)))
        parts.append(P("ARM_L", (-1, -ah + 2, -ad / 2), arm, (bw / 2 + 1, top - 2, 0)))
    lw, lhh, ld = leg
    parts.append(P("LEG_R", (-lw / 2, -lhh, -ld / 2), leg, (-bw / 4 if bw >= 8 else -1.5, lhh, 0)))
    parts.append(P("LEG_L", (-lw / 2, -lhh, -ld / 2), leg, (bw / 4 if bw >= 8 else 1.5, lhh, 0)))
    return parts

def quad(body=(10, 8, 16), leg=(4, 6, 4), head=(8, 8, 8), head_y=None, head_z=None, neck=None, tail=None, horns=None, snout=None, ears=None, legs_inset=1, mane=None):
    bw, bh, bl = body
    lw, lh, ld = leg
    by = lh                      # body bottom
    parts = [P("BODY", (-bw / 2, 0, -bl / 2), (bw, bh, bl), (0, by, 0))]
    hy = head_y if head_y is not None else by + bh - 2
    hz = head_z if head_z is not None else bl / 2
    hw, hh, hd = head
    parts.append(P("HEAD", (-hw / 2, -hh / 2, 0), head, (0, hy, hz)))
    if snout: parts.append(P("HEAD", (-snout[0] / 2, -hh / 2, hd), snout, (0, hy, hz)))
    if horns:
        parts.append(P("HEAD", (-hw / 2 - 1, hh / 2 - 2, 1), horns, (0, hy, hz)))
        parts.append(P("HEAD", (hw / 2 + 1 - horns[0], hh / 2 - 2, 1), horns, (0, hy, hz)))
    if ears:
        parts.append(P("HEAD", (-hw / 2, hh / 2, 1), ears, (0, hy, hz)))
        parts.append(P("HEAD", (hw / 2 - ears[0], hh / 2, 1), ears, (0, hy, hz)))
    if neck: parts.append(P("HEAD", (-neck[0] / 2, -hh / 2 - neck[1] + 2, 0), neck, (0, hy, hz)))
    if mane: parts.append(P("STATIC", (-mane[0] / 2, 0, -mane[2] / 2), mane, (0, by + bh - 2, bl / 2 - 3)))
    if tail: parts.append(P("TAIL", (-tail[0] / 2, -tail[1], -tail[2]), tail, (0, by + bh - 1, -bl / 2)))
    fx, fz = bw / 2 - lw / 2 - legs_inset + 1, bl / 2 - ld / 2 - 1
    for role, sx, sz in (("LEG_FR", -1, 1), ("LEG_FL", 1, 1), ("LEG_BR", -1, -1), ("LEG_BL", 1, -1)):
        parts.append(P(role, (-lw / 2, -lh, -ld / 2), leg, (sx * fx, lh, sz * fz)))
    return parts

def bird(body=(6, 6, 8), leg=(1, 5, 1), head=(4, 6, 3), wing=(1, 4, 6), beak=(4, 2, 2), comb=True, tail=None):
    bw, bh, bl = body
    lh = leg[1]
    parts = [P("BODY", (-bw / 2, 0, -bl / 2), body, (0, lh, 0)),
             P("HEAD", (-head[0] / 2, 0, -1), head, (0, lh + bh - 2, bl / 2 - 1)),
             P("HEAD", (-beak[0] / 2, head[1] - 4, head[2] - 1), beak, (0, lh + bh - 2, bl / 2 - 1))]
    if comb: parts.append(P("HEAD", (-1, head[1] - 6, head[2] - 1), (2, 2, 2), (0, lh + bh - 2, bl / 2 - 1)))
    parts += [P("LEG_R", (-0.5, -lh, -0.5), (leg[0] + 2, lh, leg[2] + 2), (-1.5, lh, 0)), P("LEG_L", (-0.5, -lh, -0.5), (leg[0] + 2, lh, leg[2] + 2), (1.5, lh, 0)),
              P("WING_R", (-1, -wing[1], -wing[2] / 2), wing, (-bw / 2, lh + bh - 1, 0)), P("WING_L", (0, -wing[1], -wing[2] / 2), wing, (bw / 2, lh + bh - 1, 0))]
    if tail: parts.append(P("TAIL", (-tail[0] / 2, -tail[1], -tail[2]), tail, (0, lh + bh - 1, -bl / 2)))
    return parts

def flyer(body=(6, 6, 6), wing=(12, 1, 8), head=None, tail=None, y=8, legs=False):
    parts = [P("BODY", (-body[0] / 2, -body[1] / 2, -body[2] / 2), body, (0, y, 0))]
    if head: parts.append(P("HEAD", (-head[0] / 2, -head[1] / 2, 0), head, (0, y, body[2] / 2)))
    parts += [P("WING_R", (-wing[0], -wing[1] / 2, -wing[2] / 2), wing, (-body[0] / 2, y + body[1] / 2 - 1, 0)),
              P("WING_L", (0, -wing[1] / 2, -wing[2] / 2), wing, (body[0] / 2, y + body[1] / 2 - 1, 0))]
    if tail: parts.append(P("TAIL", (-tail[0] / 2, -tail[1] / 2, -tail[2]), tail, (0, y, -body[2] / 2)))
    if legs:
        parts += [P("LEG_R", (-0.5, -3, -0.5), (1, 3, 1), (-1, y - body[1] / 2, 0)), P("LEG_L", (-0.5, -3, -0.5), (1, 3, 1), (1, y - body[1] / 2, 0))]
    return parts

def fish(body=(2, 4, 8), fin=(1, 3, 3), tail=(0.1, 4, 4), head=None, y=3):
    bw, bh, bl = body
    parts = [P("BODY", (-bw / 2, -bh / 2, -bl / 2), body, (0, y, 0)),
             P("TAIL", (-0.5, -tail[1] / 2, -tail[2]), (1, tail[1], tail[2]), (0, y, -bl / 2)),
             P("FIN", (-0.5, 0, -fin[2] / 2), fin, (0, y + bh / 2, 0))]
    if head: parts.append(P("HEAD", (-head[0] / 2, -head[1] / 2, 0), head, (0, y, bl / 2)))
    return parts

def tentacled(body=(12, 16, 12), n=8, tlen=18, tsize=2, y=None, ring=5, head_role="BODY"):
    bw, bh, bd = body
    y = y if y is not None else tlen
    parts = [P(head_role, (-bw / 2, 0, -bd / 2), body, (0, y, 0))]
    for i in range(n):
        a = i / n * 2 * math.pi
        parts.append(P("TENTACLE", (-tsize / 2, -tlen, -tsize / 2), (tsize, tlen, tsize), (math.cos(a) * ring, y, math.sin(a) * ring), idx=i))
    return parts

def cube_mob(s=8, inner=None, eyes=True):
    parts = [P("BODY", (-s / 2, 0, -s / 2), (s, s, s), (0, 0, 0))]
    if inner: parts.append(P("STATIC", (-inner / 2, (s - inner) / 2, -inner / 2), (inner, inner, inner), (0, 0, 0)))
    return parts

def spider(body=(10, 8, 12), head=(8, 8, 8), legl=16, y=9):
    parts = [P("HEAD", (-head[0] / 2, -head[1] / 2, 0), head, (0, y, 3)),
             P("BODY", (-3, -3, -3), (6, 6, 6), (0, y, 0)),
             P("BODY", (-body[0] / 2, -body[1] / 2, -body[2]), body, (0, y, -3))]
    for i, (z, yaw) in enumerate(((2, 45), (1, 20), (0, -20), (-1, -45))):
        parts.append(P("SPIDER_LEG", (-1, -1, -1), (legl, 2, 2), (4, y, z), (0, yaw, -35), idx=i * 2))
        parts.append(P("SPIDER_LEG", (-legl + 1, -1, -1), (legl, 2, 2), (-4, y, z), (0, yaw, 35), idx=i * 2 + 1))
    return parts

def segmented(n=5, base=(6, 4, 3), y=2):
    parts = []
    z = 0
    for i in range(n):
        s = 1 - abs(i - n * 0.35) / n
        w, h, d = max(2, base[0] * s), max(2, base[1] * s), base[2]
        parts.append(P("HEAD" if i == 0 else ("TAIL" if i == n - 1 else "BODY"), (-w / 2, 0, -d / 2), (w, h, d), (0, 0, z), idx=i))
        z -= d
    for p in parts: p["pivot"][2] -= z / 2
    return parts

def rods(head=(8, 8, 8), n=12, rod=(2, 8, 2), y=16, r=7, body=None):
    parts = [P("HEAD", (-head[0] / 2, 0, -head[1] / 2), head, (0, y, 0))]
    if body: parts.append(P("BODY", (-body[0] / 2, -body[1], -body[2] / 2), body, (0, y, 0)))
    for i in range(n):
        layer = i // 4
        a = (i % 4) / 4 * 2 * math.pi + layer * 0.6
        parts.append(P("ROD", (-rod[0] / 2, -rod[1] / 2, -rod[2] / 2), rod, (math.cos(a) * (r - layer * 2), y - 2 - layer * 6, math.sin(a) * (r - layer * 2)), idx=i))
    return parts

MODELS = {
    "humanoid": biped(),
    "zombie_classic": classic_biped(),
    "piglin": biped(head=8, snout=True, ears=True),
    "villager": biped(nose=True, robe=True, crossed=True),
    "illager": biped(nose=True),
    "witch": biped(nose=True, robe=True, crossed=True, hat=True),
    "skeleton": biped(arm=(2, 12, 2), leg=(2, 12, 2)),
    "enderman": [P("HEAD", (-5, 6, -1), (2, 5, 2), (0, 38, 0), (0, 0, 25)), P("HEAD", (3, 6, -1), (2, 5, 2), (0, 38, 0), (0, 0, -25)),
                 P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 38, 0)), P("BODY", (-4, 0, -2), (8, 12, 4), (0, 26, 0)),
                 P("ARM_R", (-1, -28, -1), (2, 30, 2), (-5, 36, 0)), P("ARM_L", (-1, -28, -1), (2, 30, 2), (5, 36, 0)),
                 P("LEG_R", (-1, -26, -1), (2, 26, 2), (-2, 26, 0)), P("LEG_L", (-1, -26, -1), (2, 26, 2), (2, 26, 0))],
    "creeper": [P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 18, 0)), P("BODY", (-4, 0, -2), (8, 12, 4), (0, 6, 0))] +
               [P(r, (-2, -6, -2), (4, 6, 4), (sx * 2, 6, sz * 4)) for r, sx, sz in (("LEG_FR", -1, 1), ("LEG_FL", 1, 1), ("LEG_BR", -1, -1), ("LEG_BL", 1, -1))],
    "spider": spider(),
    "cow": quad(body=(12, 10, 18), leg=(4, 12, 4), head=(8, 8, 6), horns=(1, 3, 1)),
    "pig": quad(body=(10, 8, 16), leg=(4, 6, 4), head=(8, 8, 8), snout=(4, 3, 1)),
    "sheep": quad(body=(10, 10, 16), leg=(4, 12, 4), head=(6, 6, 8)),
    "goat": quad(body=(9, 10, 16), leg=(3, 10, 3), head=(5, 7, 10), horns=(2, 7, 2), snout=None),
    "wolf": quad(body=(6, 6, 14), leg=(2, 8, 2), head=(6, 6, 4), snout=(3, 3, 4), ears=(2, 2, 1), tail=(2, 8, 2), mane=(8, 6, 7)),
    "fox": quad(body=(6, 6, 11), leg=(2, 6, 2), head=(8, 6, 6), snout=(4, 2, 3), ears=(2, 2, 1), tail=(4, 4, 9)),
    "cat": quad(body=(4, 5, 16), leg=(2, 6, 2), head=(5, 4, 5), snout=(3, 2, 2), ears=(1, 1, 2), tail=(1, 1, 8)),
    "bear": quad(body=(14, 14, 22), leg=(6, 10, 6), head=(7, 7, 7), snout=(5, 3, 3), ears=(2, 2, 1)),
    "panda": quad(body=(18, 16, 26), leg=(6, 9, 6), head=(13, 10, 9), snout=(7, 5, 2), ears=(5, 4, 1)),
    "hoglin": quad(body=(16, 14, 19), leg=(6, 14, 6), head=(14, 6, 19), horns=(2, 11, 2), ears=(6, 1, 4), mane=(2, 7, 15)),
    "horse": quad(body=(10, 10, 22), leg=(4, 11, 4), head=(5, 5, 7), head_y=24, neck=(4, 12, 7), ears=(2, 3, 1), tail=(3, 14, 4), mane=(2, 16, 4)),
    "llama": quad(body=(12, 10, 18), leg=(4, 14, 4), head=(6, 6, 8), head_y=34, neck=(8, 18, 6), ears=(3, 3, 2), tail=None),
    "camel": quad(body=(15, 12, 27), leg=(5, 21, 5), head=(7, 8, 19), head_y=36, neck=(7, 14, 7), tail=(3, 14, 1), mane=(9, 5, 11)),
    "sniffer": quad(body=(25, 22, 40), leg=(7, 10, 7), head=(13, 18, 11), head_y=22, snout=(13, 2, 6), ears=(1, 19, 7)),
    "ravager": quad(body=(14, 16, 20), leg=(8, 37, 8), head=(16, 20, 16), head_y=40, head_z=12, horns=(2, 14, 4), snout=(4, 8, 4)),
    "armadillo": quad(body=(8, 7, 12), leg=(2, 3, 2), head=(3, 5, 2), ears=(2, 5, 1), tail=(1, 1, 6)),
    "rabbit": quad(body=(6, 5, 10), leg=(2, 4, 3), head=(5, 4, 5), ears=(2, 5, 1), tail=(2, 2, 2)),
    "turtle": quad(body=(19, 4, 20), leg=(4, 1, 10), head=(6, 5, 6), head_y=4, legs_inset=-2),
    "frog": quad(body=(7, 3, 9), leg=(3, 3, 4), head=(7, 3, 9), head_y=6, head_z=-4, legs_inset=-1),
    "axolotl": quad(body=(8, 4, 10), leg=(3, 1, 5), head=(8, 5, 5), head_y=4, tail=(1, 5, 12), ears=(3, 5, 1), legs_inset=-1),
    "strider": [P("BODY", (-8, 0, -8), (16, 14, 16), (0, 16, 0)), P("HEAD", (-8, 14, -8), (16, 4, 16), (0, 16, 0)),
                P("LEG_R", (-2, -16, -2), (4, 16, 4), (-4, 16, 0)), P("LEG_L", (-2, -16, -2), (4, 16, 4), (4, 16, 0))],
    "chicken": bird(),
    "parrot": bird(body=(3, 6, 3), leg=(1, 2, 1), head=(2, 3, 2), wing=(1, 5, 3), beak=(1, 2, 2), comb=False, tail=(3, 4, 1)),
    "bat": flyer(body=(6, 6, 6), wing=(10, 1, 8), head=(6, 6, 6), y=10),
    "phantom": flyer(body=(5, 3, 9), wing=(16, 1, 9), head=(7, 3, 5), tail=(3, 2, 6), y=8),
    "bee": flyer(body=(7, 7, 10), wing=(9, 0.5, 6), head=None, y=6, legs=True),
    "allay": [P("HEAD", (-2.5, 0, -2.5), (5, 5, 5), (0, 14, 0)), P("BODY", (-1.5, 0, -1), (3, 5, 2), (0, 9, 0)),
              P("ARM_R", (-1, -4, -0.5), (1, 4, 1), (-2, 13, 0)), P("ARM_L", (0, -4, -0.5), (1, 4, 1), (2, 13, 0)),
              P("WING_R", (-8, -1, 0), (8, 5, 0.5), (-1, 12, -1)), P("WING_L", (0, -1, 0), (8, 5, 0.5), (1, 12, -1))],
    "vex": [P("HEAD", (-2.5, 0, -2.5), (5, 5, 5), (0, 14, 0)), P("BODY", (-1.5, 0, -1), (3, 5, 2), (0, 9, 0)), P("BODY", (-1, -4, -1), (2, 4, 2), (0, 9, 0)),
            P("ARM_R", (-1, -4, -0.5), (1, 4, 1), (-2, 13, 0)), P("ARM_L", (0, -4, -0.5), (1, 4, 1), (2, 13, 0)),
            P("WING_R", (-8, -1, 0), (8, 5, 0.5), (-1, 12, -1)), P("WING_L", (0, -1, 0), (8, 5, 0.5), (1, 12, -1))],
    "ghast": tentacled(body=(16, 16, 16), n=9, tlen=12, y=12, ring=5),
    "blaze": rods(head=(8, 8, 8), n=12, y=16, r=8),
    "breeze": rods(head=(8, 8, 8), n=4, rod=(2, 10, 2), y=18, r=4, body=(4, 8, 4)),
    "squid": tentacled(body=(12, 16, 12), n=8, tlen=18, y=18, ring=5),
    "nautilus": [P("BODY", (-7, 0, -7), (14, 12, 14), (0, 4, 0)), P("HEAD", (-4, 0, 0), (8, 6, 4), (0, 4, 7))] +
                [P("TENTACLE", (-1, -6, -1), (2, 6, 2), (-3 + i * 2, 5, 9), idx=i) for i in range(4)],
    "fish": fish(),
    "salmon": fish(body=(3, 5, 12), fin=(1, 2, 4), tail=(1, 5, 6)),
    "tropical": fish(body=(2, 3, 6), fin=(1, 4, 6), tail=(1, 3, 3)),
    "puffer": [P("BODY", (-4, 0, -4), (8, 8, 8), (0, 0, 0)), P("FIN", (-0.5, 0, -2), (1, 2, 2), (-4, 4, 0)), P("FIN", (-0.5, 0, -2), (1, 2, 2), (4, 4, 0))],
    "tadpole": [P("BODY", (-1.5, 0, -1.5), (3, 2, 3), (0, 0, 1)), P("TAIL", (0, 0, -7), (0.1, 2, 7), (0, 0, -0.5))],
    "dolphin": [P("BODY", (-4, 0, -6.5), (8, 7, 13), (0, 0, 0)), P("HEAD", (-4, 0, 0), (8, 7, 6), (0, 0, 6.5)), P("HEAD", (-1, 0, 6), (2, 2, 4), (0, 0, 6.5)),
                P("TAIL", (-2, 0, -11), (4, 5, 11), (0, 1, -6.5)), P("FIN", (-0.5, 0, -2), (1, 4, 5), (0, 7, 0)),
                P("WING_R", (-8, 0, -2), (8, 1, 4), (-4, 1, 2)), P("WING_L", (0, 0, -2), (8, 1, 4), (4, 1, 2))],
    "guardian": [P("BODY", (-6, 0, -8), (12, 12, 16), (0, 0, 0)), P("BODY", (-8, 0, -6), (16, 12, 12), (0, 0, 0)), P("BODY", (-6, -2, -6), (12, 16, 12), (0, 0, 0)),
                 P("HEAD", (-1, 5, 8), (2, 2, 1), (0, 0, 0)),
                 P("TAIL", (-2, 4, -8), (4, 4, 8), (0, 0, -8)), P("TAIL", (-1.5, 4.5, -14), (3, 3, 7), (0, 0, -8)), P("TAIL", (-1, 5, -20), (2, 2, 6), (0, 0, -8))] +
                [P("STATIC", (-1, -1, -1), (2, 9, 2), (sx * 7, 6 + sy * 7, sz * 7), (sz * 30, 0, -sx * 30)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)],
    "golem": [P("HEAD", (-4, 0, -5.5), (8, 10, 8), (0, 31, -2)), P("HEAD", (-1, 1, 2.5), (2, 4, 2), (0, 31, -2)), P("BODY", (-9, 0, -6), (18, 12, 11), (0, 19, 0)),
              P("BODY", (-4.5, 0, -3), (9, 5, 6), (0, 14, 0)), P("ARM_R", (-4, -28, -3), (4, 30, 6), (-9, 29, 0)), P("ARM_L", (0, -28, -3), (4, 30, 6), (9, 29, 0)),
              P("LEG_R", (-3, -14, -2.5), (6, 16, 5), (-4, 14, 0)), P("LEG_L", (-3, -14, -2.5), (6, 16, 5), (5, 14, 0))],
    "snow_golem": [P("BODY", (-6, 0, -6), (12, 12, 12), (0, 0, 0)), P("BODY", (-5, 0, -5), (10, 10, 10), (0, 11, 0)), P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 20, 0)),
                   P("ARM_R", (-12, 0, -1), (12, 2, 2), (-5, 17, 0), (0, 0, -30)), P("ARM_L", (0, 0, -1), (12, 2, 2), (5, 17, 0), (0, 0, 30))],
    "copper_golem": [P("HEAD", (-4, 0, -2.5), (8, 5, 5), (0, 10, 0)), P("HEAD", (-1, 5, -1), (2, 3, 2), (0, 10, 0)), P("HEAD", (-1, 1, 2.5), (2, 3, 1), (0, 10, 0)),
                     P("BODY", (-4, 0, -2), (8, 6, 4), (0, 4, 0)), P("ARM_R", (-3, -9, -1.5), (3, 10, 3), (-4, 9, 0)), P("ARM_L", (0, -9, -1.5), (3, 10, 3), (4, 9, 0)),
                     P("LEG_R", (-2, -4, -2), (4, 4, 4), (-2, 4, 0)), P("LEG_L", (-2, -4, -2), (4, 4, 4), (2, 4, 0))],
    "slime": cube_mob(8, inner=6),
    "magma": cube_mob(8),
    "shulker": [P("BODY", (-8, 0, -8), (16, 8, 16), (0, 0, 0)), P("STATIC", (-8, 0, -8), (16, 4, 16), (0, 8, 0)), P("HEAD", (-3, 0, -3), (6, 6, 6), (0, 4, 0))],
    "silverfish": segmented(7, (6, 4, 3)),
    "endermite": segmented(4, (4, 3, 3)),
    "warden": [P("BODY", (-9, 0, -5), (18, 21, 11), (0, 13, 0)), P("HEAD", (-8, 0, -5), (16, 16, 10), (0, 34, 0)),
               P("HEAD", (-14, 8, 0), (6, 10, 1), (0, 34, 0)), P("HEAD", (8, 8, 0), (6, 10, 1), (0, 34, 0)),
               P("ARM_R", (-8, -28, -4), (8, 28, 8), (-9, 32, 0)), P("ARM_L", (0, -28, -4), (8, 28, 8), (9, 32, 0)),
               P("LEG_R", (-3, -13, -3), (6, 13, 6), (-5, 13, 0)), P("LEG_L", (-3, -13, -3), (6, 13, 6), (5, 13, 0))],
    "creaking": [P("HEAD", (-3, 0, -3), (6, 10, 6), (0, 39, 0)), P("BODY", (-3, 0, -2), (6, 13, 5), (0, 26, 0)),
                 P("ARM_R", (-3, -21, -1.5), (3, 21, 3), (-3, 38, 0)), P("ARM_L", (0, -21, -1.5), (3, 21, 3), (3, 38, 0)),
                 P("LEG_R", (-1.5, -26, -1.5), (3, 26, 3), (-1.5, 26, 0)), P("LEG_L", (-1.5, -26, -1.5), (3, 26, 3), (1.5, 26, 0))],
    "wither": [P("BODY", (-10, 0, -1.5), (20, 3, 3), (0, 24, 0)), P("BODY", (-1.5, 0, -1.5), (3, 10, 3), (0, 14, 0)),
               P("BODY", (-6, 0, -1), (12, 2, 2), (0, 20, 0)), P("BODY", (-5, 0, -1), (10, 2, 2), (0, 17, 0)), P("TAIL", (-1.5, -6, -1.5), (3, 6, 3), (0, 14, 0)),
               P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 27, 0)), P("STATIC", (-3, 0, -3), (6, 6, 6), (-9, 25, 0)), P("STATIC", (-3, 0, -3), (6, 6, 6), (9, 25, 0))],
    "dragon": [P("BODY", (-12, 0, -16), (24, 24, 64), (0, 20, 0)), P("HEAD", (-8, -8, 0), (16, 16, 16), (0, 40, 52)), P("HEAD", (-6, -8, 16), (12, 5, 16), (0, 40, 52)),
               P("STATIC", (-5, 0, 0), (10, 10, 20), (0, 34, 32)), P("TAIL", (-5, 0, -60), (10, 10, 60), (0, 30, -16)),
               P("WING_R", (-56, 0, -8), (56, 4, 40), (-12, 38, 24)), P("WING_L", (0, 0, -8), (56, 4, 40), (12, 38, 24)),
               P("LEG_FR", (-4, -24, -4), (8, 24, 8), (-12, 24, 28)), P("LEG_FL", (-4, -24, -4), (8, 24, 8), (12, 24, 28)),
               P("LEG_BR", (-8, -32, -8), (16, 32, 16), (-16, 32, -20)), P("LEG_BL", (-8, -32, -8), (16, 32, 16), (16, 32, -20))],
    "arrow": [P("BODY", (-0.5, -0.5, -8), (1, 1, 16), (0, 0, 0)), P("BODY", (-2, -0.5, -8), (4, 1, 3), (0, 0, 0)), P("BODY", (-0.5, -2, -8), (1, 4, 3), (0, 0, 0))],
    "player": biped(),
    # UltimateCraft: a hooded spirit with a trailing wisp instead of legs
    "ghost": [P("HEAD", (-4, 0, -4), (8, 8, 8), (0, 18, 0)), P("BODY", (-4, 0, -2.5), (8, 10, 5), (0, 8, 0)),
              P("TAIL", (-3, -6, -2), (6, 6, 4), (0, 8, 0)), P("TAIL", (-1.5, -5, -1), (3, 5, 2), (0, 2, 0)),
              P("ARM_R", (-3, -9, -1.5), (3, 10, 3), (-5, 17, 0)), P("ARM_L", (0, -9, -1.5), (3, 10, 3), (5, 17, 0))],
    # UltimateCraft Space mobs
    "drifter": tentacled(body=(10, 8, 10), n=6, tlen=10, tsize=2, y=10, ring=3) + [P("STATIC", (-3, 0, -3), (6, 3, 6), (0, 18, 0))],
    "crawler": [P("BODY", (-6, 0, -7), (12, 6, 14), (0, 4, 0)), P("STATIC", (-4, 0, -5), (8, 3, 10), (0, 10, 0)),
                P("STATIC", (-1, 0, -1), (2, 3, 2), (-3, 12, -3)), P("STATIC", (-1, 0, -1), (2, 4, 2), (2, 12, 1)),
                P("HEAD", (-3.5, -2.5, 0), (7, 5, 5), (0, 7, 7)),
                P("LEG_FR", (-1.5, -4, -1.5), (3, 4, 3), (-5, 4, 5)), P("LEG_FL", (-1.5, -4, -1.5), (3, 4, 3), (5, 4, 5)),
                P("LEG_BR", (-1.5, -4, -1.5), (3, 4, 3), (-5, 4, -5)), P("LEG_BL", (-1.5, -4, -1.5), (3, 4, 3), (5, 4, -5))],
    # Zorvath, the Cosmic Tyrant: a crowned, robed colossus with a halo of star shards behind its shoulders
    "tyrant": [P("TAIL", (-7, -18, -4), (14, 18, 8), (0, 20, 0)), P("BODY", (-9, 0, -5), (18, 20, 10), (0, 20, 0)),
               P("HEAD", (-6, 0, -6), (12, 12, 12), (0, 40, 0)), P("HEAD", (-6, 12, -6), (2, 5, 2), (0, 40, 0)), P("HEAD", (4, 12, -6), (2, 5, 2), (0, 40, 0)),
               P("HEAD", (-1, 12, -6), (2, 7, 2), (0, 40, 0)), P("HEAD", (-6, 12, 4), (12, 3, 2), (0, 40, 0)),
               P("ARM_R", (-6, -22, -3), (6, 24, 6), (-9, 38, 0)), P("ARM_L", (0, -22, -3), (6, 24, 6), (9, 38, 0)),
               P("WING_R", (-26, -10, 0), (26, 28, 2), (-4, 40, 6), (0, 15, 0)), P("WING_L", (0, -10, 0), (26, 28, 2), (4, 40, 6), (0, -15, 0))],
    # Underground City
    "delvari": biped(robe=True, hat=False) + [P("HEAD", (-4.5, 6, -4.5), (9, 3, 9), (0, 24, 0))],
    "sentinel": biped(body=(9, 12, 5)) + [P("HEAD", (-1, 8, -5), (2, 4, 10), (0, 24, 0)), P("STATIC", (-1, -2, -1), (2, 30, 2), (7, 4, 2))],
    "keeper": biped(head=10, body=(14, 16, 8), arm=(6, 18, 6), leg=(6, 12, 6)) + [P("HEAD", (-6, 10, -6), (12, 2, 12), (0, 28, 0)),
               P("STATIC", (-2.5, -6, -2.5), (5, 6, 5), (12, 10, 0))],
    # the Sift
    "soul": [P("HEAD", (-3, 0, -3), (6, 6, 6), (0, 16, 0)), P("HEAD", (-4, 8, -4), (8, 1, 8), (0, 16, 0)), P("BODY", (-3, 0, -2), (6, 8, 4), (0, 8, 0)),
             P("TAIL", (-2, -6, -1.5), (4, 6, 3), (0, 8, 0)), P("ARM_R", (-2, -7, -1), (2, 8, 2), (-4, 15, 0)), P("ARM_L", (0, -7, -1), (2, 8, 2), (4, 15, 0))],
    "god": biped(robe=True) + [P("HEAD", (-6, 11, -6), (12, 1, 12), (0, 24, 0)), P("WING_R", (-20, -14, 0), (20, 22, 1), (-3, 22, 3), (0, 20, 0)),
            P("WING_L", (0, -14, 0), (20, 22, 1), (3, 22, 3), (0, -20, 0))],
    "avatar": [P("BODY", (-8, 0, -5), (16, 14, 10), (0, 14, 0)), P("HEAD", (-5, 0, -5), (10, 9, 10), (0, 28, 0)), P("STATIC", (-2, 0, -2), (4, 7, 4), (-6, 28, -1)),
               P("STATIC", (-2, 0, -2), (4, 9, 4), (5, 28, 1)), P("ARM_R", (-6, -18, -3), (6, 20, 6), (-10, 26, 0)), P("ARM_L", (0, -18, -3), (6, 20, 6), (10, 26, 0)),
               P("LEG_R", (-3, -14, -3), (6, 14, 6), (-4, 14, 0)), P("LEG_L", (-3, -14, -3), (6, 14, 6), (4, 14, 0))],
    "revenant": biped(body=(10, 14, 6), arm=(5, 14, 5), leg=(5, 12, 5)) + [P("HEAD", (-5, 7, -1), (2, 6, 2), (0, 26, 0), (0, 0, 30)),
                 P("HEAD", (3, 7, -1), (2, 6, 2), (0, 26, 0), (0, 0, -30)), P("STATIC", (-6, 0, -4), (12, 3, 8), (0, 24, 0))],
    "happy_ghast": tentacled(body=(16, 16, 16), n=9, tlen=10, y=10, ring=5),
    "cube": cube_mob(8),
}

def pack_uv(parts):
    """assign texture (u,v) to every box with a shelf packer; returns (tw, th)."""
    if all("u" in p for p in parts): return 64, 32
    for tw in (64, 128, 256):
        x = y = rowh = 0
        ok = True
        for p in parts:
            w, h, d = [max(1, int(round(s))) for s in p["size"]]
            bw, bh = 2 * d + 2 * w, d + h
            if bw > tw: ok = False; break
            if x + bw > tw: x, y, rowh = 0, y + rowh, 0
            p["u"], p["v"] = x, y
            x += bw; rowh = max(rowh, bh)
        th = y + rowh
        if ok:
            t = 32
            while t < th: t *= 2
            if t <= tw * 2: return tw, t
    raise RuntimeError("model too big")

# --------------------------------------------------------------------------------------------- mob table
def M(key, name, model, size, health, speed, attack=0, ai="PASSIVE", flags=(), xp=(1, 3), drops=(), colors=None, scale=1.0,
      eye=None, proj=None, rng=0, cd=20, effect=None, food=(), sound=("generic", 1.0), egg=(0x888888, 0x444444), armor=0, kb=0.0, tint=None):
    return dict(key=key, name=name, model=model, size=size, health=health, speed=speed, attack=attack, ai=ai, flags=list(flags), xp=xp,
                drops=list(drops), colors=colors or {}, scale=scale, eye=eye, proj=proj, rng=rng, cd=cd, effect=effect, food=list(food),
                sound=sound, egg=egg, armor=armor, kb=kb, tint=tint)

UND = ("UNDEAD",)
MOB_LIST = [
    # ------------------------------------------------------------------ passive
    M("allay", "Allay", "allay", (0.35, 0.6), 20, 3.0, flags=("FLYING", "NO_FALL", "PERSIST"), xp=(0, 0), colors={"*": 0x5AC8F0, "HEAD": 0x8AE0FF}, egg=(0x00DAFF, 0x00ADFF), sound=("bird", 1.6)),
    M("armadillo", "Armadillo", "armadillo", (0.7, 0.65), 12, 1.4, flags=("BREEDS",), drops=[("armadillo_scute", 0, 1, 20)], colors={"*": 0xA86A5A, "HEAD": 0xC08A7A}, food=["spider_eye"], egg=(0xAD716D, 0x824848), sound=("squeak", 1.3)),
    M("axolotl", "Axolotl", "axolotl", (0.75, 0.42), 14, 1.0, 2, ai="FISH", flags=("SWIMS", "BREEDS", "AQUATIC"), colors={"*": 0xF0A0C8, "HEAD": 0xF8B8D8}, food=["tropical_fish_bucket"], egg=(0xFBC1E3, 0xA62D74), sound=("squeak", 1.5)),
    M("bat", "Bat", "bat", (0.5, 0.9), 6, 2.0, ai="FLYER", flags=("FLYING", "NO_FALL"), xp=(0, 0), colors={"*": 0x3A2A1A}, egg=(0x4C3E30, 0x0F0F0F), sound=("squeak", 2.0)),
    M("camel", "Camel", "camel", (1.7, 2.375), 32, 0.9, flags=("BREEDS",), colors={"*": 0xD8A060, "HEAD": 0xC89050}, food=["cactus"], egg=(0xFCC369, 0xCB9337), sound=("grunt", 0.8)),
    M("camel_husk", "Camel Husk", "camel", (1.7, 2.375), 32, 0.9, flags=UND + ("BURNS",), xp=(1, 3), drops=[("rotten_flesh", 0, 2, 100)], colors={"*": 0x9A8A6A, "HEAD": 0x7A6A4A}, egg=(0x9A8A6A, 0x5A4A3A), sound=("grunt", 0.6)),
    M("cat", "Cat", "cat", (0.6, 0.7), 10, 3.0, 3, flags=("BREEDS", "TAMEABLE"), drops=[("string", 0, 2, 100)], colors={"*": 0xC8904A, "HEAD": 0xD8A060}, food=["cod", "salmon"], egg=(0xEFC88E, 0x957256), sound=("meow", 1.0)),
    M("chicken", "Chicken", "chicken", (0.4, 0.7), 4, 2.5, flags=("BREEDS", "NO_FALL", "LAYS_EGG"), drops=[("chicken", 1, 1, 100), ("feather", 0, 2, 100)], colors={"*": 0xF4F4F4, "HEAD": 0xF8F8F8, "LEG_R": 0xF0B020, "LEG_L": 0xF0B020}, food=["wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds"], egg=(0xA1A1A1, 0xFF0000), sound=("chicken", 1.0)),
    M("cod", "Cod", "fish", (0.5, 0.3), 3, 1.5, ai="FISH", flags=("SWIMS", "AQUATIC"), drops=[("cod", 1, 1, 100), ("bone_meal", 0, 1, 5)], colors={"*": 0xB09A7A}, egg=(0xC1A76A, 0xE5C48B), sound=("flop", 1.2)),
    M("copper_golem", "Copper Golem", "copper_golem", (0.49, 0.98), 12, 2.0, flags=("PERSIST",), xp=(0, 0), drops=[("copper_ingot", 1, 3, 100)], colors={"*": 0xC46A4C, "HEAD": 0xD47A5C}, egg=(0xC46A4C, 0x52A386), sound=("metal", 1.5)),
    M("cow", "Cow", "cow", (0.9, 1.4), 10, 2.0, flags=("BREEDS", "MILKABLE"), drops=[("beef", 1, 3, 100), ("leather", 0, 2, 100)], colors={"*": 0x433626, "BODY": 0x433626, "HEAD": 0x5A4A3A, "spots": 0xE8E8E8}, food=["wheat"], egg=(0x443626, 0xA1A1A1), sound=("cow", 1.0)),
    M("donkey", "Donkey", "horse", (1.3965, 1.5), 22, 1.75, flags=("BREEDS", "TAMEABLE"), drops=[("leather", 0, 2, 100)], colors={"*": 0x8A7A6A, "HEAD": 0x7A6A5A}, food=["wheat", "apple", "golden_carrot"], egg=(0x534539, 0x867566), scale=0.87, sound=("horse", 0.8)),
    M("frog", "Frog", "frog", (0.5, 0.5), 10, 1.0, flags=("BREEDS", "SWIMS"), colors={"*": 0xC08A4A, "HEAD": 0xD09A5A}, food=["slime_ball"], egg=(0xD07444, 0xFFC77C), sound=("croak", 1.0)),
    M("glow_squid", "Glow Squid", "squid", (0.8, 0.8), 10, 1.0, ai="SQUID", flags=("SWIMS", "AQUATIC", "GLOWS"), drops=[("glow_ink_sac", 1, 3, 100)], colors={"*": 0x1A6A6A, "TENTACLE": 0x2A8A8A}, egg=(0x095656, 0x85F1BC), sound=("squish", 1.2)),
    M("happy_ghast", "Happy Ghast", "happy_ghast", (4.0, 4.0), 20, 1.5, ai="FLYER", flags=("FLYING", "NO_FALL", "PERSIST"), xp=(0, 0), colors={"*": 0xF4F4F4}, scale=1.0, food=["snowball"], egg=(0xF9F9F9, 0xBCBCBC), sound=("ghast", 1.4)),
    M("horse", "Horse", "horse", (1.3965, 1.6), 22, 2.25, flags=("BREEDS", "TAMEABLE"), drops=[("leather", 0, 2, 100)], colors={"*": 0x8A5A2A, "HEAD": 0x7A4A1A, "STATIC": 0x3A2A1A, "TAIL": 0x3A2A1A}, food=["wheat", "apple", "golden_carrot", "golden_apple"], egg=(0xC09E7D, 0xEEE500), sound=("horse", 1.0)),
    M("mooshroom", "Mooshroom", "cow", (0.9, 1.4), 10, 2.0, flags=("BREEDS", "MILKABLE", "SHEARABLE"), drops=[("beef", 1, 3, 100), ("leather", 0, 2, 100)], colors={"*": 0xA02A20, "HEAD": 0xA83A30, "spots": 0xE8E8E8}, food=["wheat"], egg=(0xA00F10, 0xB7B7B7), sound=("cow", 1.0)),
    M("mule", "Mule", "horse", (1.3965, 1.6), 22, 1.75, flags=("TAMEABLE",), drops=[("leather", 0, 2, 100)], colors={"*": 0x4A3220, "HEAD": 0x3A2210}, food=["wheat", "apple", "golden_carrot"], egg=(0x1B0200, 0x51331D), scale=0.92, sound=("horse", 0.7)),
    M("ocelot", "Ocelot", "cat", (0.6, 0.7), 10, 3.0, 3, flags=("BREEDS", "FLEES_PLAYER"), colors={"*": 0xE8C060, "spots": 0x4A3A20}, food=["cod", "salmon"], egg=(0xEFDE7D, 0x564434), sound=("meow", 1.1)),
    M("parrot", "Parrot", "parrot", (0.5, 0.9), 6, 2.0, ai="FLYER", flags=("FLYING", "NO_FALL", "TAMEABLE"), drops=[("feather", 1, 2, 100)], colors={"*": 0xE02020, "HEAD": 0xE02020, "WING_R": 0x2060E0, "WING_L": 0x2060E0, "TAIL": 0x20A0E0}, food=["wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds"], egg=(0x0DA70B, 0xFF0000), sound=("bird", 1.4)),
    M("pig", "Pig", "pig", (0.9, 0.9), 10, 2.0, flags=("BREEDS",), drops=[("porkchop", 1, 3, 100)], colors={"*": 0xF0A0A0, "HEAD": 0xF4AAAA}, food=["carrot", "potato", "beetroot"], egg=(0xF0A5A2, 0xDB635F), sound=("pig", 1.0)),
    M("rabbit", "Rabbit", "rabbit", (0.4, 0.5), 3, 3.0, flags=("BREEDS", "HOPS", "FLEES_PLAYER"), drops=[("rabbit_hide", 0, 1, 100), ("rabbit", 0, 1, 100), ("rabbit_foot", 1, 1, 10)], colors={"*": 0x9A7A5A, "HEAD": 0xA88A6A}, food=["carrot", "golden_carrot", "dandelion"], egg=(0x995F40, 0x734831), sound=("squeak", 1.6)),
    M("salmon", "Salmon", "salmon", (0.7, 0.4), 3, 1.5, ai="FISH", flags=("SWIMS", "AQUATIC"), drops=[("salmon", 1, 1, 100), ("bone_meal", 0, 1, 5)], colors={"*": 0xA03A30, "FIN": 0x3A5A4A}, egg=(0xA00F10, 0x0E8474), sound=("flop", 1.1)),
    M("sheep", "Sheep", "sheep", (0.9, 1.3), 8, 2.0, flags=("BREEDS", "SHEARABLE"), drops=[("white_wool", 1, 1, 100), ("mutton", 1, 2, 100)], colors={"*": 0xEAEAEA, "HEAD": 0xD8C8B8, "LEG_FR": 0xD8C8B8, "LEG_FL": 0xD8C8B8, "LEG_BR": 0xD8C8B8, "LEG_BL": 0xD8C8B8}, food=["wheat"], egg=(0xE7E7E7, 0xFFB5B5), sound=("sheep", 1.0)),
    M("skeleton_horse", "Skeleton Horse", "horse", (1.3965, 1.6), 15, 2.0, flags=UND + ("BREEDS",), xp=(1, 3), drops=[("bone", 0, 2, 100)], colors={"*": 0xD8D8D0, "pattern": "bones"}, egg=(0x68684F, 0xE5E5D8), sound=("horse", 0.6)),
    M("sniffer", "Sniffer", "sniffer", (1.9, 1.75), 14, 1.0, flags=("BREEDS",), colors={"*": 0x8A2A1A, "HEAD": 0xA03A2A, "spots": 0x2A8A4A}, food=["torchflower_seeds"], egg=(0x871E09, 0x24A47B), sound=("grunt", 0.5)),
    M("snow_golem", "Snow Golem", "snow_golem", (0.7, 1.9), 4, 2.0, ai="GOLEM", proj="SNOWBALL", rng=10, cd=20, flags=("PERSIST",), xp=(0, 0), drops=[("snowball", 0, 15, 100)], colors={"*": 0xF4F8FA, "HEAD": 0xD88A1A, "ARM_R": 0x6A4A2A, "ARM_L": 0x6A4A2A}, egg=(0xD9F2F2, 0x81A4A4), sound=("snow", 1.2)),
    M("squid", "Squid", "squid", (0.8, 0.8), 10, 1.0, ai="SQUID", flags=("SWIMS", "AQUATIC"), drops=[("ink_sac", 1, 3, 100)], colors={"*": 0x2A3A5A, "TENTACLE": 0x3A4A6A}, egg=(0x223B4D, 0x708899), sound=("squish", 1.0)),
    M("strider", "Strider", "strider", (0.9, 1.7), 20, 1.75, flags=("BREEDS", "FIRE_IMMUNE", "WALKS_ON_LAVA"), drops=[("string", 2, 5, 100)], colors={"*": 0x9A3A3A, "HEAD": 0x7A2A2A, "LEG_R": 0x5A4A4A, "LEG_L": 0x5A4A4A}, food=["warped_fungus"], egg=(0x9C3436, 0x4D494D), sound=("grunt", 1.3)),
    M("sulfur_cube", "Sulfur Cube", "slime", (1.04, 1.04), 8, 1.5, ai="SLIME", flags=("HOPS",), xp=(1, 2), drops=[("gunpowder", 0, 1, 50)], colors={"*": 0xE0D040, "STATIC": 0xC0A020}, egg=(0xE0D040, 0x8A7A20), sound=("squish", 1.0)),
    M("tadpole", "Tadpole", "tadpole", (0.4, 0.3), 6, 1.5, ai="FISH", flags=("SWIMS", "AQUATIC"), xp=(0, 0), colors={"*": 0x5A4A3A}, food=["slime_ball"], egg=(0x6D533D, 0x160A00), sound=("flop", 1.5)),
    M("tropical_fish", "Tropical Fish", "tropical", (0.5, 0.4), 3, 1.5, ai="FISH", flags=("SWIMS", "AQUATIC"), drops=[("tropical_fish", 1, 1, 100), ("bone_meal", 0, 1, 5)], colors={"*": 0xF08A20, "FIN": 0xF0F0F0, "TAIL": 0xF0F0F0}, egg=(0xEF6915, 0xFFF9EF), sound=("flop", 1.3)),
    M("turtle", "Turtle", "turtle", (1.2, 0.4), 30, 1.0, flags=("BREEDS", "SWIMS", "IMMUNE_DROWN"), drops=[("seagrass", 0, 2, 100)], colors={"*": 0x5A8A3A, "BODY": 0x3A6A2A}, food=["seagrass"], egg=(0xE7E7E7, 0x00AFAF), sound=("grunt", 1.4)),
    M("villager", "Villager", "villager", (0.6, 1.95), 20, 2.0, ai="VILLAGER", flags=("PERSIST",), xp=(0, 0), colors={"*": 0x7A5A3A, "HEAD": 0xBD8B72, "ARM_R": 0xBD8B72, "STATIC": 0x5A3A2A}, egg=(0x563C33, 0xBD8B72), sound=("villager", 1.0)),
    M("wandering_trader", "Wandering Trader", "villager", (0.6, 1.95), 20, 2.0, ai="VILLAGER", xp=(0, 0), drops=[("milk_bucket", 0, 0, 0)], colors={"*": 0x2A4A8A, "HEAD": 0xBD8B72, "STATIC": 0x3A5A9A}, egg=(0x456296, 0xEAA430), sound=("villager", 1.1)),
    M("zombie_horse", "Zombie Horse", "horse", (1.3965, 1.6), 25, 2.0, flags=UND, xp=(1, 3), drops=[("rotten_flesh", 0, 2, 100)], colors={"*": 0x3A6A3A, "HEAD": 0x4A7A4A}, egg=(0x315234, 0x97C284), sound=("horse", 0.6)),
    # ------------------------------------------------------------------ neutral
    M("bee", "Bee", "bee", (0.7, 0.6), 10, 3.0, 2, ai="NEUTRAL", flags=("FLYING", "NO_FALL", "BREEDS", "GROUP_ANGER"), effect=("poison", 0, 10), colors={"*": 0xF0C020, "pattern": "stripes", "WING_R": 0xD0E8F0, "WING_L": 0xD0E8F0}, food=["dandelion", "poppy", "cornflower", "sunflower"], egg=(0xEDC343, 0x43241B), sound=("buzz", 1.0)),
    M("cave_spider", "Cave Spider", "spider", (0.7, 0.5), 12, 3.0, 2, ai="SPIDER", flags=("ARTHROPOD", "CLIMBS"), xp=(5, 5), effect=("poison", 0, 7), drops=[("string", 0, 2, 100), ("spider_eye", 0, 1, 33)], colors={"*": 0x0A3A4A, "HEAD": 0x1A2A3A, "eyes": 0xC00000}, scale=0.7, egg=(0x0C424E, 0xA80E0E), sound=("spider", 1.2)),
    M("dolphin", "Dolphin", "dolphin", (0.9, 0.6), 10, 3.5, 3, ai="FISH", flags=("SWIMS", "AQUATIC", "GROUP_ANGER"), drops=[("cod", 0, 1, 100)], colors={"*": 0x7A8AA0, "HEAD": 0x8A9AB0}, egg=(0x223B4D, 0xF9F9F9), sound=("squeak", 1.2)),
    M("drowned", "Drowned", "humanoid", (0.6, 1.95), 20, 2.3, 3, ai="HOSTILE", flags=UND + ("SWIMS", "BURNS", "IMMUNE_DROWN"), xp=(5, 5), drops=[("rotten_flesh", 0, 2, 100), ("copper_ingot", 0, 1, 11), ("trident", 0, 1, 1)], colors={"*": 0x4A8A8A, "BODY": 0x5A9A8A, "LEG_R": 0x3A6A8A, "LEG_L": 0x3A6A8A, "eyes": 0x80F0F0}, egg=(0x8FF1D7, 0x799C65), sound=("zombie", 0.8)),
    M("enderman", "Voidwalker", "enderman", (0.6, 2.9), 40, 3.0, 7, ai="ENDERMAN", flags=(), xp=(5, 5), drops=[("ender_pearl", 0, 1, 100)], colors={"*": 0x1A1438, "HEAD": 0x231A4A, "eyes": 0x7AE8FF, "pattern": "stars"}, egg=(0x1A1438, 0x7AE8FF), sound=("ender", 1.0)),
    M("fox", "Fox", "fox", (0.6, 0.7), 10, 3.0, 2, flags=("BREEDS", "FLEES_PLAYER"), colors={"*": 0xE07A30, "HEAD": 0xE88A40, "TAIL": 0xF0F0F0}, food=["sweet_berries", "glow_berries"], egg=(0xD5B69F, 0xCC6920), sound=("squeak", 1.0)),
    M("goat", "Goat", "goat", (0.9, 1.3), 10, 2.0, 2, ai="NEUTRAL", flags=("BREEDS", "MILKABLE"), colors={"*": 0xE8E4DA, "HEAD": 0xEAE6DC}, food=["wheat"], egg=(0xA5947C, 0x55493E), sound=("goat", 1.0)),
    M("iron_golem", "Iron Golem", "golem", (1.4, 2.7), 100, 2.5, 15, ai="GOLEM", flags=("PERSIST", "NO_FALL"), xp=(0, 0), drops=[("iron_ingot", 3, 5, 100), ("poppy", 0, 2, 100)], colors={"*": 0xD8D0C8, "pattern": "vines"}, kb=1.0, egg=(0xDBCDC1, 0x74A332), sound=("metal", 0.6)),
    M("llama", "Llama", "llama", (0.9, 1.87), 22, 1.75, 1, ai="NEUTRAL", proj="LLAMA_SPIT", rng=8, cd=40, flags=("BREEDS", "TAMEABLE", "GROUP_ANGER"), drops=[("leather", 0, 2, 100)], colors={"*": 0xE8D8B8, "HEAD": 0xD8C8A8}, food=["wheat", "hay_block"], egg=(0xC09E7D, 0x995F40), sound=("grunt", 1.2)),
    M("nautilus", "Nautilus", "nautilus", (0.9, 0.8), 15, 2.0, 2, ai="FISH", flags=("SWIMS", "AQUATIC", "TAMEABLE"), drops=[("nautilus_shell", 0, 1, 10)], colors={"*": 0xE8D8C8, "pattern": "stripes", "HEAD": 0xB08A6A, "TENTACLE": 0xB08A6A}, food=["pufferfish"], egg=(0xE8D8C8, 0xB04A2A), sound=("squish", 0.9)),
    M("panda", "Panda", "panda", (1.3, 1.25), 20, 1.5, 6, ai="NEUTRAL", flags=("BREEDS",), drops=[("bamboo", 0, 1, 100)], colors={"*": 0xF0F0F0, "LEG_FR": 0x1A1A1A, "LEG_FL": 0x1A1A1A, "LEG_BR": 0x1A1A1A, "LEG_BL": 0x1A1A1A, "eyes": 0x1A1A1A}, food=["bamboo"], egg=(0xE7E7E7, 0x1B1B22), sound=("grunt", 1.1)),
    M("piglin", "Piglin", "piglin", (0.6, 1.95), 16, 3.5, 5, ai="NEUTRAL", flags=("GROUP_ANGER",), xp=(5, 5), drops=[("golden_sword", 0, 1, 8), ("gold_nugget", 0, 2, 50)], colors={"*": 0xE8A090, "BODY": 0x6A4A2A, "LEG_R": 0x4A3A2A, "LEG_L": 0x4A3A2A}, egg=(0x995F40, 0xF9F3A4), sound=("pig", 0.7)),
    M("polar_bear", "Polar Bear", "bear", (1.4, 1.4), 30, 2.5, 6, ai="NEUTRAL", flags=(), drops=[("cod", 0, 2, 75), ("salmon", 0, 2, 25)], colors={"*": 0xF4F4F0}, egg=(0xF2F2F2, 0x959590), sound=("grunt", 0.6)),
    M("pufferfish", "Pufferfish", "puffer", (0.7, 0.7), 3, 1.5, 2, ai="FISH", flags=("SWIMS", "AQUATIC"), effect=("poison", 0, 6), drops=[("pufferfish", 1, 1, 100), ("bone_meal", 0, 1, 5)], colors={"*": 0xE8C040, "FIN": 0x3A8A8A}, egg=(0xF6B201, 0x37C3F2), sound=("flop", 1.0)),
    M("spider", "Spider", "spider", (1.4, 0.9), 16, 3.0, 2, ai="SPIDER", flags=("ARTHROPOD", "CLIMBS"), xp=(5, 5), drops=[("string", 0, 2, 100), ("spider_eye", 0, 1, 33)], colors={"*": 0x2A2420, "HEAD": 0x342C26, "eyes": 0xC00000}, egg=(0x342D27, 0xA80E0E), sound=("spider", 1.0)),
    M("trader_llama", "Trader Llama", "llama", (0.9, 1.87), 22, 1.75, 1, ai="NEUTRAL", proj="LLAMA_SPIT", rng=8, cd=40, flags=("TAMEABLE", "GROUP_ANGER"), drops=[("leather", 0, 2, 100)], colors={"*": 0xE8D8B8, "BODY": 0x2A4A9A}, egg=(0xEAA430, 0x456296), sound=("grunt", 1.2)),
    M("wolf", "Wolf", "wolf", (0.6, 0.85), 8, 3.0, 4, ai="NEUTRAL", flags=("BREEDS", "TAMEABLE", "GROUP_ANGER"), colors={"*": 0xD8D4CC, "HEAD": 0xE0DCD4}, food=["beef", "porkchop", "chicken", "mutton", "rabbit", "rotten_flesh", "cooked_beef"], egg=(0xD7D3D3, 0xCEAF96), sound=("wolf", 1.0)),
    M("zombie_nautilus", "Zombie Nautilus", "nautilus", (0.9, 0.8), 15, 2.0, 3, ai="HOSTILE", flags=UND + ("SWIMS", "AQUATIC", "IMMUNE_DROWN"), xp=(5, 5), drops=[("rotten_flesh", 0, 2, 100), ("nautilus_shell", 0, 1, 5)], colors={"*": 0x7A9A8A, "pattern": "stripes", "HEAD": 0x4A7A5A, "TENTACLE": 0x4A7A5A}, egg=(0x7A9A8A, 0x3A5A4A), sound=("zombie", 1.2)),
    M("zombified_piglin", "Zombified Piglin", "piglin", (0.6, 1.95), 20, 2.3, 8, ai="NEUTRAL", flags=UND + ("FIRE_IMMUNE", "GROUP_ANGER"), xp=(5, 5), drops=[("rotten_flesh", 0, 1, 100), ("gold_nugget", 0, 1, 100), ("gold_ingot", 0, 1, 3)], colors={"*": 0xE8A090, "BODY": 0x6A8A4A, "HEAD": 0x8AB070, "pattern": "rot"}, egg=(0xEA9393, 0x4C7129), sound=("pig", 0.6)),
    # ------------------------------------------------------------------ hostile
    M("blaze", "Blaze", "blaze", (0.6, 1.8), 20, 2.3, 6, ai="BLAZE", proj="SMALL_FIREBALL", rng=16, cd=100, flags=("FIRE_IMMUNE", "FLYING", "NO_FALL"), xp=(10, 10), drops=[("blaze_rod", 0, 1, 100)], colors={"*": 0xF8C020, "HEAD": 0xF8D040, "eyes": 0x3A1A0A}, egg=(0xF6B201, 0xFFF87E), sound=("blaze", 1.0)),
    M("bogged", "Bogged", "skeleton", (0.6, 1.99), 16, 2.5, 2, ai="RANGED", proj="ARROW", rng=15, cd=50, flags=UND + ("BURNS",), xp=(5, 5), effect=("poison", 0, 4), drops=[("bone", 0, 2, 100), ("arrow", 0, 2, 100)], colors={"*": 0xC8C8A0, "pattern": "moss"}, egg=(0x8A9A72, 0x314D1B), sound=("skeleton", 0.9)),
    M("breeze", "Breeze", "breeze", (0.6, 1.77), 30, 3.0, 1, ai="BREEZE", proj="WIND_CHARGE", rng=16, cd=40, flags=("NO_FALL",), xp=(10, 10), drops=[("breeze_rod", 1, 2, 100)], colors={"*": 0xA0B8F0, "HEAD": 0xC0D0F8}, egg=(0xAF94DF, 0x9166DF), sound=("wind", 1.0)),
    M("creaking", "Creaking", "creaking", (0.9, 2.7), 1, 3.0, 3, ai="CREAKING", flags=("NO_KNOCKBACK",), xp=(0, 0), colors={"*": 0x5F5550, "eyes": 0xF08A2A}, egg=(0x5F5F5F, 0xFC7812), sound=("wood", 0.7)),
    M("creeper", "Creeper", "creeper", (0.6, 1.7), 20, 2.5, 0, ai="CREEPER", flags=(), xp=(5, 5), drops=[("gunpowder", 0, 2, 100)], colors={"*": 0x5AB04A, "eyes": 0x101010, "pattern": "camo"}, egg=(0x0DA70B, 0x000000), sound=("creeper", 1.0)),
    M("elder_guardian", "Elder Guardian", "guardian", (1.9975, 1.9975), 80, 1.5, 8, ai="GUARDIAN", flags=("SWIMS", "AQUATIC", "PERSIST", "IMMUNE_DROWN"), xp=(10, 10), effect=("mining_fatigue", 2, 300), drops=[("prismarine_shard", 0, 2, 100), ("wet_sponge", 1, 1, 100), ("cod", 0, 1, 50)], colors={"*": 0xC8C0B0, "STATIC": 0xE8E0D0, "HEAD": 0xD08040}, scale=2.35, egg=(0xCECCBA, 0x747693), sound=("guardian", 0.7)),
    M("endermite", "Void Mite", "endermite", (0.4, 0.3), 8, 2.5, 2, ai="SILVERFISH", flags=("ARTHROPOD",), xp=(3, 3), colors={"*": 0x2E2460, "eyes": 0x7AE8FF}, egg=(0x2E2460, 0x7AE8FF), sound=("squeak", 1.8)),
    M("evoker", "Evoker", "illager", (0.6, 1.95), 24, 2.5, 6, ai="EVOKER", flags=("ILLAGER",), xp=(10, 10), drops=[("totem_of_undying", 1, 1, 100), ("emerald", 0, 1, 100)], colors={"*": 0x2A2A2A, "HEAD": 0x9A9A8A, "ARM_R": 0x2A2A2A}, egg=(0x959B9B, 0x1E1C1A), sound=("villager", 0.8)),
    M("ghast", "Ghast", "ghast", (4.0, 4.0), 10, 1.5, 6, ai="GHAST", proj="FIREBALL", rng=64, cd=60, flags=("FIRE_IMMUNE", "FLYING", "NO_FALL"), xp=(5, 5), drops=[("ghast_tear", 0, 1, 100), ("gunpowder", 0, 2, 100)], colors={"*": 0xF4F4F4, "eyes": 0x2A2A2A}, egg=(0xF9F9F9, 0xBCBCBC), sound=("ghast", 1.0)),
    M("guardian", "Guardian", "guardian", (0.85, 0.85), 30, 1.5, 6, ai="GUARDIAN", flags=("SWIMS", "AQUATIC", "IMMUNE_DROWN"), xp=(10, 10), drops=[("prismarine_shard", 0, 2, 100), ("cod", 0, 1, 40), ("prismarine_crystals", 0, 1, 40)], colors={"*": 0x5A8A7A, "STATIC": 0xE8D8C0, "HEAD": 0xF0A040}, egg=(0x5A8272, 0xF17D30), sound=("guardian", 1.0)),
    M("hoglin", "Hoglin", "hoglin", (1.3965, 1.4), 40, 3.0, 6, ai="HOSTILE", flags=("BREEDS",), xp=(5, 5), drops=[("porkchop", 2, 4, 100), ("leather", 0, 1, 100)], colors={"*": 0xC07A5A, "STATIC": 0xE8D8B0}, food=["crimson_fungus"], egg=(0xC66E55, 0x5F6464), sound=("pig", 0.5)),
    M("husk", "Husk", "humanoid", (0.6, 1.95), 20, 2.3, 3, ai="HOSTILE", flags=UND, xp=(5, 5), effect=("hunger", 0, 7), drops=[("rotten_flesh", 0, 2, 100), ("iron_ingot", 0, 1, 2)], colors={"*": 0x8A7A5A, "BODY": 0x6A5A3A, "LEG_R": 0x5A4A30, "LEG_L": 0x5A4A30}, egg=(0x797061, 0xE6CC94), sound=("zombie", 0.85)),
    M("magma_cube", "Magma Cube", "magma", (2.04, 2.04), 16, 2.0, 6, ai="SLIME", flags=("FIRE_IMMUNE", "HOPS", "NO_FALL"), xp=(4, 4), drops=[("magma_cream", 0, 1, 25)], colors={"*": 0x3A1A0A, "pattern": "magma", "eyes": 0xF8A020}, egg=(0x340000, 0xFCFC00), sound=("squish", 0.7)),
    M("parched", "Parched", "skeleton", (0.6, 1.99), 16, 2.5, 2, ai="RANGED", proj="ARROW", rng=15, cd=50, flags=UND, xp=(5, 5), effect=("weakness", 0, 8), drops=[("bone", 0, 2, 100), ("arrow", 0, 2, 100)], colors={"*": 0xD8C098}, egg=(0xD8C098, 0x8A6A4A), sound=("skeleton", 1.1)),
    M("phantom", "Phantom", "phantom", (0.9, 0.5), 20, 4.0, 2, ai="PHANTOM", flags=UND + ("FLYING", "NO_FALL", "BURNS"), xp=(5, 5), drops=[("phantom_membrane", 0, 1, 100)], colors={"*": 0x3A4A7A, "eyes": 0x80F080}, egg=(0x43518A, 0x88FF00), sound=("phantom", 1.0)),
    M("piglin_brute", "Piglin Brute", "piglin", (0.6, 1.95), 50, 3.5, 13, ai="HOSTILE", flags=("PERSIST",), xp=(20, 20), drops=[("golden_axe", 0, 1, 8)], colors={"*": 0xE8A090, "BODY": 0x2A2A2A, "LEG_R": 0x2A2A2A, "LEG_L": 0x2A2A2A}, egg=(0x592A10, 0xF9F3A4), sound=("pig", 0.55)),
    M("pillager", "Pillager", "illager", (0.6, 1.95), 24, 3.5, 4, ai="RANGED", proj="ARROW", rng=8, cd=60, flags=("ILLAGER",), xp=(5, 5), drops=[("arrow", 0, 2, 100), ("crossbow", 0, 1, 8)], colors={"*": 0x4A5A6A, "HEAD": 0x9A9A8A, "LEG_R": 0x3A3A4A, "LEG_L": 0x3A3A4A}, egg=(0x532F36, 0x959B9B), sound=("villager", 0.75)),
    M("ravager", "Ravager", "ravager", (1.95, 2.2), 100, 3.0, 12, ai="HOSTILE", flags=("NO_KNOCKBACK",), xp=(20, 20), drops=[("saddle", 1, 1, 100)], colors={"*": 0x5A5A5A, "HEAD": 0x4A4A4A}, kb=0.75, egg=(0x757470, 0x5B5049), sound=("grunt", 0.4)),
    M("shulker", "Starshell", "shulker", (1.0, 1.0), 30, 0.0, 4, ai="SHULKER", proj="SHULKER_BULLET", rng=16, cd=40, flags=("NO_KNOCKBACK", "PERSIST", "NO_FALL"), xp=(5, 5), armor=20, drops=[("shulker_shell", 0, 1, 50)], colors={"*": 0x5A5AA8, "HEAD": 0xA8F0F0}, egg=(0x5A5AA8, 0xA8F0F0), sound=("shulker", 1.0)),
    M("silverfish", "Silverfish", "silverfish", (0.4, 0.3), 8, 2.5, 1, ai="SILVERFISH", flags=("ARTHROPOD",), xp=(5, 5), colors={"*": 0x8A8A8A}, egg=(0x6E6E6E, 0x303030), sound=("squeak", 1.7)),
    M("skeleton", "Skeleton", "skeleton", (0.6, 1.99), 20, 2.5, 3, ai="RANGED", proj="ARROW", rng=15, cd=50, flags=UND + ("BURNS",), xp=(5, 5), drops=[("bone", 0, 2, 100), ("arrow", 0, 2, 100)], colors={"*": 0xC8C8C0, "pattern": "bones"}, egg=(0xC1C1C1, 0x494949), sound=("skeleton", 1.0)),
    M("slime", "Slime", "slime", (2.04, 2.04), 16, 2.0, 4, ai="SLIME", flags=("HOPS",), xp=(4, 4), drops=[("slime_ball", 0, 2, 100)], colors={"*": 0x6AB05A, "STATIC": 0x4A9A3A, "eyes": 0x1A3A1A}, egg=(0x51A03E, 0x7EBF6E), sound=("squish", 1.0)),
    M("stray", "Stray", "skeleton", (0.6, 1.99), 20, 2.5, 3, ai="RANGED", proj="ARROW", rng=15, cd=50, flags=UND + ("BURNS",), xp=(5, 5), effect=("slowness", 0, 30), drops=[("bone", 0, 2, 100), ("arrow", 0, 2, 100)], colors={"*": 0xB0C0C8, "BODY": 0x8A9AA8, "pattern": "bones"}, egg=(0x617677, 0xDDEAEA), sound=("skeleton", 0.9)),
    M("vex", "Vex", "vex", (0.4, 0.8), 14, 4.0, 9, ai="FLYER", flags=("FLYING", "NO_FALL"), xp=(0, 0), colors={"*": 0xC0C8D8, "WING_R": 0xE8F0F8, "WING_L": 0xE8F0F8}, egg=(0x7A90A4, 0xE8EDF1), sound=("vex", 1.0)),
    M("vindicator", "Vindicator", "illager", (0.6, 1.95), 24, 3.5, 13, ai="HOSTILE", flags=("ILLAGER",), xp=(5, 5), drops=[("emerald", 0, 1, 100), ("iron_axe", 0, 1, 8)], colors={"*": 0x2A3A4A, "HEAD": 0x9A9A8A, "LEG_R": 0x1A1A2A, "LEG_L": 0x1A1A2A}, egg=(0x959B9B, 0x275E61), sound=("villager", 0.7)),
    M("warden", "Warden", "warden", (0.9, 2.9), 500, 3.0, 30, ai="WARDEN", flags=("NO_KNOCKBACK", "FIRE_IMMUNE", "PERSIST", "IMMUNE_DROWN"), xp=(5, 5), drops=[("sculk_catalyst", 1, 1, 100)], colors={"*": 0x0A2A33, "HEAD": 0x123A44, "pattern": "sculk"}, kb=1.0, egg=(0x0F4649, 0x39D6E0), sound=("warden", 0.5)),
    M("witch", "Witch", "witch", (0.6, 1.95), 26, 2.5, 6, ai="WITCH", proj="POTION", rng=10, cd=60, flags=(), xp=(5, 5), drops=[("glass_bottle", 0, 2, 50), ("glowstone_dust", 0, 2, 50), ("redstone", 0, 2, 50), ("spider_eye", 0, 2, 50), ("sugar", 0, 2, 50), ("stick", 0, 2, 50)], colors={"*": 0x3A2A4A, "HEAD": 0x9AA080, "STATIC": 0x4A3A5A}, egg=(0x340000, 0x51A03E), sound=("villager", 1.2)),
    M("wither_skeleton", "Wither Skeleton", "skeleton", (0.7, 2.4), 20, 2.5, 8, ai="HOSTILE", flags=UND + ("FIRE_IMMUNE",), xp=(5, 5), effect=("wither", 0, 10), drops=[("coal", 0, 1, 33), ("bone", 0, 2, 100), ("wither_skeleton_skull", 0, 1, 3), ("stone_sword", 0, 1, 8)], colors={"*": 0x2A2A2A, "pattern": "bones"}, scale=1.2, egg=(0x141414, 0x474D4D), sound=("skeleton", 0.7)),
    M("zoglin", "Zoglin", "hoglin", (1.3965, 1.4), 40, 3.0, 6, ai="HOSTILE", flags=UND, xp=(5, 5), drops=[("rotten_flesh", 1, 3, 100)], colors={"*": 0xC08A8A, "STATIC": 0xE8D8D0, "pattern": "rot"}, egg=(0xC66E55, 0xE6E6E6), sound=("pig", 0.45)),
    M("zombie", "Zombie", "zombie_classic", (0.6, 1.95), 20, 2.3, 3, ai="HOSTILE", flags=UND + ("BURNS",), xp=(5, 5), drops=[("rotten_flesh", 0, 2, 100), ("iron_ingot", 0, 1, 2), ("carrot", 0, 1, 2), ("potato", 0, 1, 2)], colors={"*": 0x4A8A3A, "BODY": 0x2A8AA0, "LEG_R": 0x3A3A9A, "LEG_L": 0x3A3A9A}, egg=(0x00AFAF, 0x799C65), sound=("zombie", 1.0)),
    M("zombie_villager", "Zombie Villager", "villager", (0.6, 1.95), 20, 2.3, 3, ai="HOSTILE", flags=UND + ("BURNS",), xp=(5, 5), drops=[("rotten_flesh", 0, 2, 100)], colors={"*": 0x5A4A2A, "HEAD": 0x5A8A4A, "STATIC": 0x4A3A2A}, egg=(0x563C33, 0x799C65), sound=("zombie", 0.95)),
    # ------------------------------------------------------------------ bosses
    M("ender_dragon", "Void Wyrm", "dragon", (16.0, 8.0), 200, 8.0, 10, ai="BOSS_DRAGON", proj="DRAGON_FIREBALL", rng=64, cd=200, flags=("FLYING", "NO_FALL", "BOSS", "FIRE_IMMUNE", "NO_KNOCKBACK", "PERSIST", "IMMUNE_DROWN"), xp=(500, 500), colors={"*": 0x1C1A3A, "WING_R": 0x34306A, "WING_L": 0x34306A, "eyes": 0x60F0FF}, scale=0.5, egg=(0x1C1A3A, 0x60F0FF), sound=("dragon", 1.0)),
    M("wither", "Wither", "wither", (0.9, 3.5), 300, 3.5, 8, ai="BOSS_WITHER", proj="WITHER_SKULL", rng=24, cd=40, flags=UND + ("FLYING", "NO_FALL", "BOSS", "FIRE_IMMUNE", "NO_KNOCKBACK", "PERSIST", "IMMUNE_DROWN"), xp=(50, 50), effect=("wither", 1, 10), drops=[("nether_star", 1, 1, 100)], colors={"*": 0x1A1A1A, "eyes": 0x6AA0E0}, scale=1.0, egg=(0x141414, 0x4D72A0), sound=("wither", 1.0)),
    # ------------------------------------------------------------------ UltimateCraft mobs
    M("ghost", "Ghost", "ghost", (0.6, 1.9), 18, 2.6, 3, ai="GHOST", flags=UND + ("FLYING", "NO_FALL", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(6, 6),
      effect=("haunted", 0, 8), drops=[("ectoplasm", 1, 2, 100), ("soul_sand", 0, 1, 25)], colors={"*": 0xD8ECF0, "HEAD": 0xEAF6FA, "TAIL": 0xB8D8E4, "eyes": 0x1A3A5A},
      egg=(0xDDEEF2, 0x3A6A8A), sound=("vex", 0.6)),
    # Space
    M("void_drifter", "Void Drifter", "drifter", (0.9, 1.3), 14, 2.2, 3, ai="DRIFTER", flags=("FLYING", "NO_FALL", "IMMUNE_DROWN"), xp=(5, 5),
      effect=("levitation", 0, 2), drops=[("stardust", 1, 2, 100)], colors={"*": 0x2C2068, "TENTACLE": 0x7A6AE0, "STATIC": 0x9AE8FF, "eyes": 0xC0F8FF},
      egg=(0x2C2068, 0x9AE8FF), sound=("squish", 1.4)),
    M("asteroid_crawler", "Asteroid Crawler", "crawler", (1.1, 0.9), 22, 2.6, 4, ai="HOSTILE", flags=("ARTHROPOD",), xp=(6, 6), armor=6,
      drops=[("stardust", 0, 2, 100), ("end_stone", 0, 1, 40)], colors={"*": 0x6A6458, "STATIC": 0x9A8AC8, "HEAD": 0x5A5448, "eyes": 0xF0A040},
      egg=(0x6A6458, 0x9A8AC8), sound=("spider", 0.6)),
    # the ultimate boss of Space (replaces the old dragon fight); health and damage scale with difficulty in C
    M("cosmic_tyrant", "Zorvath, the Cosmic Tyrant", "tyrant", (2.4, 5.0), 600, 6.0, 12, ai="BOSS_TYRANT", proj="STAR_BOLT", rng=64, cd=60,
      flags=("FLYING", "NO_FALL", "BOSS", "FIRE_IMMUNE", "NO_KNOCKBACK", "PERSIST", "IMMUNE_DROWN"), xp=(0, 0),
      drops=[("tyrant_crown", 1, 1, 100), ("sift_key", 1, 1, 100), ("nether_star", 2, 4, 100), ("stardust", 8, 16, 100)],
      colors={"*": 0x1E1240, "HEAD": 0x2A1A5A, "TAIL": 0x140C30, "WING_R": 0x6A4AE0, "WING_L": 0x6A4AE0, "eyes": 0xFFD040, "pattern": "stars"},
      scale=1.5, egg=(0x1E1240, 0xFFD040), sound=("wither", 0.55)),
    # Underground City: the Delvari, lantern-keepers of the deep
    M("delvari_merchant", "Delvari Merchant", "delvari", (0.6, 1.95), 20, 2.0, ai="VILLAGER", flags=("PERSIST",), xp=(0, 0),
      colors={"*": 0x34405E, "HEAD": 0x9AA6BA, "STATIC": 0x2A3350, "eyes": 0xF0B040}, egg=(0x34405E, 0xF0B040), sound=("villager", 0.8)),
    M("hollow_sentinel", "Hollow Sentinel", "sentinel", (0.7, 2.0), 30, 2.4, 7, ai="HOSTILE", flags=("PERSIST",), xp=(8, 8), armor=10,
      drops=[("delvari_coin", 0, 2, 100), ("iron_nugget", 1, 3, 60)], colors={"*": 0x4A5268, "HEAD": 0x2A2E3A, "STATIC": 0x8A8A98, "eyes": 0x60E0FF},
      egg=(0x4A5268, 0x60E0FF), sound=("metal", 0.8)),
    M("deep_keeper", "Morrowgate, Keeper of the Deep", "keeper", (1.4, 3.6), 320, 2.2, 12, ai="BOSS_KEEPER", proj="SMALL_FIREBALL", rng=20, cd=60,
      flags=("BOSS", "PERSIST", "NO_KNOCKBACK", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(300, 300), armor=10,
      drops=[("void_sigil", 1, 1, 100), ("delvari_coin", 10, 20, 100), ("umbral_blade", 1, 1, 60), ("umbral_lamp", 2, 4, 100)],
      colors={"*": 0x3A4050, "HEAD": 0x2E3442, "STATIC": 0xF0B060, "eyes": 0xF0B040, "pattern": "camo"}, scale=1.6, egg=(0x3A4050, 0xF0B060), sound=("warden", 0.7)),
    # the Sift: souls of the departed and the Luminaries, an original pantheon (fiction, no real-world faith)
    M("sift_soul", "Wandering Soul", "soul", (0.5, 1.4), 10, 1.4, ai="SOUL", flags=("FLYING", "NO_FALL", "IMMUNE_DROWN", "FIRE_IMMUNE"), xp=(0, 0),
      colors={"*": 0xE8F4FF, "HEAD": 0xF6FBFF, "TAIL": 0xC8E0F8, "eyes": 0x7AB0E8}, egg=(0xE8F4FF, 0xF6E7A8), sound=("bird", 0.7)),
    M("god_solenne", "Solenne, the Dawnkeeper", "god", (0.8, 2.6), 1000, 0.0, ai="GOD", flags=("FLYING", "NO_FALL", "PERSIST", "NO_KNOCKBACK", "IMMUNE_DROWN", "FIRE_IMMUNE"), xp=(0, 0),
      colors={"*": 0xF8E8B0, "HEAD": 0xFFF4D8, "WING_R": 0xFFE68A, "WING_L": 0xFFE68A, "STATIC": 0xF4D060, "eyes": 0xFFB830}, scale=1.4, egg=(0xF8E8B0, 0xFFB830), sound=("bird", 0.5)),
    M("god_zephyrine", "Zephyrine, Lady of the High Winds", "god", (0.8, 2.6), 1000, 0.0, ai="GOD", flags=("FLYING", "NO_FALL", "PERSIST", "NO_KNOCKBACK", "IMMUNE_DROWN", "FIRE_IMMUNE"), xp=(0, 0),
      colors={"*": 0xC8E4FF, "HEAD": 0xEAF4FF, "WING_R": 0xFFFFFF, "WING_L": 0xFFFFFF, "STATIC": 0x8AC0F0, "eyes": 0x3A80D0}, scale=1.4, egg=(0xC8E4FF, 0x3A80D0), sound=("bird", 0.6)),
    M("god_thalor", "Thalor the Stoneheart", "god", (0.9, 2.8), 1000, 0.0, ai="GOD", flags=("FLYING", "NO_FALL", "PERSIST", "NO_KNOCKBACK", "IMMUNE_DROWN", "FIRE_IMMUNE"), xp=(0, 0),
      colors={"*": 0x9AA8B0, "HEAD": 0xC8D0D4, "WING_R": 0xA8DCEE, "WING_L": 0xA8DCEE, "STATIC": 0x6A7880, "eyes": 0x60E0C0}, scale=1.5, egg=(0x9AA8B0, 0x60E0C0), sound=("metal", 0.5)),
    M("thalor_avatar", "Avatar of Thalor", "avatar", (1.6, 3.2), 260, 2.4, 11, ai="BOSS_AVATAR", flags=("BOSS", "PERSIST", "NO_KNOCKBACK", "IMMUNE_DROWN", "FIRE_IMMUNE"), xp=(200, 200), armor=8,
      drops=[("divine_ingot", 2, 3, 100), ("soul_essence", 4, 8, 100), ("radiant_crystal", 2, 6, 100)],
      colors={"*": 0x8A98A2, "HEAD": 0x9AA8B0, "STATIC": 0xA8E8FF, "eyes": 0x60E0C0, "pattern": "camo"}, scale=1.3, egg=(0x8A98A2, 0xA8E8FF), sound=("metal", 0.4)),
    # Hell: a ghost for every biome, and the fortress lord
    M("ember_wraith", "Ember Wraith", "ghost", (0.6, 1.9), 20, 2.8, 4, ai="GHOST", flags=UND + ("FLYING", "NO_FALL", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(7, 7),
      drops=[("ectoplasm", 1, 2, 100), ("magma_cream", 0, 1, 50)], colors={"*": 0xF0A070, "HEAD": 0xF8C890, "TAIL": 0xE07040, "eyes": 0xFFF0A0},
      egg=(0xF0A070, 0xFFF0A0), sound=("vex", 0.5)),
    M("fungal_shade", "Fungal Shade", "ghost", (0.6, 1.9), 18, 2.4, 3, ai="GHOST", flags=UND + ("FLYING", "NO_FALL", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(7, 7),
      effect=("poison", 0, 6), drops=[("ectoplasm", 1, 2, 100), ("shroomlight", 0, 1, 30)], colors={"*": 0xB890C8, "HEAD": 0x8AD0C0, "TAIL": 0xA06AB0, "eyes": 0x2A1A3A, "pattern": "moss"},
      egg=(0xB890C8, 0x8AD0C0), sound=("vex", 0.55)),
    M("cinder_specter", "Cinder Specter", "ghost", (0.6, 1.9), 18, 2.6, 3, ai="GHOST", flags=UND + ("FLYING", "NO_FALL", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(7, 7),
      effect=("weakness", 0, 8), drops=[("ectoplasm", 1, 2, 100), ("coal", 0, 2, 60)], colors={"*": 0xA8A0A0, "HEAD": 0xC8C0C0, "TAIL": 0x6A6262, "eyes": 0xF04020},
      egg=(0xA8A0A0, 0xF04020), sound=("vex", 0.45)),
    M("brimstone_revenant", "Brimstone Revenant", "revenant", (0.9, 2.6), 220, 2.6, 10, ai="BOSS_REVENANT", proj="SMALL_FIREBALL", rng=20, cd=60,
      flags=UND + ("BOSS", "PERSIST", "NO_KNOCKBACK", "FIRE_IMMUNE", "IMMUNE_DROWN"), xp=(150, 150), armor=6,
      drops=[("brimstone_heart", 1, 1, 100), ("blaze_rod", 2, 5, 100), ("wither_skeleton_skull", 1, 1, 30)],
      colors={"*": 0x3A1A14, "HEAD": 0x2A100C, "STATIC": 0xF86A20, "eyes": 0xFFB020, "pattern": "magma"}, scale=1.4, egg=(0x3A1A14, 0xF86A20), sound=("blaze", 0.6)),
]
assert [m["key"] for m in MOB_LIST] == MOBS, set(MOBS) ^ set(m["key"] for m in MOB_LIST)
EGG_COLORS = {m["key"]: m["egg"] for m in MOB_LIST}
MOB_BY_KEY = {m["key"]: m for m in MOB_LIST}

# --------------------------------------------------------------------------------------------- spawning
import biomes as BI
BKEYS = [b["key"] for b in BI.BIOMES]
def biomes_where(pred): return [b["key"] for b in BI.BIOMES if pred(b)]
OVER_LAND = biomes_where(lambda b: b["dim"] == "OVERWORLD" and b["cat"] in ("land", "beach", "river"))
OCEANS = biomes_where(lambda b: b["cat"] == "ocean")
SPAWNS = []   # (biome, mob, cat, weight, min, max)
def S(biomes, mob, cat, w, mn, mx):
    for b in biomes: SPAWNS.append((b, mob, cat, w, mn, mx))
def monsters_default(bs, zombie="zombie", skel="skeleton", extra=()):
    S(bs, "spider", "MONSTER", 100, 1, 2); S(bs, zombie, "MONSTER", 95, 1, 3); S(bs, "zombie_villager", "MONSTER", 5, 1, 1)
    S(bs, skel, "MONSTER", 100, 1, 2); S(bs, "creeper", "MONSTER", 100, 1, 1); S(bs, "enderman", "MONSTER", 10, 1, 2); S(bs, "witch", "MONSTER", 5, 1, 1)
    S(bs, "slime", "MONSTER", 20, 1, 2)
cold = {"snowy_plains", "ice_spikes", "frozen_river", "snowy_beach", "snowy_taiga", "grove", "snowy_slopes", "frozen_peaks", "jagged_peaks"}
dry = {"desert"}
normal = [b for b in OVER_LAND if b not in cold and b not in dry and b != "mushroom_fields"]
monsters_default(normal)
monsters_default(sorted(cold), skel="stray")
monsters_default(["desert"], zombie="husk", skel="parched")
monsters_default(OCEANS + ["dripstone_caves", "lush_caves"])
S(["swamp", "mangrove_swamp"], "bogged", "MONSTER", 50, 1, 2); S(["swamp", "mangrove_swamp"], "slime", "MONSTER", 80, 1, 2)
S(["swamp"], "witch", "MONSTER", 20, 1, 1)
S(OCEANS + ["river", "frozen_river"], "drowned", "MONSTER", 100, 1, 1)
S(["dripstone_caves"], "drowned", "MONSTER", 95, 4, 4)
# pillagers come from pillager outposts (structures.c)
# vindicators / evokers live in woodland mansions
# guardians spawn in ocean monuments
S(["windswept_hills", "windswept_gravelly_hills", "stony_peaks", "jagged_peaks"], "silverfish", "MONSTER", 6, 1, 2)
S(["dripstone_caves", "lush_caves", "deep_dark"], "breeze", "MONSTER", 2, 1, 1)
S(["deep_dark"], "warden", "MONSTER", 1, 1, 1)
S(["pale_garden"], "creaking", "MONSTER", 40, 1, 1)
S(["desert"], "camel_husk", "MONSTER", 4, 1, 1)
S(["dripstone_caves", "lush_caves"], "sulfur_cube", "MONSTER", 6, 1, 2)
S(OCEANS, "zombie_nautilus", "MONSTER", 3, 1, 1)
S(["plains", "sunflower_plains"], "skeleton_horse", "MONSTER", 1, 1, 1)
S(["plains", "sunflower_plains"], "zombie_horse", "MONSTER", 1, 1, 1)
farm = [b for b in OVER_LAND if b not in ("desert", "badlands", "eroded_badlands", "wooded_badlands", "mushroom_fields", "beach", "snowy_beach", "stony_shore", "river", "frozen_river",
                                           "jagged_peaks", "frozen_peaks", "stony_peaks", "snowy_slopes", "grove", "ice_spikes", "pale_garden", "mangrove_swamp")]
for mob, w in (("sheep", 12), ("pig", 10), ("chicken", 10), ("cow", 8)): S(farm, mob, "CREATURE", w, 2, 4)
S(["plains", "sunflower_plains", "savanna", "meadow"], "horse", "CREATURE", 5, 2, 6); S(["plains", "sunflower_plains", "meadow", "savanna"], "donkey", "CREATURE", 1, 1, 3)
S(["forest", "taiga", "old_growth_pine_taiga", "old_growth_spruce_taiga", "snowy_taiga", "grove"], "wolf", "CREATURE", 8, 4, 4)
S(["taiga", "old_growth_pine_taiga", "old_growth_spruce_taiga", "snowy_taiga", "grove"], "fox", "CREATURE", 8, 2, 4)
S(["taiga", "snowy_plains", "snowy_taiga", "desert", "flower_forest", "meadow", "cherry_grove", "grove", "ice_spikes"], "rabbit", "CREATURE", 4, 2, 3)
S(["snowy_plains", "ice_spikes", "frozen_ocean", "deep_frozen_ocean"], "polar_bear", "CREATURE", 1, 1, 2)
S(["savanna_plateau", "windswept_hills", "windswept_savanna"], "llama", "CREATURE", 8, 4, 4)
S(["jungle", "sparse_jungle", "bamboo_jungle"], "parrot", "CREATURE", 40, 1, 2); S(["jungle", "bamboo_jungle"], "ocelot", "CREATURE", 2, 1, 3)
S(["jungle", "bamboo_jungle"], "panda", "CREATURE", 40, 1, 2)
S(["snowy_slopes", "jagged_peaks", "frozen_peaks"], "goat", "CREATURE", 5, 1, 3)
S(["swamp", "mangrove_swamp"], "frog", "CREATURE", 10, 2, 5)
S(["beach"], "turtle", "CREATURE", 5, 2, 5)
S(["desert"], "camel", "CREATURE", 1, 1, 1)
S(["mushroom_fields"], "mooshroom", "CREATURE", 8, 4, 8)
S(["savanna", "savanna_plateau", "badlands", "wooded_badlands"], "armadillo", "CREATURE", 10, 1, 2)
S(["flower_forest", "plains", "sunflower_plains", "meadow", "cherry_grove"], "bee", "CREATURE", 4, 2, 3)
# villagers and iron golems live in villages
S(["plains", "taiga", "savanna", "desert", "snowy_plains"], "cat", "CREATURE", 1, 1, 1)
S(["plains", "forest", "taiga", "meadow"], "wandering_trader", "CREATURE", 1, 1, 1)
S(["dark_forest", "taiga", "pale_garden"], "allay", "CREATURE", 1, 1, 2)
S(["plains", "forest", "savanna", "meadow"], "sniffer", "CREATURE", 1, 1, 1)
S(["snowy_plains", "snowy_taiga", "grove", "ice_spikes"], "snow_golem", "CREATURE", 1, 1, 1)
S(["plains", "meadow", "sunflower_plains"], "copper_golem", "CREATURE", 1, 1, 1)
S(["desert", "plains"], "happy_ghast", "CREATURE", 1, 1, 1)
for o in OCEANS:
    warm = "warm" in o or "lukewarm" in o
    coldo = "cold" in o or "frozen" in o
    S([o], "squid", "WATER", 5 if not coldo else 3, 1, 4)
    if not coldo: S([o], "dolphin", "WATER", 2, 1, 2)
    S([o], "nautilus", "WATER", 1 if warm else 0 or 1, 1, 1)
    S([o], "cod", "WATER_AMBIENT", 15 if not warm or "lukewarm" in o else 0 or 1, 3, 6)
    if warm: S([o], "tropical_fish", "WATER_AMBIENT", 25, 8, 8); S([o], "pufferfish", "WATER_AMBIENT", 15, 1, 3)
    if coldo: S([o], "salmon", "WATER_AMBIENT", 15, 1, 5)
S(["river", "frozen_river"], "salmon", "WATER_AMBIENT", 5, 1, 5); S(["river"], "squid", "WATER", 2, 1, 4)
S(["swamp", "mangrove_swamp"], "tadpole", "WATER_AMBIENT", 2, 2, 4)
S(["lush_caves"], "axolotl", "AXOLOTL", 10, 4, 6)
S([b for b in BKEYS if BI.BIOMES[BKEYS.index(b)]["dim"] == "OVERWORLD"], "glow_squid", "UNDERGROUND_WATER", 10, 2, 4)
S([b for b in BKEYS if BI.BIOMES[BKEYS.index(b)]["dim"] == "OVERWORLD"], "bat", "AMBIENT", 10, 2, 4)
# nether
S(["nether_wastes"], "zombified_piglin", "MONSTER", 100, 2, 4); S(["nether_wastes"], "ghast", "MONSTER", 50, 1, 1); S(["nether_wastes"], "magma_cube", "MONSTER", 2, 1, 4)
S(["nether_wastes"], "enderman", "MONSTER", 1, 2, 4); S(["nether_wastes"], "piglin", "MONSTER", 15, 2, 4)
# blazes and wither skeletons spawn in nether fortresses
S(["soul_sand_valley"], "skeleton", "MONSTER", 20, 5, 5); S(["soul_sand_valley"], "ghast", "MONSTER", 50, 4, 4); S(["soul_sand_valley"], "enderman", "MONSTER", 1, 4, 4)
S(["crimson_forest"], "zombified_piglin", "MONSTER", 1, 2, 4); S(["crimson_forest"], "hoglin", "MONSTER", 9, 3, 4); S(["crimson_forest"], "piglin", "MONSTER", 5, 3, 4)
S(["crimson_forest", "nether_wastes"], "zoglin", "MONSTER", 1, 1, 1)
S(["warped_forest"], "enderman", "MONSTER", 1, 4, 4); S(["basalt_deltas"], "magma_cube", "MONSTER", 100, 2, 5); S(["basalt_deltas"], "ghast", "MONSTER", 40, 1, 1)
S(["nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest", "basalt_deltas"], "strider", "LAVA", 60, 1, 2)
S(["basalt_deltas"], "ember_wraith", "MONSTER", 25, 1, 2); S(["crimson_forest", "warped_forest"], "fungal_shade", "MONSTER", 20, 1, 2); S(["nether_wastes"], "cinder_specter", "MONSTER", 15, 1, 2)
S(["soul_sand_valley"], "ghost", "MONSTER", 40, 1, 2); S(["nether_wastes", "basalt_deltas"], "ghost", "MONSTER", 4, 1, 1)   # also haunts fortresses (structures.c)
# end
S(["the_end", "end_highlands", "end_midlands", "small_end_islands", "end_barrens"], "enderman", "MONSTER", 10, 4, 4)
# shulkers guard end cities
S(["end_highlands", "end_midlands", "end_barrens"], "endermite", "MONSTER", 1, 1, 2)
S(["end_highlands", "end_midlands", "small_end_islands", "end_barrens"], "void_drifter", "MONSTER", 6, 1, 2)
S(["end_highlands", "end_midlands", "end_barrens"], "asteroid_crawler", "MONSTER", 5, 1, 3)

# --------------------------------------------------------------------------------------------- skins
FUR = {"wolf", "fox", "cat", "ocelot", "rabbit", "horse", "donkey", "mule", "llama", "trader_llama", "camel", "camel_husk", "goat", "polar_bear", "panda",
       "hoglin", "zoglin", "bat", "cow", "mooshroom", "ravager", "sniffer", "strider", "zombie_horse", "pig", "piglin", "piglin_brute", "zombified_piglin"}
WOOL = {"sheep"}
FEATHER = {"chicken", "parrot", "phantom", "allay", "vex"}
SCALE = {"cod", "salmon", "tropical_fish", "pufferfish", "guardian", "elder_guardian", "turtle", "armadillo", "nautilus", "zombie_nautilus", "axolotl", "frog", "tadpole", "dolphin"}
COUNTERSHADE = FUR | WOOL | {"dolphin", "axolotl", "frog", "chicken", "cod", "salmon", "tropical_fish", "squid"}
HUMANOID_KEYS = {"zombie", "husk", "drowned", "player", "skeleton", "stray", "bogged", "parched", "wither_skeleton", "villager", "zombie_villager", "pillager", "vindicator",
                 "evoker", "witch", "wandering_trader", "piglin", "piglin_brute", "zombified_piglin", "illusioner"}

def _field(rnd, w, h, cell=3):
    """Low-frequency value noise over a w*h face, returns f(x, y) in [0, 1]."""
    gw, gh = w // cell + 2, h // cell + 2
    g = [[rnd.random() for _ in range(gw)] for _ in range(gh)]
    def f(x, y):
        fx, fy = x / cell, y / cell
        x0, y0 = int(fx), int(fy)
        tx, ty = fx - x0, fy - y0
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        a = g[y0][x0] * (1 - tx) + g[y0][x0 + 1] * tx
        b = g[y0 + 1][x0] * (1 - tx) + g[y0 + 1][x0 + 1] * tx
        return a * (1 - ty) + b * ty
    return f

def paint_skin(mob, parts, tw, th):
    """Paints each model box with per-face lighting, edge shading, a material texture (fur / wool / feather / scale / skin),
    belly counter-shading, hooves and a proper face.  Output follows the box UV layout declared by the model."""
    img = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    key = mob["key"]
    rnd = random.Random(paint.seed_of(key))
    cols = mob["colors"]
    base = cols.get("*", 0x888888)
    pattern = cols.get("pattern")
    eyes = cols.get("eyes", 0x101010)
    rgb, shade = paint.rgb, paint.shade
    humanoid = key in HUMANOID_KEYS
    first_head = next((i for i, p in enumerate(parts) if p["role"] == "HEAD"), -1)
    head_d = max(1, int(round(parts[first_head]["size"][2]))) if first_head >= 0 else 8
    spot_col = rgb(cols.get("spots", 0xE8E8E8))

    def col_for(role): return rgb(cols.get(role, base))

    for pi, p in enumerate(parts):
        w, h, d = [max(1, int(round(s))) for s in p["size"]]
        u, v = p["u"], p["v"]
        role = p["role"]
        c = col_for(role)
        is_leg = role in ("LEG_R", "LEG_L", "LEG_FR", "LEG_FL", "LEG_BR", "LEG_BL")
        is_muzzle = role == "HEAD" and pi != first_head and p["box"][2] >= head_d - 1.5 and p["box"][1] < 4
        rects = {"right": (u, v + d, d, h), "front": (u + d, v + d, w, h), "left": (u + d + w, v + d, d, h), "back": (u + 2 * d + w, v + d, w, h),
                 "top": (u + d, v, w, d), "bottom": (u + d + w, v, w, d)}
        for face, (x0, y0, fw, fh) in rects.items():
            light = {"top": 1.14, "bottom": 0.7, "front": 1.0, "back": 0.9, "left": 0.95, "right": 0.95}[face]
            fld = _field(rnd, fw, fh, 3)
            colstreak = [rnd.uniform(-0.07, 0.07) for _ in range(fw)]
            for yy in range(fh):
                for xx in range(fw):
                    f = light * (0.9 + fld(xx, yy) * 0.2)
                    edge = (xx == 0 or yy == 0 or xx == fw - 1 or yy == fh - 1) and fw > 2 and fh > 2
                    if edge: f *= 0.92
                    if key in FUR and not humanoid:
                        f *= 1 + colstreak[xx] + rnd.uniform(-0.04, 0.04)
                        if rnd.random() < 0.05: f *= 1.15
                    elif key in WOOL:
                        f *= 1 + (0.09 if ((xx // 2 + yy // 2) % 2) else -0.05) + rnd.uniform(-0.04, 0.04)
                    elif key in FEATHER:
                        f *= (0.84 if (yy % 3 == 2 and (xx + (yy // 3) * 2) % 4 < 3) else 1.04) + rnd.uniform(-0.03, 0.03)
                    elif key in SCALE:
                        f *= (0.86 if ((xx + yy) % 4 == 0 or (xx - yy) % 4 == 0) else 1.02) + rnd.uniform(-0.02, 0.02)
                    else:
                        f *= rnd.uniform(0.94, 1.06)
                    px = shade(c, f)
                    # belly counter-shading: lighter underside on the lower third of side faces
                    if key in COUNTERSHADE and role in ("BODY", "HEAD", "TAIL") and face in ("front", "back", "left", "right") and yy >= fh * 0.7 and not humanoid:
                        px = paint.mix(px, rgb(0xF2EAD8), 0.22)
                    # hooves / paws
                    if is_leg and not humanoid and face not in ("top", "bottom") and yy >= fh - 2 and fh >= 5:
                        px = shade(px, 0.55)
                    if pattern == "spots" or "spots" in cols:
                        # irregular blotches (not a regular grid): thresholded smooth noise
                        if role == "BODY" and fld(xx, yy) > 0.66 and face not in ("bottom",): px = shade(spot_col, light * rnd.uniform(0.95, 1.04))
                    if pattern == "stripes" and role == "BODY" and (yy // 2) % 3 == 0 and face not in ("bottom",): px = rgb(0x2A1A0A)
                    if pattern == "bones" and role == "BODY" and face in ("front", "back") and yy % 3 == 1 and 1 <= xx < fw - 1: px = shade(c, 0.55)
                    if pattern == "camo" and rnd.random() < 0.3: px = shade(c, rnd.choice((0.7, 1.25)))
                    if pattern == "rot" and rnd.random() < 0.18: px = rgb(0x5A8A3A)
                    if pattern == "moss" and rnd.random() < 0.25: px = rgb(0x4A7A2A)
                    if pattern == "magma" and (xx + yy * 3 + pi) % 7 == 0: px = rgb(0xF8A020)
                    if pattern == "sculk" and rnd.random() < 0.08: px = rgb(0x39D6E0)
                    if pattern == "vines" and rnd.random() < 0.06: px = rgb(0x4A8A2A)
                    if pattern == "stars" and rnd.random() < 0.035: px = rgb(rnd.choice((0xFFFFFF, 0xC8E8FF, 0xFFE8A0)))   # starry void skin
                    if role == "STATIC" and pattern == "magma": px = rgb(0xF8A020)
                    if key in ("zombie", "husk", "zombie_villager", "drowned") and role == "BODY" and rnd.random() < 0.09: px = shade(px, 0.6)   # torn, stained clothes
                    if is_muzzle and face != "back": px = paint.mix(px, rgb(0xE8C8B0), 0.35)
                    img.putpixel((x0 + xx, y0 + yy), px)

        # ---- details on the front face of boxes
        fx, fy = u + d, v + d
        if is_muzzle and w >= 2 and h >= 2:   # nostrils
            n = rgb(0x2A1810)
            img.putpixel((fx + max(0, w // 2 - 1), fy + h // 2), n)
            if w >= 3: img.putpixel((fx + min(w - 1, w // 2 + (w % 2 == 0)), fy + h // 2), n)
        if role == "HEAD" and pi == first_head:
            ey = max(1, h // 2 - 1) if h >= 4 else 0
            e = rgb(eyes)
            dark = key in ("creeper", "enderman", "spider", "cave_spider", "blaze", "ghast", "happy_ghast", "skeleton", "stray", "wither_skeleton", "bogged", "parched")
            if w >= 6:
                left, right = w // 2 - 3, w // 2 + 1
                brow = shade(c, 0.62)
                for ex in (left, right):
                    if humanoid and key not in ("skeleton", "stray", "wither_skeleton", "bogged", "parched"):
                        for bx in range(2): img.putpixel((fx + ex + bx, fy + max(0, ey - 1)), brow)          # brow
                        img.putpixel((fx + ex, fy + ey), rgb(0xF0F0F0) if eyes == 0x101010 or eyes not in (0x80F0F0,) else e)
                        img.putpixel((fx + ex + 1, fy + ey), e)                                              # white + pupil
                    elif dark:
                        for bx in range(2): img.putpixel((fx + ex + bx, fy + ey), e); img.putpixel((fx + ex + bx, fy + ey + 1), e)
                    else:
                        img.putpixel((fx + ex, fy + ey), rgb(0xF4F4F4)); img.putpixel((fx + ex + 1, fy + ey), e)
                        img.putpixel((fx + ex + 1, fy + min(h - 1, ey + 1)), shade(e, 1.0))
                if humanoid and h >= 7 and key not in ("skeleton", "stray", "wither_skeleton", "bogged", "parched"):
                    m = shade(c, 0.5)
                    for mx in range(w // 2 - 2, w // 2 + 2): img.putpixel((fx + mx, fy + min(h - 1, ey + 4)), m)   # mouth
                    if key in ("zombie", "husk", "drowned", "zombie_villager", "zombified_piglin"):
                        img.putpixel((fx + w // 2 - 1, fy + min(h - 1, ey + 4)), rgb(0xE8E0C8)); img.putpixel((fx + w // 2 + 1, fy + min(h - 1, ey + 4)), rgb(0xE8E0C8))
                if key in ("skeleton", "stray", "wither_skeleton", "bogged", "parched"):
                    for mx in range(w // 2 - 2, w // 2 + 2):
                        img.putpixel((fx + mx, fy + min(h - 1, ey + 4)), rgb(0x303030) if mx % 2 == 0 else rgb(0xC8C8C0))   # teeth
                if key == "creeper":
                    for xx in range(w // 2 - 1, w // 2 + 1):
                        for yy in range(ey + 2, min(h, ey + 5)): img.putpixel((fx + xx, fy + yy), e)
                    for yy in (ey + 3, ey + 4):
                        img.putpixel((fx + w // 2 - 2, fy + yy), e); img.putpixel((fx + w // 2 + 1, fy + yy), e)
                if key in ("ghast", "happy_ghast"):
                    for xx in range(w // 2 - 2, w // 2 + 2): img.putpixel((fx + xx, fy + ey + 4), e)
                if key in ("spider", "cave_spider"):
                    for (ex, eyy) in ((1, 1), (w - 2, 1), (2, 3), (w - 3, 3), (3, 2), (w - 4, 2)): img.putpixel((fx + ex, fy + eyy), e)
            elif w >= 2 and h >= 2:
                for ex in (0, w - 1):
                    img.putpixel((fx + ex, fy + min(ey, h - 1)), e)
                    if h >= 4 and not dark: img.putpixel((fx + ex, fy + min(ey, h - 1) - 1), shade(c, 0.7))
    return img

# --------------------------------------------------------------------------------------------- emit
def model_list():
    names = list(MODELS.keys())
    return names

def emit(canon, item_id):
    return ""   # header part produced by emit_header(); C produced in write_assets

def header():
    s = "typedef enum {\n" + "\n".join(f"    MOB_{m.upper()}," for m in MOBS) + "\n    MOB_COUNT\n} MobType;\n"
    s += "typedef enum {\n" + "\n".join(f"    AIK_{a}," for a in AI) + "\n} MobAI;\n"
    s += "enum {\n" + "\n".join(f"    MF_{k} = {v}," for k, v in F.items()) + "\n};\n"
    s += "typedef enum {\n" + "\n".join(f"    SPAWN_{c}," for c in SPAWN_CATS) + "\n    SPAWN_CAT_COUNT\n} SpawnCategory;\n"
    s += "typedef enum {\n" + "\n".join(f"    MODEL_{m.upper()}," for m in model_list()) + "\n    MODEL_COUNT\n} ModelId;\n"
    s += "typedef enum { " + ", ".join(f"PR_{r}" for r in ROLES) + " } PartRole;\n"
    s += f"#define SPAWN_COUNT {len(SPAWNS)}\n"
    return s

SOUND_KINDS = ["generic", "zombie", "skeleton", "creeper", "spider", "cow", "pig", "sheep", "chicken", "horse", "wolf", "meow", "bird", "squeak",
               "grunt", "squish", "flop", "croak", "goat", "villager", "ender", "blaze", "ghast", "guardian", "metal", "snow", "buzz", "wind",
               "wood", "phantom", "shulker", "vex", "warden", "dragon", "wither"]

def emit_c(canon, item_id):
    import items as I
    out = ["#include \"../entity/entity.h\"\n#include \"../world/world.h\"\n"]
    rows = []
    for m in MOB_LIST:
        flags = 0
        for f in m["flags"]: flags |= F[f]
        if m["food"]: flags |= F["BREEDS"] if "BREEDS" in m["flags"] else 0
        drops = [(item_id[k], a, b, c) for k, a, b, c in m["drops"]][:6]
        drops += [(0, 0, 0, 0)] * (6 - len(drops))
        food = [item_id[f] for f in m["food"]][:7] + [0] * (7 - len(m["food"][:7]))
        eff = m["effect"]
        w, h = m["size"]
        eye = m["eye"] if m["eye"] is not None else h * 0.85
        rows.append('    {"%s", "%s", MODEL_%s, "res/textures/entity/%s.png", %.4ff, %.4ff, %.3ff, %.1ff, %.2ff, %.1ff, %d, %.2ff, AIK_%s, %d, %d, %.1ff, %d, %d, %d, {%s}, %d, %.2ff, {%s}, %d, %d, %d, %.3ff},' % (
            m["name"], m["key"], m["model"].upper(), m["key"], w, h, eye, m["health"], m["speed"], m["attack"], m["armor"], m["kb"], m["ai"],
            I.PROJ.index(m["proj"]) if m["proj"] else -1 + 0 if False else (I.PROJ.index(m["proj"]) if m["proj"] else 255), flags & 0xFFFFFFFF, m["rng"], m["cd"], m["xp"][0], m["xp"][1],
            ", ".join("{%d, %d, %d, %d}" % d for d in drops), SOUND_KINDS.index(m["sound"][0]), m["sound"][1],
            ", ".join(map(str, food)), I.EFFECTS.index(eff[0]) if eff else 0, eff[1] if eff else 0, eff[2] if eff else 0, m["scale"]))
    out.append("const MobDef MOB_DEFS[MOB_COUNT] = {\n" + "\n".join(rows) + "\n};\n")
    out.append("const SpawnEntry SPAWNS[SPAWN_COUNT] = {\n" + "\n".join("    {BIOME_%s, MOB_%s, SPAWN_%s, %d, %d, %d}," % (b.upper(), mob.upper(), c, w, a, x) for b, mob, c, w, a, x in SPAWNS) + "\n};\n")
    return "".join(out)

def write_assets(tex, root):
    os.makedirs(os.path.join(tex, "entity"), exist_ok=True)
    # models -> src/entity/model_defs.h
    sizes = {}
    lines = ["// AUTO-GENERATED by tools/content/mobs.py - do not edit by hand.", "#ifndef MODEL_DEFS_H", "#define MODEL_DEFS_H", "#include \"model.h\"", ""]
    for name, parts in MODELS.items():
        tw, th = pack_uv(parts)
        sizes[name] = (tw, th)
        lines.append(f"static const ModelPartDef MODEL_PARTS_{name.upper()}[] = {{")
        for p in parts:
            lines.append("    {PR_%s, %d, %d, {%.4ff, %.4ff, %.4ff}, {%.4ff, %.4ff, %.4ff}, {%.4ff, %.4ff, %.4ff}, {%.4ff, %.4ff, %.4ff}, %d}," % (
                p["role"], p["u"], p["v"], *p["box"], *p["size"], *p["pivot"], *p["rot"], p["idx"]))
        lines.append("};")
    lines.append("static const ModelDef MODEL_DEFS[MODEL_COUNT] = {")
    for name in MODELS:
        tw, th = sizes[name]
        lines.append(f'    {{"{name}", {tw}, {th}, MODEL_PARTS_{name.upper()}, (int)ARRAY_LEN(MODEL_PARTS_{name.upper()})}},')
    lines.append("};\n#endif")
    open(os.path.join(root, "src", "entity", "model_defs.h"), "w").write("\n".join(lines) + "\n")
    for m in MOB_LIST:
        parts = MODELS[m["model"]]
        tw, th = sizes[m["model"]]
        paint_skin(m, parts, tw, th).save(os.path.join(tex, "entity", m["key"] + ".png"))
    # player + arrow skins
    paint_skin(dict(key="player", colors={"*": 0x3A7AC0, "HEAD": 0xC89A7A, "ARM_R": 0xC89A7A, "ARM_L": 0xC89A7A, "LEG_R": 0x3A3A8A, "LEG_L": 0x3A3A8A, "eyes": 0x3A2A7A}),
               MODELS["player"], *sizes["player"]).save(os.path.join(tex, "entity", "player.png"))
    paint_skin(dict(key="arrow", colors={"*": 0x8A6A3A}), MODELS["arrow"], *sizes["arrow"]).save(os.path.join(tex, "entity", "arrow.png"))
