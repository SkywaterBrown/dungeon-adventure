#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include "config.h"
#include "dungeon.h"
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void print_help(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --ai[=who]       start in autonomous mode. who is 'jones' (default)\n");
    printf("                   or 'superman'. Plain --ai means Jones.\n");
    printf("  -w, --watch      hide HUD; center dungeon; suppress end-of-level banner\n");
    printf("  --torch          start with fog of war ON (overrides config)\n");
    printf("  --no-torch       start with fog of war OFF (overrides config)\n");
    printf("  --seed N         RNG seed (0 or omitted = use time)\n");
    printf("  --config PATH    load config from PATH (default: ~/.dungeonrc)\n");
    printf("  --trace FILE     write per-tick state trace to FILE\n");
    printf("  --stress N       run N headless dungeon generations and verify\n");
    printf("                   connectivity invariants (no ncurses)\n");
    printf("  --print-config   print effective configuration and exit\n");
    printf("  --help           show this message\n\n");
    printf("Keys:\n");
    printf("  WASD / arrows    move (manual mode)\n");
    printf("  i                toggle AI on/off\n");
    printf("  c                cycle AI: Jones <-> Superman\n");
    printf("  p                pause / unpause\n");
    printf("  t                toggle torch / fog of war\n");
    printf("  +  /  =          speed up  (decrease tick by 20 ms)\n");
    printf("  -  /  _          slow down (increase tick by 20 ms)\n");
    printf("  q                quit\n\n");
    printf("Trace format (one line per tick, space-separated):\n");
    printf("  tick level x y r c hp chests kills score\n");
}

static char *high_score_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, sizeof(path), "%s/.dungeon_highscore", home);
    return path;
}

static void load_high_score(void) {
    const char *p = high_score_path();
    if (!p) return;
    FILE *f = fopen(p, "r");
    if (!f) return;
    int s = 0, lv = 0, k = 0;
    unsigned int sd = 0;
    long ts = 0;
    if (fscanf(f, "%d %d %d %u %ld", &s, &lv, &k, &sd, &ts) == 5) {
        if (s > 0) { best_score = s; best_score_seed = sd; }
    }
    fclose(f);
}

static void save_high_score(void) {
    const char *p = high_score_path();
    if (!p) return;
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %u %ld\n",
            best_score, level, kills, best_score_seed, (long)time(NULL));
    fclose(f);
}

static int stress_verify_one(unsigned int seed) {
    if (total_chests == 0) {
        printf("FAIL seed=%u: no chests generated\n", seed);
        return 0;
    }
    for (int r = 0; r < GRID_H; r++) {
        for (int c = 0; c < GRID_W; c++) {
            Room *rm = &rooms[r][c];
            if (rm->has_chest && room_walls[r][c][rm->chest_y][rm->chest_x]) {
                printf("FAIL seed=%u: chest on wall in room %d,%d\n", seed, r, c);
                return 0;
            }
            if (rm->has_potion && room_walls[r][c][rm->potion_y][rm->potion_x]) {
                printf("FAIL seed=%u: potion on wall in room %d,%d\n", seed, r, c);
                return 0;
            }
            for (int k = 0; k < rm->num_monsters; k++)
                if (room_walls[r][c][rm->monsters[k].y][rm->monsters[k].x]) {
                    printf("FAIL seed=%u: monster on wall in room %d,%d\n", seed, r, c);
                    return 0;
                }
            if (!check_room_reachability(r, c)) {
                printf("FAIL seed=%u: unreachable region in room %d,%d\n", seed, r, c);
                return 0;
            }
        }
    }
    return 1;
}

static int run_stress(int n, unsigned int base_seed) {
    printf("Stress: %d seeds starting at %u\n", n, base_seed);
    int fails = 0;
    for (int i = 0; i < n; i++) {
        unsigned int s = base_seed + (unsigned int)i;
        srand(s);
        generate_dungeon();
        if (!stress_verify_one(s)) fails++;
    }
    printf("Stress: %d/%d passed, %d failed\n", n - fails, n, fails);
    return fails > 0 ? 1 : 0;
}

