/* Release gameplay fixes only: no input tap, tracing queue or profiler. */
#define WIN32_LEAN_AND_MEAN
#define KWENGINE_USE_RUNTIME_RESOLVER
#include <windows.h>
#include <wchar.h>
#include <kwf/plugin.h>
#include <kwengine/native_status.h>
#include <kwengine/engine_address.h>
#include <kwgameplay/config.h>
#include <stdio.h>
#define KWGAMEPLAY_THISCALL __attribute__((thiscall))
#define KWGAMEPLAY_FASTCALL __attribute__((fastcall))

static const struct kwf_context *gameplay_context;
static struct kwgameplay_config gameplay_config;
static const LONG gameplay_render_flags;
static LONG gameplay_initialization_state;
static struct kwengine_native_status gameplay_hook_status;
#include <kwengine/detail/engine_client.inc>
#include <kwengine/detail/config_file.inc>
#include <kwengine/detail/native_reads.inc>
#define memcmp kwengine_memcmp
#include "hooks/selection_hooks.inc"
#undef memcmp
#include "death_scope.inc"

static uint32_t KWF_CALL prepare(const struct kwf_context *context)
{
    struct kwengine_resolution resolution;
    if (context->role != KWF_ROLE_RENDERED)
        return KWF_UNSUPPORTED;
    gameplay_context = context;
    if (!kwengine_config_load(context, L"\\plugins\\kw_gameplay.ini", L"\\kwbot_host.ini", &gameplay_config, kwgameplay_config_parse))
        return KWF_ERROR;
    if (!kwengine_client_bind(context) || !kwengine_resolve((void *)context->game_image, context->game_image_size, &resolution))
        return KWF_UNSUPPORTED;
    if (gameplay_config.preserve_attack_move_on_loss) {
        const struct {
            uintptr_t table, method;
            uint32_t offset;
        } slots[] = {{0xaa4870, 0x7fbc25, 8}, {0xa88d88, 0x69f347, 0x1c}};
        for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
            uintptr_t slot = kwengine_va(slots[i].table) + slots[i].offset;
            uintptr_t method = kwengine_va(slots[i].method), actual = 0;
            if ((slot & 3U) || !kwengine_is_data(slot, 4) || !kwengine_is_code(method) || !read_process_raw((void *)slot, &actual, 4) ||
                actual != method)
                return KWF_UNSUPPORTED;
            uint32_t result = context->host->reserve(context->owner, slot, 4);
            if (result != KWF_OK)
                return result;
        }
        uint32_t result = gameplay_death_prepare(context);
        if (result != KWF_OK)
            return result;
    }
    gameplay_hook_status.host_image_base = (uint32_t)context->game_image;
    gameplay_hook_status.host_image_size = context->game_image_size;
    gameplay_hook_status.state = KWENGINE_NATIVE_STATE_PASSIVE_READY;
    gameplay_hook_status.result = KWENGINE_NATIVE_OK;
    return KWF_OK;
}
static uint32_t KWF_CALL activate(void)
{
    InterlockedExchange(&gameplay_initialization_state, 2);
    if (!gameplay_config.preserve_attack_move_on_loss)
        return KWF_OK;
    if (KwgameplayConfigureSelectionPreservation(1) != KWENGINE_NATIVE_OK)
        return KWF_ERROR;
    uint32_t result = gameplay_death_activate();
    if (result != KWF_OK)
        return result;
    gameplay_context->host->log(gameplay_context->owner, "Attack-Move preservation: native removal/death scopes; no command replay");
    return KWF_OK;
}
KWF_EXPORT const struct kwf_plugin *KWF_CALL KwPluginQuery(void)
{
    static const struct kwf_plugin plugin = {sizeof(plugin),  KWF_ABI_MAJOR, KWF_ABI_MINOR, KWF_ROLE_RENDERED,
                                             KWF_CAP_PATCHES, "gameplay",    prepare,       activate};
    return &plugin;
}
