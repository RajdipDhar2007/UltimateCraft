// Voice and effect synthesis for UltimateCraft's creatures, blocks and machines (included by audio.c).
//
// Creature voices use a small source-filter vocal tract: a glottal-style pulse source (pitch contour, vibrato and
// jitter) is shaped by three gliding formant resonators, mixed with breath noise and amplitude-modulated for growls,
// bleats and trills. Non-vocal sounds (hisses, chirps, bubbles, metal, creaks, buzzing) have their own small
// generators. Every creature kind has an idle, a hurt and a death sound: SND_MOB(kind, 0..2).

static void bq_retune(Biquad *b, Biquad n) { b->b0 = n.b0; b->b1 = n.b1; b->b2 = n.b2; b->a1 = n.a1; b->a2 = n.a2; }
static f32 env_ar(f32 t, f32 dur, f32 att, f32 rel) { return fminf(1.0f, t / fmaxf(att, 1e-3f)) * fminf(1.0f, (dur - t) / fmaxf(rel, 1e-3f)); }

typedef struct {
    f32 dur;
    f32 f0[3];          // pitch (Hz) at the start, middle and end
    f32 vib_hz, vib;    // vibrato rate and depth (fraction of f0)
    f32 f1[2], f2[2];   // first and second formant, start -> end (Hz)
    f32 f3;             // third formant (Hz)
    f32 breath;         // 0..1 noise in the source
    f32 am_hz, am;      // amplitude modulation: growl / bleat / trill
    f32 jit;            // random pitch wobble (rasp)
    f32 att, rel;       // attack / release (seconds)
    f32 gain;
} Vocal;

static void gen_vocal(int id, Vocal v) {
    int n; f32 *s = tmp_buf(v.dur, &n);
    Biquad b1 = bq_bandpass(v.f1[0], 4.0f), b2 = bq_bandpass(v.f2[0], 5.0f), b3 = bq_bandpass(v.f3, 6.0f);
    f32 ph = 0, jit = 0, tilt = 0, body = 0, mod_ph = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, k = t / v.dur;
        if (i % 24 == 0) {   // glide the formants
            bq_retune(&b1, bq_bandpass(lerpf(v.f1[0], v.f1[1], k), 4.0f));
            bq_retune(&b2, bq_bandpass(lerpf(v.f2[0], v.f2[1], k), 5.0f));
        }
        f32 f = k < 0.5f ? lerpf(v.f0[0], v.f0[1], k * 2) : lerpf(v.f0[1], v.f0[2], (k - 0.5f) * 2);
        jit += (nz() * v.jit - jit) * 0.08f;
        f *= (1 + v.vib * sinf(2 * PI_F * v.vib_hz * t)) * (1 + jit);
        ph += f / SR; ph -= floorf(ph);
        f32 saw = ph * 2 - 1;
        tilt += (saw - tilt) * clampf(2 * PI_F * fmaxf(500.0f, f * 5) / SR, 0.02f, 0.9f);   // spectral tilt: duller than a raw saw
        body += (saw - body) * clampf(2 * PI_F * 280.0f / SR, 0.01f, 0.5f);                  // chest resonance under the formants
        f32 src = tilt * (1 - v.breath * 0.55f) + v.breath * nz() * 0.9f;
        mod_ph += v.am_hz / SR; mod_ph -= floorf(mod_ph);
        f32 amp = v.am > 0 ? (1 - v.am) + v.am * fmaxf(0.0f, sinf(2 * PI_F * mod_ph)) : 1.0f;
        s[i] = (bq(&b1, src) + 0.7f * bq(&b2, src) + 0.35f * bq(&b3, src) + 0.45f * body * (1 - v.breath)) * amp * env_ar(t, v.dur, v.att, v.rel);
    }
    finish(id, s, n, v.gain);
}

// breathy / hissing noise through a sweeping band-pass, with optional pulsing
static void gen_hiss(int id, f32 dur, f32 fa, f32 fb, f32 q, f32 am_hz, f32 am, f32 att, f32 rel, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad b = bq_bandpass(fa, q);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, k = t / dur;
        if (i % 16 == 0) bq_retune(&b, bq_bandpass(lerpf(fa, fb, k), q));
        f32 amp = am > 0 ? (1 - am) + am * (0.5f + 0.5f * sinf(2 * PI_F * am_hz * t)) : 1.0f;
        s[i] = bq(&b, nz()) * amp * env_ar(t, dur, att, rel);
    }
    finish(id, s, n, gain);
}

