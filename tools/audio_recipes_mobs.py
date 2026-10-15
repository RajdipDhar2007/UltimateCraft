"""Creature voices: idle / hurt / death for each of the 35 voice kinds, designed from real recordings.

Farm and wild animals use field recordings (Wikimedia Commons, public domain / CC0 / CC BY). Monsters are built from the
CC0 "80 creature SFX" voice recordings, transposed, layered and run through reverb. Skeletons, golems, shulkers and the
creaking are real impacts and creaks (Kenney). See tools/audio_src/CREDITS.md.
"""
import numpy as np
from audio_dsp import *   # noqa: F401,F403
from audio_dsp import sound, load, seg, dur, gain, norm, fade, pitch, lowpass, highpass, bandpass, mix, reverb, trim_silence, loudest, denoise, soft_clip, stretch_to, call, events, SR

def cm(n): return load("commons/" + n)
def oc(n): return load(f"oga_creature80/{n}.ogg")
def sn(n): return load(f"oga_snarls/{n}.ogg")
def ki(n, i): return load(f"kenney_impact/{n}_{i:03d}.ogg")
def fin(y, tail=0.1, thr=0.05, maxs=1.8):
    """Trim lead-in room noise, cap the length and fade out."""
    y = trim_silence(y, thr, 0.02)
    if dur(y) > maxs: y = y[: int(maxs * SR)]; tail = max(tail, 0.15)
    return fade(y, 0.004, min(tail, dur(y) * 0.6))
def cut(y, maxs, tail=0.12):
    return fade(y[: int(maxs * SR)], 0.004, tail) if dur(y) > maxs else fade(y, 0.004, min(tail, dur(y) * 0.5))
def mob(kind, which, takes=3):
    return sound(f"mob_{kind}_{which}", takes)
SEMI = (0, -1.2, 1.3, -2.3, 2.4)     # per-take transposition so takes of one sound are never identical

# ============================================================================== zombie: low human-sounding groans
@mob("zombie", "idle")
def _(k): return fin(reverb(pitch(oc(("grunt_03", "grunt_02", "grunt_01")[k]), (-3, -2, -4)[k]), 0.35, 0.15), 0.2)
@mob("zombie", "hurt")
def _(k): return fin(pitch(oc(("hurt_05", "hurt_04", "hurt_05")[k]), (-3, -5, -5)[k]), 0.12)
@mob("zombie", "death")
def _(k): return fin(reverb(pitch(mix(oc(("troll_02", "monster_06", "troll_01")[k]), (gain(oc("grunt_03"), -5), 0.05)), (-4, -3, -5)[k]), 0.5, 0.2), 0.35)

# ============================================================================== skeleton: rattling real bones/impacts
def rattle(k, n, span, base=("impactGeneric_light", "impactTin_medium", "impactWood_light", "impactPlate_light"), seed=0):
    rng = np.random.RandomState(100 * n + k + seed)
    layers = []
    for i in range(n):
        name = base[rng.randint(len(base))]
        y = pitch(ki(name, rng.randint(5)), rng.uniform(-3, 5))
        y = highpass(y, 300)
        layers.append((gain(y[: int(0.18 * SR)], -rng.uniform(0, 7)), rng.uniform(0, span)))
    return mix(*layers)
@mob("skeleton", "idle")
def _(k): return fin(rattle(k, 7, 0.55), 0.15)
@mob("skeleton", "hurt")
def _(k): return fin(rattle(k, 4, 0.2, seed=7), 0.1)
@mob("skeleton", "death")
def _(k): return fin(mix(rattle(k, 16, 0.9, seed=3), (gain(ki("impactWood_heavy", k), -2), 0.0)), 0.3)

# ============================================================================== creeper: hisses
@mob("creeper", "hurt")
def _(k): return fin(highpass(pitch(oc(("spit_01", "spit_02", "spit_03")[k]), 1 + k), 400), 0.1)
@mob("creeper", "death")
def _(k): return fin(reverb(lowpass(pitch(mix(oc("spit_02"), (oc("breath"), 0.1)), -2 - k), 6000), 0.4, 0.15), 0.3)

