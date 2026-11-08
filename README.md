# Dungeon Adventure

A procedural *Adventure*-style dungeon crawler for the terminal, built to be **watched**. An autonomous hero explores a fresh 4×4 dungeon each run, fights monsters, and collects treasure — while you compare two AI personalities that share the same code but see the world through different eyes.

```
[$] Lv 1 | Score 500 | Best 2340 | Chests 2/5 | Kills 7 | HP: <3<3<3
[J] torch:on seed:1734059283 | i c p t +/- q
                                         +-+-+-+-+
######################################   |@|.|.|.|
#......#...........#.........#.......#   +-+-+-+-+
#......#....G......#....$....#.......#   |.|$|!|.|
#......#...........#.........#...1...#   +-+-+-+-+
#..................#.........#.......#   |.|.|.|.|
#..........####..........B...........#   +-+-+-+-+
#..............................+.....#   |.|.|.|.|
######################################   +-+-+-+-+
```

Two AIs, one brain:

- **Superman** — omniscient. Reads raw dungeon state. Moves with perfect purpose.
- **Jones** — limited vision. Sees only what he has walked past. Explores the dark, remembers threats, gets surprised.

Same seed, same dungeon, same monster RNG. Different eyes.

---

## Quick start

```bash
make
./dg --ai                   # watch Jones explore
./dg --ai=superman          # watch Superman
./dg                        # play manually
```

Requires ncurses and a C11 compiler. Terminal needs at least **40×23** (normal) or **40×20** (`-w` watch mode).

```bash
# Debian/Ubuntu
sudo apt install libncurses-dev

# macOS
brew install ncurses

# Arch
sudo pacman -S ncurses

# Fedora
sudo dnf install ncurses-devel

# Termux
pkg install ncurses
```

---

## Building

### With Make (recommended)

```bash
make                # release build → ./dg
make clean          # remove object files and the binary
make clean && make  # full rebuild after editing headers
```

`make` recompiles only what changed. Editing `config.h` triggers a rebuild of every module that includes it.

### Without Make

Single command, all sources at once:

```bash
gcc -Wall -Wextra -O2 -std=c11 -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

With Clang:

```bash
clang -Wall -Wextra -O2 -std=c11 -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

With debug symbols and sanitizers (useful when hacking on the AI):

```bash
gcc -Wall -Wextra -g -O0 -std=c11 \
    -fsanitize=address,undefined \
    -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

Static build (portable binary):

```bash
gcc -Wall -Wextra -O2 -std=c11 -static \
    -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

### Compiler notes

- **C standard:** `-std=c11` works everywhere. If `usleep` fails to resolve on an older libc, use `-std=gnu11` instead. `main.c` already defines `_DEFAULT_SOURCE` and `_XOPEN_SOURCE 700` at the top, so POSIX functions should be visible.
- **ncurses discovery:** if `-lncurses` fails, try `-lncursesw` (wide-character variant, common on Debian/Ubuntu) or `pkg-config --cflags --libs ncurses`.
- **macOS Homebrew ncurses:** if the system ncurses is too old, add `-I/opt/homebrew/opt/ncurses/include -L/opt/homebrew/opt/ncurses/lib` (Apple Silicon) or `/usr/local` (Intel).

---

## Features

