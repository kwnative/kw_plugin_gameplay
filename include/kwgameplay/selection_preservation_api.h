/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KWGAMEPLAY_SELECTION_PRESERVATION_API_H
#define KWGAMEPLAY_SELECTION_PRESERVATION_API_H
#include <kwengine/native_status.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Startup-only input correction. Retains armed Attack-Move during a proved
 * destruction notification with surviving selection. No command submission. */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwgameplayConfigureSelectionPreservation(uint32_t enabled);
#ifdef __cplusplus
}
#endif
#endif
