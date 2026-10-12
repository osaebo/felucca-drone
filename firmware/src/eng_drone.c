/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca)
 * DRONE engine: a contribution to this fork, under the same licence */
/* DRONE: a slow, dark drone voice, after the drone machines (Elta's Solar 42 in mind): stacked oscillators that
 * beat against each other, a 12 dB filter that screams when driven, and everything wandering a little.
 *   MODE  the stack: 3 oscillators on an interval set, UNI OCT 5TH 4TH MIN (minor triad) TRI (tritone) CLUS (semitone
 *         cluster) DARK (an octave below, a minor second above) SUS4 MAJ7 O+5 (octave and twelfth) WIDE (twelfth up,
 *         octave down); or 6 drones at once, each on its own pitch and its own drift: SOL6 (root fifth octave twelfth
 *         two octaves, an octave below) MIN6 CLU6 (a cluster around the root and its octaves) VOI6 (tritones and
 *         minor seconds over two octaves down)
 *   DTN   the spread in cents: OSC 2 up, OSC 3 down (in a 6-stack the others between and beyond)
 *   WAVE  one knob through five oscillator types, crossfading: SAW (0) SQR (32) RING (64: each oscillator's triangle
 *         ring-modulated by the SUB's sine, an octave below: bell / metal) FOLD (96: a sine through a wavefolder, its
 *         gain rising to 5x towards 127) VOID (127: noise ring-modulated by each oscillator's sine: tuned wind).
 *         The shape routings (ENV / LFO SHP, the matrix's SHP) sweep it, +-64
 *   SUB   a sine an octave below OSC 1
 *   CUT RES  the filter (Simper's state-variable filter); its TYPE (EDIT > FILTER, as ANALOG's) LP, BP or HP
 *   DRFT  each oscillator on its own random walk (Ornstein-Uhlenbeck, ~3 s time constant, up to ~+-25 cents) and the
 *         cutoff on another (up to ~+-16 steps): the breathing of an old drone box, a different one per voice
 *   GRIT  drive into the filter (tanh, 1x .. 4x) and saturation of its band-pass state inside the loop: at high
 *         resonance the filter growls and screams instead of ringing clean (a 12 dB filter with distortion)
 * A preset sets more than the engine's values (DRONE_EXTRA, drone_preset_extra): the filter TYPE, the LFO (rate or
 * SYNC, wave, FREE running, its depth on the cutoff, the shape (WAVE) and the amplitude) and the matrix's first slot
 * (S&H or SLEW onto the cutoff ...), so its movement comes with it.
 * At most 4 voices a part (engine_t.poly). Per voice the oscillators' phases and walks are in the part's engine memory
 * (drone_vs_t, one per voice slot); voice_t keeps s[0] s[1] the filter, s[2] the random numbers, s[3] the SUB's phase,
 * s[4] the cutoff's walk */
static const char *const N_DRONE_MODE[] = {"UNI", "OCT", "5TH", "4TH", "MIN", "TRI", "CLUS", "DARK",
                                           "SUS4", "MAJ7", "O+5", "WIDE", "SOL6", "MIN6", "CLU6", "VOI6",
                                           "UNI-", "UNI+", "5+8"};
/* the stacks: oscillator count and the intervals above OSC 1 (semitones; OSC 1 is 0) */
static const struct { uint8_t n; int8_t iv[6]; } DRONE_STACK[] = {
    {3, {0, 0, 0}}, {3, {0, 12, -12}}, {3, {0, 7, -12}}, {3, {0, 5, -7}},
    {3, {0, 3, 7}}, {3, {0, 6, -12}}, {3, {0, 1, 2}}, {3, {0, -12, 1}},
    {3, {0, 5, 7}}, {3, {0, 4, 11}}, {3, {0, 12, 19}}, {3, {0, 19, -12}},
    {6, {0, 7, 12, 19, 24, -12}}, {6, {0, 3, 7, 12, 15, -12}}, {6, {0, 1, -1, 12, -12, 2}}, {6, {0, 6, -12, 1, -24, 13}},
    /* the classic three-oscillator monosynth: two in unison and one an octave down (or up), root fifth octave */
    {3, {0, 0, -12}}, {3, {0, 0, 12}}, {3, {0, 7, 12}},
};
_Static_assert(NELEM(DRONE_STACK) == NELEM(N_DRONE_MODE), "DRONE_STACK: one per MODE");
static const int8_t DRONE_DET[6] = {0, 2, -2, 1, -1, 3};     /* each oscillator's share of the spread, DTN / 4 cents */
#define DRONE_POLY 4

