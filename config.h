#ifndef CONFIG_H
#define CONFIG_H

#define GRID_W       4
#define GRID_H       4
#define ROOM_W       40
#define ROOM_H       20
#define MAX_MONSTERS 3

#define INF_DIST 999999

/* AI personalities */
#define AI_JONES     0
#define AI_SUPERMAN  1

typedef struct {
    int ai_tick_us;
    int manual_tick_us;
    int start_hp;
    int monster_density;
    int chest_density;
    int g_pct, b_pct, d_pct;
    int max_monsters;
    int walls_min, walls_max;
    int loop_pct;
    int torch_enabled;
    int torch_radius;
    int treasure_pct;
    int den_pct;
    int maze_pct;
    int ai_personality;     /* AI_JONES or AI_SUPERMAN */
    unsigned int seed;
} Config;

extern Config cfg;

void config_defaults(Config *c);
int  config_load(Config *c, const char *path);
void config_print(const Config *c);

#endif