# ============================================================================== spider: chittering and hissing (insect recordings)
@mob("spider", "idle")
def _(k): return fin(highpass(pitch(oc(("bug_02", "bug_01", "bug_04")[k]), (0, 1, -1)[k]), 500), 0.1)
@mob("spider", "hurt")
def _(k): return fin(highpass(pitch(oc(("bug_01", "bug_04", "bug_02")[k]), (2, 1, 3)[k]), 500), 0.08)
@mob("spider", "death")
def _(k): return fin(reverb(pitch(mix(oc("bug_04"), (oc("bug_02"), 0.2)), -3 - k), 0.3, 0.15), 0.3)

# ============================================================================== farm animals (real recordings)
@mob("cow", "idle")
def _(k):
    src = (cm("Cow_in_Vezo.ogg"), cm("Cow_in_Antefasy.wav"), cm("Cow_in_Vezo.ogg"))[k]
    return fin(pitch(lowpass(src, 9000), (0, 0, -1.5)[k]), 0.25)
@mob("cow", "hurt")
def _(k):
    src = (cm("Cow_in_Vezo.ogg"), cm("Cow_in_Antefasy.wav"), cm("Cow_in_Vezo.ogg"))[k]
    y = pitch(src, (2.5, 2, 3.5)[k]); return fin(cut(y, 0.7), 0.15)
@mob("cow", "death")
def _(k):
    src = (cm("Cow_in_Antefasy.wav"), cm("Cow_in_Vezo.ogg"), cm("Cow_in_Antefasy.wav"))[k]
    return fin(reverb(pitch(src, (-3, -3.5, -4.5)[k]), 0.3, 0.12), 0.4)

@mob("pig", "idle")
def _(k): return fin(pitch(cm("Pig_grunt_-_Erdie.ogg"), (0, -1.5, 1.5)[k]), 0.08)
@mob("pig", "hurt")
def _(k): return fin(mix(pitch(cm("Pig_grunt_-_Erdie.ogg"), (5, 6, 4)[k]), gain(pitch(oc(("cute_09", "cute_02", "cute_08")[k]), 0), -4)), 0.15)
@mob("pig", "death")
def _(k): return fin(reverb(mix(pitch(cm("Pig_grunt_-_Erdie.ogg"), (2, 1, 3)[k]), pitch(oc(("cute_09", "cute_08", "cute_02")[k]), (-3, -2, -4)[k])), 0.3, 0.1), 0.35)

@mob("sheep", "idle")
def _(k): return fin(call(cm("Sheep_bleating.ogg"), k), 0.15)
@mob("sheep", "hurt")
def _(k): return fin(cut(pitch(call(cm("Sheep_bleating.ogg"), k + 2, min_dur=0.2), (2, 3, 2.5)[k]), 0.55), 0.1)
@mob("sheep", "death")
def _(k): return fin(reverb(pitch(call(cm("Sheep_bleating.ogg"), k), (-3, -4, -2)[k]), 0.3, 0.1), 0.35)

@mob("chicken", "idle")
def _(k): return fin(cut(call(cm("Hens_leaving_coop.ogg"), k, min_dur=0.15, max_dur=1.0), 0.7), 0.1)
@mob("chicken", "hurt")
def _(k): return fin(cut(pitch(call(cm("Hen_announcing_shes_lain_an_egg.ogg"), k), (3, 4, 2)[k]), 0.5), 0.1)
@mob("chicken", "death")
def _(k): return fin(cut(pitch(call(cm("Small_rooster_crowing.ogg"), k), (1, 2, -1)[k]), 0.9), 0.3)

@mob("horse", "idle")
def _(k): return fin(pitch(cm("Wiehern.ogg"), (0, -1, 1)[k]), 0.2)
@mob("horse", "hurt")
def _(k): return fin(cut(pitch(cm("Wiehern.ogg"), (3, 4, 2)[k]), 0.9), 0.15)
@mob("horse", "death")
def _(k): return fin(reverb(pitch(cm("Wiehern.ogg"), (-4, -5, -3)[k]), 0.4, 0.15), 0.4)