- **Procedural dungeons** — randomized DFS spanning tree + 30% loop edges. Every room guaranteed reachable.
- **Four room archetypes** — EMPTY, TREASURE, DEN, MAZE, each with distinct wall density, content guarantees, and color identity.
- **Three monster types** — Grunt (slow wanderer), Bat (flies over walls), Dragon (BFS chaser, costs 1 HP on contact).
- **Fog of war** — Bresenham line-of-sight with strict diagonal blocking. Torch toggle changes the light, not the sightline.
- **Two AI personalities** — see [The Two AIs](#the-two-ais) below.
- **Reproducible runs** — `--seed N` reproduces a run exactly (for the same binary + same AI).
- **Self-testing** — `--stress N` verifies generation invariants across thousands of seeds.
- **Trace logging** — `--trace run.log` writes per-tick state for offline analysis.
- **Persistent high score** — `~/.dungeon_highscore`, tracks the seed that set the record.
- **Config file** — `~/.dungeonrc`, every knob documented in the [full manual](manual.md).

---

## The Two AIs

The interesting part of the project is the pair. **They share the same code path** — same target priority, same local movement, same navigation, same danger field. The only difference is *what they know*.

| | Superman | Jones |
|---|---|---|
| **Sees** | Everything (raw dungeon state) | Only `seen[]` cells |
| **Chests** | All of them | Only ones he has walked past |
| **Monsters** | All of them | Only ones he has observed |
| **Doorways** | All of them | Only ones he has entered |
| **Navigation** | BFS through all floor | BFS through known floor |
| **Behavior** | Straight, purposeful | Wanders, doubles back, gets caught out |

Watch them on the same seed and the difference is immediate. Superman walks the shortest path to every goal. Jones explores, remembers, and improvises.

The design principle that drove this: *"human" isn't "limited + imperfect" — it's "purposeful + adaptive."* Adding random quirks to Superman made him feel broken, not human. Making the limitations come from *information* rather than *behavior* makes every step legible: you can predict what Jones will do because you can see what he knows. See [Design History](manual.md#21-design-history) for the iterations that led here.

---

## Controls

| Key | Action |
|---|---|
| `WASD` / arrows | Move (manual mode) |
| `i` | Toggle AI on/off |
| `c` | Cycle AI: Jones ↔ Superman |
| `p` | Pause / unpause |
| `t` | Toggle torch (fog of war) |
| `+` / `=` | Speed up (tick −20 ms) |
| `-` / `_` | Slow down (tick +20 ms) |
| `q` | Quit (saves high score) |

---

## CLI

```
dg [OPTIONS]

  --ai[=who]       Autonomous mode: who is 'jones' (default) or 'superman'
  -w, --watch      Hide HUD, center dungeon, suppress banner
  --torch          Force fog of war ON
  --no-torch       Force fog of war OFF
  --seed N         RNG seed (0 = time)
  --config PATH    Config file (default: ~/.dungeonrc)
  --trace FILE     Write per-tick state trace to FILE
  --stress N       Headless generation test (exit 0/1)
  --print-config   Dump effective configuration and exit
  --help           Show this message
```

Reproduce an interesting run from the seed shown in the HUD:

```bash
./dg --ai=superman --seed 1734059283
```

---

## Configuration

Every parameter is tunable via `~/.dungeonrc`:

```ini
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

Bootstrap with defaults:

```bash
./dg --print-config > ~/.dungeonrc
```

See [config.c](config.c) for the full key list or the [manual](manual.md#12-configuration) for ranges and effects.

---

## Project layout

```
dungeon/
├── Makefile          Build rules
├── config.h          Compile-time constants, Config struct, AI IDs
├── config.c          Config parsing, defaults, validation
├── dungeon.h         Shared types, extern declarations, API
├── dungeon.c         World gen, movement, combat, monsters, LOS
├── ai.c              Superman (truth) + Jones (memory)
├── render.c          Playfield, HUD, minimap
├── main.c            Entry point, argument parsing, game loop
└── manual.md         Full user manual and design history
```

---

## Docs

The [**full manual**](manual.md) covers everything: game rules, entity behavior, AI internals, fog of war, config reference, trace format, `--stress` invariants, design history, and how to extend the game (new monsters, new AIs, new room entities).

---

## Testing

```bash
./dg --stress 5000 --seed 1
# Stress: 5000 seeds starting at 1
# Stress: 5000/5000 passed, 0 failed
```

Verifies across seeds that:
1. At least one chest exists per level.
2. No chest, potion, or monster spawns on a wall.
3. Every doorway, chest, and potion is mutually reachable in its room.

---

## License

MIT — see [LICENSE](LICENSE).

---

## Credits

Inspired by Atari's *Adventure* (1980). No code or assets from the original game are used — everything here is procedurally generated ASCII. Built as an exercise in making AI behavior feel like character rather than mechanism.
