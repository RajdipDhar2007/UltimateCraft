#include "audio.h"
#include "../core/platform.h"
// Ogg Vorbis decoder (stb_vorbis, public domain, compiled from lib/stb): recordings in res/audio/ replace the synthesised sounds.
extern int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);

#define SR 22050          // synthesis sample rate
#define OUT_RATE 44100
#define MAX_VOICES 40
#define MUSIC_PIECES 3

typedef struct { s16 *pcm; int len; int rate; } Buffer;   // rate 0 = SR (synthesised); recordings keep their own rate
typedef struct { const Buffer *buf; f64 pos, step; f32 gl, gr; bool music; f32 lp, lpa; } Voice;   // lpa: air-absorption low-pass coefficient (1 = off)

static Buffer sounds[SND_COUNT], music[MUSIC_PIECES], sift_music, title_music;
#define MAX_VARIANTS 6
static Buffer variants[SND_COUNT][MAX_VARIANTS];   // recorded takes of a sound; one is picked at random each time it plays
static u8 nvariants[SND_COUNT];
static int recorded_sounds;
static const Buffer *pick_buffer(int id) {
    if (nvariants[id] > 1) return &variants[id][irand(0, nvariants[id] - 1)];
    return &sounds[id];
}
static bool in_sift;
static Voice voices[MAX_VOICES];
static Mutex mix_lock;
static bool backend_ok, quit_flag, inited;
static pthread_t audio_thread;
static vec3 listener;
static f32 listener_yaw, vol_master = 1, vol_sfx = 1, vol_music = 0.6f;
static f64 next_music_time = 20;

// ============================================================================ synthesis helpers
static Rng srng = {99};
static f32 nz(void) { return rng_f(&srng) * 2 - 1; }
typedef struct { f32 b0, b1, b2, a1, a2, x1, x2, y1, y2; } Biquad;
static Biquad bq_bandpass(f32 f, f32 q) {
    f32 w = 2 * PI_F * f / SR, a = sinf(w) / (2 * q), c = cosf(w), n = 1 + a;
    return (Biquad){a / n, 0, -a / n, -2 * c / n, (1 - a) / n, 0, 0, 0, 0};
}
static Biquad bq_lowpass(f32 f, f32 q) {
    f32 w = 2 * PI_F * f / SR, a = sinf(w) / (2 * q), c = cosf(w), n = 1 + a;
    return (Biquad){(1 - c) / 2 / n, (1 - c) / n, (1 - c) / 2 / n, -2 * c / n, (1 - a) / n, 0, 0, 0, 0};
}
static f32 bq(Biquad *b, f32 x) {
    f32 y = b->b0 * x + b->b1 * b->x1 + b->b2 * b->x2 - b->a1 * b->y1 - b->a2 * b->y2;
    b->x2 = b->x1; b->x1 = x; b->y2 = b->y1; b->y1 = y;
    return y;
}

static f32 *tmp_buf(f32 seconds, int *n) { *n = (int)(seconds * SR); return xcalloc((size_t)*n, sizeof(f32)); }
static void finish(int id, f32 *s, int n, f32 gain) {
    f32 peak = 1e-6f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(s[i]));
    f32 g = gain / peak;
    sounds[id].pcm = xmalloc(sizeof(s16) * n);
    sounds[id].len = n;
    for (int i = 0; i < n; i++) {
        f32 fade = i > n - 64 ? (n - i) / 64.0f : 1.0f;   // click-free tail
        sounds[id].pcm[i] = (s16)clampf(s[i] * g * fade * 32000.0f, -32767, 32767);
    }
    free(s);
}

// filtered noise burst with exponential decay and optional granular crunch
static void gen_noise(int id, f32 dur, f32 freq, f32 q, f32 decay, f32 grain, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad f = bq_bandpass(freq, q);
    f32 g = 1;
    for (int i = 0; i < n; i++) {
        if (grain > 0 && i % (int)(SR * 0.012f) == 0) g = 0.3f + 0.7f * rng_f(&srng);
        f32 t = (f32)i / SR, env = expf(-t * decay) * fminf(1, t * 400);
        s[i] = bq(&f, nz()) * env * (grain > 0 ? lerpf(1, g, grain) : 1);
    }
    finish(id, s, n, gain);
}

// voiced sound: sawtooth with pitch glide/vibrato through two formant filters
static void gen_voice(int id, f32 dur, f32 f0, f32 f1, f32 vib_hz, f32 vib_amt, f32 form1, f32 form2, f32 noise, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad a = bq_bandpass(form1, 2.5f), b = bq_bandpass(form2, 3.0f);
    f32 ph = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, k = t / dur;
        f32 f = lerpf(f0, f1, k) * (1 + vib_amt * sinf(2 * PI_F * vib_hz * t));
        ph += f / SR; ph -= floorf(ph);
        f32 src = (ph * 2 - 1) + noise * nz();
        f32 env = fminf(1, t * 30) * fminf(1, (dur - t) * 12);
        s[i] = (bq(&a, src) + 0.6f * bq(&b, src)) * env;
    }
    finish(id, s, n, gain);
}

static void gen_tone(int id, const f32 *freqs, int nf, f32 note_len, f32 decay, f32 gain) {
    int n; f32 *s = tmp_buf(note_len * nf + 0.4f, &n);
    for (int k = 0; k < nf; k++) {
        int start = (int)(k * note_len * SR);
        for (int i = start; i < n; i++) {
            f32 t = (f32)(i - start) / SR, env = expf(-t * decay) * fminf(1, t * 300);
            f32 w = 2 * PI_F * freqs[k] * t;
            s[i] += (sinf(w) + 0.3f * sinf(2 * w) + 0.15f * sinf(3.01f * w)) * env;
        }
    }
    finish(id, s, n, gain);
}