// bird / bat style chirps: n short sine sweeps
static void gen_chirp(int id, f32 dur, f32 fa, f32 fb, int n_chirps, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    int span = n / n_chirps, len = (int)(span * 0.7f);
    for (int c = 0; c < n_chirps; c++) {
        f32 ph = 0, shift = 1 + 0.06f * (rng_f(&srng) - 0.5f);
        for (int i = 0; i < len && c * span + i < n; i++) {
            f32 u = (f32)i / len, t = (f32)i / SR;
            f32 f = lerpf(fa, fb, u) * shift * (1 + 0.025f * sinf(2 * PI_F * 38 * t));
            ph += 2 * PI_F * f / SR;
            s[c * span + i] += (sinf(ph) + 0.25f * sinf(2 * ph)) * powf(sinf(PI_F * u), 1.3f);
        }
    }
    finish(id, s, n, gain);
}

// wet blips over a low gurgle: slimes, squids, guardians
static void gen_bubble(int id, f32 dur, f32 f_lo, f32 f_hi, int count, f32 body_hz, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad lp = bq_lowpass(body_hz, 1.2f);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR;
        s[i] = bq(&lp, nz()) * 0.5f * (0.6f + 0.4f * sinf(2 * PI_F * 9 * t)) * env_ar(t, dur, 0.02f, dur * 0.6f);
    }
    for (int c = 0; c < count; c++) {
        int st = (int)(rng_f(&srng) * 0.8f * n), len = (int)(SR * rng_range(&srng, 0.05f, 0.1f));
        f32 base = f_lo + (f_hi - f_lo) * rng_f(&srng), ph = 0;
        for (int i = 0; i < len && st + i < n; i++) {
            f32 u = (f32)i / len;
            ph += 2 * PI_F * base * (1 + 0.9f * u) / SR;
            s[st + i] += sinf(ph) * expf(-u * 5) * fminf(1, u * 30) * 0.9f;
        }
    }
    finish(id, s, n, gain);
}

// struck metal: inharmonic partials that ring down, plus a short noise click
static void gen_metal(int id, f32 dur, f32 f, f32 decay, f32 gain) {
    static const f32 R[5] = {1.0f, 2.76f, 5.4f, 8.93f, 13.34f}, A[5] = {1.0f, 0.7f, 0.45f, 0.28f, 0.16f};
    int n; f32 *s = tmp_buf(dur, &n);
    for (int p = 0; p < 5; p++) {
        f32 fp = f * R[p] * (1 + 0.004f * (rng_f(&srng) - 0.5f));
        if (fp > SR * 0.45f) continue;
        for (int i = 0; i < n; i++) { f32 t = (f32)i / SR; s[i] += sinf(2 * PI_F * fp * t) * expf(-t * decay * (1 + p * 0.7f)) * A[p]; }
    }
    for (int i = 0; i < (int)(0.01f * SR) && i < n; i++) s[i] += nz() * (1 - i / (0.01f * SR)) * 1.2f;
    finish(id, s, n, gain);
}

// Ender-ish: detuned sine sweep with ring modulation
static void gen_ender(int id, f32 dur, f32 fa, f32 fb, f32 ring_hz, f32 vib, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    f32 ph = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, k = t / dur;
        f32 f = lerpf(fa, fb, k) * (1 + vib * sinf(2 * PI_F * 6 * t));
        ph += 2 * PI_F * f / SR;
        f32 ring = 0.55f + 0.45f * sinf(2 * PI_F * ring_hz * t);
        s[i] = (sinf(ph) + 0.5f * sinf(ph * 1.505f) + 0.25f * sinf(ph * 2.01f)) * ring * powf(sinf(PI_F * fminf(1.0f, k * 1.05f)), 0.7f);
    }
    finish(id, s, n, gain);
}

// wood / hinge creak: stick-slip friction through resonances
static void gen_creak(int id, f32 dur, f32 f, f32 glide, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad b1 = bq_bandpass(700, 3.0f), b2 = bq_bandpass(1500, 4.0f);
    f32 ph = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, k = t / dur;
        f32 fr = f * (1 + glide * k) * (1 + 0.25f * sinf(2 * PI_F * 7 * t + 6 * sinf(2 * PI_F * 1.7f * t)));
        ph += fr / SR; ph -= floorf(ph);
        f32 slip = fmaxf(0.0f, sinf(2 * PI_F * (11 + 9 * k) * t + 2 * sinf(2 * PI_F * 3.1f * t)));
        f32 src = (ph * 2 - 1) * (0.25f + 0.75f * slip) + 0.15f * nz();
        s[i] = (bq(&b1, src) + 0.6f * bq(&b2, src)) * env_ar(t, dur, 0.05f, dur * 0.35f);
    }
    finish(id, s, n, gain);
}

