"""Sound recipes: how each game sound is built from the licensed recordings in tools/audio_src/.

Every recipe is a function of the take number (0, 1, 2 ...) so each sound ships with several slightly different takes.
Credits for the source material: tools/audio_src/CREDITS.md.
"""
import numpy as np
from audio_dsp import *   # noqa: F401,F403
from audio_dsp import sound, K, impact, load, seg, dur, gain, norm, fade, pitch, lowpass, highpass, bandpass, mix, reverb, trim_silence, loudest, denoise, soft_clip, stretch_to, SR

def tk(*names):
    """Pick the source for a take: names are file templates, cycled by take index."""
    return lambda k: names[k % len(names)]

# ================================================================================================== blocks: digging
@sound("dig_stone", 4)
def _(k): return fade(trim_silence(impact("impactMining", k)), 0.002, 0.08)
@sound("dig_wood", 4)
def _(k): return fade(trim_silence(mix(impact("impactWood_medium", k), (gain(impact("impactPlank_medium", k), -4), 0.0))), 0.002, 0.08)
@sound("dig_grass", 4)
def _(k): return fade(trim_silence(mix(gain(impact("impactSoft_medium", k), -2), gain(load(f"kenney_impact/footstep_grass_{k:03d}.ogg"), -5))), 0.002, 0.1)
@sound("dig_wool", 4)
def _(k): return fade(trim_silence(lowpass(mix(impact("impactSoft_heavy", k), gain(load(f"kenney_impact/footstep_carpet_{k:03d}.ogg"), -3)), 4500)), 0.002, 0.1)
@sound("dig_snow", 4)
def _(k): return fade(trim_silence(mix(load(f"kenney_impact/footstep_snow_{k:03d}.ogg"), gain(impact("impactSoft_medium", k), -8))), 0.002, 0.1)
@sound("dig_glass", 4)
def _(k): return fade(trim_silence(impact("impactGlass_medium", k)), 0.002, 0.1)
@sound("break_glass", 3)
def _(k): return fade(trim_silence(impact("impactGlass_heavy", k)), 0.002, 0.2)

# ================================================================================================== footsteps
@sound("step_stone", 5)
def _(k): return fade(trim_silence(load(f"kenney_impact/footstep_concrete_{k:03d}.ogg"), 0.004, 0.03), 0.002, 0.06)
@sound("step_wood", 5)
def _(k): return fade(trim_silence(load(f"kenney_impact/footstep_wood_{k:03d}.ogg"), 0.004, 0.03), 0.002, 0.06)
@sound("step_grass", 5)
def _(k): return fade(trim_silence(load(f"kenney_impact/footstep_grass_{k:03d}.ogg"), 0.004, 0.03), 0.002, 0.06)
@sound("step_wool", 5)
def _(k): return fade(trim_silence(load(f"kenney_impact/footstep_carpet_{k:03d}.ogg"), 0.004, 0.03), 0.002, 0.06)
@sound("step_snow", 5)
def _(k): return fade(trim_silence(load(f"kenney_impact/footstep_snow_{k:03d}.ogg"), 0.004, 0.03), 0.002, 0.06)
@sound("step_glass", 4)
def _(k): return fade(trim_silence(gain(impact("impactGlass_light", k), -6)), 0.002, 0.06)

# ================================================================================================== machines and furniture
@sound("door_open", 2)
def _(k): return fade(trim_silence(mix(load(f"kenney_rpg/doorOpen_{k + 1}.ogg"), (gain(load(f"kenney_rpg/creak{k + 1}.ogg"), -9), 0.02))), 0.002, 0.12)
@sound("door_close", 4)
def _(k): return fade(trim_silence(load(f"kenney_rpg/doorClose_{k + 1}.ogg")), 0.002, 0.1)
@sound("chest_open", 3)
def _(k): return fade(trim_silence(mix(gain(load(f"kenney_rpg/creak{k + 1}.ogg"), -3), (load("kenney_rpg/metalLatch.ogg"), 0.02))), 0.002, 0.12)
@sound("chest_close", 3)
def _(k): return fade(trim_silence(mix(impact("impactWood_heavy", k), (gain(load("kenney_rpg/metalClick.ogg"), -4), 0.03))), 0.002, 0.1)
@sound("lever", 2)
def _(k): return fade(trim_silence(load("kenney_rpg/metalLatch.ogg" if k == 0 else "kenney_rpg/metalClick.ogg")), 0.001, 0.05)
@sound("plate", 2)
def _(k): return fade(trim_silence(impact("impactPlate_light", k)), 0.001, 0.06)
@sound("click", 2)
def _(k): return fade(trim_silence(gain(load("kenney_rpg/metalClick.ogg"), -2)), 0.001, 0.04)
@sound("piston_out", 2)
def _(k): return fade(trim_silence(lowpass(mix(impact("impactPlate_heavy", k), gain(impact("impactMetal_medium", k), -6)), 3500)), 0.002, 0.15)
@sound("piston_in", 2)
def _(k): return fade(trim_silence(lowpass(mix(impact("impactPlate_medium", k), gain(impact("impactTin_medium", k), -6)), 3000)), 0.002, 0.12)
@sound("anvil_land", 3)
def _(k): return fade(trim_silence(mix(impact("impactMetal_heavy", k), gain(impact("impactBell_heavy", k), -8), gain(impact("impactPlate_heavy", k), -4))), 0.001, 0.4)