typedef struct {                        /* a voice slot's oscillators (the part's engine memory) */
    uint32_t ph[6];
    int32_t w[6];                       /* their walks (Q15) */
} drone_vs_t;
ENG_MEM_FITS("DRONE", sizeof(drone_vs_t) * NVOICE);
static drone_vs_t *drone_vs(const track_t *t, const voice_t *v)
{
    return (drone_vs_t *)ENG_MEM((uint32_t)(t - trk) % NPART) + (uint32_t)(v - t->v) % NVOICE;
}

static void drone_note_on(track_t *t, voice_t *v)
{
    drone_vs_t *o = drone_vs(t, v);
    uint32_t k;
    if (!o->ph[1] && !o->ph[2])         /* a fresh slot: the oscillators start apart (no click of edges at once); */
        for (k = 1; k < 6u; k++)        /* after that they run on, as the walks do */
            o->ph[k] = o->ph[0] + k * 0x2AAAAAABu + (uint32_t)v->age * 0x9E3779B9u;
    v->s[0] = v->s[1] = 0;              /* filter (kept over a retrigger: engine_t.keep) */
    if (!v->s[2])
        v->s[2] = 0x2F6B3A1 + (int32_t)v->age * 7919;   /* the random numbers, never 0 */
    v->s[3] = 0;
    v->s[4] = 0;                        /* the cutoff's walk starts where CUT is set */
    v->s[5] = v->s[6] = v->s[7] = 0;    /* LP24: its stages 3, 4 and last output */
}

/* one step of an Ornstein-Uhlenbeck walk (Q15, std ~8000): leak 1/4096 a block, a uniform step of +-256 */
static inline int32_t drone_walk(int32_t d, int32_t *nst)
{
    int32_t r = (int32_t)(noise32(nst) >> 16) - 32768;
    d += (r >> 7) - (d >> 12);
    return clamp(d, -32767, 32767);
}

/* a triangle wavefolder: x (any size up to +-2 periods) reflected into +-32767 */
static inline int32_t drone_fold(int32_t x)
{
    int32_t u = x + 32767;
    while (u < 0)
        u += 131068;
    while (u >= 131068)
        u -= 131068;
    return u < 65534 ? u - 32767 : 98301 - u;
}

/* oscillator type ty (0 SAW 1 SQR 2 RING 3 FOLD 4 VOID) at phase ph: sb the SUB's sine, nz a noise sample, fg the
 * fold's gain (Q12) */
static inline int32_t drone_type(uint32_t ty, uint32_t ph, uint32_t inc, int32_t sb, int32_t nz, int32_t fg)
{
    switch (ty) {
    case 0:
        return osc_saw(ph, inc);
    case 1:
        return (osc_saw(ph, inc) - osc_saw(ph + 0x80000000u, inc)) >> 1;
    case 2:
        return (mulq15(osc_tri(ph), sb) * 3) >> 1;
    case 3:
        return drone_fold((osc_sine(ph) * fg) >> 12);
    default: {
        int32_t s = osc_sine(ph);
        return (mulq15(s, nz) * 3 + s) >> 2;
    }
    }
}

static int32_t drone_wave(const track_t *t, const vmod_t *m)       /* WAVE with the SHP routings, 0..127 */
{
    return clamp(t->p[P_E2] + ((m->shape - (64 << 8)) >> 8), 0, 127);
}
/* the render with the WAVE's zone ta a constant (drone_render: one copy per zone, so the types fold into straight
 * code per oscillator: no switch per sample) */
