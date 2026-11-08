#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config cfg;

void config_defaults(Config *c) {
    c->ai_tick_us       = 100000;
    c->manual_tick_us   = 30000;
    c->start_hp         = 3;
    c->monster_density  = 55;
    c->chest_density    = 45;
    c->g_pct            = 40;
    c->b_pct            = 30;
    c->d_pct            = 30;
    c->max_monsters     = MAX_MONSTERS;
    c->walls_min        = 1;
    c->walls_max        = 3;
    c->loop_pct         = 30;
    c->torch_enabled    = 1;
    c->torch_radius     = 6;
    c->treasure_pct     = 15;
    c->den_pct          = 20;
    c->maze_pct         = 10;
    c->ai_personality   = AI_JONES;
    c->seed             = 0;
}

static int parse_line(const char *line, char *key, char *val) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#' || *line == '\n' || *line == '\r') return 0;
    const char *eq = strchr(line, '=');
    if (!eq) return 0;
    size_t klen = (size_t)(eq - line);
    while (klen > 0 && (line[klen-1] == ' ' || line[klen-1] == '\t')) klen--;
    if (klen == 0 || klen >= 63) return 0;
    memcpy(key, line, klen); key[klen] = '\0';
    const char *v = eq + 1;
    while (*v == ' ' || *v == '\t') v++;
    size_t vlen = strlen(v);
    while (vlen > 0 && (v[vlen-1] == '\n' || v[vlen-1] == '\r' ||
                        v[vlen-1] == ' '  || v[vlen-1] == '\t')) vlen--;
    if (vlen >= 63) return 0;
    memcpy(val, v, vlen); val[vlen] = '\0';
    return 1;
}

static void apply(Config *c, const char *key, const char *val) {
    int iv = atoi(val);
    if      (!strcmp(key, "ai_tick_us"))       c->ai_tick_us = iv;
    else if (!strcmp(key, "manual_tick_us"))   c->manual_tick_us = iv;
    else if (!strcmp(key, "start_hp"))         c->start_hp = iv;
    else if (!strcmp(key, "monster_density"))  c->monster_density = iv;
    else if (!strcmp(key, "chest_density"))    c->chest_density = iv;
    else if (!strcmp(key, "monster_g_pct"))    c->g_pct = iv;
    else if (!strcmp(key, "monster_b_pct"))    c->b_pct = iv;
    else if (!strcmp(key, "monster_d_pct"))    c->d_pct = iv;
    else if (!strcmp(key, "max_monsters"))     c->max_monsters = iv;
    else if (!strcmp(key, "walls_min"))        c->walls_min = iv;
    else if (!strcmp(key, "walls_max"))        c->walls_max = iv;
    else if (!strcmp(key, "loop_pct"))         c->loop_pct = iv;
    else if (!strcmp(key, "torch_enabled"))    c->torch_enabled = iv;
    else if (!strcmp(key, "torch_radius"))     c->torch_radius = iv;
    else if (!strcmp(key, "treasure_pct"))     c->treasure_pct = iv;
    else if (!strcmp(key, "den_pct"))          c->den_pct = iv;
    else if (!strcmp(key, "maze_pct"))         c->maze_pct = iv;
    else if (!strcmp(key, "ai_personality"))   c->ai_personality = iv;
    else if (!strcmp(key, "seed"))             c->seed = (unsigned int)strtoul(val, NULL, 10);
    else fprintf(stderr, "config: unknown key '%s'\n", key);
}

static void sanitize(Config *c) {
    if (c->ai_tick_us     < 1000)  c->ai_tick_us = 1000;
    if (c->manual_tick_us < 1000)  c->manual_tick_us = 1000;
    if (c->start_hp < 1)  c->start_hp = 1;
    if (c->start_hp > 20) c->start_hp = 20;
    if (c->monster_density < 0)   c->monster_density = 0;
    if (c->monster_density > 100) c->monster_density = 100;
    if (c->chest_density < 0)     c->chest_density = 0;
    if (c->chest_density > 100)   c->chest_density = 100;
    if (c->g_pct < 0) c->g_pct = 0;
    if (c->b_pct < 0) c->b_pct = 0;
    if (c->d_pct < 0) c->d_pct = 0;
    if (c->g_pct + c->b_pct + c->d_pct == 0) { c->g_pct = 34; c->b_pct = 33; c->d_pct = 33; }
    if (c->max_monsters < 1)             c->max_monsters = 1;
    if (c->max_monsters > MAX_MONSTERS)  c->max_monsters = MAX_MONSTERS;
    if (c->walls_min < 0) c->walls_min = 0;
    if (c->walls_max < 0) c->walls_max = 0;
    if (c->walls_max < c->walls_min) c->walls_max = c->walls_min;
    if (c->walls_max > 10) c->walls_max = 10;
    if (c->loop_pct < 0)   c->loop_pct = 0;
    if (c->loop_pct > 100) c->loop_pct = 100;
    if (c->torch_radius < 1)  c->torch_radius = 1;
    if (c->torch_radius > 30) c->torch_radius = 30;
    if (c->torch_enabled != 0) c->torch_enabled = 1;
    if (c->treasure_pct < 0)   c->treasure_pct = 0;
    if (c->treasure_pct > 100) c->treasure_pct = 100;
    if (c->den_pct < 0)        c->den_pct = 0;
    if (c->den_pct > 100)      c->den_pct = 100;
    if (c->maze_pct < 0)       c->maze_pct = 0;
    if (c->maze_pct > 100)     c->maze_pct = 100;
    if (c->ai_personality != AI_SUPERMAN) c->ai_personality = AI_JONES;
}

int config_load(Config *c, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512], key[64], val[64];
    int n = 0;
    while (fgets(line, sizeof(line), f))
        if (parse_line(line, key, val)) { apply(c, key, val); n++; }
    fclose(f);
    sanitize(c);
    return n;
}

void config_print(const Config *c) {
    printf("# Dungeon configuration\n");
    printf("ai_tick_us       = %d\n", c->ai_tick_us);
    printf("manual_tick_us   = %d\n", c->manual_tick_us);
    printf("start_hp         = %d\n", c->start_hp);
    printf("monster_density  = %d\n", c->monster_density);
    printf("chest_density    = %d\n", c->chest_density);
    printf("monster_g_pct    = %d\n", c->g_pct);
    printf("monster_b_pct    = %d\n", c->b_pct);
    printf("monster_d_pct    = %d\n", c->d_pct);
    printf("max_monsters     = %d\n", c->max_monsters);
    printf("walls_min        = %d\n", c->walls_min);
    printf("walls_max        = %d\n", c->walls_max);
    printf("loop_pct         = %d\n", c->loop_pct);
    printf("torch_enabled    = %d\n", c->torch_enabled);
    printf("torch_radius     = %d\n", c->torch_radius);
    printf("treasure_pct     = %d\n", c->treasure_pct);
    printf("den_pct          = %d\n", c->den_pct);
    printf("maze_pct         = %d\n", c->maze_pct);
    printf("# ai_personality: 0 = Jones (limited vision), 1 = Superman (omniscient)\n");
    printf("ai_personality   = %d\n", c->ai_personality);
    printf("seed             = %u\n", c->seed);
}
