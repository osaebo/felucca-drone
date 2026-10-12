/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca)
 * DRONE engine: a contribution to this fork, under the same licence */
/* DRONE: a slow, dark drone voice, after the drone machines (Elta's Solar 42 in mind): stacked oscillators that
 * beat against each other, a 12 dB filter that screams when driven, and everything wandering a little.
 *   MODE  three oscillators stacked on an interval set: UNI (unison), OCT, 5TH, 4TH, MIN (minor triad), TRI
 *         (tritone), CLUS (a semitone cluster), DARK (an octave below and a minor second above)
 *   DTN   their spread: OSC 2 up, OSC 3 down, in cents (the slow beating); the SHP routings widen it
 *   WAVE  each oscillator from a saw (0) to a square (127): a saw minus a half-cycle-shifted saw, both band-limited
 *   SUB   a sine an octave below OSC 1
 *   CUT RES  the filter (Simper's state-variable filter). Its TYPE (EDIT > FILTER, as ANALOG's): LP, BP or HP; a
 *         preset sets it (DRONE_FTYPE, ui.c apply_preset_to)
 *   DRFT  each oscillator on its own random walk (Ornstein-Uhlenbeck, ~3 s time constant, up to ~+-25 cents) and the
 *         cutoff on another (up to ~+-16 steps): the breathing of an old drone box, a different one per voice
 *   GRIT  drive into the filter (tanh, 1x .. 4x) and saturation of its band-pass state inside the loop: at high
 *         resonance the filter growls and screams instead of ringing clean (a 12 dB filter with distortion)
 * At most 4 voices a part (engine_t.poly): drones are a few long notes; three oscillators (six saws at WAVE > 0) cost
 * about as much as ANALOG's two-oscillator voice twice over.
 * Voice state: ph[0..2] the oscillators, s[0] s[1] the filter, s[2] noise (the walks' random numbers), s[3] the sub's
 * phase, s[4] s[5] the walks of OSC 2 and OSC 3, s[6] the cutoff's walk, s[7] OSC 1's walk (a third as deep) */
static const char *const N_DRONE_MODE[] = {"UNI", "OCT", "5TH", "4TH", "MIN", "TRI", "CLUS", "DARK"};
/* the intervals of OSC 2 and OSC 3 above OSC 1, semitones */
static const int8_t DRONE_IV[8][2] = {
    {0, 0}, {12, -12}, {7, -12}, {5, -7}, {3, 7}, {6, -12}, {1, 2}, {-12, 1},
};
#define DRONE_POLY 4

static void drone_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[1] = v->ph[0] + 0x55555555u;    /* the three start apart: no click of three edges at once */
    v->ph[2] = v->ph[0] + 0xAAAAAAAAu;
    v->s[0] = v->s[1] = 0;                /* filter */
    if (!v->s[2])
        v->s[2] = 0x2F6B3A1 + (int32_t)v->age * 7919;   /* the walks' random numbers, never 0 */
    v->s[3] = 0;
    v->s[4] = v->s[5] = v->s[6] = v->s[7] = 0;          /* the walks start in tune, the cutoff where it is set */
}

/* one step of an Ornstein-Uhlenbeck walk (Q15, std ~8000): leak 1/4096 a block, a uniform step of +-256 */
static inline int32_t drone_walk(int32_t d, int32_t *nst)
{
    int32_t r = (int32_t)(noise32(nst) >> 16) - 32768;
    d += (r >> 7) - (d >> 12);
    return clamp(d, -32767, 32767);
}

/* an oscillator: the saw, minus wv (Q15) of the saw half a cycle on (wv 32767: a square, twice the saw's swing) */
static inline int32_t drone_osc(uint32_t ph, uint32_t inc, int32_t wv)
{
    int32_t a = osc_saw(ph, inc);
    return wv ? a - mulq15(osc_saw(ph + 0x80000000u, inc), wv) : a;
}

