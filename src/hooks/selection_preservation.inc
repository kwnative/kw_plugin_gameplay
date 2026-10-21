/* Functional Attack-Move preservation only. No tracing, subscriber callbacks,
 * simulation/selection writes, command replay or diagnostic work. */
static volatile LONG selection_preservation_enabled, selection_preservation_tls_failed;
static DWORD selection_preservation_tls = TLS_OUT_OF_INDEXES;
static int selection_preservation_installed;
struct selection_preservation_scope {
    void *object;
    uint32_t drawable;
    struct selection_preservation_scope *previous;
};
typedef void *(KWGAMEPLAY_THISCALL *selection_preservation_destroy_fn)(void *, uint32_t);
static selection_preservation_destroy_fn selection_preservation_original_destroy;

/* Match identity is independent of the diagnostic/AI selection-vector bound.
 * Do not call kwengine_selection_read here: its 128-ID export limit is not a game
 * or input limit. Keep the same active/local/non-defeated context checks. */
struct selection_preservation_context {
    uint32_t logic, players, local, frame, player;
};
static int selection_preservation_context_once(struct selection_preservation_context *c)
{
    uint32_t vt = 0, count = 0, indexed = 0;
    uint8_t active = 0, defeated = 0;
    memset(c, 0, sizeof(*c));
    return selection_preservation_read_u32(kwengine_va(0xbe1008U), &c->logic) && selection_preservation_read_u32(c->logic, &vt) &&
           vt == kwengine_va(0xa99244U) && selection_preservation_read(NULL, c->logic + 0x3cU, &active, 1) && active == 1 &&
           selection_preservation_read_u32((uintptr_t)c->logic + 0x38, &c->frame) && c->frame &&
           selection_preservation_read_u32(kwengine_va(0xbe0b80U), &c->players) && selection_preservation_read_u32(c->players, &vt) &&
           vt == kwengine_va(0xa21e44U) && selection_preservation_read_u32((uintptr_t)c->players + 0x20, &count) && count && count <= 26 &&
           selection_preservation_read_u32((uintptr_t)c->players + 0x1c, &c->local) && selection_preservation_read_u32(c->local, &vt) &&
           vt == kwengine_va(0xa237ccU) && selection_preservation_read_u32((uintptr_t)c->local + 0x48, &c->player) && c->player < count &&
           selection_preservation_read_u32((uintptr_t)c->players + 0x24 + 4 * c->player, &indexed) && indexed == c->local &&
           selection_preservation_read(NULL, c->local + 0x238U, &defeated, 1) && !defeated;
}
static int selection_preservation_match(void)
{
    struct selection_preservation_context a, b;
    return gameplay_hook_status.host_image_base == kwengine_va(0x400000U) && selection_preservation_context_once(&a) &&
           selection_preservation_context_once(&b) && !memcmp(&a, &b, sizeof(a));
}
static uint32_t selection_preservation_decision(void *owner, void *selection, void *drawable)
{
    uint32_t vt = 0, command = 0, slot = 0, mode = 0, active = 0, count = 0, head = 0, ends[2];
    if (!InterlockedCompareExchange(&selection_preservation_enabled, 0, 0))
        return SP_DISABLED;
    if ((uint32_t)gameplay_render_flags & KWENGINE_RENDER_DISABLED)
        return SP_HEADLESS;
    if (selection_preservation_tls == TLS_OUT_OF_INDEXES)
        return SP_NO_TLS;
    struct selection_preservation_scope *destroying = TlsGetValue(selection_preservation_tls);
    if (!destroying)
        return SP_NO_SCOPE;
    /* Keep ancestor witnesses: a squad/root notification can occur from an
     * inner native destructor while the root's teardown is still active. */
    while (destroying && (destroying->drawable != (uint32_t)(uintptr_t)drawable || !destroying->object))
        destroying = destroying->previous;
    if (!destroying || !destroying->object || !destroying->drawable)
        return SP_SCOPE_MISMATCH;
    if (!selection_preservation_match())
        return SP_MATCH_CONTEXT;
    if (!selection_preservation_read_u32((uintptr_t)owner, &vt) || vt != kwengine_va(0xaa4880U))
        return SP_OWNER;
    if (!selection_preservation_read_u32((uintptr_t)owner + 0x28, &command) || !command || !selection_preservation_read_u32(command, &vt) ||
        vt != kwengine_va(0xaa2f94U))
        return SP_COMMAND;
    if (!selection_preservation_read_u32((uintptr_t)command + 0x14, &slot) || slot != (uint32_t)(uintptr_t)owner)
        return SP_SLOT;
    if (!selection_preservation_read_u32((uintptr_t)command + 0x20, &mode) || !mode)
        return SP_MODE;
    if (!selection_preservation_read_u32((uintptr_t)mode + 0x2c, &active) || active != command)
        return SP_ACTIVE;
    if (!selection_preservation_read_u32((uintptr_t)selection, &vt) || vt != kwengine_va(0xa20264U))
        return SP_SELECTION;
    if (!selection_preservation_read_u32((uintptr_t)selection + 0x44, &count) || !count)
        return SP_EMPTY;
    if (!selection_preservation_read_u32((uintptr_t)selection + 0x40, &head) || !head || !selection_preservation_read(NULL, head, ends, sizeof(ends)))
        return SP_HEAD;
    /* The native list is authoritative and already unlinked this drawable.
     * Keep the mode while it is nonempty, including other pending deaths.
     * Each later native removal still runs; the final count-zero removal uses
     * stock cancellation. No per-unit type/status/selected-bit filters, ID
     * export, count cap, whole-army scan or command replay is needed. */
    uint32_t first[3], last_next = 0, next_back = 0;
    int keep = ends[0] != head && ends[1] != head && selection_preservation_read(NULL, ends[0], first, sizeof(first)) && first[1] == head &&
               first[0] != ends[0] && first[2] >= 0x10000U && first[2] != (uint32_t)(uintptr_t)drawable &&
               selection_preservation_read_u32((uintptr_t)first[0] + 4, &next_back) && next_back == ends[0] &&
               selection_preservation_read_u32(ends[1], &last_next) && last_next == head;
    return keep ? SP_KEEP : SP_BOUNDARIES;
}
static inline int selection_preservation_remove(void *owner, void *selection, void *drawable)
{
    return selection_preservation_decision(owner, selection, drawable) == SP_KEEP;
}
static int selection_preservation_enter(struct selection_preservation_scope *scope, void *self)
{
    memset(scope, 0, sizeof(*scope));
    scope->object = self;
    if (!InterlockedCompareExchange(&selection_preservation_enabled, 0, 0))
        return 0;
    uint32_t drawable = 0, back = 0;
    if (selection_preservation_read_u32((uintptr_t)self + 0x108, &drawable) && selection_preservation_object(drawable, &back) &&
        back == (uint32_t)(uintptr_t)self)
        scope->drawable = drawable;
    scope->previous = TlsGetValue(selection_preservation_tls);
    return TlsSetValue(selection_preservation_tls, scope) != 0;
}
static void selection_preservation_leave(struct selection_preservation_scope *scope)
{
    if (!TlsSetValue(selection_preservation_tls, scope->previous)) {
        InterlockedExchange(&selection_preservation_tls_failed, 1);
        InterlockedExchange(&selection_preservation_enabled, 0);
    }
}
static void *KWGAMEPLAY_FASTCALL selection_preservation_destroy(void *self, void *unused, uint32_t flags)
{
    DWORD saved = GetLastError();
    void *result;
    (void)unused;
    struct selection_preservation_scope scope;
    int scoped = selection_preservation_enter(&scope, self);
    SetLastError(saved);
    result = selection_preservation_original_destroy(self, flags);
    saved = GetLastError();
    if (scoped)
        selection_preservation_leave(&scope);
    SetLastError(saved);
    return result;
}
typedef uint32_t(KWGAMEPLAY_THISCALL *selection_preservation_die_fn)(void *, void *);
static uint32_t KWGAMEPLAY_FASTCALL selection_preservation_death(void *module, void *target, void *object, void *damage)
{
    DWORD saved = GetLastError();
    struct selection_preservation_scope scope;
    int scoped = selection_preservation_enter(&scope, object);
    selection_preservation_die_fn call;
    memcpy(&call, &target, sizeof(call));
    SetLastError(saved);
    uint32_t result = call(module, damage);
    saved = GetLastError();
    if (scoped)
        selection_preservation_leave(&scope);
    SetLastError(saved);
    return result;
}
/* Replace exactly: push [ebp+8]; mov edx,[eax]; mov ecx,eax; call [edx].
 * Capture the original dynamic target, retain ESI Object identity and forward
 * the same module/DamageInfo once. The four padding NOPs reach the native +9. */
