#include "dungeon.h"
#include <stdlib.h>
#include <string.h>

/* =========================================================
   Shared helper
   ========================================================= */

static int try_move(int sdx, int sdy) {
    int ox = hero_x, oy = hero_y, orr = hero_r, occ = hero_c;
    hero_step(sdx, sdy);
    return !(hero_x == ox && hero_y == oy && hero_r == orr && hero_c == occ);
}

/* =========================================================
   SUPERMAN — full knowledge
   ========================================================= */

static int cell_danger_truth(int x, int y, int skip) {
    Room *rm = &rooms[hero_r][hero_c];
    int dg = 0;
    for (int i = 0; i < rm->num_monsters; i++) {
        if (!rm->monsters[i].alive) continue;
        if (i == skip) continue;
        int md = abs(rm->monsters[i].x - x) + abs(rm->monsters[i].y - y);
        if (md <= 2) dg += (3 - md);
    }
    return dg;
}

static void superman_move(int tx, int ty) {
    compute_dist_map(hero_r, hero_c, tx, ty);
    int cur_d = dist_map[hero_y][hero_x];
    if (cur_d >= INF_DIST || cur_d == 0) return;

    Room *rm = &rooms[hero_r][hero_c];
    int attack_idx = -1, threat = 0;
    for (int i = 0; i < rm->num_monsters; i++) {
        if (!rm->monsters[i].alive) continue;
        if (rm->monsters[i].x == tx && rm->monsters[i].y == ty) attack_idx = i;
        int md = abs(rm->monsters[i].x - hero_x) + abs(rm->monsters[i].y - hero_y);
        if (md < 4) threat = 1;
    }
    if (!threat && cur_d > 3 && (rand() % 100) < 3) return;
    if (!threat && cur_d > 5 && (rand() % 100) < 8) {
        int px[2] = {0, 0}, py[2] = {0, 0};
        if (last_dx != 0) { py[0] = 1; py[1] = -1; }
        else              { px[0] = 1; px[1] = -1; }
        int start = rand() % 2;
        for (int i = 0; i < 2; i++) {
            int k = (start + i) % 2;
            int nx = hero_x + px[k], ny = hero_y + py[k];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (room_walls[hero_r][hero_c][ny][nx]) continue;
            if (dist_map[ny][nx] >= INF_DIST) continue;
            if (cell_danger_truth(nx, ny, attack_idx) > 0) continue;
            if (try_move(px[k], py[k])) return;
        }
    }
    int cand[4][2], danger[4], nc = 0;
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int k = 0; k < 4; k++) {
        int nx = hero_x + dirs[k][0], ny = hero_y + dirs[k][1];
        if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
        if (room_walls[hero_r][hero_c][ny][nx]) continue;
        if (dist_map[ny][nx] != cur_d - 1) continue;
        cand[nc][0] = dirs[k][0]; cand[nc][1] = dirs[k][1];
        danger[nc] = cell_danger_truth(nx, ny, attack_idx);
        nc++;
    }
    if (nc == 0) return;
    int min_d = danger[0];
    for (int i = 1; i < nc; i++) if (danger[i] < min_d) min_d = danger[i];
    int keep[4][2], kc = 0;
    for (int i = 0; i < nc; i++)
        if (danger[i] == min_d) { keep[kc][0] = cand[i][0]; keep[kc][1] = cand[i][1]; kc++; }
    nc = kc;
    for (int i = 0; i < nc; i++) { cand[i][0] = keep[i][0]; cand[i][1] = keep[i][1]; }
    int pick = -1;
    for (int i = 0; i < nc; i++)
        if (cand[i][0] == last_dx && cand[i][1] == last_dy) { pick = i; break; }
    if (pick < 0 || (rand() % 100) < 30) pick = rand() % nc;
    try_move(cand[pick][0], cand[pick][1]);
}

