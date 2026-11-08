#include "dungeon.h"
#include <stdlib.h>
#include <string.h>

int watch_mode = 0;

Room rooms[GRID_H][GRID_W];
unsigned char room_walls[GRID_H][GRID_W][ROOM_H][ROOM_W];
unsigned char seen[GRID_H][GRID_W][ROOM_H][ROOM_W];
unsigned char known_threat[GRID_H][GRID_W][ROOM_H][ROOM_W];
int conn_ew[GRID_H][GRID_W], conn_ns[GRID_H][GRID_W];
int door_ew[GRID_H][GRID_W], door_ns[GRID_H][GRID_W];

int dist_map[ROOM_H][ROOM_W];

int hero_x, hero_y, hero_r, hero_c, hero_hp;
int kills, chests_collected, total_chests, level, tick;
int score = 0, best_score = 0;
unsigned int best_score_seed = 0;
int prev_room_r = -1, prev_room_c = -1;
int target_room_r = -1, target_room_c = -1;
int last_dx = 0, last_dy = 0;
int damage_flash = 0;

int is_wall(int x, int y) {
    return x == 0 || x == ROOM_W - 1 || y == 0 || y == ROOM_H - 1;
}

int has_los(int x0, int y0, int x1, int y1, int r, int c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    while (1) {
        if (x == x1 && y == y1) return 1;
        int e2 = 2 * err;
        int ox = x, oy = y;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
        if (x != ox && y != oy) {
            if (is_wall(x, oy) || room_walls[r][c][oy][x]) return 0;
            if (is_wall(ox, y) || room_walls[r][c][y][ox]) return 0;
        }
        if (x == x1 && y == y1) return 1;
        if (is_wall(x, y) || room_walls[r][c][y][x]) return 0;
    }
}

int room_has_content(int r, int c) {
    Room *rm = &rooms[r][c];
    if (rm->has_chest) return 1;
    if (rm->has_potion && hero_hp < cfg.start_hp) return 1;
    for (int i = 0; i < rm->num_monsters; i++)
        if (rm->monsters[i].alive) return 1;
    return 0;
}

void update_seen(void) {
    int torch = cfg.torch_enabled;
    int r2 = cfg.torch_radius * cfg.torch_radius;

    for (int y = 0; y < ROOM_H; y++) {
        for (int x = 0; x < ROOM_W; x++) {
            int visible = 0;
            if (has_los(hero_x, hero_y, x, y, hero_r, hero_c)) {
                int dx = x - hero_x, dy = y - hero_y;
                if (!torch || (dx*dx + dy*dy <= r2))
                    visible = 1;
            }
            if (!visible) continue;
            seen[hero_r][hero_c][y][x] = 1;

            int has_monster = 0;
            int monster_type = 0;
            Room *rm = &rooms[hero_r][hero_c];
            for (int i = 0; i < rm->num_monsters; i++) {
                if (!rm->monsters[i].alive) continue;
                if (rm->monsters[i].x == x && rm->monsters[i].y == y) {
                    has_monster = 1;
                    monster_type = rm->monsters[i].type;
                    break;
                }
            }
            if (has_monster) {
                if      (monster_type == 'B') known_threat[hero_r][hero_c][y][x] = THREAT_BAT;
                else if (monster_type == 'D') known_threat[hero_r][hero_c][y][x] = THREAT_DRAGON;
                else                          known_threat[hero_r][hero_c][y][x] = THREAT_GRUNT;
            } else {
                known_threat[hero_r][hero_c][y][x] = 0;
            }
        }
    }
}

void mark_visited_rooms_seen(void) {
    for (int r = 0; r < GRID_H; r++)
        for (int c = 0; c < GRID_W; c++)
            if (rooms[r][c].visited)
                for (int y = 0; y < ROOM_H; y++)
                    for (int x = 0; x < ROOM_W; x++)
                        seen[r][c][y][x] = 1;
}