static inline __attribute__((always_inline)) void drone_body(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                             const vmod_t *m, const uint32_t ta)
{
    const int16_t *p = t->p;
    drone_vs_t *o = drone_vs(t, v);
    uint32_t mode = (uint32_t)p[P_E0] % NELEM(DRONE_STACK), ft = (uint32_t)p[P_FTYPE], no = DRONE_STACK[mode].n, i, k;
    int32_t spread = p[P_E1], sub = p[P_E3] * 200, drift = p[P_E6], grit = p[P_E7];
    int32_t wave = drone_wave(t, m);                                  /* SHP routings sweep it */
    const uint32_t tb = ta + 1u;                                             /* the two types it crossfades */
    int32_t f = (wave & 31) << 10, fg = ta < 3u ? 8192 : 8192 + ((f * 12288) >> 15);   /* FOLD: 2x, to 5x */
    int32_t nst = v->s[2], wf = v->s[4];
    uint32_t ph3 = (uint32_t)v->s[3], inc[6], ph[6];
    int32_t ic1 = v->s[0], ic2 = v->s[1];
    int32_t drv = 32768 + grit * 768;                                 /* 1x .. 4x */
    int32_t lim = 60000 - grit * 300, lg = (32767 << 12) / lim;      /* the band state's tanh: +-60000 .. +-21900 */
    int32_t gain = no > 3u ? 5 : 10;                                  /* the sum of 3 or 6 to ~+-30700 (/32) */
    int32_t cut, kd;
    tsvf_t flt;
    lad_t lc = {0, 0, 4096};
    int32_t z[5] = {v->s[0], v->s[1], v->s[5], v->s[6], v->s[7]};   /* LP24's state (the SVF's: s[0] s[1]) */
    for (k = 0; k < no; k++) {                                        /* pitches, walks: once a block */
        int32_t fine = m->fine;
        if (drift) {
            o->w[k] = drone_walk(o->w[k], &nst);
            fine += k ? (o->w[k] * drift) >> 14 : (o->w[k] * drift) >> 16;   /* OSC 1 a third as deep; up to
                                                                               * ~+-60 / 4096 (~25 ct) at DRFT 127 */
        }
        inc[k] = cents_inc(m->pitch16 + DRONE_STACK[mode].iv[k] * 16, (spread * DRONE_DET[k]) >> 2, fine);
        ph[k] = o->ph[k];
    }
    if (drift)
        wf = drone_walk(wf, &nst);
    cut = (p[P_E4] << 8) + m->cutoff + ((wf * drift) >> 9);          /* the cutoff's walk: ~+-8 steps (std) */
    kd = 8192 - p[P_E5] * 7600 / 127;                                 /* tsvf_coef's damping, Q12 */
    tsvf_coef_k(&flt, cut, kd);
    if (ft == 3u)                                                     /* LP24: the ladder (dsp.c lad_tick) */
        lad_coef(&lc, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t sb = osc_sine(ph3), nz = ta >= 3u ? (int32_t)(noise32(&nst) >> 17) - 16384 : 0;
        int32_t s = 0, v1, v2, v3, y;
        nz <<= 1;                                                     /* +-32767 */
        for (k = 0; k < no; k++) {
            int32_t a = drone_type(ta, ph[k], inc[k], sb, nz, fg);
            if (f)
                a += mulq15(drone_type(tb, ph[k], inc[k], sb, nz, fg) - a, f);
            s += a;
            ph[k] += inc[k];
        }
        ph3 += inc[0] >> 1;
        s = (s * gain) >> 5;                                          /* ~+-30700 */
        if (sub)
            s += mulq15(sb, sub);                                     /* +-25400 at SUB 127 */
        if (grit)
            s = softclip(((s >> 2) * (drv >> 2)) >> 11);              /* as ANALOG's DRV: no overflow */
        else
            s = softclip(s);                                          /* tanh: the SUB's peaks round off */
        s >>= 1;
        if (ft == 3u) {                                               /* LP24: GRIT drives the ladder's tanh */
            s = soft_knee(lad_tick(&lc, s, z), 16000) << 1;
            out[i] += voice_amp(s, m, i) << 1;
            continue;
        }
        v3 = s - ic2;                                                 /* the SVF, its taps (eng_analog.c analog_tap) */
        v1 = (flt.a1 * ic1 + flt.a2 * v3) >> 13;
        v2 = ic2 + ((flt.a2 * ic1 + flt.a3 * v3) >> 13);
        ic1 = clamp(2 * v1 - ic1, -150000, 150000);
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        if (grit)                                                     /* the band state through a tanh: the growl */
            ic1 = (softclip((ic1 * lg) >> 12) * lim) >> 15;
        y = !ft ? v2 : ft == 1u ? v1 : s - ((kd * v1) >> 12) - v2;
        s = soft_knee(y, 16000) << 1;
        out[i] += voice_amp(s, m, i) << 1;
    }
    for (k = 0; k < no; k++)
        o->ph[k] = ph[k];
    if (ft == 3u) {
        ic1 = z[0];
        ic2 = z[1];
        v->s[5] = z[2];
        v->s[6] = z[3];
        v->s[7] = z[4];
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
    v->s[3] = (int32_t)ph3;
    v->s[4] = wf;
}
static __attribute__((noinline)) void drone_z0(track_t *t, voice_t *v, int32_t *o, uint32_t n, const vmod_t *m)
{
    drone_body(t, v, o, n, m, 0u);
}
static __attribute__((noinline)) void drone_z1(track_t *t, voice_t *v, int32_t *o, uint32_t n, const vmod_t *m)
{
    drone_body(t, v, o, n, m, 1u);
}
static __attribute__((noinline)) void drone_z2(track_t *t, voice_t *v, int32_t *o, uint32_t n, const vmod_t *m)
{
    drone_body(t, v, o, n, m, 2u);
}
static __attribute__((noinline)) void drone_z3(track_t *t, voice_t *v, int32_t *o, uint32_t n, const vmod_t *m)
{
    drone_body(t, v, o, n, m, 3u);
}
static void drone_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    switch ((uint32_t)drone_wave(t, m) >> 5) {
    case 0: drone_z0(t, v, out, n, m); break;
    case 1: drone_z1(t, v, out, n, m); break;
    case 2: drone_z2(t, v, out, n, m); break;
    default: drone_z3(t, v, out, n, m); break;
    }
}