static int bfs_room_next_step(int sr, int sc, int tr, int tc, int *nr, int *nc) {
    static int vis[GRID_H][GRID_W];
    static Point par[GRID_H][GRID_W];
    static Point q[GRID_H * GRID_W];
    memset(vis, 0, sizeof(vis));
    int qh = 0, qt = 0;
    q[qt++] = (Point){sc, sr}; vis[sr][sc] = 1; par[sr][sc] = (Point){-1, -1};
    while (qh < qt) {
        Point cur = q[qh++];
        if (cur.y == tr && cur.x == tc) {
            Point step = cur, p = par[cur.y][cur.x];
            while (p.x != -1 && !(p.y == sr && p.x == sc)) { step = p; p = par[step.y][step.x]; }
            *nr = step.y; *nc = step.x;
            return 1;
        }
        Room *rm = &rooms[cur.y][cur.x];
        int nr_ar[4] = {cur.y - 1, cur.y + 1, cur.y, cur.y};
        int nc_ar[4] = {cur.x, cur.x, cur.x - 1, cur.x + 1};
        int valid[4] = {rm->connect_n, rm->connect_s, rm->connect_w, rm->connect_e};
        for (int k = 0; k < 4; k++) {
            if (!valid[k]) continue;
            int r = nr_ar[k], c = nc_ar[k];
            if (r < 0 || r >= GRID_H || c < 0 || c >= GRID_W) continue;
            if (vis[r][c]) continue;
            vis[r][c] = 1; par[r][c] = cur; q[qt++] = (Point){c, r};
        }
    }
    return 0;
}

static void superman_pick_target(void) {
    static int vis[GRID_H][GRID_W];
    static Point par[GRID_H][GRID_W];
    static Point q[GRID_H * GRID_W];
    memset(vis, 0, sizeof(vis));
    int qh = 0, qt = 0;
    q[qt++] = (Point){hero_c, hero_r}; vis[hero_r][hero_c] = 1;
    par[hero_r][hero_c] = (Point){-1, -1};
    int fb_r = -1, fb_c = -1;
    while (qh < qt) {
        Point cur = q[qh++];
        int r = cur.y, c = cur.x;
        if (!(r == hero_r && c == hero_c) && room_has_content(r, c)) {
            Point step = cur, p = par[r][c];
            while (p.x != -1 && !(p.y == hero_r && p.x == hero_c)) { step = p; p = par[step.y][step.x]; }
            if (step.y != prev_room_r || step.x != prev_room_c) {
                target_room_r = r; target_room_c = c; return;
            }
            if (fb_r < 0) { fb_r = r; fb_c = c; }
        }
        Room *rm = &rooms[r][c];
        int nr_ar[4] = {r - 1, r + 1, r, r};
        int nc_ar[4] = {c, c, c - 1, c + 1};
        int valid[4] = {rm->connect_n, rm->connect_s, rm->connect_w, rm->connect_e};
        for (int k = 0; k < 4; k++) {
            if (!valid[k]) continue;
            int nr = nr_ar[k], nc = nc_ar[k];
            if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
            if (vis[nr][nc]) continue;
            vis[nr][nc] = 1; par[nr][nc] = cur; q[qt++] = (Point){nc, nr};
        }
    }
    if (fb_r >= 0) { target_room_r = fb_r; target_room_c = fb_c; }
    else { target_room_r = -1; target_room_c = -1; }
}

static void superman_tick(void) {
    Room *rm = &rooms[hero_r][hero_c];
    if (rm->has_chest) { superman_move(rm->chest_x, rm->chest_y); return; }
    if (rm->has_potion && hero_hp < cfg.start_hp) {
        superman_move(rm->potion_x, rm->potion_y); return;
    }
    int mi = -1, mdist = 999999;
    for (int i = 0; i < rm->num_monsters; i++) {
        if (!rm->monsters[i].alive) continue;
        if (hero_hp <= 1 && rm->monsters[i].type == 'D') continue;
        int d = abs(rm->monsters[i].x - hero_x) + abs(rm->monsters[i].y - hero_y);
        if (d < mdist) { mdist = d; mi = i; }
    }
    if (mi >= 0) { superman_move(rm->monsters[mi].x, rm->monsters[mi].y); return; }
    if (target_room_r < 0 || !room_has_content(target_room_r, target_room_c) ||
        (target_room_r == hero_r && target_room_c == hero_c))
        superman_pick_target();
    if (target_room_r < 0) return;
    int next_r, next_c;
    if (!bfs_room_next_step(hero_r, hero_c, target_room_r, target_room_c, &next_r, &next_c)) return;
    int inner_x = -1, inner_y = -1, ddx = 0, ddy = 0;
    if      (next_r == hero_r - 1 && rm->connect_n) { inner_x = rm->door_n; inner_y = 1;          ddx = 0;  ddy = -1; }
    else if (next_r == hero_r + 1 && rm->connect_s) { inner_x = rm->door_s; inner_y = ROOM_H - 2; ddx = 0;  ddy = 1;  }
    else if (next_c == hero_c - 1 && rm->connect_w) { inner_x = 1;          inner_y = rm->door_w; ddx = -1; ddy = 0;  }
    else if (next_c == hero_c + 1 && rm->connect_e) { inner_x = ROOM_W - 2; inner_y = rm->door_e; ddx = 1;  ddy = 0;  }
    else return;
    if (hero_x == inner_x && hero_y == inner_y) hero_step(ddx, ddy);
    else superman_move(inner_x, inner_y);
}