void compute_dist_map(int r, int c, int tx, int ty) {
    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++)
            dist_map[y][x] = INF_DIST;
    if (is_wall(tx, ty) || room_walls[r][c][ty][tx]) return;
    static Point q[ROOM_W * ROOM_H];
    int qh = 0, qt = 0;
    dist_map[ty][tx] = 0;
    q[qt++] = (Point){tx, ty};
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (qh < qt) {
        Point cur = q[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dirs[k][0], ny = cur.y + dirs[k][1];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (room_walls[r][c][ny][nx]) continue;
            if (dist_map[ny][nx] != INF_DIST) continue;
            dist_map[ny][nx] = dist_map[cur.y][cur.x] + 1;
            q[qt++] = (Point){nx, ny};
        }
    }
}

void compute_dist_map_jones(int r, int c, int tx, int ty) {
    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++)
            dist_map[y][x] = INF_DIST;
    if (is_wall(tx, ty)) return;
    if (!seen[r][c][ty][tx]) return;
    if (room_walls[r][c][ty][tx]) return;
    static Point q[ROOM_W * ROOM_H];
    int qh = 0, qt = 0;
    dist_map[ty][tx] = 0;
    q[qt++] = (Point){tx, ty};
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (qh < qt) {
        Point cur = q[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dirs[k][0], ny = cur.y + dirs[k][1];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (!seen[r][c][ny][nx]) continue;
            if (room_walls[r][c][ny][nx]) continue;
            if (dist_map[ny][nx] != INF_DIST) continue;
            dist_map[ny][nx] = dist_map[cur.y][cur.x] + 1;
            q[qt++] = (Point){nx, ny};
        }
    }
}

static int is_door_inner_cell(Room *rm, int x, int y) {
    if (rm->connect_n && x == rm->door_n && y == 1)          return 1;
    if (rm->connect_s && x == rm->door_s && y == ROOM_H - 2) return 1;
    if (rm->connect_w && x == 1 && y == rm->door_w)          return 1;
    if (rm->connect_e && x == ROOM_W - 2 && y == rm->door_e) return 1;
    return 0;
}

static int room_flood_ok(unsigned char w[ROOM_H][ROOM_W]) {
    static int vis[ROOM_H][ROOM_W];
    static Point q[ROOM_W * ROOM_H];
    memset(vis, 0, sizeof(vis));
    int sx = -1, sy = -1, total = 0;
    for (int y = 1; y < ROOM_H - 1; y++)
        for (int x = 1; x < ROOM_W - 1; x++)
            if (!w[y][x]) { total++; if (sx < 0) { sx = x; sy = y; } }
    if (sx < 0) return 1;
    int qh = 0, qt = 0, count = 0;
    q[qt++] = (Point){sx, sy}; vis[sy][sx] = 1;
    int m[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (qh < qt) {
        Point cur = q[qh++]; count++;
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + m[k][0], ny = cur.y + m[k][1];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (w[ny][nx] || vis[ny][nx]) continue;
            vis[ny][nx] = 1; q[qt++] = (Point){nx, ny};
        }
    }
    return count == total;
}

