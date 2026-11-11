/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Public gameplay lifecycle extension. Prepare only resolves/reserves; code
 * publication is brokered and rolled back by the framework on activation error. */
#include "hooks/selection_death_layout.h"
static uintptr_t gameplay_death_site;
static unsigned char gameplay_death_original[9];
static uint32_t gameplay_death_prepare(const struct kwf_context *context)
{
    if (!selection_death_discover((void *)context->game_image, context->game_image_size, &gameplay_death_site) ||
        !kwengine_code_span(gameplay_death_site, 9) || !read_process_raw((void *)gameplay_death_site, gameplay_death_original, 9))
        return KWF_UNSUPPORTED;
    /* Reserve read dependencies too: later plugins cannot rewrite the native
     * broadcast loop around our exact instruction span. */
    uint32_t result = context->host->reserve(context->owner, gameplay_death_site - 20, 20);
    if (result != KWF_OK)
        return result;
    result = context->host->reserve(context->owner, gameplay_death_site + 9, (uint32_t)sizeof(selection_death_loop) - 29);
    if (result != KWF_OK)
        return result;
    return context->host->reserve(context->owner, gameplay_death_site, 9);
}
static uint32_t gameplay_death_activate(void)
{
    uintptr_t current = 0;
    if (!selection_death_discover((void *)gameplay_context->game_image, gameplay_context->game_image_size, &current) ||
        current != gameplay_death_site)
        return KWF_CONFLICT;
    unsigned char patch[9];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0xe8;
    uint32_t rel = (uint32_t)(uintptr_t)selection_preservation_death_bridge - (uint32_t)(current + 5);
    memcpy(patch + 1, &rel, 4);
    return gameplay_context->host->write(gameplay_context->owner, current, gameplay_death_original, patch, 9);
}
