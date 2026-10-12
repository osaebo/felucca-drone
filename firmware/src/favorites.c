/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
#ifndef ENG_USER                 /* (a build without core.h's engine numbers: the user presets follow the engines) */
#define ENG_USER NENGINES
#endif
#ifndef ENGI_HOLE
#define ENGI_HOLE 0xFFu
#endif
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites __attribute__((section(".pool")));
static int favorite_has(uint32_t engine, uint32_t preset)
{
    if (engine == ENG_USER)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    return engine < NENGINES && engine != ENGI_HOLE && engine < 16u && preset < 256u &&
        ((favorites.factory[engine][preset / 8u] >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    if (engine >= NENGINES || (engine == ENG_USER ? preset >= UP_SLOTS : engine >= 16u || preset >= 256u))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == ENG_USER) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        uint8_t *b = &favorites.factory[engine][preset / 8u];
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