/* =========================================================
   JONES — same code as Superman, different knowledge source
   ========================================================= */

static int cell_danger_known(int x, int y, int skip_x, int skip_y) {
    int dg = 0;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int md = abs(dx) + abs(dy);
            if (md > 2 || md == 0) continue;
            int cx = x + dx, cy = y + dy;
            if (cx < 1 || cx >= ROOM_W - 1 || cy < 1 || cy >= ROOM_H - 1) continue;
            if (cx == skip_x && cy == skip_y) continue;
            if (known_threat[hero_r][hero_c][cy][cx]) dg += (3 - md);
        }
    }
    return dg;
}

/* Returns 1 if the hero moved, 0 otherwise. */
static int jones_move(int tx, int ty) {
    compute_dist_map_jones(hero_r, hero_c, tx, ty);

    int cur_d = dist_map[hero_y][hero_x];
    if (cur_d >= INF_DIST || cur_d == 0) return 0;

    int threat = 0;
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int md = abs(dx) + abs(dy);
            if (md == 0 || md >= 4) continue;
            int cx = hero_x + dx, cy = hero_y + dy;
            if (cx < 1 || cx >= ROOM_W - 1 || cy < 1 || cy >= ROOM_H - 1) continue;
            if (known_threat[hero_r][hero_c][cy][cx]) { threat = 1; break; }
        }
        if (threat) break;
    }
    if (!threat && cur_d > 3 && (rand() % 100) < 3) return 0;
    if (!threat && cur_d > 5 && (rand() % 100) < 8) {
        int px[2] = {0, 0}, py[2] = {0, 0};
        if (last_dx != 0) { py[0] = 1; py[1] = -1; }
        else              { px[0] = 1; px[1] = -1; }
        int start = rand() % 2;
        for (int i = 0; i < 2; i++) {
            int k = (start + i) % 2;
            int nx = hero_x + px[k], ny = hero_y + py[k];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (!seen[hero_r][hero_c][ny][nx]) continue;
            if (room_walls[hero_r][hero_c][ny][nx]) continue;
            if (dist_map[ny][nx] >= INF_DIST) continue;
            if (cell_danger_known(nx, ny, tx, ty) > 0) continue;
            if (try_move(px[k], py[k])) return 1;
        }
    }
    int cand[4][2], danger[4], nc = 0;
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int k = 0; k < 4; k++) {
        int nx = hero_x + dirs[k][0], ny = hero_y + dirs[k][1];
        if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
        if (!seen[hero_r][hero_c][ny][nx]) continue;
        if (room_walls[hero_r][hero_c][ny][nx]) continue;
        if (dist_map[ny][nx] != cur_d - 1) continue;
        cand[nc][0] = dirs[k][0]; cand[nc][1] = dirs[k][1];
        danger[nc] = cell_danger_known(nx, ny, tx, ty);
        nc++;
    }
    if (nc == 0) return 0;
    int min_d = danger[0];
    for (int i = 1; i < nc; i++) if (danger[i] < min_d) min_d = danger[i];
    int keep[4][2], kc = 0;
    for (int i = 0; i < nc; i++)
        if (danger[i] == min_d) { keep[kc][0] = cand[i][0]; keep[kc][1] = cand[i][1]; kc++; }
    nc = kc;
    for (int i = 0; i < nc; i++) { cand[i][0] = keep[i][0]; cand[i][1] = keep[i][1]; }
    int pick = -1;
    for (int i = 0; i < nc; i++)
        if (cand[i][0] == last_dx && cand[i][1] == last_dy) { pick = i; break; }
    if (pick < 0 || (rand() % 100) < 30) pick = rand() % nc;
    return try_move(cand[pick][0], cand[pick][1]);
}

