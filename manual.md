# Dungeon — Complete User Manual

A procedural *Adventure*-style dungeon crawler for the terminal. Designed primarily for **watching** — an autonomous hero explores a fresh dungeon each run, fights monsters, and collects treasure. Manual play is also supported.

The game ships with **two AI personalities** that share the same brain but see the world differently:

- **Superman** — an omniscient benchmark. Reads the raw dungeon state directly.
- **Jones** — a limited-vision adventurer. Reads only what he has actually seen.

The contrast between them is the point: same code, same priorities, same movement — different eyes. Watching them is watching two characters with different knowledge of the same world.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Requirements](#2-requirements)
3. [Installation & Build](#3-installation--build)
4. [Running the Game](#4-running-the-game)
5. [Controls](#5-controls)
6. [The Screen](#6-the-screen)
7. [The World](#7-the-world)
8. [Entities](#8-entities)
9. [Game Rules](#9-game-rules)
10. [The Two AIs](#10-the-two-ais)
11. [Fog of War & Vision](#11-fog-of-war--vision)
12. [Configuration](#12-configuration)
13. [Command-Line Reference](#13-command-line-reference)
14. [Seeds & Reproducibility](#14-seeds--reproducibility)
15. [High Score](#15-high-score)
16. [Trace Files](#16-trace-files)
17. [Self-Testing with `--stress`](#17-self-testing-with---stress)
18. [Known Limitations](#18-known-limitations)
19. [Troubleshooting](#19-troubleshooting)
20. [File Structure](#20-file-structure)
21. [Design History](#21-design-history)
22. [Extending the Game](#22-extending-the-game)
23. [Appendix: Symbol Reference](#23-appendix-symbol-reference)

---

## 1. Overview

Dungeon is a small terminal game inspired by Atari's *Adventure* (1980). A hero (the character `1`) explores a procedurally generated dungeon of connected rooms, collecting treasure chests while avoiding monsters. Every run builds a new dungeon, and every level is different from the last.

Each level's goal is simple: **collect every chest**. Do so and a new dungeon generates. Lose all HP and the level ends the same way — a fresh dungeon begins.

Score is cumulative across levels and persisted to `~/.dungeon_highscore`.

The game ships with two AI personalities:

- **Jones** (default) — plays like a person. His knowledge is bounded by what he has actually seen through line-of-sight, updated every tick. He explores the unknown by walking toward it, remembers chests he glimpsed, forgets threats that no longer exist where he last saw them, and can be surprised by something hidden behind a wall.
- **Superman** — reads the raw dungeon state directly. Always knows where every chest, potion, monster, and doorway is.

Both run on the same code with the same seed and the same monster RNG. The only difference is what they know. Switching between them mid-run (key `c`) makes the difference visible.

---

## 2. Requirements

- A POSIX-like system with **ncurses**.
- **GCC** or **Clang** with C11 support.
- **GNU Make** (or equivalent).
- A terminal at least **40 columns × 23 rows** in normal mode, or **40 × 20** in watch mode.
- **256-color support is optional but recommended.** Without color, the game runs with distinct monochrome glyphs.

### Installing ncurses

| Distro | Command |
|---|---|
| Debian/Ubuntu | `sudo apt install libncurses-dev` |
| Arch | `sudo pacman -S ncurses` |
| Fedora | `sudo dnf install ncurses-devel` |
| macOS | `brew install ncurses` |
| Termux | `pkg install ncurses` |

---

## 3. Installation & Build

From the project directory:

```bash
make
```

This produces a single executable called `dg`.

To rebuild from scratch:

```bash
make clean && make
```

If you edit any source file, `make` recompiles only that file and relinks. To change **dungeon dimensions** (grid size or room size), you must edit `config.h` and recompile — dimensions are compile-time constants because they determine array shapes.

### Manual one-liner (without Makefile)

```bash
gcc -Wall -Wextra -O2 -std=c11 -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

---

## 4. Running the Game

The simplest forms:

```bash
./dg                       # manual mode
./dg --ai                  # Jones (limited vision) by default
./dg --ai=superman         # Superman (omniscient)
./dg --ai=jones            # explicit Jones
```

Other common invocations:

```bash
./dg --ai --seed 42             # reproducible autonomous run (Jones)
./dg --ai=superman --seed 42    # same dungeon, omniscient AI
./dg --ai --no-torch            # Jones with ambient sunlight
./dg --ai=superman --torch      # Superman with fog-of-war view
./dg --ai --trace run.log       # write per-tick state to run.log
./dg --config my.cfg            # custom config file
./dg --print-config             # dump effective settings and exit
./dg --stress 5000              # headless generation test
./dg -w --ai                    # watch mode: no HUD, Jones plays
./dg --help                     # all flags
```

Press `q` at any time to quit.

---

## 5. Controls

### Movement (manual mode)

| Key | Action |
|---|---|
| `W` / `↑` | Move up |
| `S` / `↓` | Move down |
| `A` / `←` | Move left |
| `D` / `→` | Move right |

Manual mode is **turn-based**: the hero only moves when you press a key, and monsters only move on the tick that follows a hero move. You have unlimited time between moves.

### Global keys

| Key | Action |
|---|---|
| `i` | Toggle AI on/off |
| `c` | Cycle AI: Jones ↔ Superman (turns AI on if it was off) |
| `p` | Pause / unpause |
| `t` | Toggle torch (lighting) |
| `+` or `=` | Speed up (tick −20 ms) |
| `-` or `_` | Slow down (tick +20 ms) |
| `q` | Quit (saves high score) |
| `Ctrl+C` | Same as `q` — SIGINT handler saves high score and exits cleanly |

Pressing any movement key while in AI mode drops out of AI into manual control.

---

## 6. The Screen

Three regions: **top HUD**, **playfield**, **minimap**.

### Top HUD (two lines)

```
[$] Lv 1 | Score 500 | Best 2340 | Chests 2/5 | Kills 7 | HP: <3<3<3
[J] torch:on seed:1734059283 | i c p t +/- q
```

- **`[$]` / `[!]` / `[~]` / `[ ]`** — current room type tag (TREASURE, DEN, MAZE, EMPTY), colored to match.
- **Lv** — level number.
- **Score** — cumulative score this run.
- **Best** — best score ever, from `~/.dungeon_highscore`. Shown only when `best > score`.
- **Chests X/Y** — collected / total this level.
- **Kills** — cumulative monster kills.
- **HP** — one `<3` per remaining HP. Flashes (reverse video) for 3 frames after a dragon hit.
- **`[J]` / `[S]` / `[M]` / `[PAUSED]`** — current AI: Jones, Superman, Manual, Paused.
- **torch** — `on` or `off`.
- **seed** — RNG seed for this run.

### Playfield (40 × 20 cells)

| Glyph | Meaning |
|---|---|
| `#` | Wall (outer boundary or interior segment) |
| ` ` (space) | Doorway — a gap in the outer wall |
| `.` | Floor |
| `1` | Hero |
| `G` | Grunt (green) |
| `B` | Bat (cyan) |
| `D` | Dragon (red) |
| `$` | Chest (yellow) |
| `+` | Health potion (magenta) |

Room walls and floor are **tinted by room type**. Under fog of war, currently-visible cells render bright, previously-seen cells render dim, never-seen cells render blank (interior) or dim `#` (outer wall).

### Minimap (right of playfield)

A 9×9 grid showing the 4×4 room layout, drawn when the terminal is wide enough.

| Cell symbol | Meaning |
|---|---|
| `@` | Current room (reverse video, tinted by type) |
| `$` | Visited, chest you have actually seen waiting |
| `!` | Visited, has a live monster |
| `.` (dim) | Visited, empty |
| ` ` (space) | Unvisited |

Room connections are drawn as `-` / `|` separators with `+` at intersections. Rooms that are adjacent but not connected share only a `+` corner.

Silently hidden if the terminal is too narrow.

---

## 7. The World

### 7.1 Room grid

The dungeon is a **4 × 4 grid of rooms** (16 total). Change `GRID_W` and `GRID_H` in `config.h` to enlarge.

Each room is **40 × 20 cells** including outer walls, giving a **38 × 18 interior**. Change `ROOM_W` / `ROOM_H` for larger rooms — you must also enlarge your terminal.

### 7.2 Connections

A **randomized DFS spanning tree** guarantees every room is reachable. Extra edges are added with probability `loop_pct`% per unused adjacent pair (default 30%), creating loops. Loops give the AI alternative routes and make the dungeon feel less like a strict corridor.

Connections are **shared**: adjacent connected rooms know the doorway position from both sides.

### 7.3 Doorways

A doorway is a single-cell gap in the outer wall:

- **N/S doorways** at random x in `[1, ROOM_W-2]`.
- **E/W doorways** at random y in `[1, ROOM_H-2]`.

Stepping onto the doorway cell and continuing outward moves you to the adjacent room, entering one cell inward from the corresponding doorway. Doorways are never blocked by interior walls.

### 7.4 Interior walls

Each room contains a range of **straight-line wall segments** (horizontal or vertical). Range depends on archetype (see §7.5).

Before committing a segment, two safety checks run:

1. **Doorway-adjacency check** — no cell of the segment may land on the cell just inside a doorway.
2. **Connectivity check** — after tentatively placing the segment, a flood-fill verifies every floor cell is still reachable. If not, the segment is rolled back.

**Invariant:** every floor cell in every room is reachable from every doorway, and every chest, potion, monster, and hero start is on a reachable floor cell. `--stress` verifies this across thousands of seeds (§17).

Walls block hero and monster movement. **Bats can leap over single-cell interior walls** (§8.2). Walls also block line-of-sight for both the renderer and Jones's knowledge (§11).

### 7.5 Room archetypes

Each room is assigned a type at generation time. Percentages are configurable (§12).

| Type | Tag | Walls | Monsters | Chest | Wall color |
|---|---|---|---|---|---|
| **EMPTY** | `[ ]` | `walls_min`–`walls_max` (default 1–3) | `monster_density`% → 1–2 | `chest_density`% → 1 | default |
| **TREASURE** | `[$]` | 0–1 (sparse) | none | **guaranteed 1** | yellow |
| **DEN** | `[!]` | 5–8 (dense) | **guaranteed 2–3** | none | red |
| **MAZE** | `[~]` | 8–10 (very dense) | none | **guaranteed 1** | blue |

Wall *and* floor cells tint with the room color when visible. Room-type identity persists through fog of war (dim tint when remembered).

### 7.6 Doorway layout and hero position

| Exit | Enters new room at |
|---|---|
| North | `y = ROOM_H - 2` |
| South | `y = 1` |
| West | `x = ROOM_W - 2` |
| East | `x = 1` |

The hero appears just inside the corresponding doorway in the destination room and can walk straight back.

---

## 8. Entities

### 8.1 Hero (`1`, white, bold)

- Tracked in two coordinate systems: **room coordinates** (`hero_r`, `hero_c`, position in the 4×4 grid) and **local coordinates** (`hero_x`, `hero_y`, position in the current room).
- Starts each level in a random room at a random interior cell not occupied by a chest, potion, or monster.
- Starts each level at full HP (`start_hp`). HP does not carry between levels.
- Kills and score are cumulative across levels (reset only when the program restarts).

### 8.2 Monsters

| Glyph | Type | Color | Move rate | Behavior |
|---|---|---|---|---|
| `G` | Grunt | Green | Every 3rd tick | Random walk (20% chance per eligible tick) |
| `B` | Bat | Cyan | Every tick | Random walk — **flies over 1-cell interior walls** |
| `D` | Dragon | Red | Every 2nd tick | BFS chase toward the hero |

Monsters do not leave their room — they only move when the hero is in the same room.

**Dragons** use a proper BFS distance field from the hero's position (computed once per tick when a dragon is present), so they navigate around interior walls correctly.

**Bats** are the only flying entity. When a bat's random move targets an interior-wall cell, it attempts a **2-cell leap** over it (`(nx+dx, ny+dy)`). If that cell is valid interior floor, the bat lands there. If not, the bat stays put. Bats can never stop on a wall cell.

**Grunts** are the baseline slow random walker.

Monster spawn density is `monster_density`% of EMPTY rooms (default 55%). Type distribution is `monster_g_pct` / `monster_b_pct` / `monster_d_pct` (default 40/30/30). Per-room cap is `max_monsters` (default 3, bounded by compile-time `MAX_MONSTERS`).

### 8.3 Chests (`$`, yellow, bold)

- Level goal: `total_chests` is the sum of "has a chest" over all rooms.
- Each EMPTY room has `chest_density`% chance (default 45%), plus guaranteed chests in TREASURE and MAZE archetypes.
- **Guarantee:** always at least one chest per level. If random placement produced none, one is forced.
- Chests do not respawn. Collecting one removes it permanently and adds +100 score.

### 8.4 Health potions (`+`, magenta, bold)

- **Exactly one potion per level**, in a random room on a valid floor cell.
- Collecting restores **1 HP** (not full).
- Collection is gated on `hero_hp < start_hp`. Walking over a potion at full HP leaves it in place.
- If `start_hp == 1`, potions are never collectable and become decorative.

### 8.5 HP display

Repeated `<3` strings in the HUD. Lose HP when a dragon hits you. Restore all HP at level start, or +1 per potion. On dragon hit, the hearts flash (reverse video) for 3 rendered frames.

---

## 9. Game Rules

### 9.1 Movement

One cell per move, four directions, no diagonals.

- **Walls block movement.** Outer walls and interior walls are impassable.
- **Bats leap** over single-cell interior walls (§8.2).
- **Doorways are passable** — stepping onto a doorway cell and continuing outward transitions rooms.

### 9.2 Combat

Contact-based. No ranged attack, no attack button.

- **Hero meets monster** → monster dies, `kills++`, `score += 10`.
- **Hero meets dragon** → dragon dies, `kills++`, `score += 10`, **hero loses 1 HP** and `damage_flash = 3`.

**Collision is checked twice per tick** — once immediately after the hero moves (catches hero-walks-into-monster), and once after monsters move (catches monster-walks-into-hero). This prevents the "hero passed through" issue where a monster moves away on the same tick.

### 9.3 Level completion

A level ends when either:

1. **`chests_collected >= total_chests`** → `LEVEL COMPLETE` banner, `score += 500`, new dungeon, level counter increments.
2. **`hero_hp <= 0`** → `YOU DIED` banner, new dungeon, level counter increments, high score saved.

In both cases:
- `level` increments.
- `hero_hp` resets to `start_hp`.
- `tick` resets to 0.
- `prev_room`, `target_room` reset.
- Dungeon regenerates.
- Hero spawns in a new random room.

Kills and score are **not** reset between levels.

### 9.4 Monster respawn

Monsters do not respawn within a level. A level with a high kill count becomes progressively safer.

### 9.5 Scoring

| Event | Points |
|---|---|
| Chest collected | +100 |
| Monster killed | +10 |
| Level completed | +500 |

Score is cumulative and drives the high-score comparison.

---

## 10. The Two AIs

Both personalities are deterministic given the RNG state, so a fixed seed produces the same run for each.

**They share the same code path.** Every difference between them is knowledge, not heuristics. Both walk toward a chest they can see; they differ only in whether they *know* the chest is there. Both navigate the room graph; they differ only in whether they know a doorway is there. Both compute a distance field; Superman's goes through every floor cell, Jones's goes only through floor cells he has observed.

### 10.1 Superman — omniscient

Reads `rm->has_chest`, `rm->has_potion`, `rm->monsters[]`, and doorway connections **directly**. The torch toggle and `seen[]` array have no effect on his decisions.

**Priority order:**

1. Chest in current room → move to it.
2. Potion in current room and HP < max → move to it.
3. Nearest monster (skips dragons at HP ≤ 1) → move to it.
4. Otherwise → room-graph BFS to nearest room with content, avoiding `prev_room` when possible.
5. If target is an adjacent room, walk to the doorway leading there.

Local movement uses the **repeated-permutation sampler**: compute a distance field from the target, list every neighbor at `dist - 1`, score each for danger, filter to least-dangerous tier, apply momentum (~70% keep last heading), then pick uniformly.

### 10.2 Jones — limited-vision adventurer

**Only reads `seen[]` and `known_threat[]`.** Everything he targets must have been observed. He does not know anything about cells or objects outside his LOS.

**Priority order** (same as Superman, but gated on observation):

1. Chest in current room, **whose cell he has actually seen** → move to it.
2. Potion in current room, whose cell he has seen, HP < max → move to it.
3. Nearest **known** live monster (whose cell has been seen; skips dragons at HP ≤ 1) → move to it.
4. Otherwise → room-graph BFS to nearest room with **potential content** from Jones's memory (unvisited, or visited with unseen cells, or visited with a known chest/monster/potion).
5. Walk to the doorway leading toward the target room.
6. **Fallback**: if the doorway path is blocked by a cell Jones hasn't observed, take one step toward the nearest unseen cell in the current room. This reveals information and unblocks the path.

**Pathfinding** (`compute_dist_map_jones`) treats unseen cells as impassable. He cannot path through darkness — the fallback step (6) is what drives him into it.

**Memory:**
- `seen[]` — cells he has observed. Permanent within a level.
- `known_threat[]` — cells where he has observed a monster (with type code). Set every tick that cell is visible and occupied; cleared every tick that cell is visible and empty. Permanent within a level otherwise.

**Danger field** (`cell_danger_known`) reads `known_threat`. A dragon Jones saw 20 ticks ago still frightens him unless he has observed the cell since and confirmed it empty. A dragon he has never seen is invisible to him — he will walk into its room and be surprised.

### 10.3 Shared movement heuristics

Both AIs use the same natural-movement layer:

- **Momentum** — if the previous step was in direction `d` and `d` is still a valid candidate after danger filtering, it is chosen 70% of the time.
- **Hesitation** — 3% chance to do nothing for one tick when safe (no monster within 4 cells) and the goal is ≥ 4 cells away.
- **Side-step** — 8% chance of a perpendicular step when safe and the goal is ≥ 6 cells away, if that cell has zero danger.

Jones's motion is naturally less direct than Superman's because his target set is smaller and constantly shifting — but the underlying movement logic is identical.

### 10.4 Switching mid-run

Press `c` to cycle between Jones and Superman. This turns AI on if it was off. Existing state (`seen[]`, `known_threat[]`, `target_room`) carries over unchanged — switching Superman→Jones mid-dungeon means Jones starts with whatever `seen[]` accumulated while Superman was driving (which is the same `update_seen` path). Switching Jones→Superman means Superman suddenly knows everything.

### 10.5 Design philosophy

The two AIs exist so you can compare them. Jones is the *character* — limited, fallible, surprising. Superman is the *benchmark* — what mastery looks like when nothing hides from you. Watching them on the same seed makes the difference in behavior visible: Superman walks purposeful straight lines; Jones wanders, doubles back, gets caught out.

The core insight that shaped the final design: **"human" isn't "limited + imperfect." It's "purposeful + adaptive."** Adding random imperfections on top of a simple drive makes an AI feel broken, not human. Making the AI's limitations come from *information* rather than *behavior* makes its actions legible — the watcher can predict what it will do from watching it do.

That's why Jones ends up as "Superman with tunnel vision," not "Superman plus quirks." One brain, two pairs of eyes.

---

## 11. Fog of War & Vision

Fog of war is toggled with `t`, forced with `--torch` / `--no-torch`, or set via `torch_enabled` in `~/.dungeonrc`.

### 11.1 Two independent axes

**LOS is universal.** The hero cannot see through walls, ever. Under any torch setting, a wall blocks the sightline. This is enforced by `has_los()` — a Bresenham line with strict diagonal blocking (a diagonal step is blocked if *either* orthogonal neighbor is a wall).

**The torch is light.** It changes how far sight reaches:

- **Torch OFF** — ambient sunlight. Every cell with clear line-of-sight is visible across the entire room.
- **Torch ON** — darkness. Only cells within `torch_radius` *and* with line-of-sight are visible.

`update_seen()` applies both rules on every tick, unconditionally. Whether the torch is on only changes the *radius* gate.

### 11.2 Rendering tiers

| Cell state | Rendered as |
|---|---|
| Currently visible | Full brightness |
| Seen before, not currently visible | `A_DIM` version |
| Never seen, interior cell | Blank |
| Never seen, outer wall | Dim `#` (no doorway leaked) |

Chests and potions follow the same three-tier logic. Monsters are only shown when **currently visible** (they move, so no memory in the renderer).

### 11.3 Torch radius

`torch_radius` defaults to 6. Range 1–30. Larger radius = larger visible area when torch is on.

### 11.4 Toggling mid-game

Pressing `t` to turn fog on marks every cell of every previously-visited room as seen (`mark_visited_rooms_seen`). This models "you were in these rooms without fog; you'd remember them." It is a deliberate memory model but can be exploited by toggling off, walking around, toggling back on.

---

## 12. Configuration

### 12.1 Config file

The game looks for `~/.dungeonrc`. Use `--config PATH` to override.

Format: `key = value`, one per line, `#` for comments.

Example:

```
# Faster, more aggressive dungeon
ai_tick_us       = 60000
start_hp         = 2
monster_density  = 70
chest_density    = 60
monster_d_pct    = 50
loop_pct         = 50
torch_radius     = 5
torch_enabled    = 0
```

Bootstrap a config with defaults:

```bash
./dg --print-config > ~/.dungeonrc
```

### 12.2 Every key

| Key | Default | Range | Effect |
|---|---|---|---|
| `ai_tick_us` | `100000` | ≥ 1000 | Pause between AI ticks (µs). `100000` ≈ 10 FPS. |
| `manual_tick_us` | `30000` | ≥ 1000 | Debounce delay in manual mode. |
| `start_hp` | `3` | 1–20 | Starting HP each level. |
| `monster_density` | `55` | 0–100 | % of EMPTY rooms with monsters. |
| `chest_density` | `45` | 0–100 | % of EMPTY rooms with a chest. |
| `monster_g_pct` | `40` | ≥ 0 | Grunt weight. |
| `monster_b_pct` | `30` | ≥ 0 | Bat weight. |
| `monster_d_pct` | `30` | ≥ 0 | Dragon weight. |
| `max_monsters` | `3` | 1–`MAX_MONSTERS` | Per-room monster cap. |
| `walls_min` | `1` | 0–10 | Min interior wall segments in EMPTY rooms. |
| `walls_max` | `3` | 0–10 | Max interior wall segments in EMPTY rooms. |
| `loop_pct` | `30` | 0–100 | % chance of extra room connection. |
| `torch_enabled` | `1` | 0/1 | Start with fog of war. |
| `torch_radius` | `6` | 1–30 | Vision radius when torch is on. |
| `treasure_pct` | `15` | 0–100 | % of rooms assigned TREASURE archetype. |
| `den_pct` | `20` | 0–100 | % of rooms assigned DEN archetype. |
| `maze_pct` | `10` | 0–100 | % of rooms assigned MAZE archetype. |
| `ai_personality` | `0` | 0 / 1 | 0 = Jones, 1 = Superman. |
| `seed` | `0` | ≥ 0 | RNG seed. `0` = `time(NULL)`. |

Type weights don't need to sum to 100 — they're relative. Archetype percentages are per-room independent rolls: EMPTY gets the rest. Setting all three to 0 restores pure-EMPTY dungeons. Out-of-range values are clamped silently.

### 12.3 CLI overrides

Command-line flags override config-file values:

- `--seed N`, `--torch`, `--no-torch`, `--ai=jones|superman` force their respective settings.

Everything else must come from the config file.

---

## 13. Command-Line Reference

```
Usage: dg [OPTIONS]

Options:
  --ai[=who]       Autonomous mode. who is 'jones' (default) or
                   'superman'. Plain --ai means Jones.
  -w, --watch      Hide HUD; center dungeon; suppress end-of-level
                   banner. Visual only — does NOT touch AI mode.
  --torch          Force fog of war ON (overrides config).
  --no-torch       Force fog of war OFF (overrides config).
  --seed N         RNG seed. 0 or omitted = time(NULL).
  --config PATH    Config file path (default: ~/.dungeonrc).
  --trace FILE     Write per-tick state trace to FILE.
  --stress N       Headless generation test: run N seeds, verify
                   invariants, exit 0/1. No ncurses.
  --print-config   Print effective configuration and exit.
  --help           Show this message.

Keys:
  WASD / arrows    Move (manual mode)
  i                Toggle AI on/off
  c                Cycle AI: Jones <-> Superman
  p                Pause / unpause
  t                Toggle torch
  + / =            Speed up  (tick −20 ms)
  - / _            Slow down (tick +20 ms)
  q                Quit
```

Unrecognized flags are silently ignored.

---

## 14. Seeds & Reproducibility

The RNG is seeded from `time(NULL)` unless `--seed N` (or `seed` in config) is supplied. The seed appears in the HUD on every frame, so an interesting run can be reproduced:

```bash
./dg --ai=superman --seed <seed-from-hud>
```

Because each AI is deterministic given the RNG state, the same seed produces the same run — **for the same AI**. Jones and Superman on the same seed produce different runs (same dungeon, same monster decisions, different exploration paths).

Caveat: seeds reproduce only for the same binary. Code changes alter RNG consumption.

`~/.dungeon_highscore` stores the seed that set the record, so `./dg --ai --seed <field4>` reproduces the exact run that made your best score.

---

## 15. High Score

Persisted in `~/.dungeon_highscore`. Format:

```
<score> <level> <kills> <seed> <timestamp>
```

- **Loaded** at startup into `best_score` and `best_score_seed`.
- **Updated in memory** whenever `score > best_score` (also updates `best_score_seed`).
- **Written to disk** on death and on quit (including Ctrl+C).

The seed field always refers to the seed that *produced* the best score, not the current run. If the file is corrupt or unreadable, the game silently starts with `best_score = 0`.

---

## 16. Trace Files

`--trace FILE` writes one line per game tick:

```
tick level x y r c hp chests kills score
```

| Field | Meaning |
|---|---|
| `tick` | Cumulative tick counter |
| `level` | Level number |
| `x`, `y` | Hero local room coords |
| `r`, `c` | Hero room-grid coords |
| `hp` | Current HP |
| `chests` | Chests collected this level |
| `kills` | Cumulative kills |
| `score` | Cumulative score |

Flushed after every tick, so a hard kill doesn't lose the tail.

**Notes:**
- Trace written only after successful ticks (hero moved or AI acted). Paused state, non-moving key presses, and level transitions produce no lines.
- The +500 level-complete bonus is applied *after* the last tick of the level. Expect a visible +500 jump between the last line of a level and the first of the next.
- `--trace` is ignored in `--stress` mode (the trace file isn't opened).

Typical analysis:

```bash
# Rooms visited per level
awk '{print $2, $5, $6}' run.log | sort -u

# Last 20 lines before death
tail -20 run.log
```

---

## 17. Self-Testing with `--stress`

Because interior walls are generated stochastically, `--stress` runs many headless dungeon generations and verifies invariants that would be expensive to check during play.

```bash
./dg --stress 5000 --seed 1
```

For each seed in `[base_seed, base_seed + N)`, verifies:

1. **At least one chest exists.**
2. **No chest, potion, or monster is placed on a wall.**
3. **Every doorway inner cell, the chest cell, and the potion cell are mutually reachable** in each room.

Success output:

```
Stress: 5000 seeds starting at 1
Stress: 5000/5000 passed, 0 failed
```

Failure output (exits 1):

```
FAIL seed=1234: unreachable region in room 2,1
```

`--stress` checks generation-time invariants only, not runtime behavior.

---

## 18. Known Limitations

Documented behaviors of the current build.

### Superman and Jones share `seen[]`

Switching AI mid-run does not reset Jones's knowledge. Whatever `seen[]` accumulated under Superman stays there. This is intentional (the *watcher* saw it, so Jones "remembers" it), but means Superman→Jones mid-run gives Jones a head start.

### Jones's minimap shows monsters regardless of knowledge

The minimap's `!` symbol uses the true `rm->monsters[i].alive` flag, not Jones's `seen[]`. So Jones's minimap shows a living monster in a visited room even if he hasn't seen it recently. This leaks a small amount of information. Fixing would require routing the minimap through `seen[]`.

### Jones's AI does not flee at 1 HP

At HP ≤ 1 Jones skips dragons as *targets*, but will still path through a dragon's cell if it's the only shortest route to another goal. `cell_danger_known` filters to the least-dangerous shortest path but does not rule out walking directly into a remembered dragon.

### Toggling torch reveals visited rooms

`t` marks every cell of every previously-visited room as seen. Deliberate, but exploitable.

### Same seed, different AI, different run

Jones and Superman on the same seed have the same dungeon and same monster decisions, but different exploration paths. To compare them fairly, use the same seed and observe both.

### Bats can leap over walls into unreachable pockets

The bat leap checks the destination cell's wall state but not whether the destination is *reachable* from the rest of the room. In practice this is fine — `room_flood_ok` guarantees all floor cells are reachable — but a bat leaping over an interior segment could land in a chamber that requires walking back around. Not a bug, just visually surprising.

### Jones wall-hugs when exploring

Under fog of war, the unknown cells are mostly behind walls. The nearest unknown cell is almost always on a wall's shadow. Jones walks toward it — that looks like wall-hugging, but it's actually fog-lifting. It's what a person exploring a dark room does. Not a bug, just a consequence of honest vision.

---

## 19. Troubleshooting

**"Terminal too small"**

Needs at least 40×23 in normal mode (playfield 40×20 + 2 HUD + 1 spare), or 40×20 in `-w` watch mode. Enlarge terminal, or shrink `ROOM_W`/`ROOM_H` in `config.h` and rebuild.

**No colors**

Runs fine without color — glyphs are distinct. If your terminal supports 256 colors but ncurses can't detect it, set `TERM=xterm-256color` before running.

**Game is too fast/slow**

Use `+`/`-` in-game (20 ms steps, 20 ms to 400 ms), or set `ai_tick_us` in `~/.dungeonrc`.

**Walls block a doorway**

Should never happen. Run `./dg --stress 10000` and report the failing seed.

**Minimap doesn't appear**

Needs `COLS >= ROOM_W + 4 + 9` (roughly 53 columns). Enlarge terminal or shrink `ROOM_W` in `config.h`.

**Same seed gives a different result**

Only if you rebuilt the game, or if you're comparing Jones vs Superman.

**Ctrl+C loses progress**

It doesn't — SIGINT is caught, the high score and trace file are flushed, and the terminal is restored. Only `SIGKILL` (`kill -9`) loses current-run data.

**Compile error: `undefined reference to ...`**

A source file wasn't recompiled against the new header. Fix: `make clean && make`.

**Compile error: `usleep` not declared**

`-std=c11` with strict libc can hide `usleep`. Already handled by `_DEFAULT_SOURCE` / `_XOPEN_SOURCE` macros at top of `main.c`. If it still fails, replace `-std=c11` with `-std=gnu11` in the compile line.

---

## 20. File Structure

```
dungeon/
├── Makefile          Build rules
├── config.h          Compile-time constants, Config struct, AI constants
├── config.c          Config parsing, defaults, validation, print
├── dungeon.h         Shared types, extern declarations, API
├── dungeon.c         World generation, movement, combat, monsters, LOS
├── ai.c              Both AIs: Superman (truth) and Jones (memory)
├── render.c          Playfield, HUD, minimap
└── main.c            Entry point, argument parsing, game loop, signals
```

### What lives where

- **`config.h`** — compile-time constants (grid/room size, `MAX_MONSTERS`, `AI_JONES`/`AI_SUPERMAN`, threat type codes). `Config` struct and `cfg` global.
- **`config.c`** — reads `~/.dungeonrc`, applies defaults, clamps values, prints effective config.
- **`dungeon.h`** — the shared API. Included by every module. Declares `seen[]`, `known_threat[]`, `has_los()`, `update_seen()`, and `compute_dist_map_jones()`.
- **`dungeon.c`** — game rules that don't require AI decisions: layout, wall generation, room transitions, monster movement, chest/potion collection, collisions, LOS, `update_seen`, `check_room_reachability`.
- **`ai.c`** — decision logic for both personalities. `hero_tick_ai()` dispatches on `cfg.ai_personality`. Superman helpers are prefixed `superman_*`; Jones helpers are prefixed `jones_*`; `try_move`, `bfs_room_next_step`, and `cell_danger_*` are shared.
- **`render.c`** — pure rendering (HUD, playfield, minimap). Never mutates game state.
- **`main.c`** — argument parsing, ncurses init/teardown, tick loop, `--stress` driver, high-score I/O, signal handling.

---

## 21. Design History

The game has evolved through many iterations. This section documents what was tried, what worked, and what was reverted, so future maintainers understand the choices.

### The core game (built and kept)

- **4×4 room grid** with a randomized DFS spanning tree + 30% extra loop connections.
- **Interior walls** verified by flood-fill to never cut off any floor cell from any doorway.
- **Room transitions** through doorways. Position preserved across rooms.
- **Three monster types** with distinct behaviors: grunt (slow wanderer), bat (fast wanderer that flies over 1-cell walls), dragon (BFS chaser).
- **Chests** as the level goal. One guaranteed chest per level.
- **Health potions** restoring 1 HP. One per level.
- **Score** as a unified metric (+100 per chest, +10 per kill, +500 per level).
- **Persistent high score** with seed tracking.
- **`--seed`** for reproducibility.
- **`--stress`** for automated invariant verification.
- **`--trace FILE`** for per-tick state logging.
- **`--watch` / `-w`** for HUD-free viewing (visual only).
- **Room archetypes** (EMPTY / TREASURE / DEN / MAZE) with distinct wall densities, content guarantees, and color identities.
- **Line-of-sight fog of war** using Bresenham with strict diagonal blocking. Torch toggle changes the light, not the LOS.
- **Two AIs** sharing one brain: Superman (truth) and Jones (memory).

### Things added then reverted (learned from)

- **Repeated-permutation sampler for movement** — kept in the shared movement layer (momentum + uniform pick). Makes movement look natural instead of ziggurat-shaped.
- **Danger field** — kept. Superman's `cell_danger_truth` reads monsters; Jones's `cell_danger_known` reads `known_threat`. Both filter candidate moves to the least-dangerous tier.
- **Commitment timer (5 ticks)** — reverted. Jones picked a target and held it for N ticks. Fought the 3% hesitation, flickered on expiry, blocked higher-priority sightings. Removed.
- **Room-entry pause** — reverted. Paused every doorway crossing including ones used twenty times. Needed familiarity detection that was never right. Removed.
- **Doorway peek** — reverted. Same bug — paused on every transition. Removed.
- **Distance-weighted memory** — reverted. Formula inverted: known content scored *worse* than unknown at distance ≥ 2, so Jones ignored chests he could see. Removed.
- **Fading threat memory (10-tick decay)** — reverted. Magic constant; commitment still read truth; didn't solve the actual problem. Removed.
- **Explore-by-largest-region** — reverted. Was supposed to fix wall-hugging by picking the largest unseen region's centroid. Introduced a stuck bug when Jones was a BFS source (par = -1 → return 0 → falls through to room nav → walks away from frontier). Removed with the goal-field rework.
- **Goal field (single persistent goal)** — reverted. Simplification that turned into a new layer of state that could get stuck. Jones re-attempted the same blocked goal on a loop without ever exploring to unblock it. Fixed by the final fallback, then removed entirely with the Superman-parity rework.
- **Fallback to exploration** — kept (in the final version as `jones_step_toward_unknown`). When the primary navigation fails, take one step toward the nearest unseen cell. Prevents permanent freeze. This survived all the rework.

### The final design principle

The single most important lesson from the Jones iterations: **"human" isn't "limited + imperfect." It's "purposeful + adaptive."** Superman already felt human because his motion was legible — every step goes somewhere you can predict. Adding random imperfections to Jones made him feel broken, not human. The final design keeps the same code path for both AIs and makes *information* the only difference. That's what makes watching Jones feel like watching a character: you can narrate what he's doing ("he's going for the chest he saw") and you can predict what he'll do next ("he'll walk toward that doorway"). Rules without reasons look broken; limitations with reasons look like a person.

---

## 22. Extending the Game

### Adding a monster type

1. Add the glyph to the type roll in `place_monster_in_room()` (`dungeon.c`).
2. Add a `move_monsters()` branch.
3. Add a `THREAT_*` code in `dungeon.h` and a mapping in `update_seen()`.
4. Add a color pair (`main.c`) and a `render.c` mapping.
5. Optional: add a config key for spawn weight.

### Adding a room entity (e.g. a key)

1. Add `int has_key, key_x, key_y;` to `Room` in `dungeon.h`.
2. Populate it in `generate_dungeon()`.
3. Add `check_key()` alongside `collect_chest()` in `dungeon.c`.
4. Call from the tick loop in `main.c`.
5. Draw in `render.c`.
6. Extend `check_room_reachability()`.

### Changing AI behavior

All decision logic is in `ai.c`:

- **Superman's target priority** — `superman_tick()`.
- **Superman's local movement** — `superman_move()` (permutation sampler + danger filter).
- **Jones's target priority** — `jones_tick()`.
- **Jones's local movement** — `jones_move()` (permutation sampler + known-threat filter).
- **Jones's navigation** — the room-graph BFS + doorway walk in `jones_tick`.
- **Jones's fallback** — `jones_step_toward_unknown()`.
- **Jones's room choice** — `jones_pick_target()` with `jones_room_has_potential()`.
- **Danger scoring** — `cell_danger_truth()` (Superman) vs `cell_danger_known()` (Jones).

### Adding a config key

1. Add the field to `Config` in `config.h`.
2. Set its default in `config_defaults()`.
3. Add a branch to `apply()`.
4. Add a range check to `sanitize()`.
5. Add a line to `config_print()`.
6. Use `cfg.your_key` where appropriate.

### Adding a CLI flag

In `main.c`, add branches in **both** `for (int i = 1; i < argc; i++)` loops (the pre-config one handles `--help` / `--config`; the second handles everything else). Update `print_help()`.

### Enlarging the dungeon

Change `GRID_W` / `GRID_H` in `config.h`. All downstream arrays (`rooms`, `conn_ew`, `conn_ns`, `door_ew`, `door_ns`, `room_walls`, `seen`, `known_threat`, BFS buffers) resize automatically.

Change `ROOM_W` / `ROOM_H` for larger rooms — you'll need a larger terminal.

Rebuild with `make`.

### Adding a new AI personality

The two AIs share one code path. To add a third:

1. Add a new `AI_*` constant in `config.h`.
2. Add a new function `<name>_tick()` in `ai.c`.
3. In `hero_tick_ai()`, dispatch on the new personality value.
4. Choose a knowledge source (truth, `seen[]`, or something new).
5. Follow the same structure as `superman_tick` / `jones_tick` — target priority, then movement, then navigation.

The codebase is deliberately simple so that new personalities are mostly a *knowledge source* choice, not a *behavior* choice. That's the design lesson Jones taught us.

---

## 23. Appendix: Symbol Reference

| Symbol | Type | Meaning |
|---|---|---|
| `1` | Hero | Player character |
| `G` | Monster | Grunt — slow random walker |
| `B` | Monster | Bat — fast walker, leaps 1-cell interior walls |
| `D` | Monster | Dragon — BFS chaser, costs 1 HP on contact |
| `$` | Chest | Treasure, +100, counts toward level completion |
| `+` | Potion | Restores 1 HP |
| `#` | Wall | Impassable — outer boundary or interior |
| ` ` (space) | Doorway | Gap in outer wall |
| `.` | Floor | Walkable interior |
| `@` | Minimap | Current room |
| `!` | Minimap | Visited, has a live monster |
| `<3` | HUD | One HP point |

### Constants

| Constant | Value | Meaning |
|---|---|---|
| `GRID_W` | 4 | Room-grid columns |
| `GRID_H` | 4 | Room-grid rows |
| `ROOM_W` | 40 | Room width (including outer walls) |
| `ROOM_H` | 20 | Room height (including outer walls) |
| `MAX_MONSTERS` | 3 | Per-room monster cap |
| `INF_DIST` | 999999 | "unreachable" sentinel |
| `AI_JONES` | 0 | Jones personality ID |
| `AI_SUPERMAN` | 1 | Superman personality ID |
| `THREAT_GRUNT` | 1 | Grunt threat code |
| `THREAT_BAT` | 2 | Bat threat code |
| `THREAT_DRAGON` | 3 | Dragon threat code |

---

*End of manual.*