@mob("goat", "idle")
def _(k): return fin(cut(call(cm("Herd_of_goats_bleating.ogg"), k, min_dur=0.25, max_dur=1.6), 1.2), 0.15)
@mob("goat", "hurt")
def _(k): return fin(cut(pitch(call(cm("Herd_of_goats_bleating.ogg"), k + 3, min_dur=0.2, max_dur=1.2), (2, 3, 1.5)[k]), 0.6), 0.1)
@mob("goat", "death")
def _(k): return fin(reverb(pitch(call(cm("Herd_of_goats_bleating.ogg"), k, min_dur=0.3, max_dur=2.5), (-3, -4, -2)[k]), 0.3, 0.1), 0.35)

# ============================================================================== wolf and cat
@mob("wolf", "idle")
def _(k): return fin(highpass(pitch(call(sn(("monster-snarls_0", "monster-snarls-2_0", "monsters-snarls-3_0")[k]), 0, min_dur=0.3, max_dur=1.6), (2, 1, 3)[k]), 120), 0.2)
@mob("wolf", "hurt")
def _(k): return fin(pitch(oc(("barking_01", "barking_02", "barking_01")[k]), (4, 3, 5)[k]), 0.1)
@mob("wolf", "death")
def _(k):
    y = cm("Wolf_howls.ogg"); n = len(y) / SR
    return fin(cut(loudest(seg(y, n * (0.05 + 0.3 * k), n * (0.3 + 0.3 * k)), 2.2), 2.0, 0.8), 0.7)

@mob("meow", "idle")
def _(k):
    if k == 0: return fin(cm("Meow_of_a_Siamese_cat_-_freemaster2.wav"), 0.12)
    return fin(call(cm("Meow_of_a_pleading_cat.oga"), k - 1, min_dur=0.3), 0.1)
@mob("meow", "hurt")
def _(k): return fin(cut(pitch(mix(call(cm("Meow_of_a_pleading_cat.oga"), k, min_dur=0.3), (gain(oc("spit_01"), -8), 0.0)), (3, 4, 2)[k]), 0.6), 0.1)
@mob("meow", "death")
def _(k): return fin(reverb(pitch(cm("Meow_of_a_Siamese_cat_-_freemaster2.wav"), (-4, -5, -3)[k]), 0.3, 0.1), 0.4)

# ============================================================================== birds, bats and other small voices
@mob("bird", "idle")
def _(k):
    a, b, c = [pitch(oc(n), s) for n, s in ((("cute_07", "cute_08", "cute_01")[k], 7), (("cute_08", "cute_07", "cute_06")[k], 8), (("cute_07", "cute_01", "cute_08")[k], 6))]
    return fin(mix(cut(a, 0.2), (cut(b, 0.2), 0.22), (cut(c, 0.2), 0.45)), 0.08)
@mob("bird", "hurt")
def _(k): return fin(cut(pitch(oc(("cute_10", "cute_06", "cute_04")[k]), 8), 0.3), 0.06)
@mob("bird", "death")
def _(k): return fin(cut(pitch(oc(("cute_09", "cute_05", "cute_09")[k]), (4, 3, 2)[k]), 0.6), 0.25)

@mob("squeak", "idle")
def _(k): return fin(pitch(call(cm("Rabbit_oinks_and_squeaks.ogg"), k, min_dur=0.1, max_dur=0.8), (5, 6, 4)[k]), 0.05)
@mob("squeak", "hurt")
def _(k): return fin(pitch(call(cm("Rabbit_oinks_and_squeaks.ogg"), k + 3, min_dur=0.1, max_dur=0.8), (8, 9, 7)[k]), 0.04)
@mob("squeak", "death")
def _(k): return fin(cut(pitch(oc(("cute_09", "cute_02", "cute_08")[k]), 9), 0.5), 0.2)

@mob("grunt", "idle")
def _(k): return fin(reverb(pitch(oc(("grunt_01", "grunt_04", "grunt_05")[k]), (-5, -6, -4)[k]), 0.25, 0.1), 0.2)
@mob("grunt", "hurt")
def _(k): return fin(pitch(oc(("grunt_02", "hurt_05", "grunt_04")[k]), (-3, -2, -4)[k]), 0.1)
@mob("grunt", "death")
def _(k): return fin(reverb(pitch(oc(("troll_02", "troll_03", "monster_06")[k]), (-5, -4, -6)[k]), 0.4, 0.15), 0.35)