# ================================================================================================== combat and player actions
@sound("hit", 3)
def _(k): return fade(trim_silence(impact("impactPunch_medium", k)), 0.001, 0.06)
@sound("crit", 3)
def _(k): return fade(trim_silence(mix(impact("impactPunch_heavy", k), gain(impact("impactMetal_light", k), -6))), 0.001, 0.12)
@sound("swing", 3)
def _(k): return fade(trim_silence(lowpass(load(f"kenney_rpg/knifeSlice{'' if k == 0 else '2'}.ogg" if k < 2 else "kenney_rpg/drawKnife1.ogg"), 7000)), 0.003, 0.05)
@sound("arrow_hit", 3)
def _(k): return fade(trim_silence(mix(impact("impactWood_light", k), gain(load("kenney_rpg/knifeSlice2.ogg"), -10))), 0.001, 0.08)
@sound("land", 3)
def _(k): return fade(trim_silence(impact("impactSoft_heavy", k)), 0.002, 0.1)
@sound("eat", 3)
def _(k): return fade(trim_silence(mix(gain(load(f"kenney_rpg/cloth{k + 1}.ogg"), -6), (impact("impactGeneric_light", k), 0.05))), 0.002, 0.05)

import audio_recipes_mobs   # noqa: F401,E402  (creature voices)

# ================================================================================================== items, interface and effects
# (defined after the first batch so these versions replace any earlier recipe of the same name)
def ui(n): return load(f"kenney_ui/{n}.ogg")
def oc(n): return load(f"oga_creature80/{n}.ogg")
def xf(y, f_in=0.25, f_out=0.25):
    """Fade both edges: lets one-shot recordings of continuous sounds (rain, wind) be started and stopped cleanly."""
    return fade(y, f_in, f_out)

@sound("pop", 4)
def _(k): return fade(mix(pitch(ui(f"drop_00{k + 1}"), 2), (gain(pitch(ui("pluck_001"), 12 + k), -9), 0.0)), 0.001, 0.05)
@sound("xp", 3)
def _(k): return fade(trim_silence(pitch(ui(f"glass_00{k + 1}"), 5)), 0.001, 0.12)
@sound("levelup", 2)
def _(k):
    ping = trim_silence(ui(f"glass_00{k + 1}"))
    return fade(mix(*[(gain(pitch(ping, st), -2 * i), 0.11 * i) for i, st in enumerate((0, 4, 7, 12))]), 0.001, 0.4)
@sound("eat", 4)
def _(k): return fade(trim_silence(oc(f"eat_0{k + 1}")), 0.003, 0.06)
@sound("burp", 2)
def _(k): return fade(trim_silence(oc(f"burp_0{k + 1}")), 0.003, 0.1)
@sound("explode", 3)
def _(k):
    return fade(trim_silence(mix(load(f"kenney_scifi/explosionCrunch_00{k}.ogg"), gain(load(f"kenney_scifi/lowFrequency_explosion_00{k % 2}.ogg"), -3))), 0.001, 0.5)
@sound("bow", 3)
def _(k): return fade(trim_silence(mix(pitch(ui(f"pluck_00{k % 2 + 1}"), -7), (gain(highpass(load(f"kenney_rpg/knifeSlice{'' if k % 2 == 0 else '2'}.ogg"), 1500), -6), 0.04))), 0.001, 0.15)
@sound("cave", 2)
def _(k): return fade(lowpass(reverb(pitch(load(f"kenney_scifi/spaceEngineLow_00{k}.ogg"), -3), 1.0, 0.4), 1800), 0.4, 0.6)
@sound("hell_rumble", 2)
def _(k): return fade(lowpass(pitch(load(f"kenney_scifi/spaceEngineLow_00{k + 2}.ogg"), -6), 700), 0.5, 0.8)
@sound("hell_wail", 2)
def _(k): return fade(reverb(pitch(oc("howl"), (-3, -5)[k]), 1.6, 0.5), 0.1, 0.9)
@sound("player_hurt", 3)
def _(k): return fade(trim_silence(pitch(oc(("hurt_02", "hurt_03", "hurt_04")[k]), (-2, -1, -3)[k]), 0.05), 0.003, 0.08)
@sound("player_death", 2)
def _(k): return fade(reverb(pitch(oc(("hurt_03", "hurt_02")[k]), -4), 0.4, 0.15), 0.003, 0.35)
import audio_recipes_env   # noqa: F401,E402  (environment + small animals; replaces earlier placeholders)