static void gen_pluck(int id, f32 freq, f32 dur, f32 gain) {   // Karplus-Strong string
    int n; f32 *s = tmp_buf(dur, &n);
    int p = (int)(SR / freq);
    f32 *ring = xcalloc((size_t)p, sizeof(f32));
    for (int i = 0; i < p; i++) ring[i] = nz();
    for (int i = 0; i < n; i++) {
        int j = i % p;
        f32 v = ring[j];
        ring[j] = 0.5f * (v + ring[(j + 1) % p]) * 0.996f;
        s[i] = v;
    }
    free(ring);
    finish(id, s, n, gain);
}

static void gen_explosion(int id) {
    int n; f32 *s = tmp_buf(1.8f, &n);
    Biquad lp = bq_lowpass(300, 0.7f);
    f32 brown = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR;
        brown = brown * 0.985f + nz() * 0.15f;
        f32 env = expf(-t * 2.6f) * fminf(1, t * 200);
        f32 crack = t < 0.08f ? nz() * (1 - t / 0.08f) : 0;
        s[i] = bq(&lp, brown * 3) * env + crack * 0.6f + 0.4f * sinf(2 * PI_F * 45 * t) * env;
    }
    finish(id, s, n, 1.0f);
}

static void gen_clicks(int id, f32 dur, int count, f32 freq, f32 gain) {   // rattles / clucks
    int n; f32 *s = tmp_buf(dur, &n);
    for (int c = 0; c < count; c++) {
        int start = (int)((c + rng_f(&srng) * 0.4f) * n / count);
        Biquad b = bq_bandpass(freq * (0.8f + rng_f(&srng) * 0.4f), 6);
        for (int i = 0; i < (int)(SR * 0.04f) && start + i < n; i++) {
            f32 t = (f32)i / SR;
            s[start + i] += bq(&b, nz() + (i < 20 ? 1.0f : 0)) * expf(-t * 90);
        }
    }
    finish(id, s, n, gain);
}

#include "voices.inc"

// ============================================================================ the Sift's music: bells over a pad
static void gen_sift_music(Buffer *out, u64 seed) {
    f32 dur = 60; int n = (int)(dur * SR);
    f32 *s = xcalloc((size_t)n, sizeof(f32));
    Rng r = {seed};
    static const int lyd[] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19};
    f32 root = 261.6f; int deg = 5;
    for (f32 c = 0; c < dur - 6; c += 7.5f) {   // pad: root, fifth and ninth with a slow swell
        f32 base = root * 0.5f * powf(2, (rng_int(&r, 0, 1) ? 0 : 7) / 12.0f);
        f32 fr[3] = {base, base * 1.4983f, base * 2.2449f};
        for (int i = 0; i < (int)(8.5f * SR) && (int)(c * SR) + i < n; i++) {
            f32 tt = (f32)i / SR, env = fminf(1, tt / 2.5f) * expf(-fmaxf(0, tt - 4) * 0.8f);
            f32 v = 0; for (int k = 0; k < 3; k++) v += sinf(2 * PI_F * fr[k] * tt) * (k ? 0.5f : 0.8f);
            s[(int)(c * SR) + i] += 0.18f * env * v;
        }
    }
    for (f32 t = 1.0f; t < dur - 5; t += rng_range(&r, 1.1f, 1.7f)) {   // bells
        if (rng_f(&r) < 0.2f) continue;
        deg = clampi(deg + rng_int(&r, -2, 2), 0, 11);
        f32 f = root * powf(2, lyd[deg] / 12.0f); int st = (int)(t * SR);
        for (int i = 0; i < (int)(4.0f * SR) && st + i < n; i++) {
            f32 tt = (f32)i / SR, w = 2 * PI_F * f * tt;
            s[st + i] += 0.6f * expf(-tt * 1.1f) * fminf(1, tt * 300) * (sinf(w) + 0.3f * sinf(2.76f * w) * expf(-tt * 4));
        }
    }
    int d1 = (int)(0.45f * SR), d2 = (int)(0.71f * SR);
    for (int i = n - 1; i >= d2; i--) s[i] += 0.32f * s[i - d1] + 0.22f * s[i - d2];
    f32 peak = 1e-6f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(s[i]));
    out->pcm = xmalloc(sizeof(s16) * n); out->len = n;
    for (int i = 0; i < n; i++) { f32 fade = fminf(1, fminf(i / (3.0f * SR), (n - i) / (4.0f * SR))); out->pcm[i] = (s16)(s[i] / peak * 0.75f * fade * 32000); }
    free(s);
}
void audio_set_sift(bool sift) { in_sift = sift; }
// the title theme: a slow, warm arpeggio in D major over long bass notes, with a bell melody on top
static void gen_title_music(Buffer *out) {
    f32 dur = 64; int n = (int)(dur * SR);
    f32 *s = xcalloc((size_t)n, sizeof(f32));
    static const f32 CH[4][3] = {{146.8f, 220.0f, 293.7f}, {123.5f, 185.0f, 246.9f}, {98.0f, 146.8f, 196.0f}, {110.0f, 164.8f, 220.0f}};   // D, Bm, G, A
    static const int MEL[16] = {9, 7, 4, 2, 4, 7, 9, 11, 12, 11, 9, 7, 9, 4, 2, 0};
    for (int bar = 0; bar < 16; bar++) {
        const f32 *c = CH[bar % 4]; f32 t0 = bar * 4.0f;
        for (int i = 0; i < (int)(4.2f * SR) && (int)(t0 * SR) + i < n; i++) {   // bass
            f32 t = (f32)i / SR; s[(int)(t0 * SR) + i] += 0.35f * sinf(2 * PI_F * c[0] * 0.5f * t) * fminf(1, t * 4) * expf(-t * 0.5f);
        }
        for (int k = 0; k < 8; k++) {   // arpeggio
            f32 f = c[k % 3] * (k >= 3 && k < 6 ? 2 : 1); int st = (int)((t0 + k * 0.5f) * SR);
            for (int i = 0; i < (int)(1.6f * SR) && st + i < n; i++) { f32 t = (f32)i / SR; s[st + i] += 0.16f * sinf(2 * PI_F * f * t) * expf(-t * 2.5f) * fminf(1, t * 200); }
        }
        if (bar >= 4) {   // melody from the fifth bar
            f32 f = 293.7f * powf(2, MEL[bar] / 12.0f); int st = (int)((t0 + 0.02f) * SR);
            for (int i = 0; i < (int)(3.5f * SR) && st + i < n; i++) { f32 t = (f32)i / SR, w = 2 * PI_F * f * t; s[st + i] += 0.3f * expf(-t * 0.9f) * fminf(1, t * 150) * (sinf(w) + 0.25f * sinf(2 * w)); }
        }
    }
    int d1 = (int)(0.37f * SR);
    for (int i = n - 1; i >= d1; i--) s[i] += 0.3f * s[i - d1];
    f32 peak = 1e-6f; for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(s[i]));
    out->pcm = xmalloc(sizeof(s16) * n); out->len = n;
    for (int i = 0; i < n; i++) { f32 fade = fminf(1, fminf(i / (2.0f * SR), (n - i) / (4.0f * SR))); out->pcm[i] = (s16)(s[i] / peak * 0.7f * fade * 32000); }
    free(s);
}

