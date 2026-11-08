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

Terminal needs at least **40×23** (normal) or **40×20** (`-w` watch mode). For platform-specific build instructions, see [Building](#building) at the end of this document.

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

**On Windows:** `~` resolves to `%USERPROFILE%` only if `HOME` is set. Add `set "HOME=%USERPROFILE%"` before running `dg.exe`, or the config and high-score files become silent no-ops.

---

## Project layout

```
dungeon-adventure/
├── Makefile          Build rules
├── config.h          Compile-time constants, Config struct, AI IDs
├── config.c          Config parsing, defaults, validation
├── dungeon.h         Shared types, extern declarations, API
├── dungeon.c         World gen, movement, combat, monsters, LOS
├── ai.c              Superman (truth) + Jones (memory)
├── render.c          Playfield, HUD, minimap
├── main.c            Entry point, argument parsing, game loop
├── manual.md         Full user manual and design history
└── LICENSE
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

The stress test runs headless — no curses UI required. It's useful in CI, or as a smoke test after a build change.

---

## Troubleshooting

**"Terminal too small"** — the playfield needs 40×23 in normal mode, or 40×20 in `-w` watch mode. Enlarge the terminal, or edit `ROOM_W`/`ROOM_H` in `config.h` and rebuild.

**Build fails with `fatal error: ncurses.h: No such file or directory`** — the ncurses headers aren't visible to the compiler. On Linux, `sudo apt install libncurses-dev` (or distro equivalent). On Windows with PDCurses, the `ncurses.h` shim + `-IC:\pdcurses` flag must both be present. See [Building](#building).

**`undefined reference to 'initscr'` at link time** — the ncurses library isn't linked, or the name is wrong. Try `-lncursesw` instead of `-lncurses` on Linux. On Windows with PDCurses, confirm `C:\pdcurses\wincon\pdcurses.a` exists before building.

**PDCurses build fails with `unknown type name 'PCONSOLE_SCREEN_BUFFER_INFOEX'`** — your compiler predates the Windows API in question. Add `INFOEX=N` to the `mingw32-make` command (see [Building](#building)).

**`usleep is deprecated` warnings** — cosmetic. MinGW's `<unistd.h>` marks `usleep` as legacy; the function works fine. Suppress with `-Wno-deprecated-declarations` if desired.

**Same seed gives a different result** — expected if you rebuilt the game (RNG consumption changed), or if you're comparing Jones vs Superman (same dungeon, different exploration paths).

**`--stress` or `--print-config` work but the game won't draw** — the curses library linked correctly (they don't touch it) but the terminal can't initialize. Try running from a real terminal, not a piped/redirected context.

---

## License

MIT — see [LICENSE](LICENSE).

---

## Credits

Inspired by Atari's *Adventure* (1980). No code or assets from the original game are used — everything here is procedurally generated ASCII. Built as an exercise in making AI behavior feel like character rather than mechanism.

---

## Building

The game builds on Linux, macOS, and Windows. Choose the path that matches your platform.

### Linux

```bash
# Debian / Ubuntu
sudo apt install build-essential libncurses-dev

# Fedora
sudo dnf install gcc make ncurses-devel

# Arch
sudo pacman -S base-devel ncurses
```

Then:

```bash
make
# or, without the Makefile:
gcc -Wall -Wextra -O2 -std=c11 -o dg main.c config.c dungeon.c ai.c render.c -lncurses
```

### macOS

```bash
brew install ncurses
make
```

If Homebrew's ncurses isn't picked up automatically:

```bash
make CPPFLAGS=-I/opt/homebrew/opt/ncurses/include LDFLAGS=-L/opt/homebrew/opt/ncurses/lib
```

### Windows — two independent paths

Both produce a `dg.exe` that runs from plain Command Prompt. **Option A** uses MSYS2 (modern, needs DLLs). **Option B** uses MinGW.org + PDCurses (32-bit, statically linked, single-file portable executable).

#### Option A — MSYS2 + ncurses (64-bit, recommended for development)

Assumes MSYS2 installed at `C:\msys64` with `mingw-w64-x86_64-ncurses`.

```bash
# Inside MINGW64 shell
pacman -S mingw-w64-x86_64-ncurses
cd /c/dev/dungeon-adventure
gcc -Wall -Wextra -O2 -std=c11 \
    -I/mingw64/include/ncursesw \
    -o dg.exe main.c config.c dungeon.c ai.c render.c \
    -lncursesw
```

To run from CMD, make the DLL directory findable:

```cmd
set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "HOME=%USERPROFILE%"
dg.exe -w --ai=jones --seed 42
```

Or copy the required DLLs next to `dg.exe`:

```cmd
copy C:\msys64\mingw64\bin\libncursesw6.dll .
```

**Characteristics:** modern 64-bit compiler, best rendering fidelity, ~50 KB binary + ~600 KB of DLLs.

#### Option B — MinGW.org + PDCurses (32-bit, portable single-file executable)

Assumes MinGW.org GCC on PATH (`D:\MinGW\bin\gcc.exe`) and PDCurses 3.9 extracted to `C:\pdcurses`. PDCurses is source-compatible with ncurses — same API surface (`initscr`, `mvaddch`, `COLOR_PAIR`, etc.).

1. **Build PDCurses.** Older compilers (GCC ≤ 6) need `INFOEX=N` to skip the Windows `CONSOLE_SCREEN_BUFFER_INFOEX` API they don't know about:

   ```cmd
   cd /d C:\pdcurses\wincon
   mingw32-make -f Makefile CC=gcc AR=ar RANLIB=ranlib INFOEX=N
   ```

2. **Create an `ncurses.h` shim** so `#include <ncurses.h>` resolves to PDCurses' `curses.h`:

   ```cmd
   cd /d C:\dev\dungeon-adventure
   echo #include ^<curses.h^> > ncurses.h
   ```

3. **Build the game.** Note `-IC:\pdcurses` (the root) — PDCurses 3.9 places `curses.h` at the top of the archive, not in a subfolder:

   ```cmd
   gcc -Wall -Wextra -O2 -std=gnu11 -I. -IC:\pdcurses ^
       -o dg.exe main.c config.c dungeon.c ai.c render.c ^
       C:\pdcurses\wincon\pdcurses.a
   ```

4. **Run:**

   ```cmd
   set "HOME=%USERPROFILE%"
   dg.exe -w --ai=jones --seed 42
   ```

**Characteristics:** 32-bit, statically linked, **single-file executable** — copy `dg.exe` anywhere, no DLLs needed. Modern features like `A_DIM` may render as normal brightness in legacy `conhost.exe`; Windows Terminal renders them correctly.

**Notes for both Windows options:**

- `-std=gnu11` (not `-std=c11`) on MinGW — `usleep` needs GNU dialect.
- `set "HOME=%USERPROFILE%"` enables `~/.dungeonrc` and `~/.dungeon_highscore` on Windows. Windows doesn't set `HOME` by default.
- The `ncurses.h` shim is only used by Option B. **Do not commit it** — it shadows MSYS2's `ncurses.h` on Linux and breaks Option A's sibling build. Add it to `.gitignore`.

### Rebuilding after edits

`make` recompiles only what changed. Editing `config.h` triggers a rebuild of every module that includes it. Full rebuild:

```bash
make clean && make
```

Compile-time constants (`GRID_W`, `GRID_H`, `ROOM_W`, `ROOM_H`, `MAX_MONSTERS`) live in `config.h`. To change dungeon dimensions, edit that file and rebuild.
