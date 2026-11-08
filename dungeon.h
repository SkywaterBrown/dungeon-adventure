#ifndef DUNGEON_H
#define DUNGEON_H

#include "config.h"

typedef struct { int x, y; } Point;
typedef struct { int x, y; char type; int alive; } Monster;

typedef enum {
    ROOM_EMPTY = 0,
    ROOM_TREASURE,
    ROOM_DEN,
    ROOM_MAZE
} RoomType;

typedef struct {
    RoomType type;
    int connect_n, connect_s, connect_e, connect_w;
    int door_n, door_s, door_e, door_w;
    Monster monsters[MAX_MONSTERS];
    int num_monsters;
    int has_chest, chest_x, chest_y;
    int has_potion, potion_x, potion_y;
    int visited;
} Room;

/* Threat type codes stored in known_threat[] */
#define THREAT_GRUNT   1
#define THREAT_BAT     2
#define THREAT_DRAGON  3

extern int watch_mode;

extern Room rooms[GRID_H][GRID_W];
extern unsigned char room_walls[GRID_H][GRID_W][ROOM_H][ROOM_W];
extern unsigned char seen[GRID_H][GRID_W][ROOM_H][ROOM_W];
extern unsigned char known_threat[GRID_H][GRID_W][ROOM_H][ROOM_W];
extern int conn_ew[GRID_H][GRID_W], conn_ns[GRID_H][GRID_W];
extern int door_ew[GRID_H][GRID_W], door_ns[GRID_H][GRID_W];

extern int hero_x, hero_y, hero_r, hero_c, hero_hp;
extern int kills, chests_collected, total_chests, level, tick;
extern int score, best_score;
extern unsigned int best_score_seed;
extern int prev_room_r, prev_room_c;
extern int target_room_r, target_room_c;
extern int last_dx, last_dy;
extern int damage_flash;

extern int dist_map[ROOM_H][ROOM_W];

int  is_wall(int x, int y);
int  has_los(int x0, int y0, int x1, int y1, int r, int c);
int  room_has_content(int r, int c);
int  check_room_reachability(int r, int c);
void compute_dist_map(int r, int c, int tx, int ty);
void compute_dist_map_jones(int r, int c, int tx, int ty);
void update_seen(void);
void mark_visited_rooms_seen(void);

void generate_dungeon(void);
void spawn_hero(void);
void hero_step(int dx, int dy);
void collect_chest(void);
void collect_potion(void);
void move_monsters(void);
void check_collisions(void);

void hero_tick_ai(void);

void render(int ai_mode, int paused);

#endif