// ============================================================================ music (generated ambient piano)
static void gen_music(Buffer *out, u64 seed) {
    f32 dur = 48;
    int n = (int)(dur * SR);
    f32 *s = xcalloc((size_t)n, sizeof(f32));
    Rng r = {seed};
    static const int scale[] = {0, 2, 4, 7, 9, 12, 14, 16, 19, 21};   // major pentatonic
    f32 root = 196.0f * powf(2, rng_int(&r, -3, 3) / 12.0f);
    f32 beat = rng_range(&r, 0.62f, 0.85f);
    int degree = 4;
    for (f32 t = 0.5f; t < dur - 4; t += beat * (rng_f(&r) < 0.3f ? 2 : 1)) {
        if (rng_f(&r) < 0.18f) continue;   // rests
        degree = clampi(degree + rng_int(&r, -2, 2), 0, 9);
        f32 freqs[2] = {root * powf(2, scale[degree] / 12.0f), 0};
        if (rng_f(&r) < 0.35f) freqs[1] = root * powf(2, (scale[MAXI(0, degree - 3)] - 12) / 12.0f);
        for (int k = 0; k < 2; k++) {
            if (!freqs[k]) continue;
            int start = (int)(t * SR);
            f32 amp = k ? 0.5f : rng_range(&r, 0.6f, 1.0f);
            for (int i = 0; i < (int)(3.5f * SR) && start + i < n; i++) {
                f32 tt = (f32)i / SR, w = 2 * PI_F * freqs[k] * tt;
                f32 env = expf(-tt * 1.4f) * fminf(1, tt * 200);
                s[start + i] += amp * env * (sinf(w) + 0.35f * sinf(2 * w) * expf(-tt * 3) + 0.1f * sinf(3 * w) * expf(-tt * 5));
            }
        }
    }
    // soft echo for space
    int d1 = (int)(0.37f * SR), d2 = (int)(0.53f * SR);
    for (int i = n - 1; i >= d2; i--) s[i] += 0.28f * s[i - d1] + 0.18f * s[i - d2];
    f32 peak = 1e-6f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(s[i]));
    out->pcm = xmalloc(sizeof(s16) * n); out->len = n;
    for (int i = 0; i < n; i++) {
        f32 fade = fminf(1, fminf(i / (2.0f * SR), (n - i) / (3.0f * SR)));
        out->pcm[i] = (s16)(s[i] / peak * 0.8f * fade * 32000);
    }
    free(s);
}