/* name, {MODE, DTN, WAVE, SUB, CUT, RES, DRFT, GRIT}, {A D S R}, fenv, mono, FX(dist, chorus, delay, reverb), PAT */
#define DP(nm, mo, dt, wv, sb, ct, rs, df, gr, a, r, d_, c_, l_, v_, pt) \
    {nm, {mo, dt, wv, sb, ct, rs, df, gr}, {a, 127, 127, r}, 0, 0, FX(d_, c_, l_, v_), PAT(pt)}
/* a monosynth preset: its own A D S R, the filter envelope's amount, MONO */
#define DPM(nm, mo, dt, wv, sb, ct, rs, df, gr, a, d, su, r, fe, d_, c_, l_, v_, pt) \
    {nm, {mo, dt, wv, sb, ct, rs, df, gr}, {a, d, su, r}, fe, 1, FX(d_, c_, l_, v_), PAT(pt)}
static const preset_t DRONE_PRESETS[] = {
    DP("ABYSS", 7, 14, 0, 110, 38, 40, 60, 10, 110, 115, 0, 20, 25, 110, 8),
    DP("MONOLITH", 1, 10, 10, 90, 46, 55, 40, 30, 95, 110, 10, 10, 20, 95, 8),
    DP("CATACOMB", 2, 18, 5, 70, 42, 70, 70, 20, 105, 118, 0, 30, 35, 120, 5),
    DP("TRITONE", 5, 8, 16, 80, 50, 60, 50, 25, 100, 112, 5, 20, 30, 105, 5),
    DP("SWARM", 6, 40, 0, 40, 58, 35, 100, 40, 90, 105, 15, 40, 25, 90, 5),
    DP("DIRGE", 4, 12, 8, 60, 44, 50, 45, 15, 108, 116, 0, 35, 30, 115, 5),
    DP("FURNACE", 1, 24, 22, 100, 54, 85, 55, 90, 80, 100, 35, 0, 20, 80, 8),
    DP("VOID HUM", 0, 6, 0, 127, 30, 20, 30, 5, 100, 118, 0, 15, 15, 100, 8),
    DP("SOLAR WIND", 1, 20, 31, 30, 62, 92, 80, 60, 90, 110, 0, 30, 45, 110, 5),
    DP("RED GIANT", 2, 16, 31, 90, 40, 75, 50, 70, 85, 105, 20, 10, 30, 95, 8),
    DP("ORBIT", 3, 30, 25, 50, 70, 100, 90, 45, 95, 112, 0, 45, 55, 115, 5),
    DP("COSMODRONE", 0, 34, 31, 100, 36, 60, 75, 35, 100, 118, 0, 50, 30, 120, 8),
    DP("SCREAMER", 6, 12, 28, 20, 66, 115, 60, 110, 70, 95, 25, 0, 40, 90, 5),
    DP("DEEP FIELD", 7, 24, 16, 127, 28, 30, 90, 20, 115, 118, 0, 25, 40, 127, 8),
    /* 1.5.1-dr2: the 6-drone stacks, RING / FOLD / VOID, movement in every preset */
    DP("SOL SIX", 12, 22, 20, 60, 48, 55, 60, 25, 100, 116, 0, 35, 30, 115, 5),
    DP("IRON CHOIR", 13, 16, 70, 40, 55, 50, 50, 20, 100, 114, 0, 40, 30, 110, 5),
    DP("BLACK SUN", 15, 18, 32, 110, 36, 60, 70, 40, 108, 118, 10, 20, 30, 120, 8),
    DP("HIVE", 14, 30, 8, 30, 62, 40, 110, 30, 90, 108, 10, 40, 25, 100, 5),
    DP("BELL TOMB", 2, 10, 64, 60, 60, 40, 40, 10, 95, 116, 0, 20, 40, 120, 7),
    DP("FOLDED", 1, 12, 92, 70, 64, 45, 40, 20, 90, 110, 0, 25, 30, 100, 8),
    DP("STATIC SEA", 0, 8, 122, 40, 70, 30, 60, 10, 100, 118, 0, 30, 20, 115, 5),
    DP("AURORA", 10, 20, 48, 30, 72, 35, 70, 15, 105, 118, 0, 60, 45, 120, 5),
    DP("HALO", 9, 14, 12, 50, 58, 30, 50, 10, 100, 116, 0, 50, 35, 115, 5),
    DP("SUS VAULT", 8, 16, 20, 70, 46, 45, 55, 20, 100, 114, 0, 30, 30, 110, 5),
    DP("WIDE ARC", 11, 18, 24, 80, 50, 50, 60, 25, 100, 114, 0, 30, 35, 110, 8),
    DP("RUST", 6, 20, 100, 50, 58, 80, 70, 80, 85, 105, 30, 10, 25, 90, 5),
    DP("PULSAR", 2, 10, 40, 60, 62, 70, 40, 30, 60, 100, 0, 20, 45, 100, 8),
    DP("ICE CAVE", 3, 8, 80, 20, 46, 75, 50, 15, 100, 116, 0, 40, 50, 120, 7),
    DP("GHOST", 0, 4, 112, 60, 52, 30, 90, 5, 105, 118, 0, 30, 40, 120, 5),
    DP("MAGMA", 7, 30, 96, 127, 40, 70, 60, 100, 90, 110, 30, 0, 20, 90, 8),
    DP("DESERT", 4, 10, 36, 50, 56, 60, 50, 20, 100, 114, 0, 30, 40, 110, 5),
    DP("NEUTRON", 13, 40, 30, 90, 34, 95, 80, 60, 95, 112, 10, 20, 30, 100, 8),
    /* the classic three-oscillator monosynth through the LP24 ladder (DRONE_EXTRA: LP24, glide, LOW priority, KEY ->
     * CUT as keyboard tracking); the amp envelope's decay to a lower sustain shapes the filter too (fenv) */
    DPM("3OSC BASS", 16, 12, 0, 0, 30, 45, 10, 35, 0, 62, 95, 30, 55, 10, 0, 10, 15, 2),
    DPM("BIG MONO", 1, 16, 8, 40, 36, 55, 12, 50, 0, 70, 90, 35, 45, 15, 0, 15, 20, 8),
    DPM("SOLO LEAD", 17, 10, 0, 0, 58, 50, 8, 40, 2, 80, 110, 50, 30, 0, 15, 35, 30, 4),
    DPM("FIFTH LEAD", 18, 8, 20, 0, 54, 55, 10, 45, 4, 80, 105, 50, 35, 0, 15, 35, 30, 4),
    DPM("PEDAL BASS", 16, 8, 16, 80, 26, 30, 8, 20, 10, 90, 100, 60, 25, 0, 0, 10, 25, 8),
    DPM("SQUELCH", 0, 6, 32, 0, 28, 100, 6, 40, 0, 58, 75, 25, 63, 10, 0, 20, 15, 1),
    DPM("STAB BRASS", 0, 18, 0, 0, 40, 30, 10, 30, 28, 80, 100, 45, 45, 0, 20, 20, 35, 6),
    DPM("THICK SQR", 10, 10, 32, 30, 50, 40, 10, 35, 2, 75, 100, 45, 35, 0, 15, 30, 30, 4),
};
#undef DP
#undef DPM

