/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Executes the release hook bodies with bounded memory/native-call fixtures.
 * No game, Wine, diagnostic implementation or historical bridge is included. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kwf/plugin.h>
#include <kwengine/native_status.h>
#define KWGAMEPLAY_THISCALL __attribute__((thiscall))
#define KWGAMEPLAY_FASTCALL __attribute__((fastcall))
typedef int32_t LONG;
typedef uint32_t DWORD;
#define TLS_OUT_OF_INDEXES UINT32_MAX
static DWORD last_error;
static void *tls_value;
static int fail_tls, nested, fail_restore_after_call;
static unsigned writes, restores, native_removals, native_destroys, native_deaths;
static int bad_remove_signature;
static LONG gameplay_render_flags, gameplay_initialization_state;
static struct kwengine_native_status gameplay_hook_status;
static unsigned char logic[0x40], players[0xa0], local[0x23c], owner[0x30], command[0x30], mode[0x30], selection[0x48];
static unsigned char object[0x10c], child_object[0x10c], drawable[0x148], child_drawable[0x148], destructor_body[16];
static uint32_t observer_table[3], destroy_table[8], owner_table[4], command_table[0x14], selection_table[4];
static uint32_t logic_table[4], players_table[4], local_table[4], root_logic, root_players, head[2], node[3];
static void *region_address[32];
static size_t region_size[32], region_count;
static LONG InterlockedCompareExchange(volatile LONG *pointer, LONG value, LONG expected)
{
    __atomic_compare_exchange_n(pointer, &expected, value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static LONG InterlockedExchange(volatile LONG *pointer, LONG value)
{
    return __atomic_exchange_n(pointer, value, __ATOMIC_SEQ_CST);
}
static DWORD GetLastError(void)
{
    return last_error;
}
static void SetLastError(DWORD value)
{
    last_error = value;
}
static DWORD TlsAlloc(void)
{
    return 3;
}
static void *TlsGetValue(DWORD key)
{
    assert(key == 3);
    last_error = 0;
    return tls_value;
}
static int TlsSetValue(DWORD key, void *value)
{
    assert(key == 3);
    last_error = 456;
    if (fail_tls)
        return 0;
    tls_value = value;
    return 1;
}
static void MemoryBarrier(void)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
static void put(void *base, size_t offset, uintptr_t value)
{
    uint32_t word = (uint32_t)value;
    memcpy((char *)base + offset, &word, 4);
}
static uint32_t word(const void *base, size_t offset)
{
    uint32_t value;
    memcpy(&value, (const char *)base + offset, 4);
    return value;
}
static int in_region(uintptr_t address, size_t bytes)
{
    for (size_t i = 0; i < region_count; ++i) {
        uintptr_t base = (uintptr_t)region_address[i];
        if (address >= base && bytes <= region_size[i] && address - base <= region_size[i] - bytes)
            return 1;
    }
    return 0;
}
static void KWGAMEPLAY_THISCALL original_remove(void *self, void *selected, void *removed);
static void *KWGAMEPLAY_THISCALL original_destroy(void *self, uint32_t flags);
static uintptr_t function_remove(void)
{
    uintptr_t out;
    void(KWGAMEPLAY_THISCALL * fn)(void *, void *, void *) = original_remove;
    memcpy(&out, &fn, 4);
    return out;
}
static uintptr_t function_destroy(void)
{
    uintptr_t out;
    void *(KWGAMEPLAY_THISCALL * fn)(void *, uint32_t) = original_destroy;
    memcpy(&out, &fn, 4);
    return out;
}
static uintptr_t kwengine_va(uintptr_t reference)
{
    switch (reference) {
    case 0x400000:
        return 0x400000;
    case 0xbe1008:
        return (uintptr_t)&root_logic;
    case 0xbe0b80:
        return (uintptr_t)&root_players;
    case 0xa99244:
        return (uintptr_t)logic_table;
    case 0xa21e44:
        return (uintptr_t)players_table;
    case 0xa237cc:
        return (uintptr_t)local_table;
    case 0xaa4870:
        return (uintptr_t)observer_table;
    case 0xaa4880:
        return (uintptr_t)owner_table;
    case 0xaa2f94:
        return (uintptr_t)command_table;
    case 0xa20264:
        return (uintptr_t)selection_table;
    case 0xa88d88:
        return (uintptr_t)destroy_table;
    case 0x7fbc25:
        return function_remove();
    case 0x69f347:
        return function_destroy();
    default:
        return 0;
    }
}
static int kwengine_is_data(uintptr_t address, size_t bytes)
{
    return in_region(address, bytes);
}
static int kwengine_is_code(uintptr_t address)
{
    return address == function_remove() || address == function_destroy() || address == (uintptr_t)destructor_body;
}
static int read_process_raw(const void *address, void *out, size_t bytes)
{
    last_error = 123;
    // Signature fixtures only: actual guard logic still executes unchanged.
    if ((uintptr_t)address == function_remove() && bytes == 15) {
        static const unsigned char signature[15] = {0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x05, 0xe8, 0, 0, 0, 0, 0xc2, 0x08, 0};
        memcpy(out, signature, bytes);
        if (bad_remove_signature)
            ((unsigned char *)out)[0] ^= 1;
        return 1;
    }
    if ((uintptr_t)address == function_destroy() && bytes == 13) {
        static const unsigned char signature[13] = {0x56, 0x8b, 0xf1, 0xe8, 0, 0, 0, 0, 0xf6, 0x44, 0x24, 0x08, 1};
        memcpy(out, signature, bytes);
        put(out, 4, (uintptr_t)destructor_body - function_destroy() - 8);
        return 1;
    }
    if (!in_region((uintptr_t)address, bytes))
        return 0;
    memcpy(out, address, bytes);
    return 1;
}
static int read_process_value(const void *address, void *out, size_t bytes)
{
    return read_process_raw(address, out, bytes);
}
static uint32_t broker_write(void *unused, uintptr_t address, const void *expected, const void *replacement, uint32_t bytes)
{
    (void)unused;
    assert(bytes == 4 && in_region(address, bytes) && !memcmp((void *)address, expected, bytes));
    memcpy((void *)address, replacement, bytes);
    ++writes;
    return KWF_OK;
}
static uint32_t broker_restore(void *unused, uintptr_t address, uint32_t bytes)
{
    (void)unused;
    (void)address;
    (void)bytes;
    ++restores;
    return KWF_OK;
}
static void broker_log(void *unused, const char *message)
{
    (void)unused;
    (void)message;
    assert(0 && "unexpected rollback failure");
}
static const struct kwf_host host = {.write = broker_write, .restore = broker_restore, .log = broker_log};
static const struct kwf_context context = {.host = &host};
static const struct kwf_context *gameplay_context = &context;
#include "../src/hooks/selection_hooks.inc"

static void arm(void)
{
    put(owner, 0x28, (uintptr_t)command);
    put(command, 0, (uintptr_t)command_table);
    put(command, 0x14, (uintptr_t)owner);
    put(command, 0x20, (uintptr_t)mode);
    put(mode, 0x2c, (uintptr_t)command);
}
static void KWGAMEPLAY_THISCALL original_remove(void *self, void *selected, void *removed)
{
    assert(self == owner + 0x14 && selected == selection && removed == drawable && GetLastError() == 77);
    ++native_removals;
    put(owner, 0x28, 0);
    SetLastError(778);
}
static void *KWGAMEPLAY_THISCALL original_destroy(void *self, uint32_t flags)
{
    assert((self == object || self == child_object) && flags == 0x81 && GetLastError() == 77);
    ++native_destroys;
    if (nested && self == object) {
        void *result = selection_preservation_destroy(child_object, NULL, flags);
        assert((uintptr_t)result == 0x76543210 && GetLastError() == 777);
        SetLastError(77);
    }
    selection_preservation_on_remove(owner + 0x14, NULL, selection, drawable);
    if (fail_restore_after_call)
        fail_tls = 1;
    SetLastError(777);
    return (void *)(uintptr_t)0x76543210;
}
static uint32_t KWGAMEPLAY_THISCALL original_die(void *module, void *damage)
{
    assert(module == mode && damage == command && GetLastError() == 77);
    ++native_deaths;
    // Native onDie may already sever object/drawable backlinks.
    put(object, 0x108, 0);
    put(drawable, 0x140, 0);
    selection_preservation_on_remove(owner + 0x14, NULL, selection, drawable);
    SetLastError(719);
    return 0x89abcdef;
}
static void register_region(void *address, size_t bytes)
{
    assert(region_count < 32);
    region_address[region_count] = address;
    region_size[region_count++] = bytes;
}
static void setup(void)
{
#define REGION(value) register_region(value, sizeof(value))
    REGION(logic);
    REGION(players);
    REGION(local);
    REGION(owner);
    REGION(command);
    REGION(mode);
    REGION(selection);
    REGION(object);
    REGION(child_object);
    REGION(drawable);
    REGION(child_drawable);
    REGION(destructor_body);
    REGION(observer_table);
    REGION(destroy_table);
    REGION(owner_table);
    REGION(command_table);
    REGION(selection_table);
    REGION(logic_table);
    REGION(players_table);
    REGION(local_table);
    REGION(head);
    REGION(node);
    register_region(&root_logic, 4);
    register_region(&root_players, 4);
#undef REGION
    root_logic = (uintptr_t)logic;
    root_players = (uintptr_t)players;
    put(logic, 0, (uintptr_t)logic_table);
    logic[0x3c] = 1;
    put(logic, 0x38, 100);
    put(players, 0, (uintptr_t)players_table);
    put(players, 0x20, 2);
    put(players, 0x1c, (uintptr_t)local);
    put(players, 0x28, (uintptr_t)local);
    put(local, 0, (uintptr_t)local_table);
    put(local, 0x48, 1);
    put(owner, 0, (uintptr_t)owner_table);
    put(selection, 0, (uintptr_t)selection_table);
    put(selection, 0x44, 1);
    put(selection, 0x40, (uintptr_t)head);
    head[0] = head[1] = (uintptr_t)node;
    node[0] = node[1] = (uintptr_t)head;
    node[2] = (uintptr_t)child_drawable;
    put(object, 0x108, (uintptr_t)drawable);
    put(object, 0xec, 17);
    put(drawable, 0, (uintptr_t)owner_table);
    put(drawable, 0x140, (uintptr_t)object);
    put(child_object, 0x108, (uintptr_t)child_drawable);
    put(child_object, 0xec, 18);
    put(child_drawable, 0, (uintptr_t)owner_table);
    put(child_drawable, 0x140, (uintptr_t)child_object);
    observer_table[1] = observer_table[2] = function_remove();
    destroy_table[7] = function_destroy();
    memcpy(destructor_body, "\x55\x8b\xec\x51\x56\x8b\xf1\x8b\x46\x68\xc7\x06", 12);
    put(destructor_body, 12, (uintptr_t)destroy_table);
    gameplay_initialization_state = 2;
    gameplay_hook_status.host_image_base = 0x400000;
    gameplay_hook_status.state = KWENGINE_NATIVE_STATE_PASSIVE_READY;
    gameplay_hook_status.result = KWENGINE_NATIVE_OK;
    arm();
}
int main(void)
{
    setup();
    (void)&selection_preservation_death_bridge;
    assert(KwgameplayConfigureSelectionPreservation(2) == KWENGINE_NATIVE_ERROR_CONFIG);
    gameplay_initialization_state = 1;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_ERROR_CONFIG && writes == 0);
    gameplay_initialization_state = 2;
    gameplay_render_flags = KWENGINE_RENDER_DISABLED;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_OK && writes == 0);
    gameplay_render_flags = 0;
    destroy_table[7] = 1;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_ERROR_HOOK && writes == 0);
    destroy_table[7] = function_destroy();
    observer_table[2] = 1;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_ERROR_HOOK && writes == 0);
    observer_table[2] = function_remove();
    bad_remove_signature = 1;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_ERROR_HOOK && writes == 0);
    bad_remove_signature = 0;
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_OK);
    assert(writes == 2 && !restores && observer_table[1] == function_remove());
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_OK && writes == 2);
    SetLastError(77);
    assert((uintptr_t)selection_preservation_destroy(object, NULL, 0x81) == 0x76543210);
    assert(!native_removals && native_destroys == 1 && !tls_value && GetLastError() == 777 && word(owner, 0x28));
    // Manual changes and final selection loss must still use native reset.
    SetLastError(77);
    selection_preservation_on_remove(owner + 0x14, NULL, selection, drawable);
    assert(native_removals == 1 && !word(owner, 0x28) && GetLastError() == 778);
    arm();
    put(selection, 0x44, 0);
    SetLastError(77);
    selection_preservation_destroy(object, NULL, 0x81);
    assert(native_removals == 2 && !word(owner, 0x28));
    put(selection, 0x44, 1);
    struct selection_preservation_scope scope = {object, (uintptr_t)drawable, NULL};
    tls_value = &scope;
    arm();
    assert(selection_preservation_remove(owner, selection, drawable));
    put(selection, 0x44, 16385);
    assert(selection_preservation_remove(owner, selection, drawable));
    put(selection, 0x44, 1);
    local[0x238] = 1;
    assert(!selection_preservation_remove(owner, selection, drawable));
    local[0x238] = 0;
    logic[0x3c] = 0;
    assert(!selection_preservation_remove(owner, selection, drawable));
    logic[0x3c] = 1;
    put(command, 0, (uintptr_t)owner_table);
    assert(!selection_preservation_remove(owner, selection, drawable));
    arm();
    put(command, 0x14, 0);
    assert(!selection_preservation_remove(owner, selection, drawable));
    arm();
    put(mode, 0x2c, 0);
    assert(!selection_preservation_remove(owner, selection, drawable));
    arm();
    node[1] = 0;
    assert(!selection_preservation_remove(owner, selection, drawable));
    node[1] = (uintptr_t)head;
    scope.drawable = (uintptr_t)child_drawable;
    assert(!selection_preservation_remove(owner, selection, drawable));
    scope.drawable = (uintptr_t)drawable;
    tls_value = NULL;
    nested = 1;
    SetLastError(77);
    selection_preservation_destroy(object, NULL, 0x81);
    nested = 0;
    assert(native_removals == 2 && !tls_value && GetLastError() == 777);
    selection_preservation_die_fn die = original_die;
    void *target;
    memcpy(&target, &die, 4);
    SetLastError(77);
    assert(selection_preservation_death(mode, target, object, command) == 0x89abcdef);
    assert(native_deaths == 1 && native_removals == 2 && !tls_value && GetLastError() == 719);
    put(object, 0x108, (uintptr_t)drawable);
    put(drawable, 0x140, (uintptr_t)object);
    assert(KwgameplayConfigureSelectionPreservation(0) == KWENGINE_NATIVE_OK);
    arm();
    SetLastError(77);
    selection_preservation_destroy(object, NULL, 0x81);
    assert(native_removals == 3);
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_OK);
    arm();
    fail_restore_after_call = 1;
    SetLastError(77);
    selection_preservation_destroy(object, NULL, 0x81);
    assert(!selection_preservation_enabled && selection_preservation_tls_failed && GetLastError() == 777);
    assert(KwgameplayConfigureSelectionPreservation(1) == KWENGINE_NATIVE_ERROR_IO);
    puts(
        "PASS release preservation: two functional slots only, native fallback, context/identity/list guards, nested death, ABI/error forwarding, TLS fail-closed");
    return 0;
}