static void synth_all(void) {
    // block materials: dig (louder, longer) and steps
    struct { f32 f, q, decay, grain; } M[8] = {
        {2600, 0.8f, 30, 0.3f},  // stone
        {650, 1.6f, 38, 0.1f},   // wood
        {1500, 0.7f, 22, 0.8f},  // grass
        {1100, 0.6f, 20, 1.0f},  // gravel
        {3800, 0.7f, 18, 0.5f},  // sand
        {4200, 1.2f, 35, 0.2f},  // glass
        {500, 0.7f, 25, 0.3f},   // wool
        {2200, 0.6f, 22, 0.9f},  // snow
    };
    (void)M;
    for (int i = 0; i < 8; i++) {   // layered impacts: tick + thud + ring + granular crunch per material
        gen_material(SND_DIG_STONE + i, i, false);
        gen_material(SND_STEP_STONE + i, i, true);
    }
    { // glass shatter: noise + ringing partials
        int n; f32 *s = tmp_buf(0.6f, &n);
        Biquad b = bq_bandpass(5000, 0.8f);
        for (int i = 0; i < n; i++) {
            f32 t = (f32)i / SR;
            s[i] = bq(&b, nz()) * expf(-t * 9) + 0.3f * (sinf(2 * PI_F * 2900 * t) + sinf(2 * PI_F * 4100 * t)) * expf(-t * 7) * (rng_f(&srng) > 0.5f);
        }
        finish(SND_BREAK_GLASS, s, n, 0.9f);
    }
    gen_voice(SND_HURT, 0.25f, 260, 150, 0, 0, 700, 1200, 0.1f, 0.9f);
    synth_world_sfx();
    synth_creatures();
    { f32 f[1] = {0}; (void)f; int n; f32 *s = tmp_buf(0.07f, &n);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] = sinf(2 * PI_F * (400 + 9000 * t) * t) * expf(-t * 40); }
      finish(SND_POP, s, n, 0.8f); }
    { f32 f[1] = {1568}; gen_tone(SND_XP, f, 1, 0.1f, 14, 0.7f); }
    { f32 f[4] = {523, 659, 784, 1046}; gen_tone(SND_LEVELUP, f, 4, 0.11f, 4, 0.8f); }
    { int n; f32 *s = tmp_buf(0.03f, &n); for (int i = 0; i < n; i++) s[i] = ((i / 12) % 2 ? 1 : -1) * expf(-(f32)i / SR * 200); finish(SND_CLICK, s, n, 0.5f); }
    gen_clicks(SND_EAT, 0.18f, 3, 1800, 0.8f);
    gen_voice(SND_BURP, 0.35f, 110, 80, 18, 0.08f, 400, 900, 0.2f, 0.8f);
    gen_noise(SND_SPLASH, 0.6f, 1400, 0.5f, 6, 0.6f, 0.9f);
    gen_noise(SND_SWIM, 0.35f, 900, 0.6f, 8, 0.7f, 0.6f);
    gen_explosion(SND_EXPLODE);
    { int n; f32 *s = tmp_buf(1.5f, &n); Biquad b = bq_bandpass(3000, 0.6f);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] = bq(&b, nz()) * fminf(1, t * 3) * (0.8f + 0.2f * sinf(t * 60)); }
      finish(SND_FUSE, s, n, 0.7f); }
    gen_pluck(SND_BOW, 170, 0.4f, 0.9f);
    gen_noise(SND_ARROW_HIT, 0.12f, 900, 1.5f, 40, 0, 0.8f);
    { int n; f32 *s = tmp_buf(0.2f, &n);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; Biquad b = bq_bandpass(500 + 3000 * t / 0.2f, 1.0f); (void)b; s[i] = nz() * sinf(PI_F * t / 0.2f); }
      Biquad lp = bq_lowpass(2500, 0.7f); for (int i = 0; i < n; i++) s[i] = bq(&lp, s[i]);
      finish(SND_SWING, s, n, 0.5f); }
    { int n; f32 *s = tmp_buf(0.15f, &n);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] = (sinf(2 * PI_F * 110 * t) + 0.6f * nz() * expf(-t * 60)) * expf(-t * 30); }
      finish(SND_HIT, s, n, 0.9f); }
    { int n; f32 *s = tmp_buf(0.25f, &n);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] = (sinf(2 * PI_F * 110 * t) * expf(-t * 30)) + 0.4f * sinf(2 * PI_F * 2600 * t) * expf(-t * 18); }
      finish(SND_CRIT, s, n, 0.9f); }
    gen_clicks(SND_FIRE, 0.5f, 7, 2500, 0.6f);
    { int n; f32 *s = tmp_buf(0.2f, &n);
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] = (sinf(2 * PI_F * 70 * t) + 0.3f * nz()) * expf(-t * 25); }
      finish(SND_LAND, s, n, 0.8f); }
    { int n; f32 *s = tmp_buf(3.5f, &n); f32 lp = 0;   // Hell: a slow subterranean rumble
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; lp += (nz() - lp) * 0.02f; s[i] = (lp * 3 + 0.4f * sinf(2 * PI_F * 38 * t)) * fminf(1, t / 1.2f) * fminf(1, (3.5f - t) / 1.5f); }
      finish(SND_HELL_RUMBLE, s, n, 0.7f); }
    { int n; f32 *s = tmp_buf(2.4f, &n); f32 ph = 0;   // Hell: a far-off, falling wail
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR, f = 520 - 170 * t / 2.4f + 9 * sinf(2 * PI_F * 5 * t); ph += 2 * PI_F * f / SR;
          s[i] = (sinf(ph) + 0.25f * sinf(2 * ph) + 0.08f * nz()) * fminf(1, t / 0.4f) * expf(-fmaxf(0, t - 0.8f) * 1.6f); }
      finish(SND_HELL_WAIL, s, n, 0.35f); }
    { int n; f32 *s = tmp_buf(2.0f, &n); f32 lp = 0, hp = 0;   // rain: soft hiss with random droplets, edges faded so it loops
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR, x = nz(); lp += (x - lp) * 0.35f; hp = lp - hp * 0.2f;
          f32 drop = (frand() < 0.0009f) ? frand_range(0.3f, 1.0f) : 0; s[i] = hp * 0.35f + drop; s[i] *= fminf(1, fminf(t / 0.25f, (2.0f - t) / 0.25f)); }
      finish(SND_RAIN, s, n, 0.45f); }
    { int n; f32 *s = tmp_buf(4.5f, &n); f32 lp = 0, lp2 = 0;   // thunder: a crack, then a long rolling rumble
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; lp += (nz() - lp) * 0.05f; lp2 += (lp - lp2) * 0.08f;
          f32 crack = t < 0.15f ? nz() * (1 - t / 0.15f) : 0, roll = lp2 * 6 * expf(-t * 0.7f) * (0.6f + 0.4f * sinf(t * 7 + sinf(t * 3)));
          s[i] = crack * 0.6f + roll; }
      finish(SND_THUNDER, s, n, 0.9f); }
    { int n; f32 *s = tmp_buf(3.5f, &n); f32 ph = 0, lp = 0;   // cave ambience: a low, slowly bending tone with breathy noise
      for (int i = 0; i < n; i++) { f32 t = (f32)i / SR, f = 70 + 25 * sinf(t * 1.3f) - 15 * t; ph += 2 * PI_F * f / SR; lp += (nz() - lp) * 0.01f;
          s[i] = (sinf(ph) * 0.6f + sinf(ph * 1.5f) * 0.2f + lp * 2) * sinf(PI_F * t / 3.5f); }
      finish(SND_CAVE, s, n, 0.5f); }
    // mobs
    gen_voice(SND_ZOMBIE_IDLE, 1.0f, 95, 80, 5, 0.06f, 450, 900, 0.25f, 0.8f);
    gen_voice(SND_ZOMBIE_HURT, 0.35f, 140, 100, 8, 0.05f, 500, 1100, 0.3f, 0.9f);
    gen_voice(SND_ZOMBIE_DEATH, 1.1f, 110, 55, 4, 0.08f, 420, 850, 0.3f, 0.9f);
    gen_clicks(SND_SKELETON_IDLE, 0.5f, 9, 2200, 0.7f);
    gen_clicks(SND_SKELETON_HURT, 0.25f, 5, 1800, 0.8f);
    gen_clicks(SND_SKELETON_DEATH, 0.9f, 16, 1500, 0.8f);
    gen_noise(SND_CREEPER_HURT, 0.35f, 2800, 0.8f, 6, 0.2f, 0.7f);
    gen_noise(SND_CREEPER_DEATH, 0.7f, 1600, 0.8f, 4, 0.4f, 0.7f);
    gen_voice(SND_SPIDER_IDLE, 0.45f, 300, 260, 30, 0.2f, 2200, 3400, 0.9f, 0.6f);
    gen_voice(SND_SPIDER_DEATH, 0.7f, 260, 160, 25, 0.2f, 1800, 3000, 0.9f, 0.7f);
    gen_voice(SND_COW_IDLE, 1.0f, 130, 105, 4, 0.03f, 380, 800, 0.05f, 0.8f);
    gen_voice(SND_COW_HURT, 0.4f, 160, 120, 6, 0.04f, 420, 900, 0.1f, 0.9f);
    gen_voice(SND_PIG_IDLE, 0.22f, 300, 220, 0, 0, 900, 1800, 0.25f, 0.8f);
    gen_voice(SND_PIG_HURT, 0.3f, 420, 300, 12, 0.05f, 1000, 2000, 0.3f, 0.9f);
    gen_voice(SND_SHEEP_IDLE, 0.7f, 290, 270, 7, 0.08f, 700, 1500, 0.05f, 0.8f);
    gen_clicks(SND_CHICKEN_IDLE, 0.35f, 3, 1100, 0.7f);
    gen_voice(SND_CHICKEN_HURT, 0.2f, 900, 700, 20, 0.1f, 1600, 2800, 0.2f, 0.8f);
    for (int i = 0; i < MUSIC_PIECES; i++) gen_music(&music[i], 1000 + i * 77);
    gen_sift_music(&sift_music, 4242);
    gen_title_music(&title_music);
}