int main(int argc, char *argv[]) {
    int ai_mode = 0, print_config = 0, paused = 0;
    int stress_n = 0;
    int have_seed = 0;
    unsigned int seed_override = 0;
    const char *explicit_config = NULL;
    const char *trace_path = NULL;
    FILE *trace_fp = NULL;
    int torch_override = -1;
    int ai_override = -1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { print_help(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--config") && i + 1 < argc) explicit_config = argv[++i];
    }

    config_defaults(&cfg);

    char default_path[512];
    const char *load_path = explicit_config;
    if (!load_path) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(default_path, sizeof(default_path), "%s/.dungeonrc", home);
            load_path = default_path;
        }
    }
    if (load_path) {
        int n = config_load(&cfg, load_path);
        if (n < 0 && explicit_config)
            fprintf(stderr, "warning: config file '%s' not found\n", load_path);
    }

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--ai"))           { ai_mode = 1; }
        else if (!strncmp(a, "--ai=", 5)) {
            ai_mode = 1;
            const char *who = a + 5;
            if      (!strcmp(who, "superman")) ai_override = AI_SUPERMAN;
            else if (!strcmp(who, "jones"))    ai_override = AI_JONES;
            else fprintf(stderr, "warning: unknown --ai value '%s' (using jones)\n", who);
        }
        else if (!strcmp(a, "-w") || !strcmp(a, "--watch")) {
            watch_mode = 1;
            /* Visual only — does NOT touch ai_mode. */
        }
        else if (!strcmp(a, "--torch"))        torch_override = 1;
        else if (!strcmp(a, "--no-torch"))     torch_override = 0;
        else if (!strcmp(a, "--print-config")) print_config = 1;
        else if (!strcmp(a, "--stress") && i + 1 < argc) stress_n = atoi(argv[++i]);
        else if (!strcmp(a, "--trace") && i + 1 < argc) trace_path = argv[++i];
        else if (!strcmp(a, "--seed") && i + 1 < argc) {
            seed_override = (unsigned int)strtoul(argv[++i], NULL, 10);
            have_seed = 1;
        }
        else if (!strcmp(a, "--config") && i + 1 < argc) i++;
    }
    if (ai_override >= 0)     cfg.ai_personality = ai_override;
    if (torch_override >= 0)  cfg.torch_enabled = torch_override;
    if (have_seed)            cfg.seed = seed_override;
    if (cfg.seed == 0)        cfg.seed = (unsigned int)time(NULL);

    if (print_config) { config_print(&cfg); return 0; }
    if (stress_n > 0) return run_stress(stress_n, cfg.seed);

    srand(cfg.seed);
    load_high_score();

    initscr(); cbreak(); noecho(); curs_set(0);
    nodelay(stdscr, TRUE); keypad(stdscr, TRUE);

    int min_lines = watch_mode ? ROOM_H : ROOM_H + 3;
    if (COLS < ROOM_W || LINES < min_lines) {
        endwin();
        fprintf(stderr, "Terminal too small (need at least %dx%d).\n", ROOM_W, min_lines);
        return 1;
    }
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN,   COLOR_BLACK);
        init_pair(2, COLOR_RED,     COLOR_BLACK);
        init_pair(3, COLOR_YELLOW,  COLOR_BLACK);
        init_pair(4, COLOR_CYAN,    COLOR_BLACK);
        init_pair(5, COLOR_WHITE,   COLOR_BLACK);
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(7, COLOR_BLUE,    COLOR_BLACK);
    }

    if (trace_path) {
        trace_fp = fopen(trace_path, "w");
        if (!trace_fp)
            fprintf(stderr, "warning: cannot open trace file '%s'\n", trace_path);
    }

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    level = 1; kills = 0; tick = 0;
    generate_dungeon(); spawn_hero();

    while (1) {
        if (g_stop) break;

        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;
        if (ch == 'i' || ch == 'I') ai_mode = !ai_mode;
        if (ch == 'c' || ch == 'C') {
            cfg.ai_personality = (cfg.ai_personality == AI_JONES) ? AI_SUPERMAN : AI_JONES;
            ai_mode = 1;
        }
        if (ch == 'p' || ch == 'P') paused = !paused;
        if (ch == 't' || ch == 'T') {
            cfg.torch_enabled = !cfg.torch_enabled;
            if (cfg.torch_enabled) mark_visited_rooms_seen();
        }
        if (ch == '+' || ch == '=') {
            cfg.ai_tick_us -= 20000;
            if (cfg.ai_tick_us < 20000) cfg.ai_tick_us = 20000;
        }
        if (ch == '-' || ch == '_') {
            cfg.ai_tick_us += 20000;
            if (cfg.ai_tick_us > 400000) cfg.ai_tick_us = 400000;
        }
        if (ch == KEY_RESIZE) { endwin(); refresh(); clear(); }

        if (paused) { render(ai_mode, paused); usleep(30000); continue; }

        int ticked = 0;
        if (ai_mode) { tick++; hero_tick_ai(); ticked = 1; }
        else {
            int dx = 0, dy = 0;
            if      (ch == KEY_LEFT  || ch == 'a' || ch == 'A') dx = -1;
            else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') dx = 1;
            else if (ch == KEY_UP    || ch == 'w' || ch == 'W') dy = -1;
            else if (ch == KEY_DOWN  || ch == 's' || ch == 'S') dy = 1;
            if (dx || dy) { tick++; hero_step(dx, dy); ticked = 1; }
        }

        if (ticked) {
            collect_chest();
            collect_potion();
            check_collisions();
            move_monsters();
            check_collisions();
            update_seen();

            if (score > best_score) { best_score = score; best_score_seed = cfg.seed; }

            if (trace_fp) {
                fprintf(trace_fp, "%d %d %d %d %d %d %d %d %d %d\n",
                        tick, level, hero_x, hero_y, hero_r, hero_c,
                        hero_hp, chests_collected, kills, score);
                fflush(trace_fp);
            }
        }

        if (chests_collected >= total_chests || hero_hp <= 0) {
            int completed = (chests_collected >= total_chests);
            if (completed) score += 500;
            if (score > best_score) { best_score = score; best_score_seed = cfg.seed; }

            if (!watch_mode) {
                render(ai_mode, paused);
                int ox = (COLS - ROOM_W) / 2, oy = (LINES - ROOM_H) / 2;
                if (ox < 0) ox = 0;
                if (oy < 2) oy = 2;
                attron(A_BOLD);
                mvprintw(oy + ROOM_H / 2, ox + ROOM_W / 2 - (hero_hp <= 0 ? 4 : 7),
                         hero_hp <= 0 ? "YOU DIED" : "LEVEL COMPLETE");
                attroff(A_BOLD);
                refresh();
            }
            usleep(1200000);

            if (hero_hp <= 0) save_high_score();

            level++; tick = 0;
            prev_room_r = prev_room_c = -1;
            target_room_r = target_room_c = -1;
            generate_dungeon(); spawn_hero();
            continue;
        }

        render(ai_mode, paused);
        if (damage_flash > 0) damage_flash--;
        usleep(ai_mode ? cfg.ai_tick_us : cfg.manual_tick_us);
    }

    save_high_score();
    if (trace_fp) fclose(trace_fp);
    endwin();
    return 0;
}
