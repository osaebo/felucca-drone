/* SPDX-License-Identifier: GPL-3.0-only */
/* Re-hits (the DRONE fork): a DRUM lane struck again while it rings restarts its drum (as loud as the first hit, HOLD
 * on or off) without a click: eng_drum.c drum_bleed fades the sample the hit before stopped at over ~3 ms. Every kit,
 * its kick and a tom, struck twice 90 ms apart (the kick still near full): the re-hit's high-frequency energy (the
 * second difference, RMS) stays within twice the first hit's own onset (the old cut: 2.5 .. 20 times).
 *   build/host/rehit_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t W[FS * 2];
static int bad;

static double hf(uint32_t a, uint32_t b)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < b; i++) {
        double d = (double)W[i] - 2.0 * W[i - 1] + W[i - 2];
        e += d * d;
    }
    return sqrt(e / (double)(b - a));
}
static double peak(uint32_t a, uint32_t b)
{
    double m = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        if (fabs((double)W[i]) > m)
            m = fabs((double)W[i]);
    return m;
}

/* kit pi, key k struck at 0.1 s and again gap ms later (30 ms each), HOLD h; the left channel into W */
static void strike(uint32_t pi, uint32_t key, uint32_t gap, int h, uint32_t *t2)
{
    uint32_t i, f, t1 = FS / 10, hl = FS * 30u / 1000u;
    int32_t o[2 * CTL];
    *t2 = t1 + FS * gap / 1000u;
    for (i = 0; i < NVOICE; i++)
        inst.v[i].active = 0;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = ENGI_DRUM;
    eng_mem_claim(&inst);
    host_preset(&inst, ENGI_DRUM, pi);
    for (i = 0; i < 4u; i++)
        inst.p[P_DIST + i] = 0;                         /* dry: the sends' tails are not the hit */
    inst.p[P_AHOLD] = (int16_t)h;
    song.master_q12 = 4096;
    fm1_in.notes = 0;
    for (f = 0; f < 64u; f++)
        mix_block(o, CTL);
    for (f = 0; f < FS; f += CTL) {
        fm1_in.notes = (f >= t1 && f < t1 + hl) || (f >= *t2 && f < *t2 + hl) ? 1u << key : 0u;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            W[f + i] = o[2 * i];
    }
}

int main(void)
{
    const engine_t *e = ENGINES[ENGI_DRUM];
    static const uint32_t KEYS[2] = {7, 12};             /* C: the kick, F: a tom */
    uint32_t pi, k, h;
    for (pi = 0; pi < e->npresets; pi++)
        for (k = 0; k < 2u; k++) {
            uint32_t t1 = FS / 10, t2, on = t1;
            double h1, h2, p1, p2;
            strike(pi, KEYS[k], 90, 0, &t2);
            while (on < t1 + FS / 10 && abs(W[on]) < 50)
                on++;
            h1 = hf(on - 5, on + 220);
            h2 = hf(t2 - 40, t2 + 260);
            p1 = peak(t1, t1 + FS / 50);
            p2 = peak(t2, t2 + FS / 50);
            printf("rehit: %-10s %-4s hit %7.0f  re-hit %7.0f   HF onset %6.0f  re-hit %6.0f  %s\n", e->presets[pi].name,
                   k ? "TOM" : "KICK", p1, p2, h1, h2, p2 > 0.7 * p1 && h2 <= 2.0 * h1 + 4.0 ? "ok" : "FAIL");
            bad += !(p2 > 0.7 * p1 && h2 <= 2.0 * h1 + 4.0);
        }
    for (h = 0; h < 2u; h++) {                           /* LOFI DEEP's kick, quick re-hits, HOLD off and on */
        uint32_t gaps[3] = {60, 150, 400}, g;
        for (g = 0; g < 3u; g++) {
            uint32_t t2;
            double p1, p2;
            strike(1, 7, gaps[g], (int)h, &t2);
            p1 = peak(FS / 10, FS / 10 + FS / 50);
            p2 = peak(t2, t2 + FS / 50);
            printf("rehit: LOFI DEEP kick, HOLD %s, again after %3u ms: %7.0f / %7.0f  %s\n", h ? "ON " : "OFF", gaps[g],
                   p2, p1, p2 > 0.9 * p1 ? "ok" : "FAIL");
            bad += !(p2 > 0.9 * p1);
        }
    }
    printf("rehit: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