// ============================================================================ recorded sounds (res/audio/<name>_<take>.ogg)
static const char *SOUND_FILE[SND_COUNT] = {
    [SND_DIG_STONE] = "dig_stone", [SND_DIG_WOOD] = "dig_wood", [SND_DIG_GRASS] = "dig_grass", [SND_DIG_GRAVEL] = "dig_gravel", [SND_DIG_SAND] = "dig_sand",
    [SND_DIG_GLASS] = "dig_glass", [SND_DIG_WOOL] = "dig_wool", [SND_DIG_SNOW] = "dig_snow",
    [SND_STEP_STONE] = "step_stone", [SND_STEP_WOOD] = "step_wood", [SND_STEP_GRASS] = "step_grass", [SND_STEP_GRAVEL] = "step_gravel", [SND_STEP_SAND] = "step_sand",
    [SND_STEP_GLASS] = "step_glass", [SND_STEP_WOOL] = "step_wool", [SND_STEP_SNOW] = "step_snow",
    [SND_BREAK_GLASS] = "break_glass", [SND_HURT] = "player_hurt", [SND_POP] = "pop", [SND_XP] = "xp", [SND_LEVELUP] = "levelup", [SND_CLICK] = "click",
    [SND_EAT] = "eat", [SND_BURP] = "burp", [SND_SPLASH] = "splash", [SND_SWIM] = "swim", [SND_EXPLODE] = "explode", [SND_FUSE] = "fuse", [SND_BOW] = "bow",
    [SND_ARROW_HIT] = "arrow_hit", [SND_SWING] = "swing", [SND_HIT] = "hit", [SND_CRIT] = "crit", [SND_FIRE] = "fire", [SND_LAND] = "land",
    [SND_ZOMBIE_IDLE] = "mob_zombie_idle", [SND_ZOMBIE_HURT] = "mob_zombie_hurt", [SND_ZOMBIE_DEATH] = "mob_zombie_death",
    [SND_SKELETON_IDLE] = "mob_skeleton_idle", [SND_SKELETON_HURT] = "mob_skeleton_hurt", [SND_SKELETON_DEATH] = "mob_skeleton_death",
    [SND_CREEPER_HURT] = "mob_creeper_hurt", [SND_CREEPER_DEATH] = "mob_creeper_death", [SND_SPIDER_IDLE] = "mob_spider_idle", [SND_SPIDER_DEATH] = "mob_spider_death",
    [SND_COW_IDLE] = "mob_cow_idle", [SND_COW_HURT] = "mob_cow_hurt", [SND_PIG_IDLE] = "mob_pig_idle", [SND_PIG_HURT] = "mob_pig_hurt",
    [SND_SHEEP_IDLE] = "mob_sheep_idle", [SND_CHICKEN_IDLE] = "mob_chicken_idle", [SND_CHICKEN_HURT] = "mob_chicken_hurt",
    [SND_HELL_RUMBLE] = "hell_rumble", [SND_HELL_WAIL] = "hell_wail", [SND_RAIN] = "rain", [SND_THUNDER] = "thunder", [SND_CAVE] = "cave",
    [SND_PLAYER_HURT] = "player_hurt", [SND_PLAYER_DEATH] = "player_death", [SND_DOOR_OPEN] = "door_open", [SND_DOOR_CLOSE] = "door_close",
    [SND_CHEST_OPEN] = "chest_open", [SND_CHEST_CLOSE] = "chest_close", [SND_LEVER] = "lever", [SND_PISTON_OUT] = "piston_out", [SND_PISTON_IN] = "piston_in",
    [SND_ANVIL_LAND] = "anvil_land", [SND_PLATE] = "plate",
};
static const char *MOB_KIND_FILE[MOB_SOUND_KINDS] = {   // order of SOUND_KINDS in tools/content/mobs.py
    "generic", "zombie", "skeleton", "creeper", "spider", "cow", "pig", "sheep", "chicken", "horse", "wolf", "meow", "bird", "squeak", "grunt", "squish", "flop",
    "croak", "goat", "villager", "ender", "blaze", "ghast", "guardian", "metal", "snow", "buzz", "wind", "wood", "phantom", "shulker", "vex", "warden", "dragon", "wither",
};
static bool load_recording(const char *path, Buffer *out) {
    int ch = 0, rate = 0; short *pcm = NULL;
    int n = stb_vorbis_decode_filename(path, &ch, &rate, &pcm);
    if (n <= 0 || !pcm) return false;
    if (ch > 1) { for (int i = 0; i < n; i++) { int s = 0; for (int c = 0; c < ch; c++) s += pcm[i * ch + c]; pcm[i] = (short)(s / ch); } }   // mono mix-down
    out->pcm = (s16 *)pcm; out->len = n; out->rate = rate;
    return true;
}
static void load_recordings(void) {
    for (int id = 1; id < SND_COUNT; id++) {
        char stem[64];
        if (id >= SND_MOB_FIRST && id <= SND_MOB_LAST) {
            static const char *W[3] = {"idle", "hurt", "death"};
            snprintf(stem, sizeof stem, "mob_%s_%s", MOB_KIND_FILE[(id - SND_MOB_FIRST) / 3], W[(id - SND_MOB_FIRST) % 3]);
        } else if (SOUND_FILE[id]) snprintf(stem, sizeof stem, "%s", SOUND_FILE[id]);
        else continue;
        for (int k = 0; k < MAX_VARIANTS; k++) {
            char path[160]; snprintf(path, sizeof path, "res/audio/%s_%d.ogg", stem, k);
            if (!load_recording(path, &variants[id][nvariants[id]])) break;
            nvariants[id]++;
        }
        if (nvariants[id]) { free(sounds[id].pcm); sounds[id] = variants[id][0]; recorded_sounds++; }
    }
}

