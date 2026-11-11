/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KWGAMEPLAY_SELECTION_DEATH_LAYOUT_H
#define KWGAMEPLAY_SELECTION_DEATH_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Native Object::onDie module broadcast. The entire loop is matched uniquely
 * in executable sections; no executable address or protection thunk is used.
 * ESI is Object, EAX is DieInterface*, EBP+8 is DamageInfo*. */
static const unsigned char selection_death_loop[] = {0x8b, 0xbe, 0xa8, 0x02, 0,    0,    0xeb, 0x30, 0x8d, 0x48, 0x10, 0x8b, 0x01, 0xff, 0x50, 0x1c,
                                                     0x3b, 0xc3, 0x74, 0x09, 0xff, 0x75, 0x08, 0x8b, 0x10, 0x8b, 0xc8, 0xff, 0x12, 0x8b, 0x07, 0x8d,
                                                     0x48, 0x10, 0x8b, 0x01, 0xff, 0x50, 0x60, 0x3b, 0xc3, 0x74, 0x0a, 0x8b, 0x10, 0x8b, 0xc8, 0xff,
                                                     0x92, 0x98, 0x02, 0,    0,    0x83, 0xc7, 0x04, 0x8b, 0x07, 0x3b, 0xc3, 0x75, 0xca};
static inline uint32_t selection_death_word(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
static inline int selection_death_discover(const void *image, size_t size, uintptr_t *site)
{
    const unsigned char *p = image;
    *site = 0;
    if (!p || size < 0x100 || p[0] != 'M' || p[1] != 'Z')
        return 0;
    uint32_t nt = selection_death_word(p + 60);
    if (nt > size - 24 || memcmp(p + nt, "PE\0\0", 4) || p[nt + 4] != 0x4c || p[nt + 5] != 1)
        return 0;
    unsigned count = p[nt + 6] | (unsigned)p[nt + 7] << 8, optional = p[nt + 20] | (unsigned)p[nt + 21] << 8;
    if (!count || count > 96 || optional < 96 || optional > size - (size_t)nt - 24U)
        return 0;
    size_t sections = (size_t)nt + 24 + optional;
    if (sections > size || (size_t)count * 40 > size - sections || p[nt + 24] != 0x0b || p[nt + 25] != 1 ||
        selection_death_word(p + nt + 24 + 56) != size)
        return 0;
    uintptr_t found = 0;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char *s = p + sections + i * 40;
        uint32_t flags = selection_death_word(s + 36), at = selection_death_word(s + 12), n = selection_death_word(s + 8);
        if ((flags & 0x60000000U) != 0x60000000U)
            continue;
        if (at > size || n > size - at)
            return 0;
        if (n < sizeof(selection_death_loop))
            continue;
        for (size_t j = 0; j <= n - sizeof(selection_death_loop); ++j) {
            if (p[at + j] != selection_death_loop[0] || memcmp(p + at + j, selection_death_loop, sizeof(selection_death_loop)))
                continue;
            if (found)
                return 0;
            found = (uintptr_t)(p + at + j + 20);
        }
    }
    *site = found;
    return found != 0;
}
#endif