# ============================================================================== wet things: slimes, squids, fish, frogs
@mob("squish", "idle")
def _(k): return fin(cut(mix(load(f"kenney_scifi/slime_00{k % 2}.ogg"), (gain(pitch(oc(("burble_01", "burble_02", "burble_01")[k]), 2), -4), 0.05)), 1.0), 0.12)
@mob("squish", "hurt")
def _(k): return fin(cut(mix(pitch(load(f"kenney_scifi/slime_00{k % 2}.ogg"), 3), gain(pitch(oc("spit_03"), 1), -4)), 0.5), 0.08)
@mob("squish", "death")
def _(k): return fin(reverb(pitch(mix(cut(load(f"kenney_scifi/slime_00{(k + 1) % 2}.ogg"), 0.8), oc("burble_02")), -3), 0.35, 0.15), 0.35)

@mob("flop", "idle")
def _(k): return fin(mix(ki("impactSoft_medium", k), gain(pitch(oc("burble_02"), 7), -8)), 0.08)
@mob("flop", "hurt")
def _(k): return fin(mix(ki("impactSoft_heavy", k), gain(pitch(oc("spit_01"), 3), -9)), 0.06)
@mob("flop", "death")
def _(k): return fin(mix(ki("impactSoft_heavy", k), (ki("impactSoft_medium", k + 1), 0.12), (gain(pitch(oc("burble_01"), 6), -9), 0.05)), 0.12)

@mob("croak", "idle")
def _(k): return fin(pitch(oc(("burp_01", "burp_02", "burp_01")[k]), (7, 6, 8)[k]), 0.08)
@mob("croak", "hurt")
def _(k): return fin(cut(pitch(oc(("burp_02", "burp_01", "burp_02")[k]), (10, 9, 11)[k]), 0.25), 0.05)
@mob("croak", "death")
def _(k): return fin(reverb(pitch(oc(("burp_01", "burp_02", "burp_01")[k]), (3, 2, 4)[k]), 0.25, 0.1), 0.25)

@mob("guardian", "idle")
def _(k): return fin(reverb(lowpass(pitch(oc(("burble_01", "burble_02", "burble_01")[k]), -5), 2500), 0.6, 0.35), 0.3)
@mob("guardian", "hurt")
def _(k): return fin(reverb(lowpass(pitch(oc(("burble_02", "burble_01", "burble_02")[k]), -3), 3000), 0.5, 0.3), 0.15)
@mob("guardian", "death")
def _(k): return fin(reverb(lowpass(pitch(mix(oc("burble_01"), oc("roar_02")), -6), 2000), 0.9, 0.4), 0.5)

# ============================================================================== people-ish: villagers, vexes
@mob("villager", "idle")
def _(k): return fin(pitch(oc("ooh"), (-2, -1, -3)[k]), 0.15)
@mob("villager", "hurt")
def _(k): return fin(cut(pitch(oc(("hurt_02", "hurt_03", "hurt_01")[k]), (-3, -2, -4)[k]), 0.5), 0.1)
@mob("villager", "death")
def _(k): return fin(reverb(pitch(oc(("hurt_03", "hurt_02", "hurt_03")[k]), (-5, -6, -4)[k]), 0.3, 0.1), 0.3)

@mob("vex", "idle")
def _(k):
    a = pitch(oc(("cute_01", "cute_03", "cute_06")[k]), 9)
    return fin(mix(cut(a, 0.2), (cut(pitch(a, 1), 0.2), 0.17), (cut(pitch(a, -1), 0.2), 0.34)), 0.08)
@mob("vex", "hurt")
def _(k): return fin(cut(pitch(oc(("cute_10", "cute_04", "cute_01")[k]), 11), 0.3), 0.06)
@mob("vex", "death")
def _(k): return fin(reverb(cut(pitch(oc(("cute_09", "cute_05", "cute_09")[k]), 5), 0.7), 0.4, 0.2), 0.3)

