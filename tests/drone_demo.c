/* SPDX-License-Identifier: GPL-3.0-only */
/* DRONE demos: every DRONE preset (its sends and voice mode, PRESET as hostsim) held as a low drone, a second
 * note joining, then released into the reverb; plus a CPU comparison against ANALOG.
 *   build/host/drone_demo OUTDIR */
#define main hostsim_main
#include "hostsim.c"
#undef main

static void render(FILE *w, uint32_t frames, uint32_t (*keys)(uint32_t f))
{
    uint32_t f, i;
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        fm1_in.notes = keys(f);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(w, o[2 * i], o[2 * i + 1]);
    }
}

/* key 7 = C at the octave song.octave; C held from 0.05 s, G (key 14) joins at 7 s, all off at 15 s */
static uint32_t keys_fifth(uint32_t f)
{
    uint32_t n = 0;
    if (f > FS / 20 && f < FS * 15) n |= 1u << 7;
    if (f > FS * 7 && f < FS * 15) n |= 1u << 14;
    return n;
}
/* C, then Eb (key 10) */
static uint32_t keys_minor(uint32_t f)
{
    uint32_t n = 0;
    if (f > FS / 20 && f < FS * 15) n |= 1u << 7;
    if (f > FS * 7 && f < FS * 15) n |= 1u << 10;
    return n;
}
static uint32_t keys_one(uint32_t f) { return f > FS / 20 && f < FS * 15 ? 1u << 7 : 0u; }

static void setup(uint32_t e, uint32_t pi, int oct)
{
    uint32_t i, k;
    for (k = 0; k < NVOICE; k++)
        inst.v[k].active = 0;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = (uint8_t)e;
    eng_mem_claim(&inst);
    host_preset(&inst, e, pi);
    song.octave = (int8_t)oct;
    song.master_q12 = 4096;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/drone_demo";
    const engine_t *e = ENGINES[ENGI_DRONE];
    char path[256];
    uint32_t k;
    for (k = 0; k < e->npresets; k++) {
        uint32_t frames = FS * 21u;
        FILE *w;
        char nm[32];
        uint32_t j;
        for (j = 0; e->presets[k].name[j] && j < 31; j++)
            nm[j] = e->presets[k].name[j] == ' ' ? '_' : e->presets[k].name[j];
        nm[j] = 0;
        snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, k + 1, nm);
        w = fopen(path, "wb");
        if (!w) { perror(path); return 1; }
        wav_hdr(w, frames);
        setup(ENGI_DRONE, k, -2);
        {
            uint32_t mo = (uint32_t)e->presets[k].e[0];
            render(w, frames, mo >= 12u || mo == 6u || k == 12u ? keys_one : mo == 4u || mo == 5u ? keys_minor : keys_fifth);
        }
        fclose(w);
        printf("%s\n", path);
    }
    {   /* CPU (dry): every DRONE preset with 4 notes held (its cap), against ANALOG SAW LEAD with 8 */
        double worst = 0, an;
        uint32_t wk = 0, r;
        for (r = 0; r <= e->npresets; r++) {
            clock_t c0;
            uint32_t f;
            int32_t o[2 * CTL];
            double ns;
            setup(r < e->npresets ? ENGI_DRONE : 0u, r < e->npresets ? r : 0u, 0);
            inst.p[P_VOICE] = V_POLY;
            for (f = 0; f < 4u; f++)
                inst.p[P_DIST + f] = 0;
            inst.p[P_ATK] = 0;                             /* sounding at once: the cost of the whole voice */
            fm1_in.notes = 0;
            for (f = 0; f < 64u; f++)                      /* keys up first: the next keys are new notes */
                mix_block(o, CTL);
            c0 = clock();
            for (f = 0; f < FS * 4u; f += CTL) {
                fm1_in.notes = r < e->npresets ? (1u << 7) | (1u << 11) | (1u << 14) | (1u << 18) : 0xFFu << 7;
                mix_block(o, CTL);
            }
            ns = (double)(clock() - c0) * 1e9 / CLOCKS_PER_SEC / (FS * 4.0);
            if (r < e->npresets && ns > worst) { worst = ns; wk = r; }
            if (getenv("CPUALL")) printf("  %-12s %.1f\n", r < e->npresets ? e->presets[r].name : "ANALOG", ns);
            if (r == e->npresets) an = ns;
        }
        printf("cpu (dry): heaviest DRONE preset %s, 4 voices: %.1f ns / sample; ANALOG SAW LEAD 8 voices %.1f (%.2f x)\n",
               e->presets[wk].name, worst, an, worst / an);
    }
    return 0;
}
