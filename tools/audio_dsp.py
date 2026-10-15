"""
UltimateCraft sound-design helpers (used by build_audio.py and audio_recipes.py).

Turns the licensed source recordings in tools/audio_src/ (CC0 / public domain, see tools/audio_src/CREDITS.md) into the
game's sound files res/audio/<name>_<take>.ogg (mono Ogg Vorbis). Several takes per sound are produced so repeated
sounds (footsteps, animal calls) never sound identical. The game falls back to its built-in synthesiser for any sound
whose files are missing (see src/audio/audio.c).

    python tools/build_audio.py              # rebuild everything
    python tools/build_audio.py cow wolf     # only sounds whose name contains one of these words

Requires numpy, scipy and ffmpeg (pip install numpy scipy imageio-ffmpeg).
"""
import os, sys, subprocess, tempfile, hashlib, random
import numpy as np
from scipy import signal
from scipy.io import wavfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "audio_src")
OUT = os.path.join(ROOT, "res", "audio")
SR = 44100

def ffmpeg_exe():
    from shutil import which
    exe = which("ffmpeg")
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return exe or "ffmpeg"
FFMPEG = ffmpeg_exe()

# ------------------------------------------------------------------------------------------------ DSP helpers
_cache = {}
def load(rel):
    """Decode any source file to mono float32 at 44.1 kHz."""
    if rel in _cache: return _cache[rel].copy()
    path = os.path.join(SRC, rel)
    raw = subprocess.run([FFMPEG, "-v", "error", "-i", path, "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"], capture_output=True, check=True).stdout
    y = np.frombuffer(raw, dtype=np.float32).copy()
    _cache[rel] = y
    return y.copy()

def seg(y, t0=0.0, t1=None):
    a = int(t0 * SR); b = len(y) if t1 is None else int(t1 * SR)
    return y[a:b].copy()
def dur(y): return len(y) / SR
def gain(y, db): return y * (10 ** (db / 20))
def norm(y, peak=0.9):
    m = float(np.max(np.abs(y))) if len(y) else 0
    return y * (peak / m) if m > 1e-6 else y
def fade(y, fin=0.005, fout=0.03):
    y = y.copy(); a, b = int(fin * SR), int(fout * SR)
    if a: y[:a] *= np.linspace(0, 1, a)
    if b and b < len(y): y[-b:] *= np.linspace(1, 0, b)
    return y
def pitch(y, semitones):
    """Varispeed: pitch and duration change together, like slowing a tape (natural for animal calls)."""
    if abs(semitones) < 1e-3: return y
    f = 2 ** (semitones / 12)
    return np.interp(np.arange(0, len(y), f), np.arange(len(y)), y).astype(np.float32)
def stretch_to(y, seconds):
    return signal.resample(y, int(seconds * SR)).astype(np.float32) if len(y) else y
def lowpass(y, hz, order=4):
    return signal.sosfilt(signal.butter(order, hz / (SR / 2), "low", output="sos"), y).astype(np.float32)
def highpass(y, hz, order=4):
    return signal.sosfilt(signal.butter(order, hz / (SR / 2), "high", output="sos"), y).astype(np.float32)
def bandpass(y, lo, hi, order=3):
    return signal.sosfilt(signal.butter(order, [lo / (SR / 2), hi / (SR / 2)], "band", output="sos"), y).astype(np.float32)
def mix(*layers):
    """layers: (signal, start_seconds) pairs or bare signals; the result is as long as the longest layer."""
    items = [(l, 0.0) if not isinstance(l, tuple) else l for l in layers]
    n = max(int(s * SR) + len(y) for y, s in items)
    out = np.zeros(n, dtype=np.float32)
    for y, s in items: out[int(s * SR): int(s * SR) + len(y)] += y
    return out
def reverb(y, seconds=0.6, wet=0.3, seed=1):
    rng = np.random.RandomState(seed)
    n = int(seconds * SR)
    ir = rng.randn(n).astype(np.float32) * np.exp(-np.linspace(0, 7, n)).astype(np.float32)
    ir = lowpass(ir, 5000, 2)
    w = signal.fftconvolve(y, ir).astype(np.float32)
    w = w / (np.max(np.abs(w)) + 1e-9) * np.max(np.abs(y))
    out = np.zeros(len(w), dtype=np.float32); out[: len(y)] += y * (1 - wet * 0.5); out += w * wet
    return out
def soft_clip(y, drive=2.0): return np.tanh(y * drive) / np.tanh(drive)
def trim_silence(y, thresh=0.02, pad=0.01):
    idx = np.where(np.abs(y) > thresh * (np.max(np.abs(y)) + 1e-9))[0]
    if len(idx) == 0: return y
    return y[max(0, idx[0] - int(pad * SR)): idx[-1] + int(pad * SR)]
def loudest(y, seconds):
    """The loudest window of the given length (to pick the best call out of a long recording)."""
    w = int(seconds * SR)
    if len(y) <= w: return y
    e = np.convolve(y ** 2, np.ones(w, dtype=np.float32), "valid")
    s = int(np.argmax(e)); return y[s: s + w]