/* Does this room have anything worth returning for, from Jones's memory? */
static int jones_room_has_potential(int r, int c) {
    Room *t = &rooms[r][c];
    if (!t->visited) return 1;
    if (t->has_chest && seen[r][c][t->chest_y][t->chest_x]) return 1;
    if (t->has_potion && hero_hp < cfg.start_hp &&
        seen[r][c][t->potion_y][t->potion_x]) return 1;
    for (int y = 1; y < ROOM_H - 1; y++)
        for (int x = 1; x < ROOM_W - 1; x++) {
            if (!seen[r][c][y][x]) return 1;
            if (known_threat[r][c][y][x]) return 1;
        }
    return 0;
}

/* Same structure as superman_pick_target, different predicate. */
static void jones_pick_target(void) {
    static int vis[GRID_H][GRID_W];
    static Point par[GRID_H][GRID_W];
    static Point q[GRID_H * GRID_W];
    memset(vis, 0, sizeof(vis));
    int qh = 0, qt = 0;
    q[qt++] = (Point){hero_c, hero_r}; vis[hero_r][hero_c] = 1;
    par[hero_r][hero_c] = (Point){-1, -1};
    int fb_r = -1, fb_c = -1;
    while (qh < qt) {
        Point cur = q[qh++];
        int r = cur.y, c = cur.x;
        if (!(r == hero_r && c == hero_c) && jones_room_has_potential(r, c)) {
            Point step = cur, p = par[r][c];
            while (p.x != -1 && !(p.y == hero_r && p.x == hero_c)) { step = p; p = par[step.y][step.x]; }
            if (step.y != prev_room_r || step.x != prev_room_c) {
                target_room_r = r; target_room_c = c; return;
            }
            if (fb_r < 0) { fb_r = r; fb_c = c; }
        }
        Room *rm = &rooms[r][c];
        int nr_ar[4] = {r - 1, r + 1, r, r};
        int nc_ar[4] = {c, c, c - 1, c + 1};
        int valid[4] = {rm->connect_n, rm->connect_s, rm->connect_w, rm->connect_e};
        for (int k = 0; k < 4; k++) {
            if (!valid[k]) continue;
            int nr = nr_ar[k], nc = nc_ar[k];
            if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
            if (vis[nr][nc]) continue;
            vis[nr][nc] = 1; par[nr][nc] = cur; q[qt++] = (Point){nc, nr};
        }
    }
    if (fb_r >= 0) { target_room_r = fb_r; target_room_c = fb_c; }
    else { target_room_r = -1; target_room_c = -1; }
}

/* Fallback when navigation to the target room fails:
   take one step toward the nearest unseen cell in the current room.
   Keeps Jones from permanently freezing if a doorway is unreachable
   through known floor. Only fires if the room has unseen cells. */
static int jones_step_toward_unknown(int *sdx, int *sdy) {
    static int dist[ROOM_H][ROOM_W];
    static Point par[ROOM_H][ROOM_W];
    static Point q[ROOM_W * ROOM_H];
    memset(dist, 0xff, sizeof(dist));

    int qh = 0, qt = 0;
    int m[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int y = 1; y < ROOM_H - 1; y++) {
        for (int x = 1; x < ROOM_W - 1; x++) {
            if (seen[hero_r][hero_c][y][x]) continue;
            int adj = 0;
            for (int k = 0; k < 4; k++) {
                int nx = x + m[k][0], ny = y + m[k][1];
                if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
                if (seen[hero_r][hero_c][ny][nx] &&
                    !room_walls[hero_r][hero_c][ny][nx]) { adj = 1; break; }
            }
            if (adj) {
                dist[y][x] = 0;
                par[y][x] = (Point){-1, -1};
                q[qt++] = (Point){x, y};
            }
        }
    }
    if (qh == qt) return 0;

    while (qh < qt) {
        Point cur = q[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + m[k][0], ny = cur.y + m[k][1];
            if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
            if (dist[ny][nx] != -1) continue;
            if (!seen[hero_r][hero_c][ny][nx]) continue;
            if (room_walls[hero_r][hero_c][ny][nx]) continue;
            dist[ny][nx] = dist[cur.y][cur.x] + 1;
            par[ny][nx] = cur;
            q[qt++] = (Point){nx, ny};
        }
    }

    if (dist[hero_y][hero_x] < 0) return 0;
    Point first = par[hero_y][hero_x];
    if (first.x == -1) return 0;
    *sdx = first.x - hero_x;
    *sdy = first.y - hero_y;
    return 1;
}