// bee / insect buzz
static void gen_buzz(int id, f32 dur, f32 f, f32 wobble, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad b1 = bq_bandpass(520, 2.5f), b2 = bq_bandpass(1250, 3.0f);
    f32 ph = 0;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR;
        f32 fr = f * (1 + wobble * sinf(2 * PI_F * 7 * t) + 0.02f * nz());
        ph += fr / SR; ph -= floorf(ph);
        f32 src = ph * 2 - 1;
        s[i] = (bq(&b1, src) + 0.5f * bq(&b2, src)) * (0.75f + 0.25f * sinf(2 * PI_F * 95 * t)) * env_ar(t, dur, 0.06f, 0.12f);
    }
    finish(id, s, n, gain);
}

// a deep thud with a noisy body: pistons, landings, shells
static void gen_thump(int id, f32 dur, f32 f, f32 noise_hz, f32 decay, f32 gain) {
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad b = bq_lowpass(noise_hz, 0.8f);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR;
        s[i] = (sinf(2 * PI_F * f * (1 + 0.8f * expf(-t * 40)) * t) * 0.9f + bq(&b, nz()) * 0.7f) * expf(-t * decay) * fminf(1, t * 500);
    }
    finish(id, s, n, gain);
}

// ============================================================================ blocks: layered impacts
typedef struct { f32 nf, nq, nd, na, tf, td, ta, r1, r2, rd, ra; int cn; f32 cf; } Mat;
static const Mat MATS[8] = {
    {2800, 0.8f, 36, 0.7f, 105, 45, 0.8f, 0, 0, 0, 0, 0, 0},                 // stone: sharp tick over a dull thud
    {1500, 1.2f, 50, 0.25f, 170, 28, 0.5f, 360, 610, 26, 0.7f, 0, 0},        // wood: hollow knock
    {1700, 0.7f, 24, 0.6f, 85, 35, 0.4f, 0, 0, 0, 0, 6, 2600},               // grass: swish and rustle
    {1200, 0.6f, 22, 0.35f, 70, 30, 0.2f, 0, 0, 0, 0, 16, 1800},             // gravel: many tiny crunches
    {3600, 0.6f, 20, 0.7f, 65, 25, 0.3f, 0, 0, 0, 0, 6, 4200},               // sand: soft hiss
    {4600, 1.2f, 45, 0.4f, 0, 0, 0, 2350, 3150, 16, 0.7f, 0, 0},             // glass: ringing
    {520, 0.7f, 28, 0.5f, 80, 30, 0.6f, 0, 0, 0, 0, 0, 0},                   // wool: muffled
    {2400, 0.6f, 26, 0.5f, 60, 30, 0.2f, 0, 0, 0, 0, 12, 3800},              // snow: squeaky crunch
};
static void gen_material(int id, int m, bool step) {
    const Mat *M = &MATS[m];
    f32 dur = step ? 0.15f : 0.27f, dm = step ? 1.5f : 1.0f, am = step ? 0.75f : 1.0f;
    int n; f32 *s = tmp_buf(dur, &n);
    Biquad nb = bq_bandpass(M->nf * (step ? 0.85f : 1.0f), M->nq);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / SR, v = bq(&nb, nz()) * expf(-t * M->nd * dm) * M->na * fminf(1, t * 500);
        if (M->ta > 0) v += sinf(2 * PI_F * M->tf * (1 + 0.35f * expf(-t * 60)) * t) * expf(-t * M->td * dm) * M->ta * (step ? 0.7f : 1.0f);
        if (M->ra > 0) v += (sinf(2 * PI_F * M->r1 * t) + 0.6f * sinf(2 * PI_F * M->r2 * t)) * expf(-t * M->rd * dm) * M->ra * 0.5f;
        s[i] = v * am;
    }
    for (int c = 0; c < M->cn; c++) {   // granular crunches
        int st = (int)(rng_f(&srng) * 0.7f * n);
        Biquad cb = bq_bandpass(M->cf * (0.6f + 0.8f * rng_f(&srng)), 3.0f);
        f32 ca = 0.25f + 0.5f * rng_f(&srng);
        for (int i = 0; i < (int)(SR * 0.02f) && st + i < n; i++) s[st + i] += bq(&cb, nz()) * expf(-(f32)i / SR * 220) * ca * am;
    }
    finish(id, s, n, step ? 0.7f : 0.9f);
}