static void drone_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t mode = (uint32_t)p[P_E0] & 7u, ft = (uint32_t)p[P_FTYPE], i;
    int32_t spread = p[P_E1] + ((m->shape - (64 << 8)) >> 8);      /* SHP routings widen / narrow it */
    int32_t wv = p[P_E2] * 258, sub = p[P_E3] * 200, drift = p[P_E6], grit = p[P_E7];
    int32_t nst = v->s[2];
    int32_t w1 = v->s[7], w2 = v->s[4], w3 = v->s[5], wf = v->s[6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], ph3 = (uint32_t)v->s[3];
    int32_t ic1 = v->s[0], ic2 = v->s[1];
    int32_t drv = 32768 + grit * 768;                                 /* 1x .. 4x */
    int32_t lim = 60000 - grit * 300, lg = (32767 << 12) / lim;      /* the band state's tanh: +-60000 .. +-21900 */
    int32_t comp = (int32_t)((32767u << 15) / (uint32_t)(32767 + wv));   /* the square's double swing, Q15 */
    int32_t cut, k, f1, f2, f3;
    uint32_t inc1, inc2, inc3;
    tsvf_t flt;
    spread = clamp(spread, 0, 127);
    if (drift) {                                                      /* the walks: once a block */
        w1 = drone_walk(w1, &nst);
        w2 = drone_walk(w2, &nst);
        w3 = drone_walk(w3, &nst);
        wf = drone_walk(wf, &nst);
    }
    f1 = m->fine + ((w1 * drift) >> 16);                              /* 1/4096: OSC 1 a third as deep */
    f2 = m->fine + ((w2 * drift) >> 14);                              /* up to ~+-60 (~25 ct) at DRFT 127 */
    f3 = m->fine + ((w3 * drift) >> 14);
    inc1 = cents_inc(m->pitch16, 0, f1);
    inc2 = cents_inc(m->pitch16 + DRONE_IV[mode][0] * 16, spread >> 1, f2);
    inc3 = cents_inc(m->pitch16 + DRONE_IV[mode][1] * 16, -(spread >> 1), f3);
    cut = (p[P_E4] << 8) + m->cutoff + ((wf * drift) >> 9);          /* the cutoff's walk: ~+-8 steps (std) */
    k = 8192 - p[P_E5] * 7600 / 127;                                  /* tsvf_coef's damping, Q12 */
    tsvf_coef_k(&flt, cut, k);
    for (i = 0; i < n; i++) {
        int32_t s = drone_osc(ph0, inc1, wv) + drone_osc(ph1, inc2, wv) + drone_osc(ph2, inc3, wv);  /* +-196602 */
        int32_t v1, v2, v3, y;
        ph0 += inc1;
        ph1 += inc2;
        ph2 += inc3;
        s = (((s >> 2) * comp) >> 13) * 5 >> 4;                       /* ~+-30700 whatever the WAVE */
        if (sub) {
            ph3 += inc1 >> 1;
            s += mulq15(osc_sine(ph3), sub);                          /* +-25400 at SUB 127 */
        }
        if (grit)
            s = softclip(((s >> 2) * (drv >> 2)) >> 11);              /* as ANALOG's DRV: no overflow */
        else
            s = softclip(s);                                          /* tanh: the SUB's peaks round off */
        s >>= 1;
        v3 = s - ic2;                                                 /* the SVF, its taps (eng_analog.c analog_tap) */
        v1 = (flt.a1 * ic1 + flt.a2 * v3) >> 13;
        v2 = ic2 + ((flt.a2 * ic1 + flt.a3 * v3) >> 13);
        ic1 = clamp(2 * v1 - ic1, -150000, 150000);
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        if (grit)                                                     /* the band state through a tanh: the growl */
            ic1 = (softclip((ic1 * lg) >> 12) * lim) >> 15;
        y = !ft ? v2 : ft == 1u ? v1 : s - ((k * v1) >> 12) - v2;
        s = soft_knee(y, 16000) << 1;
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
    v->s[3] = (int32_t)ph3;
    v->s[4] = w2;
    v->s[5] = w3;
    v->s[6] = wf;
    v->s[7] = w1;
}