static void jones_tick(void) {
    Room *rm = &rooms[hero_r][hero_c];

    /* Same priority as Superman, but every check is gated on seen[]. */
    if (rm->has_chest && seen[hero_r][hero_c][rm->chest_y][rm->chest_x]) {
        if (jones_move(rm->chest_x, rm->chest_y)) return;
    }
    if (rm->has_potion && hero_hp < cfg.start_hp &&
        seen[hero_r][hero_c][rm->potion_y][rm->potion_x]) {
        if (jones_move(rm->potion_x, rm->potion_y)) return;
    }

    int mi = -1, mdist = 999999;
    for (int i = 0; i < rm->num_monsters; i++) {
        Monster *m = &rm->monsters[i];
        if (!m->alive) continue;
        if (!seen[hero_r][hero_c][m->y][m->x]) continue;
        if (hero_hp <= 1 && m->type == 'D') continue;
        int d = abs(m->x - hero_x) + abs(m->y - hero_y);
        if (d < mdist) { mdist = d; mi = i; }
    }
    if (mi >= 0) {
        if (jones_move(rm->monsters[mi].x, rm->monsters[mi].y)) return;
    }

    /* Navigate to a room with potential content. */
    if (target_room_r < 0 ||
        (target_room_r == hero_r && target_room_c == hero_c) ||
        !jones_room_has_potential(target_room_r, target_room_c)) {
        jones_pick_target();
    }

    /* Wander fallback: no room in the dungeon has potential. */
    if (target_room_r < 0) {
        int adj_r[4], adj_c[4], n = 0;
        if (rm->connect_n) { adj_r[n] = hero_r - 1; adj_c[n] = hero_c;     n++; }
        if (rm->connect_s) { adj_r[n] = hero_r + 1; adj_c[n] = hero_c;     n++; }
        if (rm->connect_w) { adj_r[n] = hero_r;     adj_c[n] = hero_c - 1; n++; }
        if (rm->connect_e) { adj_r[n] = hero_r;     adj_c[n] = hero_c + 1; n++; }
        int found = -1;
        for (int i = 0; i < n; i++)
            if (adj_r[i] != prev_room_r || adj_c[i] != prev_room_c) { found = i; break; }
        if (found < 0 && n > 0) found = 0;
        if (found >= 0) { target_room_r = adj_r[found]; target_room_c = adj_c[found]; }
    }

    if (target_room_r >= 0) {
        int next_r, next_c;
        if (bfs_room_next_step(hero_r, hero_c, target_room_r, target_room_c, &next_r, &next_c)) {
            int inner_x = -1, inner_y = -1, ddx = 0, ddy = 0;
            if      (next_r == hero_r - 1 && rm->connect_n) { inner_x = rm->door_n; inner_y = 1;          ddx = 0;  ddy = -1; }
            else if (next_r == hero_r + 1 && rm->connect_s) { inner_x = rm->door_s; inner_y = ROOM_H - 2; ddx = 0;  ddy = 1;  }
            else if (next_c == hero_c - 1 && rm->connect_w) { inner_x = 1;          inner_y = rm->door_w; ddx = -1; ddy = 0;  }
            else if (next_c == hero_c + 1 && rm->connect_e) { inner_x = ROOM_W - 2; inner_y = rm->door_e; ddx = 1;  ddy = 0;  }
            if (inner_x >= 0) {
                if (hero_x == inner_x && hero_y == inner_y) { hero_step(ddx, ddy); return; }
                if (jones_move(inner_x, inner_y)) return;
            }
        }
    }

    /* Last resort: reveal a new cell in the current room. Keeps Jones
       out of a permanent freeze when the doorway path is blocked by
       terrain he hasn't observed yet. */
    int sdx = 0, sdy = 0;
    if (jones_step_toward_unknown(&sdx, &sdy))
        try_move(sdx, sdy);
}

/* =========================================================
   Entry point
   ========================================================= */

void hero_tick_ai(void) {
    if (cfg.ai_personality == AI_SUPERMAN) superman_tick();
    else                                   jones_tick();
}
