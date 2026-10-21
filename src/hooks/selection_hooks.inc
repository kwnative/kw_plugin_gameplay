/* Only functional selection-removal/destruction hooks; no diagnostic callbacks. */
#include <kwgameplay/selection_preservation_api.h>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

enum {
    SP_KEEP = 1,
    SP_DISABLED,
    SP_HEADLESS,
    SP_NO_TLS,
    SP_NO_SCOPE,
    SP_SCOPE_MISMATCH,
    SP_MATCH_CONTEXT,
    SP_OWNER,
    SP_COMMAND,
    SP_SLOT,
    SP_MODE,
    SP_ACTIVE,
    SP_SELECTION,
    SP_EMPTY,
    SP_HEAD,
    SP_BOUNDARIES
};
typedef void(KWGAMEPLAY_THISCALL *selection_preservation_remove_fn)(void *, void *, void *);
static selection_preservation_remove_fn selection_preservation_original_remove;
static int selection_preservation_remove_installed;
struct selection_preservation_patch {
    void **slot;
    void *original, *hook;
};

static int selection_preservation_read(void *context, uint32_t address, void *output, size_t size)
{
    (void)context;
    return address >= 0x10000U && (uint64_t)address + size <= UINT64_C(0x100000000) &&
           read_process_value((const void *)(uintptr_t)address, output, size);
}
static int selection_preservation_read_u32(uintptr_t address, uint32_t *output)
{
    return address <= UINT32_MAX && selection_preservation_read(NULL, (uint32_t)address, output, 4);
}
static uint32_t selection_preservation_object(uint32_t drawable, uint32_t *object_address)
{
    uint32_t object = 0, back = 0, id = 0, vtable = 0;
    if (!selection_preservation_read_u32(drawable, &vtable) || !kwengine_is_data(vtable, 4) ||
        !selection_preservation_read_u32((uintptr_t)drawable + 0x140, &object))
        return 0;
    if (selection_preservation_read_u32((uintptr_t)object + 0x108, &back) && back == drawable &&
        selection_preservation_read_u32((uintptr_t)object + 0xec, &id)) {
        *object_address = object;
        return id;
    }
    return 0;
}
static int selection_preservation_publish(struct selection_preservation_patch *patches, size_t count)
{
    MemoryBarrier();
    for (size_t i = 0; i < count; ++i) {
        struct selection_preservation_patch *patch = &patches[i];
        void *actual = NULL;
        if (kwengine_is_data((uintptr_t)patch->slot, 4) && read_process_raw(patch->slot, &actual, 4) && actual == patch->original &&
            gameplay_context->host->write(gameplay_context->owner, (uintptr_t)patch->slot, &patch->original, &patch->hook, 4) == KWF_OK)
            continue;
        while (i) {
            patch = &patches[--i];
            if (gameplay_context->host->restore(gameplay_context->owner, (uintptr_t)patch->slot, 4) != KWF_OK)
                gameplay_context->host->log(gameplay_context->owner, "selection-preservation rollback failed");
        }
        return 0;
    }
    return 1;
}
static int selection_preservation_install_remove(void);
#include "selection_preservation.inc"

static void KWGAMEPLAY_FASTCALL selection_preservation_on_remove(void *self, void *unused, void *selection, void *drawable)
{
    const DWORD saved = GetLastError();
    (void)unused;
    const int preserve = selection_preservation_remove((unsigned char *)self - 0x14, selection, drawable);
    SetLastError(saved);
    if (!preserve)
        selection_preservation_original_remove(self, selection, drawable);
}
static int selection_preservation_install_remove(void)
{
    if (selection_preservation_remove_installed)
        return 1;
    const uintptr_t observer = kwengine_va(0xaa4870U), original = kwengine_va(0x7fbc25U);
    unsigned char code[15];
    if (!observer || !original || !kwengine_is_data(observer, 12) || !kwengine_is_code(original) ||
        !read_process_value((void *)original, code, sizeof(code)) || memcmp(code, "\x8b\x49\x14\x85\xc9\x74\x05\xe8", 8) ||
        memcmp(code + 12, "\xc2\x08\x00", 3))
        return 0;
    struct selection_preservation_patch patch;
    void(KWGAMEPLAY_FASTCALL * hook)(void *, void *, void *, void *) = selection_preservation_on_remove;
    patch.slot = (void **)(observer + 8);
    patch.original = (void *)original;
    memcpy(&patch.hook, &hook, sizeof(hook));
    memcpy(&selection_preservation_original_remove, &original, sizeof(original));
    selection_preservation_remove_installed = selection_preservation_publish(&patch, 1);
    return selection_preservation_remove_installed;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
