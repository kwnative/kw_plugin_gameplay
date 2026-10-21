#include <kwgameplay/config.h>
#include <kwengine/config.h>

int kwgameplay_config_parse(const void *data, size_t size, void *output, int legacy, size_t *line)
{
    struct kwgameplay_config config = {0};
    static const struct kwengine_config_field fields[] = {
        {"preserve_attack_move_on_loss", offsetof(struct kwgameplay_config, preserve_attack_move_on_loss), KWENGINE_CONFIG_BOOL, 0U, 1U, NULL},
    };
    if (!output || !kwengine_config_parse(data, size, &config, sizeof(config), fields, sizeof(fields) / sizeof(fields[0]), legacy, line))
        return 0;
    *(struct kwgameplay_config *)output = config;
    return 1;
}