/* what a preset sets beyond the engine's values (ui.c apply_preset_to, after the defaults; drone_preset_extra):
 * the filter TYPE (0 LP 1 BP 2 HP); the LFO: RATE, WAVE (0 SIN 1 TRI 2 SAW 3 SQR 4 S&H), SYNC (0 OFF, 1 4BAR,
 * 2 2BAR ...), FREE running, its depth on the cutoff (FLT), the WAVE (SHP) and the level (AMP); the matrix's first
 * slot: SRC (1 LFO, 5 RAND, 9 S&H, 10 SLEW), DST (1 PITCH 2 CUT 3 SHP 5 PAN 9 REV) and AMT */
typedef struct {
    uint8_t ft, rate, wave, sync;
    int8_t flt, shp;
    uint8_t amp, msrc, mdst;
    int8_t mamt;
    uint8_t gl, prio;            /* glide (GLD), note priority (1 LOW) */
    int8_t pit;                  /* vibrato: the LFO on the pitch (PIT), faded in over FADE */
    uint8_t fade;
} drone_extra_t;
static const drone_extra_t DRONE_EXTRA[] = {
    {0, 8, 0, 0, 10, 0, 0, 0, 0, 0},            /* ABYSS: a slow sine on the cutoff */
    {0, 15, 1, 0, 8, 0, 0, 0, 0, 0},            /* MONOLITH */
    {0, 20, 0, 0, 0, 0, 0, 10, 2, 14},          /* CATACOMB: SLEW on the cutoff */
    {0, 5, 0, 0, 12, 4, 0, 0, 0, 0},            /* TRITONE */
    {0, 40, 0, 0, 0, 0, 0, 9, 2, 16},           /* SWARM: S&H on the cutoff */
    {0, 0, 0, 1, 14, 0, 0, 0, 0, 0},            /* DIRGE: 4 bars a sweep */
    {0, 25, 1, 0, 10, 6, 0, 0, 0, 0},           /* FURNACE */
    {0, 3, 0, 0, 6, 0, 0, 0, 0, 0},             /* VOID HUM */
    {1, 12, 0, 0, 18, 0, 0, 0, 0, 0},           /* SOLAR WIND: BP */
    {0, 10, 0, 0, 0, 4, 0, 10, 2, 12},          /* RED GIANT */
    {1, 18, 1, 0, 20, 0, 0, 1, 5, 30},          /* ORBIT: BP, the LFO pans it too */
    {0, 0, 0, 1, 12, 10, 0, 0, 0, 0},           /* COSMODRONE */
    {1, 30, 4, 0, 10, 0, 0, 9, 2, 10},          /* SCREAMER: BP, S&H */
    {0, 2, 0, 0, 8, 0, 0, 0, 0, 0},             /* DEEP FIELD */
    {0, 0, 0, 1, 14, 8, 0, 0, 0, 0},            /* SOL SIX */
    {0, 8, 1, 0, 0, 16, 0, 0, 0, 0},            /* IRON CHOIR: the LFO through RING */
    {0, 6, 0, 0, 0, 0, 0, 10, 2, 14},           /* BLACK SUN */
    {0, 50, 4, 0, 0, 0, 0, 9, 2, 12},           /* HIVE */
    {0, 6, 0, 0, 10, 0, 0, 0, 0, 0},            /* BELL TOMB */
    {0, 10, 1, 0, 0, 20, 0, 0, 0, 0},           /* FOLDED: the LFO sweeps the fold */
    {0, 4, 0, 0, 10, 0, 0, 10, 3, 10},          /* STATIC SEA */
    {0, 0, 0, 1, 0, 18, 0, 1, 5, 24},           /* AURORA */
    {0, 0, 0, 2, 8, 0, 0, 0, 0, 0},             /* HALO */
    {0, 6, 1, 0, 10, 0, 0, 0, 0, 0},            /* SUS VAULT */
    {0, 0, 0, 2, 16, 0, 0, 0, 0, 0},            /* WIDE ARC */
    {0, 0, 0, 0, 0, 0, 0, 9, 2, 10},            /* RUST */
    {0, 30, 3, 0, 0, 0, 50, 0, 0, 0},           /* PULSAR: a square on the level */
    {2, 10, 0, 0, 10, 0, 0, 0, 0, 0},           /* ICE CAVE: HP */
    {0, 0, 0, 0, 0, 0, 0, 10, 1, 4},            /* GHOST: SLEW on the pitch */
    {0, 5, 1, 0, 0, 14, 0, 0, 0, 0},            /* MAGMA */
    {0, 0, 0, 1, 12, 0, 0, 0, 0, 0},            /* DESERT */
    {1, 25, 4, 0, 0, 0, 0, 9, 2, 14},           /* NEUTRON: BP, S&H */
    /* the monosynth set: LP24, KEY -> CUT (keyboard tracking), glide, LOW priority, a late vibrato on the leads */
    {3, 0, 0, 0, 0, 0, 0, 4, 2, 24, 0, 1, 0, 0},       /* 3OSC BASS */
    {3, 0, 0, 0, 0, 0, 0, 4, 2, 24, 20, 1, 0, 0},      /* BIG MONO */
    {3, 86, 0, 0, 0, 0, 0, 4, 2, 40, 50, 1, 1, 95},    /* SOLO LEAD */
    {3, 86, 0, 0, 0, 0, 0, 4, 2, 40, 40, 1, 1, 95},    /* FIFTH LEAD */
    {3, 0, 0, 0, 0, 0, 0, 4, 2, 16, 0, 1, 0, 0},       /* PEDAL BASS */
    {3, 0, 0, 0, 0, 0, 0, 4, 2, 24, 30, 1, 0, 0},      /* SQUELCH */
    {3, 0, 0, 0, 0, 0, 0, 4, 2, 32, 10, 1, 0, 0},      /* STAB BRASS */
    {3, 86, 0, 0, 0, 0, 0, 4, 2, 32, 30, 1, 1, 90},    /* THICK SQR */
};
_Static_assert(NELEM(DRONE_EXTRA) == NELEM(DRONE_PRESETS), "DRONE_EXTRA: one per preset");