static void generate_room_walls(int r, int c, int wmin, int wmax) {
    Room *rm = &rooms[r][c];
    unsigned char (*w)[ROOM_W] = room_walls[r][c];
    memset(w, 0, ROOM_H * ROOM_W * sizeof(unsigned char));
    int span = wmax - wmin + 1;
    if (span < 1) span = 1;
    int num = wmin + rand() % span;

    for (int s = 0; s < num; s++) {
        int horiz = rand() % 2;
        int len, x1, y1;
        if (horiz) {
            len = 4 + rand() % 8;
            int max_x1 = ROOM_W - 3 - len;
            if (max_x1 < 2) continue;
            x1 = 2 + rand() % (max_x1 - 1);
            y1 = 2 + rand() % (ROOM_H - 4);
        } else {
            len = 3 + rand() % 6;
            int max_y1 = ROOM_H - 3 - len;
            if (max_y1 < 2) continue;
            x1 = 2 + rand() % (ROOM_W - 4);
            y1 = 2 + rand() % (max_y1 - 1);
        }
        int ok = 1;
        for (int i = 0; i <= len; i++) {
            int wx = horiz ? x1 + i : x1;
            int wy = horiz ? y1 : y1 + i;
            if (is_door_inner_cell(rm, wx, wy)) { ok = 0; break; }
        }
        if (!ok) continue;
        for (int i = 0; i <= len; i++) {
            int wx = horiz ? x1 + i : x1;
            int wy = horiz ? y1 : y1 + i;
            w[wy][wx] = 1;
        }
        if (!room_flood_ok(w))
            for (int i = 0; i <= len; i++) {
                int wx = horiz ? x1 + i : x1;
                int wy = horiz ? y1 : y1 + i;
                w[wy][wx] = 0;
            }
    }
}