# ============================================================================== monsters from the voice recordings
@mob("ender", "idle")
def _(k): return fin(reverb(pitch(oc(("alien_02", "alien_01", "alien_03")[k]), (-2, -1, -3)[k]), 0.6, 0.3), 0.3)
@mob("ender", "hurt")
def _(k): return fin(reverb(pitch(oc(("weird_02", "alien_05", "weird_01")[k])[::-1].copy(), 1), 0.4, 0.25), 0.15)
@mob("ender", "death")
def _(k): return fin(reverb(pitch(mix(oc("alien_03"), oc("weird_05")), -5), 1.0, 0.45), 0.6)

@mob("blaze", "idle")
def _(k): return fin(lowpass(pitch(oc("breath"), (-2, -1, -3)[k]), 3000), 0.3)
@mob("blaze", "hurt")
def _(k): return fin(highpass(pitch(oc(("spit_01", "spit_02", "spit_03")[k]), 1), 300), 0.1)
@mob("blaze", "death")
def _(k): return fin(reverb(lowpass(pitch(oc(("spit_02", "roar_01", "spit_03")[k]), -3), 5000), 0.6, 0.3), 0.5)

@mob("ghast", "idle")
def _(k): return fin(reverb(stretch_to(pitch(oc(("cute_04", "cute_05", "cute_10")[k]), (-2, -3, -1)[k]), 1.3), 0.8, 0.4), 0.5)
@mob("ghast", "hurt")
def _(k): return fin(reverb(stretch_to(pitch(oc(("cute_09", "cute_02", "cute_08")[k]), 0), 0.8), 0.5, 0.3), 0.3)
@mob("ghast", "death")
def _(k): return fin(reverb(stretch_to(pitch(oc(("cute_05", "cute_09", "cute_04")[k]), -5), 1.8), 1.0, 0.45), 0.8)

@mob("phantom", "idle")
def _(k): return fin(reverb(pitch(oc(("scream_01", "scream_02", "scream_01")[k]), (3, 4, 2)[k]), 0.4, 0.2), 0.3)
@mob("phantom", "hurt")
def _(k): return fin(cut(pitch(oc(("scream_02", "scream_01", "scream_02")[k]), 5), 0.45), 0.1)
@mob("phantom", "death")
def _(k): return fin(reverb(pitch(oc(("scream_01", "scream_02", "scream_01")[k]), -3), 0.8, 0.35), 0.5)

@mob("warden", "idle")
def _(k): return fin(reverb(pitch(oc(("roar_02", "grunt_03", "troll_02")[k]), (-9, -8, -10)[k]), 0.9, 0.4), 0.5)
@mob("warden", "hurt")
def _(k): return fin(reverb(pitch(oc(("grunt_01", "roar_03", "grunt_03")[k]), (-8, -9, -7)[k]), 0.5, 0.3), 0.25)
@mob("warden", "death")
def _(k): return fin(reverb(pitch(mix(oc("roar_02"), oc("troll_02")), -11), 1.4, 0.5), 0.9)

@mob("dragon", "idle")
def _(k): return fin(reverb(pitch(oc(("roar_01", "roar_03", "roar_02")[k]), (-5, -6, -7)[k]), 0.9, 0.35), 0.4)
@mob("dragon", "hurt")
def _(k): return fin(reverb(pitch(oc(("roar_03", "roar_01", "roar_03")[k]), (-3, -4, -2)[k]), 0.6, 0.3), 0.25)
@mob("dragon", "death")
def _(k): return fin(reverb(pitch(mix(oc("roar_02"), (oc("roar_01"), 0.15)), -8), 1.3, 0.45), 0.9)

@mob("wither", "idle")
def _(k): return fin(reverb(pitch(oc(("scream_02", "troll_03", "monster_06")[k]), (-6, -5, -7)[k]), 0.9, 0.4), 0.4)
@mob("wither", "hurt")
def _(k): return fin(reverb(pitch(oc(("troll_01", "scream_02", "monster_03")[k]), (-4, -6, -3)[k]), 0.5, 0.3), 0.2)
@mob("wither", "death")
def _(k): return fin(reverb(pitch(mix(oc("scream_01"), oc("troll_02")), -8), 1.3, 0.5), 0.8)