// ============================================================================ the creature bank
#define VC(id, ...) gen_vocal((id), (Vocal){__VA_ARGS__})
static void synth_creatures(void) {
    //            dur   f0 (start, mid, end)  vib       formant1    formant2     f3    breath  am       jit  att   rel   gain
    // --- 0 generic creature (no idle voice)
    VC(SND_MOB(0, 1), 0.30f, {330, 400, 280}, 7, 0.05f, {700, 800}, {1500, 1700}, 3000, 0.15f, 0, 0, 0.04f, 0.01f, 0.10f, 0.9f);
    VC(SND_MOB(0, 2), 0.70f, {320, 260, 140}, 6, 0.06f, {650, 500}, {1400, 1100}, 3000, 0.20f, 0, 0, 0.05f, 0.02f, 0.30f, 0.9f);
    // --- 1 zombie: wet, low groans
    VC(SND_MOB(1, 0), 1.10f, {95, 82, 70}, 5, 0.05f, {450, 380}, {900, 750}, 2500, 0.25f, 28, 0.35f, 0.04f, 0.12f, 0.30f, 0.8f);
    VC(SND_MOB(1, 1), 0.40f, {150, 135, 100}, 8, 0.05f, {600, 500}, {1100, 900}, 2500, 0.30f, 35, 0.30f, 0.05f, 0.02f, 0.15f, 0.9f);
    VC(SND_MOB(1, 2), 1.30f, {120, 82, 45}, 4, 0.08f, {520, 350}, {950, 700}, 2500, 0.35f, 25, 0.40f, 0.06f, 0.05f, 0.50f, 0.9f);
    // --- 2 skeleton: rattling bones
    gen_clicks(SND_MOB(2, 0), 0.55f, 10, 1600, 0.7f);
    gen_clicks(SND_MOB(2, 1), 0.28f, 6, 1300, 0.85f);
    gen_clicks(SND_MOB(2, 2), 1.0f, 18, 1100, 0.85f);
    // --- 3 creeper: silent idle, sharp hiss-pops
    gen_hiss(SND_MOB(3, 1), 0.32f, 3200, 1800, 1.0f, 0, 0, 0.005f, 0.2f, 0.8f);
    gen_hiss(SND_MOB(3, 2), 0.70f, 2400, 900, 0.9f, 0, 0, 0.005f, 0.5f, 0.8f);
    // --- 4 spider: raspy hiss and chitter
    gen_hiss(SND_MOB(4, 0), 0.50f, 3400, 2600, 1.6f, 34, 0.7f, 0.04f, 0.2f, 0.65f);
    gen_hiss(SND_MOB(4, 1), 0.38f, 4200, 2200, 1.4f, 42, 0.8f, 0.01f, 0.2f, 0.8f);
    gen_hiss(SND_MOB(4, 2), 0.90f, 3600, 1200, 1.2f, 28, 0.8f, 0.02f, 0.6f, 0.8f);
    // --- 5 cow: a long, round "moo"
    VC(SND_MOB(5, 0), 1.30f, {112, 128, 96}, 4.5f, 0.03f, {330, 520}, {700, 950}, 2500, 0.06f, 0, 0, 0.01f, 0.12f, 0.35f, 0.9f);
    VC(SND_MOB(5, 1), 0.45f, {150, 165, 118}, 6, 0.04f, {450, 580}, {900, 1050}, 2500, 0.10f, 0, 0, 0.02f, 0.02f, 0.15f, 0.9f);
    VC(SND_MOB(5, 2), 0.95f, {140, 118, 66}, 5, 0.05f, {420, 360}, {850, 700}, 2500, 0.12f, 0, 0, 0.03f, 0.03f, 0.40f, 0.9f);
    // --- 6 pig: snorting oink and a high squeal
    VC(SND_MOB(6, 0), 0.34f, {210, 175, 140}, 0, 0, {520, 420}, {1450, 1100}, 2600, 0.35f, 15, 0.55f, 0.10f, 0.01f, 0.10f, 0.9f);
    VC(SND_MOB(6, 1), 0.55f, {720, 980, 660}, 9, 0.05f, {900, 1100}, {2200, 2600}, 3500, 0.15f, 0, 0, 0.03f, 0.01f, 0.15f, 0.9f);
    VC(SND_MOB(6, 2), 0.95f, {850, 620, 250}, 6, 0.07f, {800, 600}, {2100, 1500}, 3500, 0.18f, 0, 0, 0.04f, 0.01f, 0.40f, 0.9f);
    // --- 7 sheep: a trembling bleat
    VC(SND_MOB(7, 0), 0.90f, {330, 310, 270}, 0, 0, {850, 950}, {1500, 1700}, 2800, 0.08f, 24, 0.55f, 0.03f, 0.04f, 0.20f, 0.8f);
    VC(SND_MOB(7, 1), 0.40f, {400, 340, 280}, 0, 0, {900, 1000}, {1600, 1800}, 2800, 0.10f, 26, 0.50f, 0.04f, 0.02f, 0.15f, 0.85f);
    VC(SND_MOB(7, 2), 0.85f, {330, 260, 150}, 0, 0, {800, 650}, {1500, 1200}, 2800, 0.15f, 24, 0.55f, 0.05f, 0.02f, 0.35f, 0.85f);
    // --- 8 chicken: bawk and squawk
    VC(SND_MOB(8, 0), 0.40f, {650, 720, 520}, 0, 0, {1100, 900}, {2200, 1800}, 3200, 0.15f, 13, 0.85f, 0.06f, 0.005f, 0.10f, 0.9f);
    VC(SND_MOB(8, 1), 0.30f, {1000, 1350, 800}, 0, 0, {1500, 1300}, {2600, 2200}, 3500, 0.20f, 0, 0, 0.10f, 0.005f, 0.10f, 0.9f);
    VC(SND_MOB(8, 2), 0.55f, {900, 700, 380}, 0, 0, {1400, 1000}, {2400, 1700}, 3500, 0.25f, 18, 0.5f, 0.10f, 0.005f, 0.25f, 0.9f);
    // --- 9 horse: whinny, snort-squeal, falling cry
    VC(SND_MOB(9, 0), 1.40f, {380, 540, 300}, 9, 0.08f, {600, 820}, {1500, 2000}, 3200, 0.25f, 0, 0, 0.05f, 0.05f, 0.30f, 0.85f);
    VC(SND_MOB(9, 1), 0.70f, {600, 920, 520}, 11, 0.10f, {800, 1000}, {1700, 2200}, 3200, 0.20f, 0, 0, 0.06f, 0.01f, 0.20f, 0.9f);
    VC(SND_MOB(9, 2), 1.50f, {460, 360, 150}, 7, 0.10f, {700, 500}, {1500, 1100}, 3000, 0.25f, 0, 0, 0.06f, 0.02f, 0.50f, 0.9f);
    // --- 10 wolf: growl, yelp, whine
    VC(SND_MOB(10, 0), 0.90f, {85, 95, 75}, 0, 0, {450, 500}, {900, 1000}, 2500, 0.35f, 38, 0.55f, 0.08f, 0.08f, 0.30f, 0.9f);
    VC(SND_MOB(10, 1), 0.35f, {560, 960, 600}, 0, 0, {800, 1100}, {2000, 2600}, 3200, 0.10f, 0, 0, 0.03f, 0.01f, 0.10f, 0.9f);
    VC(SND_MOB(10, 2), 1.10f, {700, 860, 340}, 6, 0.04f, {700, 500}, {1800, 1400}, 3000, 0.10f, 0, 0, 0.03f, 0.10f, 0.40f, 0.85f);
    // --- 11 cat: "mi-aow", hissing yowl, dying mew
    VC(SND_MOB(11, 0), 0.70f, {480, 830, 540}, 5, 0.03f, {700, 550}, {2300, 2000}, 3200, 0.12f, 0, 0, 0.02f, 0.04f, 0.20f, 0.9f);
    VC(SND_MOB(11, 1), 0.50f, {700, 1000, 780}, 12, 0.06f, {900, 800}, {2500, 2200}, 3500, 0.45f, 0, 0, 0.10f, 0.01f, 0.15f, 0.9f);
    VC(SND_MOB(11, 2), 0.90f, {620, 520, 250}, 6, 0.05f, {750, 500}, {2200, 1600}, 3200, 0.15f, 0, 0, 0.04f, 0.02f, 0.35f, 0.9f);
    // --- 12 bird: chirps
    gen_chirp(SND_MOB(12, 0), 0.50f, 2100, 3300, 3, 0.8f);
    gen_chirp(SND_MOB(12, 1), 0.22f, 3400, 1800, 2, 0.9f);
    gen_chirp(SND_MOB(12, 2), 0.60f, 3000, 900, 1, 0.9f);
    // --- 13 squeak (bats, armadillos, axolotls): thin, very high
    gen_chirp(SND_MOB(13, 0), 0.30f, 4200, 5200, 3, 0.7f);
    gen_chirp(SND_MOB(13, 1), 0.18f, 5400, 3000, 2, 0.8f);
    gen_chirp(SND_MOB(13, 2), 0.40f, 4800, 1800, 1, 0.8f);
    // --- 14 grunt (camels, sniffers): low, breathy
    VC(SND_MOB(14, 0), 0.80f, {72, 86, 66}, 0, 0, {350, 400}, {750, 800}, 2200, 0.50f, 20, 0.40f, 0.08f, 0.10f, 0.30f, 0.9f);
    VC(SND_MOB(14, 1), 0.40f, {120, 105, 80}, 0, 0, {480, 420}, {950, 850}, 2300, 0.40f, 25, 0.35f, 0.06f, 0.02f, 0.15f, 0.9f);
    VC(SND_MOB(14, 2), 1.10f, {100, 75, 45}, 0, 0, {420, 320}, {850, 650}, 2200, 0.45f, 20, 0.45f, 0.07f, 0.04f, 0.50f, 0.9f);
    // --- 15 squish (slimes, squids, cubes)
    gen_bubble(SND_MOB(15, 0), 0.40f, 260, 700, 3, 260, 0.8f);
    gen_bubble(SND_MOB(15, 1), 0.30f, 400, 900, 4, 320, 0.9f);
    gen_bubble(SND_MOB(15, 2), 0.70f, 200, 600, 7, 220, 0.9f);
    // --- 16 flop (fish): wet slaps
    gen_thump(SND_MOB(16, 0), 0.20f, 150, 1400, 24, 0.8f);
    gen_thump(SND_MOB(16, 1), 0.16f, 220, 1800, 32, 0.8f);
    gen_thump(SND_MOB(16, 2), 0.28f, 110, 1100, 18, 0.8f);
    // --- 17 croak (frogs): resonant ribbit
    VC(SND_MOB(17, 0), 0.50f, {100, 92, 84}, 0, 0, {420, 360}, {950, 800}, 2400, 0.10f, 22, 0.90f, 0.06f, 0.01f, 0.10f, 0.9f);
    VC(SND_MOB(17, 1), 0.30f, {140, 120, 95}, 0, 0, {500, 420}, {1100, 900}, 2400, 0.15f, 24, 0.85f, 0.06f, 0.01f, 0.10f, 0.9f);
    VC(SND_MOB(17, 2), 0.70f, {110, 85, 55}, 0, 0, {440, 340}, {900, 700}, 2400, 0.15f, 20, 0.85f, 0.06f, 0.01f, 0.30f, 0.9f);
    // --- 18 goat: rough, bleating "meh"
    VC(SND_MOB(18, 0), 0.90f, {270, 320, 255}, 0, 0, {900, 1000}, {1600, 1800}, 3000, 0.12f, 30, 0.50f, 0.12f, 0.03f, 0.15f, 0.85f);
    VC(SND_MOB(18, 1), 0.40f, {350, 440, 310}, 0, 0, {950, 1100}, {1700, 1900}, 3000, 0.15f, 32, 0.50f, 0.12f, 0.02f, 0.12f, 0.9f);
    VC(SND_MOB(18, 2), 0.90f, {300, 240, 130}, 0, 0, {850, 650}, {1500, 1200}, 3000, 0.20f, 28, 0.55f, 0.14f, 0.02f, 0.35f, 0.9f);
    // --- 19 villager: nasal "hrmm", "hurh", "hrrh"
    VC(SND_MOB(19, 0), 0.55f, {125, 170, 140}, 0, 0, {400, 450}, {1600, 1800}, 2900, 0.08f, 0, 0, 0.03f, 0.05f, 0.15f, 0.85f);
    VC(SND_MOB(19, 1), 0.35f, {175, 150, 118}, 0, 0, {500, 450}, {1500, 1300}, 2900, 0.15f, 0, 0, 0.04f, 0.02f, 0.12f, 0.9f);
    VC(SND_MOB(19, 2), 0.85f, {160, 118, 70}, 0, 0, {480, 360}, {1400, 1000}, 2900, 0.20f, 0, 0, 0.05f, 0.03f, 0.40f, 0.9f);
    // --- 20 ender: eerie sweeps
    gen_ender(SND_MOB(20, 0), 0.90f, 320, 520, 55, 0.02f, 0.8f);
    gen_ender(SND_MOB(20, 1), 0.55f, 900, 260, 90, 0.03f, 0.9f);
    gen_ender(SND_MOB(20, 2), 1.40f, 600, 90, 40, 0.04f, 0.9f);
    // --- 21 blaze: breathing flame
    gen_hiss(SND_MOB(21, 0), 0.90f, 700, 1500, 0.6f, 6, 0.5f, 0.2f, 0.4f, 0.7f);
    gen_hiss(SND_MOB(21, 1), 0.35f, 2500, 900, 0.8f, 0, 0, 0.005f, 0.2f, 0.85f);
    gen_hiss(SND_MOB(21, 2), 1.20f, 1800, 400, 0.6f, 9, 0.5f, 0.02f, 0.9f, 0.85f);
    // --- 22 ghast: plaintive wails
    VC(SND_MOB(22, 0), 1.60f, {480, 700, 520}, 6, 0.10f, {650, 900}, {2000, 2800}, 3600, 0.20f, 0, 0, 0.04f, 0.15f, 0.50f, 0.8f);
    VC(SND_MOB(22, 1), 0.80f, {900, 640, 520}, 8, 0.10f, {900, 700}, {2400, 2000}, 3600, 0.25f, 0, 0, 0.05f, 0.02f, 0.30f, 0.85f);
    VC(SND_MOB(22, 2), 1.80f, {650, 480, 160}, 8, 0.12f, {700, 450}, {2000, 1300}, 3600, 0.25f, 0, 0, 0.06f, 0.05f, 0.80f, 0.85f);
    // --- 23 guardian: deep underwater gurgle
    gen_bubble(SND_MOB(23, 0), 0.90f, 110, 320, 5, 160, 0.8f);
    gen_bubble(SND_MOB(23, 1), 0.45f, 160, 420, 4, 200, 0.9f);
    gen_bubble(SND_MOB(23, 2), 1.20f, 90, 260, 9, 130, 0.9f);
    // --- 24 metal (golems): clanks
    gen_metal(SND_MOB(24, 0), 0.50f, 300, 7, 0.55f);
    gen_metal(SND_MOB(24, 1), 0.60f, 420, 6, 0.9f);
    gen_metal(SND_MOB(24, 2), 1.10f, 260, 4, 0.95f);
    // --- 25 snow (snow golems): soft crunch
    gen_material(SND_MOB(25, 0), 7, false);
    gen_hiss(SND_MOB(25, 1), 0.25f, 4200, 2600, 0.7f, 0, 0, 0.005f, 0.2f, 0.7f);
    gen_hiss(SND_MOB(25, 2), 0.60f, 3200, 1400, 0.6f, 14, 0.7f, 0.01f, 0.45f, 0.75f);
    // --- 26 buzz (bees)
    gen_buzz(SND_MOB(26, 0), 0.90f, 225, 0.03f, 0.55f);
    gen_buzz(SND_MOB(26, 1), 0.45f, 330, 0.08f, 0.8f);
    gen_buzz(SND_MOB(26, 2), 0.70f, 260, 0.10f, 0.8f);
    // --- 27 wind (breezes)
    gen_hiss(SND_MOB(27, 0), 1.10f, 500, 1100, 1.8f, 3, 0.4f, 0.3f, 0.5f, 0.6f);
    gen_hiss(SND_MOB(27, 1), 0.45f, 1800, 700, 1.4f, 0, 0, 0.01f, 0.3f, 0.8f);
    gen_hiss(SND_MOB(27, 2), 1.00f, 1400, 300, 1.5f, 5, 0.5f, 0.03f, 0.7f, 0.8f);
    // --- 28 wood (creaking): stressed timber
    gen_creak(SND_MOB(28, 0), 0.90f, 70, 0.5f, 0.8f);
    gen_creak(SND_MOB(28, 1), 0.40f, 120, 0.4f, 0.85f);
    gen_creak(SND_MOB(28, 2), 1.20f, 90, -0.4f, 0.9f);
    // --- 29 phantom: rasping screech
    VC(SND_MOB(29, 0), 0.90f, {900, 1300, 750}, 20, 0.05f, {1500, 2500}, {2600, 3500}, 4000, 0.55f, 0, 0, 0.15f, 0.03f, 0.20f, 0.75f);
    VC(SND_MOB(29, 1), 0.45f, {1300, 1700, 1000}, 22, 0.06f, {1700, 2600}, {2800, 3600}, 4000, 0.50f, 0, 0, 0.15f, 0.01f, 0.15f, 0.8f);
    VC(SND_MOB(29, 2), 1.00f, {1100, 800, 300}, 18, 0.07f, {1500, 900}, {2600, 1600}, 4000, 0.55f, 0, 0, 0.18f, 0.01f, 0.50f, 0.8f);
    // --- 30 shulker: shell clack and hum
    gen_thump(SND_MOB(30, 0), 0.30f, 90, 700, 14, 0.6f);
    gen_metal(SND_MOB(30, 1), 0.30f, 520, 18, 0.8f);
    gen_thump(SND_MOB(30, 2), 0.60f, 70, 900, 8, 0.9f);
    // --- 31 vex: shrill cackle
    VC(SND_MOB(31, 0), 0.70f, {820, 1000, 850}, 14, 0.15f, {1200, 1500}, {2600, 3000}, 3800, 0.10f, 16, 0.90f, 0.05f, 0.02f, 0.10f, 0.75f);
    VC(SND_MOB(31, 1), 0.35f, {1100, 1500, 1050}, 12, 0.12f, {1500, 1800}, {2800, 3200}, 3800, 0.10f, 18, 0.85f, 0.05f, 0.01f, 0.10f, 0.8f);
    VC(SND_MOB(31, 2), 0.80f, {1000, 800, 450}, 14, 0.12f, {1300, 900}, {2600, 1800}, 3800, 0.15f, 14, 0.85f, 0.06f, 0.01f, 0.35f, 0.8f);
    // --- 32 warden: sub-bass growls
    VC(SND_MOB(32, 0), 1.80f, {55, 48, 40}, 0, 0, {250, 200}, {500, 420}, 1500, 0.50f, 14, 0.60f, 0.10f, 0.20f, 0.50f, 0.95f);
    VC(SND_MOB(32, 1), 0.80f, {80, 70, 52}, 0, 0, {320, 260}, {650, 520}, 1700, 0.50f, 18, 0.55f, 0.10f, 0.03f, 0.30f, 0.95f);
    VC(SND_MOB(32, 2), 2.20f, {65, 45, 28}, 0, 0, {260, 160}, {520, 330}, 1500, 0.55f, 12, 0.65f, 0.12f, 0.10f, 1.00f, 0.95f);
    // --- 33 dragon: roars
    VC(SND_MOB(33, 0), 1.60f, {70, 150, 60}, 0, 0, {400, 800}, {800, 1500}, 2500, 0.60f, 55, 0.45f, 0.12f, 0.08f, 0.40f, 0.95f);
    VC(SND_MOB(33, 1), 0.80f, {110, 200, 90}, 0, 0, {500, 900}, {1000, 1700}, 2600, 0.55f, 60, 0.40f, 0.12f, 0.02f, 0.30f, 0.95f);
    VC(SND_MOB(33, 2), 2.50f, {140, 90, 40}, 0, 0, {800, 350}, {1500, 700}, 2400, 0.60f, 45, 0.50f, 0.15f, 0.05f, 1.20f, 0.95f);
    // --- 34 wither: raspy moans
    VC(SND_MOB(34, 0), 1.50f, {300, 420, 240}, 3, 0.10f, {700, 500}, {1500, 1100}, 3000, 0.45f, 45, 0.50f, 0.15f, 0.05f, 0.30f, 0.85f);
    VC(SND_MOB(34, 1), 0.60f, {500, 700, 380}, 4, 0.10f, {900, 650}, {1800, 1400}, 3000, 0.45f, 50, 0.50f, 0.15f, 0.02f, 0.20f, 0.9f);
    VC(SND_MOB(34, 2), 2.00f, {420, 300, 80}, 3, 0.12f, {800, 400}, {1600, 900}, 3000, 0.50f, 40, 0.55f, 0.18f, 0.03f, 0.90f, 0.9f);
}

