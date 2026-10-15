#!/usr/bin/env python3
"""
UltimateCraft sound-design pipeline.

Turns the licensed source recordings in tools/audio_src/ (CC0 / public domain, see tools/audio_src/CREDITS.md) into the
game's sound files res/audio/<name>_<take>.ogg (mono Ogg Vorbis). Several takes per sound are produced so repeated
sounds (footsteps, animal calls) never sound identical. The game falls back to its built-in synthesiser for any sound
whose files are missing (see src/audio/audio.c). The recipes live in tools/audio_recipes.py.

    python tools/build_audio.py              # rebuild everything
    python tools/build_audio.py cow wolf     # only sounds whose name contains one of these words

Requires numpy, scipy and ffmpeg (pip install numpy scipy imageio-ffmpeg).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_dsp as D
import audio_recipes   # noqa: F401  (registers every sound)

def main():
    words = sys.argv[1:]
    done = 0
    for name, (fn, takes) in sorted(D.SOUNDS.items()):
        if words and not any(w in name for w in words): continue
        for k, y in enumerate(D.prepare_takes([fn(k) for k in range(takes)])):
            D.write_ogg(name, k, y, finished=True)
            done += 1
    print(f"wrote {done} files to {D.OUT}")

if __name__ == "__main__":
    main()
