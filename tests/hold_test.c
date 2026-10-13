/* SPDX-License-Identifier: GPL-3.0-only */
/* HOLD with the ARP off (the DRONE fork, seq.c hold_block): a key latches its note, the same key again lets it go,
 * key-ups do nothing; HOLD off / the ARP on / a panic let every latched note go; HOLD with the ARP on is the ARP's
 * latch as before. Through the keys (fm1_in.notes) and mix_block, as the device runs them.
 *   build/host/hold_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t gated(uint32_t note)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        if (inst.v[k].active && inst.v[k].gate && inst.v[k].note == note)
            n++;
    return n;
}
static void blocks(uint32_t keys, uint32_t nb)
{
    int32_t o[2 * CTL];
    fm1_in.notes = keys;
    while (nb--)
        mix_block(o, CTL);
}
static int bad;
static void check(const char *what, int ok)
{
    printf("hold: %-64s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(void)
{
    uint32_t i, c4, e4;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = ENGI_DRONE;
    eng_mem_claim(&inst);
    host_preset(&inst, ENGI_DRONE, 0);
    song.master_q12 = 4096;
    inst.p[P_AHOLD] = 1;
    blocks(0, 8);
    c4 = kb_map(&inst, 7);
    e4 = kb_map(&inst, 11);
    blocks(1u << 7, 4);
    blocks(0, 40);
    check("a key let go: its note sounds on (latched)", gated(c4) == 1u);
    blocks(1u << 11, 4);
    blocks(0, 40);
    check("a second key latches too", gated(c4) == 1u && gated(e4) == 1u);
    blocks(1u << 7, 4);
    check("the latched key again: its note lets go at once", gated(c4) == 0u && gated(e4) == 1u);
    blocks(0, 40);
    check("its key-up does nothing", gated(c4) == 0u && gated(e4) == 1u);
    blocks(1u << 7, 4);
    blocks(0, 4);
    inst.p[P_AHOLD] = 0;
    blocks(0, 4);
    check("HOLD OFF: every latched note lets go", gated(c4) == 0u && gated(e4) == 0u);
    blocks(1u << 7, 4);
    blocks(0, 4);
    check("HOLD OFF: a key plays as always (up = off)", gated(c4) == 0u);
    inst.p[P_AHOLD] = 1;
    blocks(1u << 7, 4);
    blocks(0, 4);
    inst.p[P_AMODE] = 1;
    blocks(0, 4);
    check("the ARP switched on: the latched note lets go", gated(c4) == 0u || inst.nheld == 0u);
    blocks(1u << 7, 4);
    blocks(0, 40);
    check("ARP on with HOLD: the ARP's own latch (the key stays in the chord)", inst.nheld == 1u);
    inst.p[P_AMODE] = 0;
    inst.p[P_AHOLD] = 1;
    blocks(0, 8);
    blocks(1u << 7, 4);
    blocks(0, 4);
    panic_req |= 1u;
    blocks(0, 4);
    blocks(1u << 7, 4);
    check("after a panic: the key latches anew (nothing stale)", gated(c4) == 1u);
    {   /* a MONO track (3OSC BASS: LEGATO, LOW priority): one note latched at a time; a higher key takes over */
        uint32_t g4 = kb_map(&inst, 14), k;
        for (k = 0; k < NVOICE; k++)
            inst.v[k].active = 0;
        for (i = 0; i < ENGINES[ENGI_DRONE]->npresets && !str_eq(ENGINES[ENGI_DRONE]->presets[i].name, "3OSC BASS"); i++)
            ;
        for (k = 0; k < P_E0; k++) inst.p[k] = TP[k].def;
        host_preset(&inst, ENGI_DRONE, i);
        inst.p[P_AHOLD] = 1;
        blocks(0, 8);
        blocks(1u << 7, 4);
        blocks(0, 40);
        blocks(1u << 14, 4);
        blocks(0, 40);
        check("MONO, LOW priority: a higher key takes over the latched note", inst.p[P_PRIO] == 1 && gated(g4) == 1u &&
              gated(c4) == 0u);
        blocks(1u << 14, 4);
        blocks(0, 40);
        check("MONO: the latched key again lets it go, nothing latched under it sounds", gated(g4) == 0u && gated(c4) == 0u);
    }
    {   /* hits are not latched: DRUM (LOFI DEEP) and SLICE in MODE ONE strike again on every press, HOLD or not */
        uint32_t e, k, r;
        for (r = 0; r < 2u; r++) {
            uint32_t n = 0, pi = 0;
            e = r ? 0u : ENGI_DRUM;
            if (r) {
                for (e = 0; e < NENGINES && !str_eq(ENGINES[e]->name, "SLICE"); e++)
                    ;
                if (e == NENGINES)
                    break;                          /* (FELUCCA_SLICE=0) */
                for (pi = 0; pi < ENGINES[e]->npresets && ENGINES[e]->presets[pi].e[4] != 0; pi++)
                    ;                               /* a preset in MODE ONE (CHOP) */
            } else
                pi = 1;                             /* LOFI DEEP */
            for (k = 0; k < NVOICE; k++)
                inst.v[k].active = 0;
            for (k = 0; k < P_E0; k++) inst.p[k] = TP[k].def;
            inst.eng_req = inst.engine = (uint8_t)e;
            eng_mem_claim(&inst);
            host_preset(&inst, e, pi);
            inst.p[P_AHOLD] = 1;
            blocks(0, 8);
            for (k = 0; k < 4u; k++) {                  /* four quick presses (~50 ms apart) */
                uint32_t a = vage;
                blocks(1u << 7, 4);
                n += vage != a;
                blocks(0, 64);
            }
            check(r ? "SLICE MODE ONE under HOLD: 4 quick presses of a key, 4 hits, nothing latched"
                    : "DRUM under HOLD: 4 quick presses of the kick, 4 hits, nothing latched",
                  n == 4u && !hold_has(&inst, kb_map(&inst, 7)));
        }
    }
    {   /* a latched note that has ended on its own (SUS 0, decayed to silence): its key strikes it again */
        uint32_t k, a, n;
        for (k = 0; k < NVOICE; k++)
            inst.v[k].active = 0;
        for (k = 0; k < P_E0; k++) inst.p[k] = TP[k].def;
        inst.eng_req = inst.engine = ENGI_DRONE;
        eng_mem_claim(&inst);
        host_preset(&inst, ENGI_DRONE, 0);
        inst.p[P_AHOLD] = 1;
        inst.p[P_ATK] = 0;
        inst.p[P_DEC] = 30;
        inst.p[P_SUS] = 0;
        blocks(0, 8);
        c4 = kb_map(&inst, 7);
        blocks(1u << 7, 4);
        blocks(0, 4);
        for (n = 0; n < 4000u && hold_sounding(&inst, c4); n++)
            blocks(0, 1);
        check("SUS 0: the latched note decays to silence, still latched", n < 4000u && hold_has(&inst, c4));
        a = vage;
        blocks(1u << 7, 4);
        check("SUS 0: its key again strikes it anew (not a silent let go), latched again",
              vage != a && gated(c4) == 1u && hold_has(&inst, c4) && hold_sounding(&inst, c4));
        blocks(0, 4);
        blocks(1u << 7, 4);
        check("SUS 0: pressed again while it sounds: it lets go", gated(c4) == 0u && !hold_has(&inst, c4));
        blocks(0, 4);
    }
    printf("hold: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