static void drone_preset_extra(track_t *t, uint32_t pi)
{
    const drone_extra_t *x = &DRONE_EXTRA[pi % NELEM(DRONE_EXTRA)];
    t->p[P_FTYPE] = x->ft;
    t->p[P_LRATE] = x->rate;
    t->p[P_LWAVE] = x->wave;
    t->p[P_LSYNC] = x->sync;
    t->p[P_LTRIG] = 1;                  /* FREE: one slow sweep across every note */
    t->p[P_LD_FLT] = x->flt;
    t->p[P_LD_SHP] = x->shp;
    t->p[P_LD_AMP] = x->amp;
    if (x->msrc) {
        t->p[P_M1SRC] = x->msrc;
        t->p[P_M1DST] = x->mdst;
        t->p[P_M1AMT] = x->mamt;
    }
    t->p[P_GLIDE] = x->gl;
    t->p[P_PRIO] = x->prio;
    if (x->pit) {                       /* vibrato (the LFO's rate is RATE) */
        t->p[P_LD_PIT] = x->pit;
        t->p[P_LFADE] = x->fade;
    }
}

static const engine_t ENG_DRONE = {
    .name = "DRONE",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"MODE", F_ENUM, 0, (int16_t)(NELEM(N_DRONE_MODE) - 1), 7, N_DRONE_MODE, 0},
        {"DTN", F_INT, 0, 127, 14, 0, "ct"},
        {"WAVE", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 90, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 44, 0, 0},
        {"RES", F_PCT, 0, 127, 50, 0, 0},
        {"DRFT", F_PCT, 0, 127, 50, 0, 0},
        {"GRIT", F_PCT, 0, 127, 15, 0, 0},
    },
    .presets = DRONE_PRESETS,
    .npresets = NELEM(DRONE_PRESETS),
    .note_on = drone_note_on,
    .render = drone_render,
    .knob = {P_E4, P_E5, P_E2, P_E6},     /* HOME: CUT RES WAVE DRFT */
    .poly = DRONE_POLY,
    .keep = 0xF3,                         /* the filter (LP24: s[5..7] too), the cutoff's walk over a retrigger */
    .preset = drone_preset_extra,
    .mem = sizeof(drone_vs_t) * NVOICE,
};

/* engine 14: a hole (core.h ENGI_HOLE): never offered (eng_ok), silent, no presets */
static void hole_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void hole_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_HOLE = {
    .name = "--",
    .page_title = {"-", "-"},
    .edit = {
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .note_on = hole_note_on,
    .render = hole_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
/* DRONE's favourites are bits of favorites.factory[15]'s first bytes: bytes 10, 11 and 14.. hold settings (ui.c) */
_Static_assert(NELEM(DRONE_PRESETS) <= 80, "DRONE's favourites would reach the settings in favorites.factory[15]");