static const preset_t DRONE_PRESETS[] = {
    /* name, {MODE, DTN, WAVE, SUB, CUT, RES, DRFT, GRIT}, {A D S R}, fenv, mono, FX(dist, chorus, delay, reverb) */
    {"ABYSS", {7, 14, 0, 110, 38, 40, 60, 10}, {110, 127, 127, 115}, 0, 0, FX(0, 20, 25, 110), PAT(8)},
    {"MONOLITH", {1, 10, 40, 90, 46, 55, 40, 30}, {95, 127, 127, 110}, 0, 0, FX(10, 10, 20, 95), PAT(8)},
    {"CATACOMB", {2, 18, 20, 70, 42, 70, 70, 20}, {105, 127, 127, 118}, 0, 0, FX(0, 30, 35, 120), PAT(5)},
    {"TRITONE", {5, 8, 64, 80, 50, 60, 50, 25}, {100, 127, 127, 112}, 0, 0, FX(5, 20, 30, 105), PAT(5)},
    {"SWARM", {6, 40, 0, 40, 58, 35, 100, 40}, {90, 127, 127, 105}, 0, 0, FX(15, 40, 25, 90), PAT(5)},
    {"DIRGE", {4, 12, 30, 60, 44, 50, 45, 15}, {108, 127, 127, 116}, 0, 0, FX(0, 35, 30, 115), PAT(5)},
    {"FURNACE", {1, 24, 90, 100, 54, 85, 55, 90}, {80, 127, 127, 100}, 0, 0, FX(35, 0, 20, 80), PAT(8)},
    {"VOID HUM", {0, 6, 0, 127, 30, 20, 30, 5}, {100, 127, 127, 120}, 0, 0, FX(0, 15, 15, 100), PAT(8)},
    /* square stacks and a band-pass that screams: the drone-machine side (DRONE_FTYPE: BP where marked) */
    {"SOLAR WIND", {1, 20, 127, 30, 62, 92, 80, 60}, {90, 127, 127, 110}, 0, 0, FX(0, 30, 45, 110), PAT(5)},   /* BP */
    {"RED GIANT", {2, 16, 127, 90, 40, 75, 50, 70}, {85, 127, 127, 105}, 0, 0, FX(20, 10, 30, 95), PAT(8)},
    {"ORBIT", {3, 30, 100, 50, 70, 100, 90, 45}, {95, 127, 127, 112}, 0, 0, FX(0, 45, 55, 115), PAT(5)},       /* BP */
    {"COSMODRONE", {0, 34, 127, 100, 36, 60, 75, 35}, {100, 127, 127, 118}, 0, 0, FX(0, 50, 30, 120), PAT(8)},
    {"SCREAMER", {6, 12, 110, 20, 66, 115, 60, 110}, {70, 127, 127, 95}, 0, 0, FX(25, 0, 40, 90), PAT(5)},      /* BP */
    {"DEEP FIELD", {7, 24, 64, 127, 28, 30, 90, 20}, {115, 127, 127, 118}, 0, 0, FX(0, 25, 40, 127), PAT(8)},
};
/* each preset's filter TYPE (P_FTYPE: 0 LP, 1 BP, 2 HP), set as it loads (ui.c apply_preset_to) */
static const uint8_t DRONE_FTYPE[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0};
_Static_assert(NELEM(DRONE_FTYPE) == NELEM(DRONE_PRESETS), "DRONE_FTYPE: one per preset");

static const engine_t ENG_DRONE = {
    .name = "DRONE",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"MODE", F_ENUM, 0, 7, 7, N_DRONE_MODE, 0},
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
    .knob = {P_E4, P_E5, P_E1, P_E6},     /* HOME: CUT RES DTN DRFT */
    .poly = DRONE_POLY,
    .keep = 0x03,                         /* the filter over a retrigger */
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
