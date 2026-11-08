#include "dungeon.h"
#include <ncurses.h>
#include <string.h>

static int room_type_pair(RoomType t) {
    switch (t) {
    case ROOM_TREASURE: return 3;
    case ROOM_DEN:      return 2;
    case ROOM_MAZE:     return 7;
    default:            return 0;
    }
}

static const char *room_type_tag(RoomType t) {
    switch (t) {
    case ROOM_TREASURE: return "[$] ";
    case ROOM_DEN:      return "[!] ";
    case ROOM_MAZE:     return "[~] ";
    default:            return "[ ] ";
    }
}

static int mm_has_alive_monster(int r, int c) {
    for (int i = 0; i < rooms[r][c].num_monsters; i++)
        if (rooms[r][c].monsters[i].alive) return 1;
    return 0;
}

static void render_minimap(int ox, int oy) {
    int mm_x = ox + ROOM_W + 4;
    int mm_y = oy;
    int w = 2 * GRID_W + 1;
    int h = 2 * GRID_H + 1;
    if (mm_x + w >= COLS) return;
    if (mm_y + h >= LINES) return;

    for (int row = 0; row <= GRID_H; row++) {
        int y = mm_y + row * 2;
        for (int col = 0; col <= GRID_W; col++) {
            mvaddch(y, mm_x + col * 2, '+');
            if (col < GRID_W) mvaddch(y, mm_x + col * 2 + 1, '-');
        }
    }
    for (int row = 0; row < GRID_H; row++) {
        int y = mm_y + row * 2 + 1;
        for (int col = 0; col <= GRID_W; col++)
            mvaddch(y, mm_x + col * 2, '|');
    }
    for (int row = 0; row < GRID_H; row++) {
        for (int col = 0; col < GRID_W; col++) {
            int y = mm_y + row * 2 + 1;
            int x = mm_x + col * 2 + 1;
            Room *rm = &rooms[row][col];
            int tp = room_type_pair(rm->type);

            if (row == hero_r && col == hero_c) {
                if (tp) attron(COLOR_PAIR(tp) | A_REVERSE | A_BOLD);
                else    attron(A_REVERSE | A_BOLD);
                mvaddch(y, x, '@');
                if (tp) attroff(COLOR_PAIR(tp) | A_REVERSE | A_BOLD);
                else    attroff(A_REVERSE | A_BOLD);
            } else if (!rm->visited) {
                mvaddch(y, x, ' ');
            } else if (rm->has_chest &&
                       seen[row][col][rm->chest_y][rm->chest_x]) {
                attron(COLOR_PAIR(3) | A_BOLD);
                mvaddch(y, x, '$');
                attroff(COLOR_PAIR(3) | A_BOLD);
            } else if (mm_has_alive_monster(row, col)) {
                attron(COLOR_PAIR(2) | A_BOLD);
                mvaddch(y, x, '!');
                attroff(COLOR_PAIR(2) | A_BOLD);
            } else if (tp) {
                attron(COLOR_PAIR(tp) | A_DIM);
                mvaddch(y, x, '.');
                attroff(COLOR_PAIR(tp) | A_DIM);
            } else {
                attron(A_DIM);
                mvaddch(y, x, '.');
                attroff(A_DIM);
            }
        }
    }
}