// ============================================================================ mixer
static void mix(s16 *out, int frames) {
    static f32 acc[4096 * 2];
    if (frames > 4096) frames = 4096;
    memset(acc, 0, sizeof(f32) * frames * 2);
    mutex_lock(&mix_lock);
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *vo = &voices[v];
        if (!vo->buf) continue;
        f32 mv = vol_master * (vo->music ? vol_music : vol_sfx);
        for (int i = 0; i < frames; i++) {
            int p = (int)vo->pos;
            if (p + 1 >= vo->buf->len) { vo->buf = NULL; break; }
            f32 fr = (f32)(vo->pos - p);
            f32 raw = (vo->buf->pcm[p] * (1 - fr) + vo->buf->pcm[p + 1] * fr) / 32768.0f;
            vo->lp += (raw - vo->lp) * vo->lpa;   // distant sounds lose their highs
            f32 smp = vo->lp * mv;
            acc[i * 2] += smp * vo->gl;
            acc[i * 2 + 1] += smp * vo->gr;
            vo->pos += vo->step;
        }
    }
    mutex_unlock(&mix_lock);
    for (int i = 0; i < frames * 2; i++) {
        f32 x = acc[i];
        x = x / (1.0f + fabsf(x) * 0.25f);   // soft limiter
        out[i] = (s16)clampf(x * 32767.0f, -32767, 32767);
    }
}

static void voice_start(const Buffer *b, f32 gl, f32 gr, f32 pitch, bool is_music, f32 damp) {
    if (!backend_ok || !b->pcm) return;
    mutex_lock(&mix_lock);
    int slot = -1;
    for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].buf) { slot = i; break; }
    if (slot < 0) for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].music) { slot = i; break; }   // steal
    if (slot >= 0) voices[slot] = (Voice){b, 0, (f64)(b->rate ? b->rate : SR) / OUT_RATE * pitch, gl, gr, is_music, 0, 1.0f - 0.8f * clampf(damp, 0, 1)};
    mutex_unlock(&mix_lock);
}

