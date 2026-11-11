/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <kwgameplay/config.h>
static int parse(const char *text, struct kwgameplay_config *config, int legacy)
{
    size_t line = 0;
    return kwgameplay_config_parse(text, text ? strlen(text) : 0, config, legacy, &line);
}
int main(void)
{
    struct kwgameplay_config config;
    assert(parse(NULL, &config, 0) && !config.preserve_attack_move_on_loss);
    assert(parse("preserve_attack_move_on_loss=true", &config, 0) && config.preserve_attack_move_on_loss);
    assert(parse("preserve_attack_move_on_loss=false", &config, 0) && !config.preserve_attack_move_on_loss);
    assert(!parse("preserve_attack_move_on_loss=invalid", &config, 0));
    assert(!parse("preserve_attack_move_on_loss=true\npreserve_attack_move_on_loss=false", &config, 0));
    const char *removed[] = {"selection_command_trace=true", "command_flow_tap=true", "performance_probe=true"};
    for (unsigned i = 0; i < sizeof(removed) / sizeof(*removed); ++i) {
        assert(!parse(removed[i], &config, 0));
        assert(parse(removed[i], &config, 1) && !config.preserve_attack_move_on_loss);
    }
    assert(parse("command_flow_tap=true\npreserve_attack_move_on_loss=true", &config, 1) && config.preserve_attack_move_on_loss);
    puts("PASS release gameplay config: only functional setting; retired diagnostics rejected/legacy-ignored");
    return 0;
}