// ============================================================================ the player and the machinery
static void synth_world_sfx(void) {
    VC(SND_PLAYER_HURT, 0.30f, {150, 135, 105}, 0, 0, {620, 540}, {1050, 950}, 2500, 0.25f, 0, 0, 0.05f, 0.01f, 0.10f, 0.9f);     // "ugh"
    VC(SND_PLAYER_DEATH, 0.95f, {140, 100, 60}, 3, 0.05f, {700, 450}, {1100, 800}, 2500, 0.30f, 0, 0, 0.06f, 0.02f, 0.45f, 0.9f);  // "aaargh"
    gen_creak(SND_DOOR_OPEN, 0.34f, 150, 0.6f, 0.7f);
    gen_thump(SND_DOOR_CLOSE, 0.22f, 130, 1100, 28, 0.85f);
    gen_creak(SND_CHEST_OPEN, 0.42f, 95, 0.5f, 0.7f);
    gen_thump(SND_CHEST_CLOSE, 0.26f, 100, 900, 24, 0.85f);
    { int n; f32 *s = tmp_buf(0.12f, &n);   // lever / switch: a metallic double click
      for (int c = 0; c < 2; c++) {
          int st = c ? (int)(0.07f * SR) : 0; Biquad b = bq_bandpass(c ? 1100.0f : 2000.0f, 5.0f);
          for (int i = 0; i < (int)(0.03f * SR) && st + i < n; i++) s[st + i] += bq(&b, nz() + (i < 12 ? 1.5f : 0)) * expf(-(f32)i / SR * 110);
      }
      finish(SND_LEVER, s, n, 0.8f); }
    gen_thump(SND_PISTON_OUT, 0.30f, 55, 500, 12, 0.9f);
    gen_hiss(SND_PISTON_IN, 0.28f, 600, 250, 0.8f, 0, 0, 0.01f, 0.2f, 0.8f);
    gen_metal(SND_ANVIL_LAND, 0.90f, 230, 5, 0.95f);
    { int n; f32 *s = tmp_buf(0.09f, &n); Biquad b = bq_bandpass(3200, 4.0f);   // pressure plate
      for (int i = 0; i < n; i++) s[i] = bq(&b, nz() + (i < 8 ? 1.0f : 0)) * expf(-(f32)i / SR * 90) + sinf(2 * PI_F * 140 * (f32)i / SR) * expf(-(f32)i / SR * 60) * 0.5f;
      finish(SND_PLATE, s, n, 0.7f); }
}