static void shuffle_ints(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void dfs_maze(int r, int c, int vis[GRID_H][GRID_W]) {
    vis[r][c] = 1;
    int dirs[4] = {0, 1, 2, 3}; shuffle_ints(dirs, 4);
    for (int k = 0; k < 4; k++) {
        int nr = r, nc = c;
        if      (dirs[k] == 0) nr--;
        else if (dirs[k] == 1) nr++;
        else if (dirs[k] == 2) nc++;
        else                   nc--;
        if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
        if (vis[nr][nc]) continue;
        if      (nr == r - 1) conn_ns[r - 1][c] = 1;
        else if (nr == r + 1) conn_ns[r][c]     = 1;
        else if (nc == c + 1) conn_ew[r][c]     = 1;
        else                  conn_ew[r][c - 1] = 1;
        dfs_maze(nr, nc, vis);
    }
}

static int place_chest_in_room(int r, int c) {
    Room *rm = &rooms[r][c];
    for (int at = 0; at < 50; at++) {
        int cx = 2 + rand() % (ROOM_W - 4);
        int cy = 2 + rand() % (ROOM_H - 4);
        if (room_walls[r][c][cy][cx]) continue;
        int ov = 0;
        for (int i = 0; i < rm->num_monsters; i++)
            if (rm->monsters[i].x == cx && rm->monsters[i].y == cy) ov = 1;
        if (ov) continue;
        if (rm->has_potion && rm->potion_x == cx && rm->potion_y == cy) continue;
        rm->has_chest = 1; rm->chest_x = cx; rm->chest_y = cy;
        return 1;
    }
    return 0;
}

static int place_monster_in_room(int r, int c) {
    Room *rm = &rooms[r][c];
    if (rm->num_monsters >= cfg.max_monsters) return 0;
    int roll = rand() % 100;
    char t;
    if (roll < cfg.g_pct)                       t = 'G';
    else if (roll < cfg.g_pct + cfg.b_pct)      t = 'B';
    else                                        t = 'D';

    for (int at = 0; at < 50; at++) {
        int mx = 2 + rand() % (ROOM_W - 4);
        int my = 2 + rand() % (ROOM_H - 4);
        if (room_walls[r][c][my][mx]) continue;
        int ov = 0;
        for (int j = 0; j < rm->num_monsters; j++)
            if (rm->monsters[j].x == mx && rm->monsters[j].y == my) ov = 1;
        if (ov) continue;
        if (rm->has_chest && rm->chest_x == mx && rm->chest_y == my) continue;
        Monster *m = &rm->monsters[rm->num_monsters++];
        m->x = mx; m->y = my; m->alive = 1; m->type = t;
        return 1;
    }
    return 0;
}

void generate_dungeon(void) {
    memset(conn_ew, 0, sizeof(conn_ew));
    memset(conn_ns, 0, sizeof(conn_ns));
    memset(rooms, 0, sizeof(rooms));
    memset(seen, 0, sizeof(seen));
    memset(known_threat, 0, sizeof(known_threat));
    static int vis[GRID_H][GRID_W];
    memset(vis, 0, sizeof(vis));
    dfs_maze(0, 0, vis);

    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W; c++) {
        if (c < GRID_W - 1 && !conn_ew[r][c] && rand() % 100 < cfg.loop_pct) conn_ew[r][c] = 1;
        if (r < GRID_H - 1 && !conn_ns[r][c] && rand() % 100 < cfg.loop_pct) conn_ns[r][c] = 1;
    }
    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W - 1; c++)
        if (conn_ew[r][c]) door_ew[r][c] = 1 + rand() % (ROOM_H - 2);
    for (int r = 0; r < GRID_H - 1; r++) for (int c = 0; c < GRID_W; c++)
        if (conn_ns[r][c]) door_ns[r][c] = 1 + rand() % (ROOM_W - 2);

    int cum_t = cfg.treasure_pct;
    int cum_d = cum_t + cfg.den_pct;
    int cum_m = cum_d + cfg.maze_pct;
    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W; c++) {
        int roll = rand() % 100;
        if (roll < cum_t) rooms[r][c].type = ROOM_TREASURE;
        else if (roll < cum_d) rooms[r][c].type = ROOM_DEN;
        else if (roll < cum_m) rooms[r][c].type = ROOM_MAZE;
        else rooms[r][c].type = ROOM_EMPTY;
    }

    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W; c++) {
        Room *rm = &rooms[r][c];
        if (r > 0          && conn_ns[r-1][c]) { rm->connect_n = 1; rm->door_n = door_ns[r-1][c]; }
        if (r < GRID_H - 1 && conn_ns[r][c])   { rm->connect_s = 1; rm->door_s = door_ns[r][c];   }
        if (c < GRID_W - 1 && conn_ew[r][c])   { rm->connect_e = 1; rm->door_e = door_ew[r][c];   }
        if (c > 0          && conn_ew[r][c-1]) { rm->connect_w = 1; rm->door_w = door_ew[r][c-1]; }
    }

    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W; c++) {
        int wmin = cfg.walls_min, wmax = cfg.walls_max;
        switch (rooms[r][c].type) {
        case ROOM_TREASURE: wmin = 0; wmax = 1; break;
        case ROOM_DEN:      wmin = 5; wmax = 8; break;
        case ROOM_MAZE:     wmin = 8; wmax = 10; break;
        case ROOM_EMPTY:
        default: break;
        }
        generate_room_walls(r, c, wmin, wmax);
    }

    total_chests = 0; chests_collected = 0;
    for (int r = 0; r < GRID_H; r++) for (int c = 0; c < GRID_W; c++) {
        Room *rm = &rooms[r][c];
        switch (rm->type) {
        case ROOM_TREASURE:
            if (place_chest_in_room(r, c)) total_chests++;
            break;
        case ROOM_DEN: {
            int n = 2 + rand() % 2;
            if (n > cfg.max_monsters) n = cfg.max_monsters;
            for (int i = 0; i < n; i++) place_monster_in_room(r, c);
            break;
        }
        case ROOM_MAZE:
            if (place_chest_in_room(r, c)) total_chests++;
            break;
        case ROOM_EMPTY:
        default:
            if (rand() % 100 < cfg.monster_density) {
                int n = 1 + rand() % 2;
                for (int i = 0; i < n; i++) place_monster_in_room(r, c);
            }
            if (rand() % 100 < cfg.chest_density) {
                if (place_chest_in_room(r, c)) total_chests++;
            }
            break;
        }
    }

    if (total_chests == 0) {
        for (int r = 0; r < GRID_H && total_chests == 0; r++)
            for (int c = 0; c < GRID_W && total_chests == 0; c++)
                if (place_chest_in_room(r, c)) total_chests++;
    }

    {
        int pr = rand() % GRID_H;
        int pc = rand() % GRID_W;
        Room *prm = &rooms[pr][pc];
        for (int at = 0; at < 200; at++) {
            int px = 2 + rand() % (ROOM_W - 4);
            int py = 2 + rand() % (ROOM_H - 4);
            if (room_walls[pr][pc][py][px]) continue;
            if (prm->has_chest && prm->chest_x == px && prm->chest_y == py) continue;
            int ov = 0;
            for (int i = 0; i < prm->num_monsters; i++)
                if (prm->monsters[i].x == px && prm->monsters[i].y == py) ov = 1;
            if (ov) continue;
            prm->has_potion = 1;
            prm->potion_x = px;
            prm->potion_y = py;
            break;
        }
    }
}

