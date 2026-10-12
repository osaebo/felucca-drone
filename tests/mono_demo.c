/* SPDX-License-Identifier: GPL-3.0-only */
/* The DRONE fork's monosynth presets (ANALOG 12.., DRONE 32..: the LP24 ladder, glide, keyboard tracking) playing a
 * phrase each: basses at C2, leads at C4, legato overlaps for the glide.
 *   build/host/mono_demo OUTDIR */
#define main hostsim_main
#include "hostsim.c"
#undef main

typedef struct { uint8_t key, at, len; } ev_t;            /* key (7 = C), start and length in 16ths */
static const ev_t BASS[] = {{7, 0, 2}, {7, 3, 1}, {19, 4, 1}, {7, 6, 2}, {10, 8, 2}, {12, 11, 1}, {14, 12, 2},
                            {19, 14, 2}, {7, 16, 2}, {7, 19, 1}, {19, 20, 1}, {17, 22, 2}, {14, 24, 3}, {12, 27, 1},
                            {10, 28, 2}, {12, 30, 2}, {7, 32, 6}};
static const ev_t LEAD[] = {{7, 0, 5}, {14, 4, 5}, {16, 8, 2}, {14, 10, 2}, {12, 12, 4}, {11, 15, 2}, {12, 16, 2},
                            {14, 18, 6}, {19, 23, 9}, {17, 31, 3}, {16, 33, 3}, {14, 35, 12}};
/* a lead (C4, the lead phrase) or a bass (C2): by its name */
static int is_lead(const char *nm)
{
    static const char *const W[] = {"LEAD", "FLUTE", "BRASS", "THICK"};
    uint32_t i, j, k;
    for (i = 0; i < NELEM(W); i++)
        for (j = 0; nm[j]; j++) {
            for (k = 0; W[i][k] && nm[j + k] == W[i][k]; k++)
                ;
            if (!W[i][k])
                return 1;
        }
    return 0;
}
static const ev_t *ph;
static uint32_t nph;
static uint32_t keys_at(uint32_t f)
{
    uint32_t s16 = f / (FS / 8u), n = 0, i;                 /* 120 BPM: a 16th = 1/8 s */
    for (i = 0; i < nph; i++)
        if (s16 >= ph[i].at && s16 < (uint32_t)ph[i].at + ph[i].len)
            n |= 1u << ph[i].key;
    return n;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/mono_demo";
    static const struct { uint8_t e, first; } SET[2] = {{0, 12}, {ENGI_DRONE, 32}};
    uint32_t s, k, i, f, n = 0;
    for (s = 0; s < 2u; s++)
        for (k = SET[s].first; k < ENGINES[SET[s].e]->npresets; k++) {
            const preset_t *pr = &ENGINES[SET[s].e]->presets[k];
            int lead = is_lead(pr->name);
            uint32_t frames = FS * (lead ? 7u : 6u);
            char path[256], nm[32];
            FILE *w;
            for (i = 0; pr->name[i] && i < 31u; i++)
                nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
            nm[i] = 0;
            snprintf(path, sizeof path, "%s/%02u_%s_%s.wav", dir, ++n, SET[s].e ? "DRONE" : "ANALOG", nm);
            w = fopen(path, "wb");
            if (!w) { perror(path); return 1; }
            wav_hdr(w, frames);
            for (i = 0; i < NVOICE; i++)
                inst.v[i].active = 0;
            for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
            for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
            inst.eng_req = inst.engine = SET[s].e;
            eng_mem_claim(&inst);
            host_preset(&inst, SET[s].e, k);
            song.octave = (int8_t)(lead ? 0 : -2);
            song.master_q12 = 4096;
            ph = lead ? LEAD : BASS;
            nph = lead ? NELEM(LEAD) : NELEM(BASS);
            for (f = 0; f < frames; f += CTL) {
                int32_t o[2 * CTL];
                fm1_in.notes = keys_at(f);
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++)
                    wav_put(w, o[2 * i], o[2 * i + 1]);
            }
            fclose(w);
            printf("%s\n", path);
        }
    return 0;
}
