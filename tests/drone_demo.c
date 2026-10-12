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
        render(w, frames, k == 4 || k == 6 || k == 12 ? keys_one : k == 5 || k == 3 || k == 10 ? keys_minor : keys_fifth);
        fclose(w);
        printf("%s\n", path);
    }
    {   /* CPU: 4 voices of the heaviest DRONE preset against 4 of ANALOG SAW LEAD, 10 s each */
        static const uint32_t ENG[2] = {ENGI_DRONE, 0};
        double ns[2];
        uint32_t r;
        for (r = 0; r < 2; r++) {
            clock_t c0;
            uint32_t f;
            int32_t o[2 * CTL];
            setup(ENG[r], r ? 0u : 6u, 0);
            inst.p[P_VOICE] = V_POLY;
            for (f = 0; f < 4u; f++)
                inst.p[P_DIST + f] = 0;                    /* dry: the engines alone */
            c0 = clock();
            for (f = 0; f < FS * 10u; f += CTL) {
                fm1_in.notes = r ? 0xFFu << 7 : (1u << 7) | (1u << 11) | (1u << 14) | (1u << 18);
                mix_block(o, CTL);
            }
            ns[r] = (double)(clock() - c0) * 1e9 / CLOCKS_PER_SEC / (FS * 10.0);
        }
        printf("cpu (dry): DRONE FURNACE 4 voices (its cap) %.1f ns / sample, ANALOG SAW LEAD 8 voices %.1f ns / sample (%.2f x)\n", ns[0], ns[1],
               ns[0] / ns[1]);
    }
    return 0;
}