void spawn_hero(void) {
    hero_r = rand() % GRID_H; hero_c = rand() % GRID_W;
    Room *rm = &rooms[hero_r][hero_c];
    for (int at = 0; at < 500; at++) {
        hero_x = 1 + rand() % (ROOM_W - 2);
        hero_y = 1 + rand() % (ROOM_H - 2);
        if (room_walls[hero_r][hero_c][hero_y][hero_x]) continue;
        int ok = 1;
        if (rm->has_chest && hero_x == rm->chest_x && hero_y == rm->chest_y) ok = 0;
        if (rm->has_potion && hero_x == rm->potion_x && hero_y == rm->potion_y) ok = 0;
        for (int i = 0; i < rm->num_monsters; i++)
            if (rm->monsters[i].alive && rm->monsters[i].x == hero_x && rm->monsters[i].y == hero_y) ok = 0;
        if (ok) break;
    }
    hero_hp = cfg.start_hp;
    rm->visited = 1;
    last_dx = last_dy = 0;
    damage_flash = 0;
    update_seen();
}

void hero_step(int dx, int dy) {
    Room *rm = &rooms[hero_r][hero_c];
    int nx = hero_x + dx, ny = hero_y + dy;
    int old_r = hero_r, old_c = hero_c, moved = 0;
    if      (rm->connect_n && dx == 0  && dy == -1 && nx == rm->door_n    && ny == 0)          { hero_r--; hero_y = ROOM_H - 2; moved = 1; }
    else if (rm->connect_s && dx == 0  && dy == 1  && nx == rm->door_s    && ny == ROOM_H - 1) { hero_r++; hero_y = 1;           moved = 1; }
    else if (rm->connect_w && dx == -1 && dy == 0  && nx == 0             && ny == rm->door_w) { hero_c--; hero_x = ROOM_W - 2;  moved = 1; }
    else if (rm->connect_e && dx == 1  && dy == 0  && nx == ROOM_W - 1    && ny == rm->door_e) { hero_c++; hero_x = 1;           moved = 1; }
    if (moved) {
        rooms[old_r][old_c].visited = 1;
        rooms[hero_r][hero_c].visited = 1;
        prev_room_r = old_r; prev_room_c = old_c;
        last_dx = dx; last_dy = dy;
        update_seen();
        return;
    }
    if (is_wall(nx, ny)) return;
    if (room_walls[hero_r][hero_c][ny][nx]) return;
    hero_x = nx; hero_y = ny;
    last_dx = dx; last_dy = dy;
    update_seen();
}

void collect_potion(void) {
    Room *rm = &rooms[hero_r][hero_c];
    if (rm->has_potion && hero_x == rm->potion_x && hero_y == rm->potion_y) {
        if (hero_hp >= cfg.start_hp) return;
        rm->has_potion = 0;
        hero_hp++;
    }
}