# ============================================================================== real impacts and creaks as "voices"
@mob("metal", "idle")
def _(k): return fin(mix(ki("impactMetal_light", k), (gain(ki("impactTin_medium", k + 1), -6), 0.06)), 0.25)
@mob("metal", "hurt")
def _(k): return fin(mix(ki("impactMetal_medium", k), gain(ki("impactPlate_medium", k), -5)), 0.3)
@mob("metal", "death")
def _(k): return fin(mix(ki("impactMetal_heavy", k), gain(ki("impactBell_heavy", k), -9), (ki("impactPlate_heavy", k + 1), 0.15)), 0.6)

@mob("snow", "idle")
def _(k): return fin(load(f"kenney_impact/footstep_snow_{k:03d}.ogg"), 0.1)
@mob("snow", "hurt")
def _(k): return fin(highpass(mix(load(f"kenney_impact/footstep_snow_{k + 1:03d}.ogg"), gain(pitch(oc("spit_03"), 2), -6)), 500), 0.08)
@mob("snow", "death")
def _(k): return fin(mix(load(f"kenney_impact/footstep_snow_{k:03d}.ogg"), (load(f"kenney_impact/footstep_snow_{k + 2:03d}.ogg"), 0.12), (ki("impactSoft_heavy", k), 0.05)), 0.3)

@mob("buzz", "idle")
def _(k): return fin(lowpass(pitch(oc(("bug_04", "bug_03", "bug_04")[k]), (-4, -3, -5)[k]), 4000), 0.1)
@mob("buzz", "hurt")
def _(k): return fin(pitch(oc(("bug_01", "bug_04", "bug_03")[k]), 3), 0.06)
@mob("buzz", "death")
def _(k): return fin(pitch(oc(("bug_04", "bug_02", "bug_03")[k]), -6), 0.3)

@mob("wind", "idle")
def _(k): return fin(reverb(lowpass(stretch_to(pitch(oc("breath"), -2 - k), 1.2), 2500), 0.7, 0.3), 0.5)
@mob("wind", "hurt")
def _(k): return fin(lowpass(pitch(oc(("spit_01", "spit_02", "spit_03")[k]), -1), 4000), 0.1)
@mob("wind", "death")
def _(k): return fin(reverb(lowpass(stretch_to(pitch(oc("breath"), -4 - k), 1.4), 2000), 0.9, 0.4), 0.7)

@mob("wood", "idle")
def _(k): return fin(load(f"kenney_rpg/creak{k + 1}.ogg"), 0.2)
@mob("wood", "hurt")
def _(k): return fin(mix(pitch(load(f"kenney_rpg/creak{k + 1}.ogg"), 2), (ki("impactWood_medium", k), 0.0)), 0.15)
@mob("wood", "death")
def _(k): return fin(reverb(mix(pitch(load(f"kenney_rpg/creak{k + 1}.ogg"), -3), (ki("impactWood_heavy", k), 0.2), (ki("impactPlank_medium", k), 0.35)), 0.4, 0.2), 0.4)

@mob("shulker", "idle")
def _(k): return fin(pitch(ki("impactPlate_light", k), -3), 0.15)
@mob("shulker", "hurt")
def _(k): return fin(mix(ki("impactTin_medium", k), gain(ki("impactWood_light", k), -4)), 0.12)
@mob("shulker", "death")
def _(k): return fin(reverb(mix(ki("impactPlate_heavy", k), (ki("impactWood_heavy", k), 0.08)), 0.4, 0.2), 0.4)

@mob("generic", "hurt")
def _(k): return fin(pitch(oc(("hurt_01", "hurt_02", "hurt_04")[k]), (0, 1, 0)[k]), 0.1)
@mob("generic", "death")
def _(k): return fin(reverb(pitch(oc(("hurt_05", "hurt_03", "hurt_04")[k]), (-3, -4, -3)[k]), 0.3, 0.12), 0.3)