void render(int ai_mode, int paused) {
    erase();
    int ox = (COLS - ROOM_W) / 2;
    int oy = (LINES - ROOM_H) / 2;
    if (ox < 0) ox = 0;
    if (watch_mode) {
        if (oy < 0) oy = 0;
    } else {
        if (oy < 2) oy = 2;
    }

    Room *rm = &rooms[hero_r][hero_c];
    int room_pair = room_type_pair(rm->type);

    if (!watch_mode) {
        const char *tag = room_type_tag(rm->type);

        if (room_pair) attron(COLOR_PAIR(room_pair) | A_BOLD);
        else           attron(A_BOLD);
        mvprintw(oy - 2, ox, "%s", tag);
        if (room_pair) attroff(COLOR_PAIR(room_pair) | A_BOLD);
        else           attroff(A_BOLD);

        if (best_score > score)
            mvprintw(oy - 2, ox + (int)strlen(tag),
                     "Lv %d | Score %d | Best %d | Chests %d/%d | Kills %d | HP: ",
                     level, score, best_score, chests_collected, total_chests, kills);
        else
            mvprintw(oy - 2, ox + (int)strlen(tag),
                     "Lv %d | Score %d | Chests %d/%d | Kills %d | HP: ",
                     level, score, chests_collected, total_chests, kills);
        int hp_attr = COLOR_PAIR(2) | A_BOLD;
        if (damage_flash > 0) hp_attr |= A_REVERSE;
        for (int i = 0; i < hero_hp; i++) { attron(hp_attr); printw("<3"); attroff(hp_attr); }

        const char *ai_name = ai_mode
            ? (cfg.ai_personality == AI_SUPERMAN ? "[S]" : "[J]")
            : "[M]";
        if (paused) ai_name = "[PAUSED]";
        mvprintw(oy - 1, ox, "%s torch:%s seed:%u | i c p t +/- q",
                 ai_name, cfg.torch_enabled ? "on" : "off", cfg.seed);
    }

    int torch = cfg.torch_enabled;
    int r2 = cfg.torch_radius * cfg.torch_radius;

    static unsigned char vis[ROOM_H][ROOM_W];
    for (int y = 0; y < ROOM_H; y++) {
        for (int x = 0; x < ROOM_W; x++) {
            if (!has_los(hero_x, hero_y, x, y, hero_r, hero_c)) { vis[y][x] = 0; continue; }
            if (torch) {
                int dx = x - hero_x, dy = y - hero_y;
                vis[y][x] = (dx*dx + dy*dy <= r2);
            } else {
                vis[y][x] = 1;
            }
        }
    }

    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++) {
            int visible = vis[y][x];
            int was_seen = seen[hero_r][hero_c][y][x];

            if (is_wall(x, y)) {
                int is_door = (rm->connect_n && x == rm->door_n && y == 0) ||
                              (rm->connect_s && x == rm->door_s && y == ROOM_H - 1) ||
                              (rm->connect_w && x == 0          && y == rm->door_w) ||
                              (rm->connect_e && x == ROOM_W - 1 && y == rm->door_e);
                if (visible) {
                    if (room_pair) attron(COLOR_PAIR(room_pair) | A_BOLD);
                    mvaddch(oy + y, ox + x, is_door ? ' ' : '#');
                    if (room_pair) attroff(COLOR_PAIR(room_pair) | A_BOLD);
                } else if (was_seen) {
                    if (room_pair) attron(COLOR_PAIR(room_pair) | A_DIM);
                    else           attron(A_DIM);
                    mvaddch(oy + y, ox + x, is_door ? ' ' : '#');
                    if (room_pair) attroff(COLOR_PAIR(room_pair) | A_DIM);
                    else           attroff(A_DIM);
                } else {
                    attron(A_DIM);
                    mvaddch(oy + y, ox + x, '#');
                    attroff(A_DIM);
                }
            } else if (room_walls[hero_r][hero_c][y][x]) {
                if (visible) {
                    if (room_pair) attron(COLOR_PAIR(room_pair) | A_BOLD);
                    mvaddch(oy + y, ox + x, '#');
                    if (room_pair) attroff(COLOR_PAIR(room_pair) | A_BOLD);
                } else if (was_seen) {
                    if (room_pair) attron(COLOR_PAIR(room_pair) | A_DIM);
                    else           attron(A_DIM);
                    mvaddch(oy + y, ox + x, '#');
                    if (room_pair) attroff(COLOR_PAIR(room_pair) | A_DIM);
                    else           attroff(A_DIM);
                } else {
                    mvaddch(oy + y, ox + x, ' ');
                }
            } else {
                if (visible) {
                    if (room_pair) attron(COLOR_PAIR(room_pair));
                    mvaddch(oy + y, ox + x, '.');
                    if (room_pair) attroff(COLOR_PAIR(room_pair));
                } else if (was_seen) {
                    if (room_pair) attron(COLOR_PAIR(room_pair) | A_DIM);
                    else           attron(A_DIM);
                    mvaddch(oy + y, ox + x, '.');
                    if (room_pair) attroff(COLOR_PAIR(room_pair) | A_DIM);
                    else           attroff(A_DIM);
                } else {
                    mvaddch(oy + y, ox + x, ' ');
                }
            }
        }

    if (rm->has_chest) {
        int visible = vis[rm->chest_y][rm->chest_x];
        int was_seen = seen[hero_r][hero_c][rm->chest_y][rm->chest_x];
        if (visible) {
            attron(COLOR_PAIR(3) | A_BOLD);
            mvaddch(oy + rm->chest_y, ox + rm->chest_x, '$');
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else if (was_seen) {
            attron(COLOR_PAIR(3) | A_DIM);
            mvaddch(oy + rm->chest_y, ox + rm->chest_x, '$');
            attroff(COLOR_PAIR(3) | A_DIM);
        }
    }

    if (rm->has_potion) {
        int visible = vis[rm->potion_y][rm->potion_x];
        int was_seen = seen[hero_r][hero_c][rm->potion_y][rm->potion_x];
        if (visible) {
            attron(COLOR_PAIR(6) | A_BOLD);
            mvaddch(oy + rm->potion_y, ox + rm->potion_x, '+');
            attroff(COLOR_PAIR(6) | A_BOLD);
        } else if (was_seen) {
            attron(COLOR_PAIR(6) | A_DIM);
            mvaddch(oy + rm->potion_y, ox + rm->potion_x, '+');
            attroff(COLOR_PAIR(6) | A_DIM);
        }
    }

    for (int i = 0; i < rm->num_monsters; i++) {
        Monster *m = &rm->monsters[i];
        if (!m->alive) continue;
        if (!vis[m->y][m->x]) continue;
        int cp = (m->type == 'G') ? 1 : (m->type == 'B') ? 4 : 2;
        attron(COLOR_PAIR(cp) | A_BOLD);
        mvaddch(oy + m->y, ox + m->x, m->type);
        attroff(COLOR_PAIR(cp) | A_BOLD);
    }

    attron(COLOR_PAIR(5) | A_BOLD);
    mvaddch(oy + hero_y, ox + hero_x, '1');
    attroff(COLOR_PAIR(5) | A_BOLD);

    render_minimap(ox, oy);
    refresh();
}