void move_monsters(void) {
    Room *rm = &rooms[hero_r][hero_c];

    int need_dist = 0;
    for (int i = 0; i < rm->num_monsters; i++)
        if (rm->monsters[i].alive && rm->monsters[i].type == 'D') { need_dist = 1; break; }
    if (need_dist) compute_dist_map(hero_r, hero_c, hero_x, hero_y);

    for (int i = 0; i < rm->num_monsters; i++) {
        Monster *m = &rm->monsters[i];
        if (!m->alive) continue;
        int should = (m->type == 'G' && tick % 3 == 0) ||
                     (m->type == 'B') ||
                     (m->type == 'D' && tick % 2 == 0);
        if (!should) continue;

        int mdx = 0, mdy = 0;
        if (m->type == 'D') {
            int cur_d = dist_map[m->y][m->x];
            if (cur_d >= INF_DIST) continue;
            int best_d = cur_d;
            int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
            for (int k = 0; k < 4; k++) {
                int nx = m->x + dirs[k][0], ny = m->y + dirs[k][1];
                if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
                if (room_walls[hero_r][hero_c][ny][nx]) continue;
                if (dist_map[ny][nx] < best_d) {
                    best_d = dist_map[ny][nx];
                    mdx = dirs[k][0]; mdy = dirs[k][1];
                }
            }
        } else {
            int rr = rand() % 5;
            if      (rr == 0) mdx = -1;
            else if (rr == 1) mdx = 1;
            else if (rr == 2) mdy = -1;
            else if (rr == 3) mdy = 1;
        }

        int nx = m->x + mdx, ny = m->y + mdy;
        if (is_wall(nx, ny)) {
            /* outer wall — blocked */
        } else if (room_walls[hero_r][hero_c][ny][nx]) {
            if (m->type == 'B') {
                int fx = nx + mdx, fy = ny + mdy;
                if (fx >= 1 && fx < ROOM_W - 1 && fy >= 1 && fy < ROOM_H - 1 &&
                    !room_walls[hero_r][hero_c][fy][fx]) {
                    m->x = fx; m->y = fy;
                }
            }
        } else {
            m->x = nx; m->y = ny;
        }
    }
}

void check_collisions(void) {
    Room *rm = &rooms[hero_r][hero_c];
    for (int i = 0; i < rm->num_monsters; i++) {
        Monster *m = &rm->monsters[i];
        if (!m->alive) continue;
        if (m->x == hero_x && m->y == hero_y) {
            m->alive = 0; kills++;
            score += 10;
            if (m->type == 'D') {
                hero_hp--;
                damage_flash = 3;
            }
        }
    }
}

void collect_chest(void) {
    Room *rm = &rooms[hero_r][hero_c];
    if (rm->has_chest && hero_x == rm->chest_x && hero_y == rm->chest_y) {
        rm->has_chest = 0;
        chests_collected++;
        score += 100;
    }
}

int check_room_reachability(int r, int c) {
    Room *rm = &rooms[r][c];
    unsigned char (*w)[ROOM_W] = room_walls[r][c];

    Point pts[8];
    int np = 0;
    if (rm->connect_n) pts[np++] = (Point){rm->door_n, 1};
    if (rm->connect_s) pts[np++] = (Point){rm->door_s, ROOM_H - 2};
    if (rm->connect_w) pts[np++] = (Point){1, rm->door_w};
    if (rm->connect_e) pts[np++] = (Point){ROOM_W - 2, rm->door_e};
    if (rm->has_chest) pts[np++] = (Point){rm->chest_x, rm->chest_y};
    if (rm->has_potion) pts[np++] = (Point){rm->potion_x, rm->potion_y};
    if (np == 0) return 1;

    for (int i = 0; i < np; i++)
        if (w[pts[i].y][pts[i].x]) return 0;

    static int vis[ROOM_H][ROOM_W];
    static Point q[ROOM_W * ROOM_H];
    memset(vis, 0, sizeof(vis));
    int qh = 0, qt = 0;
    q[qt++] = pts[0]; vis[pts[0].y][pts[0].x] = 1;
    int m[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (qh < qt) {
        Point cur = q[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + m[k][0], ny = cur.y + m[k][1];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (w[ny][nx] || vis[ny][nx]) continue;
            vis[ny][nx] = 1;
            q[qt++] = (Point){nx, ny};
        }
    }
    for (int i = 1; i < np; i++)
        if (!vis[pts[i].y][pts[i].x]) return 0;
    return 1;
}
