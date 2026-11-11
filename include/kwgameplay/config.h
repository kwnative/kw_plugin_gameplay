/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct kwgameplay_config {
    uint32_t preserve_attack_move_on_loss;
};
int kwgameplay_config_parse(const void *, size_t, void *, int, size_t *);
#ifdef __cplusplus
}
#endif
