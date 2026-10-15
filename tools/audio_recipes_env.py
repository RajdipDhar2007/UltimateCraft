"""Environment, small animals and ground sounds built from real field recordings (Wikimedia Commons, PD / CC0 / CC BY).

Imported last, so these recipes replace earlier placeholders of the same name. Credits: tools/audio_src/CREDITS.md.
"""
import numpy as np
from audio_dsp import *   # noqa: F401,F403
from audio_dsp import sound, load, seg, dur, gain, norm, fade, pitch, lowpass, highpass, bandpass, mix, reverb, trim_silence, loudest, denoise, soft_clip, stretch_to, call, events, SR
from audio_recipes_mobs import fin, cut, cm, mob

def ki(n, i): return load(f"kenney_impact/{n}_{i:03d}.ogg")
GRAVEL = "Footstep_on_Gravel_Gravity_Sound_.mp3"
T = (0, -1.5, 1.5, -3, 3)     # transposition per take

# ================================================================================================== ground sounds
@sound("step_gravel", 5)
def _(k): return fade(trim_silence(pitch(cm(GRAVEL), T[k]), 0.03, 0.02), 0.003, 0.06)
@sound("dig_gravel", 4)
def _(k): return fade(trim_silence(mix(pitch(cm(GRAVEL), T[k] - 3), (gain(ki("impactSoft_medium", k), -5), 0.0)), 0.03, 0.02), 0.003, 0.1)
@sound("step_sand", 5)
def _(k): return fade(trim_silence(lowpass(pitch(cm(GRAVEL), 4 + T[k]), 7000), 0.03, 0.02), 0.003, 0.07)
@sound("dig_sand", 4)
def _(k): return fade(trim_silence(lowpass(mix(pitch(cm(GRAVEL), 2 + T[k]), (gain(ki("impactSoft_medium", k), -6), 0.0)), 7500), 0.03, 0.02), 0.003, 0.1)

# ================================================================================================== water, fire, weather
@sound("splash", 4)
def _(k): return fade(trim_silence(call(cm("Bathtub_water_splashes.ogg"), k, min_dur=0.3, max_dur=1.5), 0.04, 0.02), 0.003, 0.2)
@sound("swim", 3)
def _(k): return fade(trim_silence(gain(pitch(call(cm("Bathtub_water_splashes.ogg"), k + 4, min_dur=0.25, max_dur=1.2), 2), -3), 0.04, 0.02), 0.003, 0.2)
@sound("rain", 2)
def _(k): return fade(seg(cm("Rain_1_.ogg"), 8 + 12 * k, 11 + 12 * k), 0.35, 0.35)
@sound("thunder", 3)
def _(k):
    y = cm("Nosferatu_thunderclap_-_Richard_Humphries.ogg")
    return fade(pitch(seg(y, 3.0 + 0.6 * k, 9.5 + 0.6 * k), (0, -2, 1.5)[k]), 0.01, 1.2)
@sound("fire", 3)
def _(k):
    y = cm("Bones_breaking_wood_fire_ice_crackling.ogg")
    ev = events(y, min_dur=0.04, max_dur=0.5, rel=0.15, gap=0.03)
    rng = np.random.RandomState(40 + k)
    layers = []
    for i in range(7):
        t0, t1, _ = ev[rng.randint(min(len(ev), 14))]
        layers.append((gain(seg(y, t0, t1), -rng.uniform(0, 6)), rng.uniform(0, 1.1)))
    return fade(mix(*layers), 0.01, 0.3)
@sound("fuse", 2)
def _(k): return fade(highpass(seg(cm("WWS_Fireoftheforge.ogg"), 3 + 4 * k, 4.6 + 4 * k), 500), 0.05, 0.25)

# ================================================================================================== small animals
@mob("bird", "idle")
def _(k):
    y = cm("House_Sparrow_202279702.ogg")
    a, b, c = (call(y, k * 3 + i, min_dur=0.08, max_dur=0.7, rel=0.12) for i in range(3))
    return fin(mix(cut(a, 0.3), (cut(b, 0.3), 0.3), (cut(c, 0.3), 0.6)), 0.08)
@mob("bird", "hurt")
def _(k): return fin(cut(pitch(call(cm("House_Sparrow_202279702.ogg"), k, min_dur=0.08, max_dur=0.6, rel=0.12), 3), 0.3), 0.06)
@mob("bird", "death")
def _(k): return fin(cut(pitch(call(cm("House_Sparrow_202279702.ogg"), k + 5, min_dur=0.1, max_dur=0.8, rel=0.12), -4), 0.7), 0.25)

@mob("squeak", "idle")
def _(k): return fin(call(cm("Hoary_bat_chirp_recording.ogg"), k, min_dur=0.04, max_dur=0.5, rel=0.12), 0.05)
@mob("squeak", "hurt")
def _(k): return fin(pitch(call(cm("Hoary_bat_chirp_recording.ogg"), k + 2, min_dur=0.04, max_dur=0.5, rel=0.12), 3), 0.04)
@mob("squeak", "death")
def _(k): return fin(reverb(pitch(cm("Bat_avk_.wav"), (-1, -2, 0)[k]), 0.3, 0.12), 0.25)

@mob("buzz", "idle")
def _(k): return fin(seg(cm("Bee_buzzing_sound_animal_noises_.opus"), 1.0 + 2.2 * k, 2.2 + 2.2 * k), 0.2)
@mob("buzz", "hurt")
def _(k): return fin(pitch(seg(cm("Bee_buzzing_sound_animal_noises_.opus"), 6.0 + k, 6.5 + k), 3), 0.1)
@mob("buzz", "death")
def _(k): return fin(stretch_to(pitch(seg(cm("Bee_buzzing_sound_animal_noises_.opus"), 8 + k, 9 + k), -5), 1.2), 0.5)

@mob("croak", "idle")
def _(k): return fin(pitch(call(cm("E_bicolor.ogg"), k, min_dur=0.2, max_dur=1.2, rel=0.2), -5), 0.1)
@mob("croak", "hurt")
def _(k): return fin(cut(pitch(call(cm("E_bicolor.ogg"), k, min_dur=0.2, max_dur=1.2, rel=0.2), -3), 0.4), 0.06)
@mob("croak", "death")
def _(k): return fin(reverb(pitch(call(cm("E_bicolor.ogg"), k, min_dur=0.2, max_dur=1.2, rel=0.2), -8), 0.3, 0.12), 0.3)

# the breeze: real gusts of wind
@mob("wind", "idle")
def _(k): return fin(fade(lowpass(loudest(seg(cm("Howling_wind.ogg"), 10 + 30 * k, 40 + 30 * k), 1.4), 3500), 0.3, 0.45), 0.5, 0.01)
@mob("wind", "hurt")
def _(k): return fin(highpass(loudest(seg(cm("Howling_wind.ogg"), 5 + 20 * k, 25 + 20 * k), 0.5), 400), 0.2, 0.01)
@mob("wind", "death")
def _(k): return fin(reverb(lowpass(pitch(loudest(seg(cm("Howling_wind.ogg"), 20 + 25 * k, 50 + 25 * k), 1.6), -3), 2500), 0.7, 0.3), 0.7, 0.01)