static void __attribute__((naked)) selection_preservation_death_bridge(void)
{
    __asm__ __volatile__("pushl 8(%%ebp)\n\tpushl %%esi\n\tmovl (%%eax),%%edx\n\tmovl (%%edx),%%edx\n\tmovl %%eax,%%ecx\n\tcall %P0\n\tret" ::"i"(
        selection_preservation_death));
}
uint32_t KWENGINE_CALL KwgameplayConfigureSelectionPreservation(uint32_t enabled)
{
    if (enabled > 1)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!enabled || ((uint32_t)gameplay_render_flags & KWENGINE_RENDER_DISABLED)) {
        InterlockedExchange(&selection_preservation_enabled, 0);
        return KWENGINE_NATIVE_OK;
    }
    if (InterlockedCompareExchange(&gameplay_initialization_state, 0, 0) != 2 || gameplay_hook_status.result != KWENGINE_NATIVE_OK ||
        (gameplay_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && gameplay_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (InterlockedCompareExchange(&selection_preservation_tls_failed, 0, 0))
        return KWENGINE_NATIVE_ERROR_IO;
    if (!selection_preservation_installed) {
        uintptr_t table = kwengine_va(0xa88d88U), destroy = kwengine_va(0x69f347U);
        unsigned char code[13], body[16];
        uint32_t target = 0;
        int32_t rel;
        if (!table || !destroy || !kwengine_is_data(table, 0x20) || !selection_preservation_read_u32(table + 0x1c, &target) || target != destroy ||
            !read_process_raw((void *)destroy, code, sizeof(code)) || memcmp(code, "\x56\x8b\xf1\xe8", 4) ||
            memcmp(code + 8, "\xf6\x44\x24\x08\x01", 5))
            return KWENGINE_NATIVE_ERROR_HOOK;
        memcpy(&rel, code + 4, 4);
        uintptr_t destructor = destroy + 8U + rel;
        if (!kwengine_is_code(destructor) || !read_process_value((void *)destructor, body, sizeof(body)) ||
            memcmp(body, "\x55\x8b\xec\x51\x56\x8b\xf1\x8b\x46\x68\xc7\x06", 12) || memcmp(body + 12, &table, 4))
            return KWENGINE_NATIVE_ERROR_HOOK;
        if (selection_preservation_tls == TLS_OUT_OF_INDEXES) {
            selection_preservation_tls = TlsAlloc();
            if (selection_preservation_tls == TLS_OUT_OF_INDEXES)
                return KWENGINE_NATIVE_ERROR_IO;
        }
        if (!selection_preservation_install_remove())
            return KWENGINE_NATIVE_ERROR_HOOK;
        struct selection_preservation_patch patch;
        void *(KWGAMEPLAY_FASTCALL * hook)(void *, void *, uint32_t) = selection_preservation_destroy;
        patch.slot = (void **)(table + 0x1c);
        patch.original = (void *)destroy;
        memcpy(&patch.hook, &hook, 4);
        memcpy(&selection_preservation_original_destroy, &destroy, 4);
        selection_preservation_installed = selection_preservation_publish(&patch, 1);
        if (!selection_preservation_installed)
            return KWENGINE_NATIVE_ERROR_HOOK;
    }
    InterlockedExchange(&selection_preservation_enabled, 1);
    return KWENGINE_NATIVE_OK;
}