// ---- subtitles: a caption per audible sound (recorded even without audio output)
static const char *CAPTION[SND_COUNT] = {
    [SND_DIG_STONE] = "Block broken", [SND_DIG_WOOD] = "Block broken", [SND_DIG_GRASS] = "Block broken", [SND_DIG_GRAVEL] = "Block broken",
    [SND_DIG_SAND] = "Block broken", [SND_DIG_GLASS] = "Glass breaks", [SND_DIG_WOOL] = "Block broken", [SND_DIG_SNOW] = "Block broken",
    [SND_STEP_STONE] = "Footsteps", [SND_STEP_WOOD] = "Footsteps", [SND_STEP_GRASS] = "Footsteps", [SND_STEP_GRAVEL] = "Footsteps",
    [SND_STEP_SAND] = "Footsteps", [SND_STEP_GLASS] = "Footsteps", [SND_STEP_WOOL] = "Footsteps", [SND_STEP_SNOW] = "Footsteps",
    [SND_BREAK_GLASS] = "Glass shatters", [SND_HURT] = "Player hurts", [SND_POP] = "Item pops", [SND_XP] = "Experience gained",
    [SND_LEVELUP] = "Level up", [SND_CLICK] = "Click", [SND_EAT] = "Eating", [SND_BURP] = "Burp", [SND_SPLASH] = "Splash", [SND_SWIM] = "Swimming",
    [SND_EXPLODE] = "Explosion", [SND_FUSE] = "Fuse hisses", [SND_BOW] = "Bow fires", [SND_ARROW_HIT] = "Arrow hits", [SND_SWING] = "Swoosh",
    [SND_HIT] = "Hit", [SND_CRIT] = "Critical hit", [SND_FIRE] = "Fire crackles", [SND_LAND] = "Landing",
    [SND_ZOMBIE_IDLE] = "Zombie groans", [SND_ZOMBIE_HURT] = "Zombie hurts", [SND_ZOMBIE_DEATH] = "Zombie dies",
    [SND_SKELETON_IDLE] = "Skeleton rattles", [SND_SKELETON_HURT] = "Skeleton hurts", [SND_SKELETON_DEATH] = "Skeleton dies",
    [SND_CREEPER_HURT] = "Creature hurts", [SND_CREEPER_DEATH] = "Creature dies", [SND_SPIDER_IDLE] = "Spider hisses", [SND_SPIDER_DEATH] = "Spider dies",
    [SND_COW_IDLE] = "Cow moos", [SND_COW_HURT] = "Cow hurts", [SND_PIG_IDLE] = "Pig oinks", [SND_PIG_HURT] = "Pig hurts", [SND_SHEEP_IDLE] = "Sheep baahs",
    [SND_CHICKEN_IDLE] = "Chicken clucks", [SND_CHICKEN_HURT] = "Chicken hurts", [SND_HELL_RUMBLE] = "Deep rumble", [SND_HELL_WAIL] = "Distant wail",
    [SND_RAIN] = "Rain", [SND_THUNDER] = "Thunder", [SND_CAVE] = "Eerie noise",
    [SND_PLAYER_HURT] = "Player hurts", [SND_PLAYER_DEATH] = "Player dies", [SND_DOOR_OPEN] = "Door creaks", [SND_DOOR_CLOSE] = "Door closes",
    [SND_CHEST_OPEN] = "Chest opens", [SND_CHEST_CLOSE] = "Chest closes", [SND_LEVER] = "Lever clicks", [SND_PISTON_OUT] = "Piston extends",
    [SND_PISTON_IN] = "Piston retracts", [SND_ANVIL_LAND] = "Anvil lands", [SND_PLATE] = "Pressure plate clicks",
};
// subtitles for the creature bank: idle / hurt / death per voice kind (order of SOUND_KINDS in tools/content/mobs.py)
static const char *MOB_CAPTION[MOB_SOUND_KINDS][3] = {
    {NULL, "Creature hurts", "Creature dies"}, {"Zombie groans", "Zombie hurts", "Zombie dies"}, {"Skeleton rattles", "Skeleton hurts", "Skeleton dies"},
    {NULL, "Creeper hisses", "Creeper dies"}, {"Spider hisses", "Spider hurts", "Spider dies"}, {"Cow moos", "Cow hurts", "Cow dies"},
    {"Pig oinks", "Pig squeals", "Pig dies"}, {"Sheep baahs", "Sheep hurts", "Sheep dies"}, {"Chicken clucks", "Chicken squawks", "Chicken dies"},
    {"Horse whinnies", "Horse hurts", "Horse dies"}, {"Wolf growls", "Wolf yelps", "Wolf whines"}, {"Cat meows", "Cat hisses", "Cat dies"},
    {"Bird chirps", "Bird squawks", "Bird dies"}, {"Squeaking", "Squeak", "Creature dies"}, {"Creature grunts", "Creature hurts", "Creature dies"},
    {"Squelching", "Squelch", "Squelch"}, {"Fish flops", "Fish flops", "Fish flops"}, {"Frog croaks", "Frog hurts", "Frog dies"},
    {"Goat bleats", "Goat hurts", "Goat dies"}, {"Villager mumbles", "Villager hurts", "Villager dies"}, {"Voidwalker whirs", "Voidwalker hurts", "Voidwalker dies"},
    {"Flames breathe", "Flame flares", "Flames die"}, {"Ghast cries", "Ghast screams", "Ghast dies"}, {"Guardian gurgles", "Guardian hurts", "Guardian dies"},
    {"Metal clanks", "Metal clangs", "Metal crashes"}, {"Snow crunches", "Snow puffs", "Snow collapses"}, {"Buzzing", "Angry buzzing", "Buzzing stops"},
    {"Wind swirls", "Wind gusts", "Wind fades"}, {"Wood creaks", "Wood groans", "Wood splinters"}, {"Phantom screeches", "Phantom hurts", "Phantom dies"},
    {"Shell clicks", "Shell clangs", "Shell cracks"}, {"Vex cackles", "Vex hurts", "Vex dies"}, {"Deep growl", "Roaring", "Dying roar"},
    {"Dragon growls", "Dragon hurts", "Dragon dies"}, {"Wither moans", "Wither hurts", "Wither dies"},
};
static Caption caps[8]; static int ncaps;
int g_captions_total;
static void caption_push(int id, f32 pan, bool positional) {
    const char *t = id > SND_NONE && id < SND_COUNT ? CAPTION[id] : NULL;
    if (id >= SND_MOB_FIRST && id <= SND_MOB_LAST) t = MOB_CAPTION[(id - SND_MOB_FIRST) / 3][(id - SND_MOB_FIRST) % 3];
    if (!t) return;
    g_captions_total++;
    f64 now = plat_time();
    for (int i = 0; i < ncaps; i++) if (caps[i].text == t) { caps[i].time = now; caps[i].pan = pan; caps[i].positional = positional; return; }
    if (ncaps == 8) { memmove(caps, caps + 1, sizeof caps[0] * 7); ncaps--; }
    caps[ncaps++] = (Caption){t, pan, positional, now};
    if (g_narrate) printf("[narrate] sound: %s\n", t);
}
int audio_captions(Caption *out, int max) {   // captions heard in the last 3 seconds, newest last
    f64 now = plat_time(); int n = 0;
    for (int i = 0; i < ncaps && n < max; i++) if (now - caps[i].time < 3.0) out[n++] = caps[i];
    return n;
}
bool g_narrate;

void sound_play(int id, f32 volume, f32 pitch) {
    if (id <= SND_NONE || id >= SND_COUNT) return;
    if (volume > 0.05f) caption_push(id, 0, false);
    voice_start(pick_buffer(id), volume * 0.7f, volume * 0.7f, pitch, false, 0);
}
void sound_play_at(int id, vec3 pos, f32 volume, f32 pitch) {
    if (id <= SND_NONE || id >= SND_COUNT) return;
    vec3 d = v3_sub(pos, listener);
    f32 dist = v3_len(d);
    f32 range = 16.0f * fmaxf(1.0f, volume);
    if (dist > range) return;
    f32 g = volume * (1.0f - dist / range);
    vec3 right = v3(-cosf(listener_yaw), 0, sinf(listener_yaw));
    f32 pan = dist > 0.5f ? clampf(v3_dot(v3_norm(d), right), -1, 1) * 0.7f : 0;
    if (g > 0.03f) caption_push(id, pan, dist > 1.5f);
    voice_start(pick_buffer(id), g * sqrtf((1 - pan) * 0.5f), g * sqrtf((1 + pan) * 0.5f), pitch, false, powf(dist / range, 0.7f));
}
void audio_set_listener(vec3 p, f32 yaw) { listener = p; listener_yaw = yaw; }
void audio_set_volumes(f32 m, f32 s, f32 mu) { vol_master = m; vol_sfx = s; vol_music = mu; }
bool audio_backend_active(void) { return backend_ok; }