def denoise(y, amount=0.6):
    """Spectral gating: tames the hiss of field recordings."""
    f, t, Z = signal.stft(y, SR, nperseg=1024)
    mag = np.abs(Z); floor = np.percentile(mag, 15, axis=1, keepdims=True) * 1.5
    mask = np.clip((mag - floor * amount) / (mag + 1e-9), 0.05, 1)
    _, out = signal.istft(Z * mask, SR, nperseg=1024)
    return out[: len(y)].astype(np.float32)


def events(y, min_dur=0.15, max_dur=3.0, rel=0.22, pad=0.04, gap=0.12):
    """Find the individual calls in a long recording: returns [(t0, t1, energy)] sorted loudest first."""
    hop = int(0.01 * SR)
    n = len(y) // hop
    if n < 3: return [(0.0, len(y) / SR, float(np.sum(y ** 2)))]
    env = np.sqrt(np.convolve(y ** 2, np.ones(hop * 2, dtype=np.float32) / (hop * 2), "same"))[: n * hop].reshape(n, hop).mean(axis=1)
    thr = rel * np.percentile(env, 99)
    on = env > thr
    segs, i = [], 0
    while i < n:
        if on[i]:
            j = i
            while j < n and (on[j] or (j + int(gap * 100) < n and on[j: j + int(gap * 100)].any())): j += 1
            segs.append((i, j)); i = j
        else: i += 1
    out = []
    for a, b in segs:
        t0, t1 = max(0.0, a * 0.01 - pad), min(len(y) / SR, b * 0.01 + pad)
        if min_dur <= t1 - t0 <= max_dur:
            seg_y = y[int(t0 * SR): int(t1 * SR)]
            out.append((t0, t1, float(np.sum(seg_y ** 2))))
    return sorted(out, key=lambda e: -e[2])
def call(y, k, **kw):
    """The k-th loudest call of a recording (cycling), cut out with short fades."""
    ev = events(y, **kw)
    t0, t1, _ = ev[k % len(ev)]
    return fade(seg(y, t0, t1), 0.004, 0.05)

# ------------------------------------------------------------------------------------------------ output
SOUNDS = {}      # name -> function(take_index) -> float32 signal ; number of takes
def sound(name, takes=3):
    def deco(fn):
        SOUNDS[name] = (fn, takes)
        return fn
    return deco

def dc_block(y, hz=18.0):
    """High-pass at a few Hz: removes the DC offset some source recordings carry (it makes clicks and wastes headroom)."""
    sos = signal.butter(2, hz / (SR / 2), "highpass", output="sos")
    return signal.sosfilt(sos, y).astype(np.float32)

def active_rms(y):
    """RMS over the audible part only (frames within 40 dB of the peak), so a long quiet tail does not make a sound look soft."""
    fr = 441
    if len(y) < 2 * fr: return float(np.sqrt(np.mean(y ** 2))) + 1e-9
    m = np.array([np.sqrt(np.mean(y[i:i + fr] ** 2)) for i in range(0, len(y) - fr, fr)])
    a = m[m > m.max() * 0.01]
    return float(np.sqrt(np.mean(a ** 2))) + 1e-9 if len(a) else float(m.max()) + 1e-9

def soft_limit(y, ceiling=0.85, knee=0.6):
    """Transparent peak limiter: untouched below `knee`*ceiling, then a smooth curve that never exceeds `ceiling`."""
    k = ceiling * knee; a = np.abs(y)
    over = a > k
    out = y.copy()
    out[over] = np.sign(y[over]) * (k + (ceiling - k) * np.tanh((a[over] - k) / (ceiling - k)))
    return out

def prepare_takes(takes, peak=0.85):
    """Finish all takes of one sound together: DC removal, fades, then every take is brought to the same audible loudness
    (the median, never more than 6 dB of change) so no take of a footstep or call is suddenly twice as loud as its siblings."""
    ys = [norm(fade(dc_block(y), 0.003, 0.04), 0.9) for y in takes]
    if len(ys) > 1:
        r = [active_rms(y) for y in ys]; target = float(np.median(r))
        ys = [y * float(np.clip(target / ri, 0.5, 2.0)) for y, ri in zip(ys, r)]
    return [soft_limit(y, peak) for y in ys]

def write_ogg(name, take, y, peak=0.9, finished=False):
    """Encode one take. `finished` takes come from prepare_takes; others are normalised here. The decoded file is checked and
    re-encoded 1.5 dB lower while the Vorbis codec's overshoot would still clip the game's 16-bit mixer."""
    if not finished: y = soft_limit(dc_block(norm(fade(y, 0.003, 0.04), peak)), peak)
    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, f"{name}_{take}.ogg")
    for attempt in range(6):
        with tempfile.TemporaryDirectory() as td:
            wav = os.path.join(td, "x.wav")
            wavfile.write(wav, SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
            subprocess.run([FFMPEG, "-v", "error", "-y", "-i", wav, "-c:a", "libvorbis", "-q:a", "3", out], check=True)
        raw = subprocess.run([FFMPEG, "-v", "error", "-i", out, "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"], capture_output=True, check=True).stdout
        dec = np.frombuffer(raw, dtype=np.float32)
        if len(dec) == 0 or float(np.max(np.abs(dec))) <= 0.985: return
        y = y * 0.84

def K(rel): return load(rel)
def impact(name, i): return load(f"kenney_impact/{name}_{i:03d}.ogg")

