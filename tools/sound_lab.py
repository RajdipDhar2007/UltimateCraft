#!/usr/bin/env python3
"""Renders spectrograms of the game's creature voices (used for the README "sound lab" image).

    ultimatecraft --export-sounds out/wav        # writes every sound as out/wav/sound_NN.wav
    python tools/sound_lab.py out/wav docs/images/sound_lab.jpg

Needs numpy and matplotlib. Creature sounds start at SND_MOB_FIRST (see src/audio/audio.h; variant 0 of each sound is exported): the id of
sound `which` (0 idle, 1 hurt, 2 death) of voice `kind` is  SND_MOB_FIRST + kind * 3 + which.
"""
import os, sys, wave
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

KINDS = ["generic", "zombie", "skeleton", "creeper", "spider", "cow", "pig", "sheep", "chicken", "horse", "wolf", "meow", "bird",
         "squeak", "grunt", "squish", "flop", "croak", "goat", "villager", "ender", "blaze", "ghast", "guardian", "metal", "snow",
         "buzz", "wind", "wood", "phantom", "shulker", "vex", "warden", "dragon", "wither"]
SHOW = [("cow", 0, "Cow - moo"), ("pig", 1, "Pig - squeal"), ("wolf", 0, "Wolf - growl"), ("meow", 0, "Cat - meow"),
        ("horse", 0, "Horse - whinny"), ("ghast", 0, "Ghast - wail"), ("warden", 0, "Warden - sub-bass growl"), ("dragon", 2, "Dragon - death roar")]

def first_creature_id(wav_dir):
    # SND_MOB_FIRST is the first id after SND_PLATE; the generic idle slot (silent) has no file, so find the gap.
    ids = sorted(int(f[6:-4]) for f in os.listdir(wav_dir) if f.startswith("sound_") and f.endswith(".wav"))
    for a, b in zip(ids, ids[1:]):
        if b - a == 2 and a >= 60: return a + 1   # the missing id is the silent generic idle
    raise SystemExit("could not locate the creature bank")

def load(path):
    w = wave.open(path)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768
    return x, w.getframerate()

def main():
    wav_dir, out = sys.argv[1], sys.argv[2]
    base = first_creature_id(wav_dir)
    fig, axes = plt.subplots(2, 4, figsize=(16, 6.2), facecolor="#0d1117")
    for ax, (kind, which, title) in zip(axes.flat, SHOW):
        x, sr = load(os.path.join(wav_dir, "sound_%02d.wav" % (base + KINDS.index(kind) * 3 + which)))
        ax.set_facecolor("#0d1117")
        ax.specgram(x, NFFT=512, Fs=sr, noverlap=448, cmap="magma", vmin=-110, vmax=-20)
        ax.set_ylim(0, 5000)
        ax.set_title(title, color="#e6edf3", fontsize=12, pad=6)
        ax.tick_params(colors="#8b949e", labelsize=8)
        for s in ax.spines.values(): s.set_color("#30363d")
    fig.suptitle("UltimateCraft sound lab - creature voices built from real recordings (spectrograms, 0-5 kHz)", color="#e6edf3", fontsize=15, y=0.985)
    fig.text(0.5, 0.012, "time (s)  ->        real animal and voice recordings, edited with tools/build_audio.py: transposed, layered and given room reverb - sources in tools/audio_src/CREDITS.md",
             color="#8b949e", ha="center", fontsize=9)
    plt.tight_layout(rect=(0, 0.03, 1, 0.95))
    fig.savefig(out, dpi=110, facecolor=fig.get_facecolor(), pil_kwargs={"quality": 88})
    print("wrote", out)

if __name__ == "__main__":
    main()