void audio_update(bool in_game, bool night) {
    if (!backend_ok) return;
    f64 now = plat_time();
    bool playing = false;
    mutex_lock(&mix_lock);
    for (int i = 0; i < MAX_VOICES; i++) if (voices[i].buf && voices[i].music) playing = true;
    mutex_unlock(&mix_lock);
    if (!playing && now > next_music_time) {
        int k = irand(0, MUSIC_PIECES - 1);
        voice_start(!in_game ? &title_music : in_sift ? &sift_music : &music[k], 0.55f, 0.55f, in_sift ? 1.0f : night ? 0.89f : 1.0f, true, 0);
        next_music_time = now + (in_game ? 48 + irand(120, 300) : 66);   // the title theme loops
    }
}

// ============================================================================ backends
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
static HWAVEOUT wo;
static void *backend_thread(void *arg) {
    (void)arg;
    enum { NB = 4, FR = 1024 };
    static s16 data[NB][FR * 2];
    WAVEHDR hdr[NB];
    memset(hdr, 0, sizeof hdr);
    for (int i = 0; i < NB; i++) {
        hdr[i].lpData = (LPSTR)data[i];
        hdr[i].dwBufferLength = FR * 4;
        waveOutPrepareHeader(wo, &hdr[i], sizeof(WAVEHDR));
        mix(data[i], FR);
        waveOutWrite(wo, &hdr[i], sizeof(WAVEHDR));
    }
    int cur = 0;
    while (!quit_flag) {
        if (hdr[cur].dwFlags & WHDR_DONE) {
            mix(data[cur], FR);
            waveOutWrite(wo, &hdr[cur], sizeof(WAVEHDR));
            cur = (cur + 1) % NB;
        } else Sleep(2);
    }
    waveOutReset(wo);
    for (int i = 0; i < NB; i++) waveOutUnprepareHeader(wo, &hdr[i], sizeof(WAVEHDR));
    waveOutClose(wo);
    return NULL;
}
static bool backend_open(void) {
    WAVEFORMATEX f;
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 2; f.nSamplesPerSec = OUT_RATE; f.wBitsPerSample = 16;
    f.nBlockAlign = 4; f.nAvgBytesPerSec = OUT_RATE * 4;
    return waveOutOpen(&wo, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
}
#elif defined(__linux__)
#include <dlfcn.h>
typedef struct snd_pcm snd_pcm_t;
static void *alsa;
static snd_pcm_t *pcm;
static int (*p_open)(snd_pcm_t **, const char *, int, int);
static int (*p_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*p_writei)(snd_pcm_t *, const void *, unsigned long);
static int (*p_recover)(snd_pcm_t *, int, int);
static int (*p_close)(snd_pcm_t *);
static void *backend_thread(void *arg) {
    (void)arg;
    static s16 data[1024 * 2];
    while (!quit_flag) {
        mix(data, 1024);
        long r = p_writei(pcm, data, 1024);
        if (r < 0) p_recover(pcm, (int)r, 1);
    }
    p_close(pcm);
    return NULL;
}
static bool backend_open(void) {
    alsa = dlopen("libasound.so.2", RTLD_NOW);
    if (!alsa) return false;
    p_open = (int (*)(snd_pcm_t **, const char *, int, int))dlsym(alsa, "snd_pcm_open");
    p_set_params = (int (*)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned))dlsym(alsa, "snd_pcm_set_params");
    p_writei = (long (*)(snd_pcm_t *, const void *, unsigned long))dlsym(alsa, "snd_pcm_writei");
    p_recover = (int (*)(snd_pcm_t *, int, int))dlsym(alsa, "snd_pcm_recover");
    p_close = (int (*)(snd_pcm_t *))dlsym(alsa, "snd_pcm_close");
    if (!p_open || !p_set_params || !p_writei || !p_recover || !p_close) return false;
    if (p_open(&pcm, "default", 0 /*PLAYBACK*/, 0) < 0) return false;
    if (p_set_params(pcm, 2 /*S16_LE*/, 3 /*RW_INTERLEAVED*/, 2, OUT_RATE, 1, 60000) < 0) { p_close(pcm); return false; }
    return true;
}
#else
static void *backend_thread(void *arg) { (void)arg; return NULL; }
static bool backend_open(void) { return false; }
#endif

bool audio_init(void) {
    mutex_init(&mix_lock);
    f64 t0 = plat_time();
    synth_all();
    load_recordings();
    inited = true;
    backend_ok = backend_open();
    if (backend_ok) pthread_create(&audio_thread, NULL, backend_thread, NULL);
    int voiced = 0;
    for (int i = 1; i < SND_COUNT; i++) voiced += sounds[i].pcm != NULL;   // a few creature slots (generic / creeper idle) are deliberately silent
    LOG_INFO("audio: %d sounds (%d from recordings) + %d music pieces ready in %.2fs, output %s", voiced, recorded_sounds, MUSIC_PIECES, plat_time() - t0,
             backend_ok ? "enabled" : "unavailable (game continues silently)");
    next_music_time = plat_time() + 4;
    return backend_ok;
}

void audio_shutdown(void) {
    if (backend_ok) { quit_flag = true; pthread_join(audio_thread, NULL); }
    backend_ok = false;
}

bool audio_export_wav(int id, const char *path) {
    if (!inited || id <= 0 || id >= SND_COUNT || !sounds[id].pcm) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    u32 n = (u32)sounds[id].len * 2, v;
    u16 w;
    fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); w = 1; fwrite(&w, 2, 1, f); fwrite(&w, 2, 1, f);
    v = sounds[id].rate ? sounds[id].rate : SR; fwrite(&v, 4, 1, f); v *= 2; fwrite(&v, 4, 1, f); w = 2; fwrite(&w, 2, 1, f); w = 16; fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(sounds[id].pcm, 2, sounds[id].len, f);
    fclose(f);
    return true;
}
