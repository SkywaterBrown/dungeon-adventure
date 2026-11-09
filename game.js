// =============================================================
// Constants
// =============================================================
const GRID_W = 4, GRID_H = 4;
const ROOM_W = 40, ROOM_H = 20;
const MAX_MONSTERS = 3;
const INF = 999999;
const AI_JONES = 0, AI_SUPERMAN = 1;
const ROOM_EMPTY = 0, ROOM_TREASURE = 1, ROOM_DEN = 2, ROOM_MAZE = 3;
const CELL = 16;
const DIRS = [[1,0],[-1,0],[0,1],[0,-1]];

// =============================================================
// State
// =============================================================
const S = {
  cfg: {
    aiTickUs: 100000, manualTickUs: 30000,
    startHp: 3, monsterDensity: 55, chestDensity: 45,
    gPct: 40, bPct: 30, dPct: 30, maxMonsters: 3,
    wallsMin: 1, wallsMax: 3, loopPct: 30,
    torchEnabled: 1, torchRadius: 6,
    treasurePct: 15, denPct: 20, mazePct: 10,
    aiPersonality: AI_JONES, seed: 0,
  },
  rngState: 1, seedUsed: 0,
  rooms: [], roomWalls: [], seen: [], knownThreat: [],
  connEW: [], connNS: [], doorEW: [], doorNS: [],
  distMap: [],
  heroX: 0, heroY: 0, heroR: 0, heroC: 0, heroHP: 0,
  kills: 0, chestsCollected: 0, totalChests: 0, level: 1, tick: 0,
  score: 0, bestScore: 0,
  prevRoomR: -1, prevRoomC: -1,
  targetRoomR: -1, targetRoomC: -1,
  lastDx: 0, lastDy: 0,
  damageFlash: 0,
  aiMode: true, paused: false, ended: false,
};

// =============================================================
// RNG
// =============================================================
function nextRand() {
  let x = S.rngState;
  x ^= x << 13; x >>>= 0;
  x ^= x >>> 17;
  x ^= x << 5; x >>>= 0;
  S.rngState = x;
  return x;
}
function randInt(n) { return nextRand() % n; }

// =============================================================
// Helpers
// =============================================================
function isWall(x, y) { return x === 0 || x === ROOM_W - 1 || y === 0 || y === ROOM_H - 1; }
function make2D() { const a = []; for (let y = 0; y < ROOM_H; y++) a[y] = new Uint8Array(ROOM_W); return a; }
function make4D() {
  const a = [];
  for (let r = 0; r < GRID_H; r++) { a[r] = []; for (let c = 0; c < GRID_W; c++) a[r][c] = make2D(); }
  return a;
}
function makeRoom() {
  return {
    type: ROOM_EMPTY,
    connectN: 0, connectS: 0, connectE: 0, connectW: 0,
    doorN: 0, doorS: 0, doorE: 0, doorW: 0,
    monsters: [],
    hasChest: 0, chestX: 0, chestY: 0,
    hasPotion: 0, potionX: 0, potionY: 0,
    visited: 0,
  };
}

// =============================================================
// Dungeon generation
// =============================================================
function dfsMaze(r, c, vis) {
  vis[r * GRID_W + c] = 1;
  const dirs = [0, 1, 2, 3];
  for (let i = 3; i > 0; i--) { const j = randInt(i + 1); [dirs[i], dirs[j]] = [dirs[j], dirs[i]]; }
  for (const d of dirs) {
    let nr = r, nc = c;
    if (d === 0) nr--; else if (d === 1) nr++; else if (d === 2) nc++; else nc--;
    if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
    if (vis[nr * GRID_W + nc]) continue;
    if (nr === r - 1) S.connNS[r - 1][c] = 1;
    else if (nr === r + 1) S.connNS[r][c] = 1;
    else if (nc === c + 1) S.connEW[r][c] = 1;
    else S.connEW[r][c - 1] = 1;
    dfsMaze(nr, nc, vis);
  }
}

function isDoorInnerCell(rm, x, y) {
  if (rm.connectN && x === rm.doorN && y === 1) return true;
  if (rm.connectS && x === rm.doorS && y === ROOM_H - 2) return true;
  if (rm.connectW && x === 1 && y === rm.doorW) return true;
  if (rm.connectE && x === ROOM_W - 2 && y === rm.doorE) return true;
  return false;
}

function roomFloodOk(walls) {
  const vis = [];
  for (let y = 0; y < ROOM_H; y++) vis[y] = new Uint8Array(ROOM_W);
  let sx = -1, sy = -1, total = 0;
  for (let y = 1; y < ROOM_H - 1; y++)
    for (let x = 1; x < ROOM_W - 1; x++)
      if (!walls[y][x]) { total++; if (sx < 0) { sx = x; sy = y; } }
  if (sx < 0) return true;
  const q = [[sx, sy]]; vis[sy][sx] = 1;
  let count = 0, qh = 0;
  while (qh < q.length) {
    const [cx, cy] = q[qh++]; count++;
    for (const [dx, dy] of DIRS) {
      const nx = cx + dx, ny = cy + dy;
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (walls[ny][nx] || vis[ny][nx]) continue;
      vis[ny][nx] = 1; q.push([nx, ny]);
    }
  }
  return count === total;
}

function generateRoomWalls(r, c, wmin, wmax) {
  const walls = S.roomWalls[r][c];
  for (let y = 0; y < ROOM_H; y++) walls[y].fill(0);
  const span = Math.max(1, wmax - wmin + 1);
  const num = wmin + randInt(span);
  for (let s = 0; s < num; s++) {
    const horiz = randInt(2) === 1;
    let len, x1, y1;
    if (horiz) {
      len = 4 + randInt(8);
      const maxX1 = ROOM_W - 3 - len;
      if (maxX1 < 2) continue;
      x1 = 2 + randInt(maxX1 - 1);
      y1 = 2 + randInt(ROOM_H - 4);
    } else {
      len = 3 + randInt(6);
      const maxY1 = ROOM_H - 3 - len;
      if (maxY1 < 2) continue;
      x1 = 2 + randInt(ROOM_W - 4);
      y1 = 2 + randInt(maxY1 - 1);
    }
    let ok = true;
    for (let i = 0; i <= len; i++) {
      const wx = horiz ? x1 + i : x1, wy = horiz ? y1 : y1 + i;
      if (isDoorInnerCell(S.rooms[r][c], wx, wy)) { ok = false; break; }
    }
    if (!ok) continue;
    for (let i = 0; i <= len; i++) {
      const wx = horiz ? x1 + i : x1, wy = horiz ? y1 : y1 + i;
      walls[wy][wx] = 1;
    }
    if (!roomFloodOk(walls)) {
      for (let i = 0; i <= len; i++) {
        const wx = horiz ? x1 + i : x1, wy = horiz ? y1 : y1 + i;
        walls[wy][wx] = 0;
      }
    }
  }
}

function placeChestInRoom(r, c) {
  const rm = S.rooms[r][c];
  for (let at = 0; at < 50; at++) {
    const cx = 2 + randInt(ROOM_W - 4), cy = 2 + randInt(ROOM_H - 4);
    if (S.roomWalls[r][c][cy][cx]) continue;
    let ov = false;
    for (const m of rm.monsters) if (m.x === cx && m.y === cy) ov = true;
    if (ov) continue;
    if (rm.hasPotion && rm.potionX === cx && rm.potionY === cy) continue;
    rm.hasChest = 1; rm.chestX = cx; rm.chestY = cy;
    return true;
  }
  return false;
}

function placeMonsterInRoom(r, c) {
  const rm = S.rooms[r][c];
  if (rm.monsters.length >= S.cfg.maxMonsters) return false;
  const roll = randInt(100);
  let t;
  if (roll < S.cfg.gPct) t = 'G';
  else if (roll < S.cfg.gPct + S.cfg.bPct) t = 'B';
  else t = 'D';
  for (let at = 0; at < 50; at++) {
    const mx = 2 + randInt(ROOM_W - 4), my = 2 + randInt(ROOM_H - 4);
    if (S.roomWalls[r][c][my][mx]) continue;
    let ov = false;
    for (const m of rm.monsters) if (m.x === mx && m.y === my) ov = true;
    if (ov) continue;
    if (rm.hasChest && rm.chestX === mx && rm.chestY === my) continue;
    rm.monsters.push({ x: mx, y: my, type: t, alive: 1 });
    return true;
  }
  return false;
}

function generateDungeon() {
  S.rooms = []; S.roomWalls = []; S.seen = []; S.knownThreat = [];
  S.connEW = []; S.connNS = []; S.doorEW = []; S.doorNS = [];
  for (let r = 0; r < GRID_H; r++) {
    S.rooms[r] = [];
    S.connEW[r] = new Uint8Array(GRID_W);
    S.connNS[r] = new Uint8Array(GRID_W);
    S.doorEW[r] = new Int32Array(GRID_W);
    S.doorNS[r] = new Int32Array(GRID_W);
    for (let c = 0; c < GRID_W; c++) S.rooms[r][c] = makeRoom();
  }
  S.roomWalls = make4D();
  S.seen = make4D();
  S.knownThreat = make4D();
  S.distMap = [];
  for (let y = 0; y < ROOM_H; y++) S.distMap[y] = new Int32Array(ROOM_W);

  const vis = new Uint8Array(GRID_H * GRID_W);
  dfsMaze(0, 0, vis);

  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
    if (c < GRID_W - 1 && !S.connEW[r][c] && randInt(100) < S.cfg.loopPct) S.connEW[r][c] = 1;
    if (r < GRID_H - 1 && !S.connNS[r][c] && randInt(100) < S.cfg.loopPct) S.connNS[r][c] = 1;
  }
  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W - 1; c++)
    if (S.connEW[r][c]) S.doorEW[r][c] = 1 + randInt(ROOM_H - 2);
  for (let r = 0; r < GRID_H - 1; r++) for (let c = 0; c < GRID_W; c++)
    if (S.connNS[r][c]) S.doorNS[r][c] = 1 + randInt(ROOM_W - 2);

  const cumT = S.cfg.treasurePct, cumD = cumT + S.cfg.denPct, cumM = cumD + S.cfg.mazePct;
  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
    const roll = randInt(100);
    if (roll < cumT) S.rooms[r][c].type = ROOM_TREASURE;
    else if (roll < cumD) S.rooms[r][c].type = ROOM_DEN;
    else if (roll < cumM) S.rooms[r][c].type = ROOM_MAZE;
    else S.rooms[r][c].type = ROOM_EMPTY;
  }

  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
    const rm = S.rooms[r][c];
    if (r > 0 && S.connNS[r-1][c]) { rm.connectN = 1; rm.doorN = S.doorNS[r-1][c]; }
    if (r < GRID_H - 1 && S.connNS[r][c]) { rm.connectS = 1; rm.doorS = S.doorNS[r][c]; }
    if (c < GRID_W - 1 && S.connEW[r][c]) { rm.connectE = 1; rm.doorE = S.doorEW[r][c]; }
    if (c > 0 && S.connEW[r][c-1]) { rm.connectW = 1; rm.doorW = S.doorEW[r][c-1]; }
  }

  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
    let wmin = S.cfg.wallsMin, wmax = S.cfg.wallsMax;
    switch (S.rooms[r][c].type) {
      case ROOM_TREASURE: wmin = 0; wmax = 1; break;
      case ROOM_DEN: wmin = 5; wmax = 8; break;
      case ROOM_MAZE: wmin = 8; wmax = 10; break;
    }
    generateRoomWalls(r, c, wmin, wmax);
  }

  S.totalChests = 0; S.chestsCollected = 0;
  for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
    const rm = S.rooms[r][c];
    switch (rm.type) {
      case ROOM_TREASURE: if (placeChestInRoom(r, c)) S.totalChests++; break;
      case ROOM_DEN: {
        let n = 2 + randInt(2);
        if (n > S.cfg.maxMonsters) n = S.cfg.maxMonsters;
        for (let i = 0; i < n; i++) placeMonsterInRoom(r, c);
        break;
      }
      case ROOM_MAZE: if (placeChestInRoom(r, c)) S.totalChests++; break;
      default:
        if (randInt(100) < S.cfg.monsterDensity) {
          const n = 1 + randInt(2);
          for (let i = 0; i < n; i++) placeMonsterInRoom(r, c);
        }
        if (randInt(100) < S.cfg.chestDensity) {
          if (placeChestInRoom(r, c)) S.totalChests++;
        }
        break;
    }
  }
  if (S.totalChests === 0) {
    outer: for (let r = 0; r < GRID_H; r++) for (let c = 0; c < GRID_W; c++) {
      if (placeChestInRoom(r, c)) { S.totalChests++; break outer; }
    }
  }

  const pr = randInt(GRID_H), pc = randInt(GRID_W);
  const prm = S.rooms[pr][pc];
  for (let at = 0; at < 200; at++) {
    const px = 2 + randInt(ROOM_W - 4), py = 2 + randInt(ROOM_H - 4);
    if (S.roomWalls[pr][pc][py][px]) continue;
    if (prm.hasChest && prm.chestX === px && prm.chestY === py) continue;
    let ov = false;
    for (const m of prm.monsters) if (m.x === px && m.y === py) ov = true;
    if (ov) continue;
    prm.hasPotion = 1; prm.potionX = px; prm.potionY = py;
    break;
  }
}

function spawnHero() {
  S.heroR = randInt(GRID_H); S.heroC = randInt(GRID_W);
  const rm = S.rooms[S.heroR][S.heroC];
  for (let at = 0; at < 500; at++) {
    S.heroX = 1 + randInt(ROOM_W - 2);
    S.heroY = 1 + randInt(ROOM_H - 2);
    if (S.roomWalls[S.heroR][S.heroC][S.heroY][S.heroX]) continue;
    let ok = true;
    if (rm.hasChest && S.heroX === rm.chestX && S.heroY === rm.chestY) ok = false;
    if (rm.hasPotion && S.heroX === rm.potionX && S.heroY === rm.potionY) ok = false;
    for (const m of rm.monsters) if (m.alive && m.x === S.heroX && m.y === S.heroY) ok = false;
    if (ok) break;
  }
  S.heroHP = S.cfg.startHp;
  rm.visited = 1;
  S.lastDx = 0; S.lastDy = 0; S.damageFlash = 0;
  updateSeen();
}

// =============================================================
// Movement
// =============================================================
function heroStep(dx, dy) {
  const rm = S.rooms[S.heroR][S.heroC];
  const nx = S.heroX + dx, ny = S.heroY + dy;
  const oldR = S.heroR, oldC = S.heroC;
  let moved = false;
  if (rm.connectN && dx === 0 && dy === -1 && nx === rm.doorN && ny === 0) {
    S.heroR--; S.heroY = ROOM_H - 2; moved = true;
  } else if (rm.connectS && dx === 0 && dy === 1 && nx === rm.doorS && ny === ROOM_H - 1) {
    S.heroR++; S.heroY = 1; moved = true;
  } else if (rm.connectW && dx === -1 && dy === 0 && nx === 0 && ny === rm.doorW) {
    S.heroC--; S.heroX = ROOM_W - 2; moved = true;
  } else if (rm.connectE && dx === 1 && dy === 0 && nx === ROOM_W - 1 && ny === rm.doorE) {
    S.heroC++; S.heroX = 1; moved = true;
  }
  if (moved) {
    S.rooms[oldR][oldC].visited = 1;
    S.rooms[S.heroR][S.heroC].visited = 1;
    S.prevRoomR = oldR; S.prevRoomC = oldC;
    S.lastDx = dx; S.lastDy = dy;
    updateSeen();
    return;
  }
  if (isWall(nx, ny)) return;
  if (S.roomWalls[S.heroR][S.heroC][ny][nx]) return;
  S.heroX = nx; S.heroY = ny;
  S.lastDx = dx; S.lastDy = dy;
  updateSeen();
}

function tryMove(dx, dy) {
  const ox = S.heroX, oy = S.heroY, orr = S.heroR, occ = S.heroC;
  heroStep(dx, dy);
  return !(S.heroX === ox && S.heroY === oy && S.heroR === orr && S.heroC === occ);
}

function moveMonsters() {
  const rm = S.rooms[S.heroR][S.heroC];
  let needDist = false;
  for (const m of rm.monsters) if (m.alive && m.type === 'D') { needDist = true; break; }
  if (needDist) computeDistMap(S.heroR, S.heroC, S.heroX, S.heroY);

  for (const m of rm.monsters) {
    if (!m.alive) continue;
    const should = (m.type === 'G' && S.tick % 3 === 0) ||
                   (m.type === 'B') ||
                   (m.type === 'D' && S.tick % 2 === 0);
    if (!should) continue;
    let mdx = 0, mdy = 0;
    if (m.type === 'D') {
      const curD = S.distMap[m.y][m.x];
      if (curD >= INF) continue;
      let bestD = curD;
      for (const [dx, dy] of DIRS) {
        const nx = m.x + dx, ny = m.y + dy;
        if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
        if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
        if (S.distMap[ny][nx] < bestD) {
          bestD = S.distMap[ny][nx];
          mdx = dx; mdy = dy;
        }
      }
    } else {
      const rr = randInt(5);
      if (rr === 0) mdx = -1; else if (rr === 1) mdx = 1;
      else if (rr === 2) mdy = -1; else if (rr === 3) mdy = 1;
    }
    const nx = m.x + mdx, ny = m.y + mdy;
    if (isWall(nx, ny)) {
      // blocked
    } else if (S.roomWalls[S.heroR][S.heroC][ny][nx]) {
      if (m.type === 'B') {
        const fx = nx + mdx, fy = ny + mdy;
        if (fx >= 1 && fx < ROOM_W - 1 && fy >= 1 && fy < ROOM_H - 1 &&
            !S.roomWalls[S.heroR][S.heroC][fy][fx]) {
          m.x = fx; m.y = fy;
        }
      }
    } else {
      m.x = nx; m.y = ny;
    }
  }
}

// =============================================================
// Interactions
// =============================================================
function checkCollisions() {
  const rm = S.rooms[S.heroR][S.heroC];
  for (const m of rm.monsters) {
    if (!m.alive) continue;
    if (m.x === S.heroX && m.y === S.heroY) {
      m.alive = 0; S.kills++; S.score += 10;
      if (m.type === 'D') { S.heroHP--; S.damageFlash = 3; }
    }
  }
}

function collectChest() {
  const rm = S.rooms[S.heroR][S.heroC];
  if (rm.hasChest && S.heroX === rm.chestX && S.heroY === rm.chestY) {
    rm.hasChest = 0;
    S.chestsCollected++;
    S.score += 100;
  }
}

function collectPotion() {
  const rm = S.rooms[S.heroR][S.heroC];
  if (rm.hasPotion && S.heroX === rm.potionX && S.heroY === rm.potionY) {
    if (S.heroHP >= S.cfg.startHp) return;
    rm.hasPotion = 0;
    S.heroHP++;
  }
}

// =============================================================
// Line of sight
// =============================================================
function hasLOS(x0, y0, x1, y1, r, c) {
  const dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  let err = dx + dy, x = x0, y = y0;
  while (true) {
    if (x === x1 && y === y1) return true;
    const e2 = 2 * err;
    const ox = x, oy = y;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
    if (x !== ox && y !== oy) {
      if (isWall(x, oy) || S.roomWalls[r][c][oy][x]) return false;
      if (isWall(ox, y) || S.roomWalls[r][c][y][ox]) return false;
    }
    if (x === x1 && y === y1) return true;
    if (isWall(x, y) || S.roomWalls[r][c][y][x]) return false;
  }
}

function updateSeen() {
  const torch = S.cfg.torchEnabled;
  const r2 = S.cfg.torchRadius * S.cfg.torchRadius;
  const rm = S.rooms[S.heroR][S.heroC];
  for (let y = 0; y < ROOM_H; y++) {
    for (let x = 0; x < ROOM_W; x++) {
      let visible = false;
      if (hasLOS(S.heroX, S.heroY, x, y, S.heroR, S.heroC)) {
        const dx = x - S.heroX, dy = y - S.heroY;
        if (!torch || dx * dx + dy * dy <= r2) visible = true;
      }
      if (!visible) continue;
      S.seen[S.heroR][S.heroC][y][x] = 1;
      let mt = 0;
      for (const m of rm.monsters) {
        if (!m.alive) continue;
        if (m.x === x && m.y === y) { mt = m.type; break; }
      }
      S.knownThreat[S.heroR][S.heroC][y][x] =
        mt === 'B' ? 2 : mt === 'D' ? 3 : mt === 'G' ? 1 : 0;
    }
  }
}

// =============================================================
// Distance maps
// =============================================================
function computeDistMap(r, c, tx, ty) {
  for (let y = 0; y < ROOM_H; y++) for (let x = 0; x < ROOM_W; x++) S.distMap[y][x] = INF;
  if (isWall(tx, ty) || S.roomWalls[r][c][ty][tx]) return;
  const q = [[tx, ty]];
  S.distMap[ty][tx] = 0;
  let qh = 0;
  while (qh < q.length) {
    const [cx, cy] = q[qh++];
    for (const [dx, dy] of DIRS) {
      const nx = cx + dx, ny = cy + dy;
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (S.roomWalls[r][c][ny][nx]) continue;
      if (S.distMap[ny][nx] !== INF) continue;
      S.distMap[ny][nx] = S.distMap[cy][cx] + 1;
      q.push([nx, ny]);
    }
  }
}

function computeDistMapJones(r, c, tx, ty) {
  for (let y = 0; y < ROOM_H; y++) for (let x = 0; x < ROOM_W; x++) S.distMap[y][x] = INF;
  if (isWall(tx, ty)) return;
  if (!S.seen[r][c][ty][tx]) return;
  if (S.roomWalls[r][c][ty][tx]) return;
  const q = [[tx, ty]];
  S.distMap[ty][tx] = 0;
  let qh = 0;
  while (qh < q.length) {
    const [cx, cy] = q[qh++];
    for (const [dx, dy] of DIRS) {
      const nx = cx + dx, ny = cy + dy;
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (!S.seen[r][c][ny][nx]) continue;
      if (S.roomWalls[r][c][ny][nx]) continue;
      if (S.distMap[ny][nx] !== INF) continue;
      S.distMap[ny][nx] = S.distMap[cy][cx] + 1;
      q.push([nx, ny]);
    }
  }
}

// =============================================================
// Room-graph BFS
// =============================================================
function bfsRoomNextStep(sr, sc, tr, tc) {
  const vis = new Uint8Array(GRID_H * GRID_W);
  const parR = new Int32Array(GRID_H * GRID_W).fill(-2);
  const parC = new Int32Array(GRID_H * GRID_W).fill(-2);
  const q = [[sr, sc]];
  vis[sr * GRID_W + sc] = 1;
  parR[sr * GRID_W + sc] = -1; parC[sr * GRID_W + sc] = -1;
  let qh = 0;
  while (qh < q.length) {
    const [cr, cc] = q[qh++];
    if (cr === tr && cc === tc) {
      let stepR = cr, stepC = cc;
      let pR = parR[stepR * GRID_W + stepC], pC = parC[stepR * GRID_W + stepC];
      while (pR !== -1 && !(pR === sr && pC === sc)) {
        stepR = pR; stepC = pC;
        pR = parR[stepR * GRID_W + stepC]; pC = parC[stepR * GRID_W + stepC];
      }
      return [stepR, stepC];
    }
    const rm = S.rooms[cr][cc];
    const nrs = [cr - 1, cr + 1, cr, cr];
    const ncs = [cc, cc, cc - 1, cc + 1];
    const vals = [rm.connectN, rm.connectS, rm.connectW, rm.connectE];
    for (let k = 0; k < 4; k++) {
      if (!vals[k]) continue;
      const nr = nrs[k], nc = ncs[k];
      if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
      if (vis[nr * GRID_W + nc]) continue;
      vis[nr * GRID_W + nc] = 1;
      parR[nr * GRID_W + nc] = cr; parC[nr * GRID_W + nc] = cc;
      q.push([nr, nc]);
    }
  }
  return null;
}

// =============================================================
// Superman AI
// =============================================================
function cellDangerTruth(x, y, skip) {
  const rm = S.rooms[S.heroR][S.heroC];
  let dg = 0;
  for (let i = 0; i < rm.monsters.length; i++) {
    if (!rm.monsters[i].alive) continue;
    if (i === skip) continue;
    const md = Math.abs(rm.monsters[i].x - x) + Math.abs(rm.monsters[i].y - y);
    if (md <= 2) dg += (3 - md);
  }
  return dg;
}

function supermanMove(tx, ty) {
  computeDistMap(S.heroR, S.heroC, tx, ty);
  const curD = S.distMap[S.heroY][S.heroX];
  if (curD >= INF || curD === 0) return;

  const rm = S.rooms[S.heroR][S.heroC];
  let attackIdx = -1, threat = 0;
  for (let i = 0; i < rm.monsters.length; i++) {
    if (!rm.monsters[i].alive) continue;
    if (rm.monsters[i].x === tx && rm.monsters[i].y === ty) attackIdx = i;
    const md = Math.abs(rm.monsters[i].x - S.heroX) + Math.abs(rm.monsters[i].y - S.heroY);
    if (md < 4) threat = 1;
  }
  if (!threat && curD > 3 && randInt(100) < 3) return;
  if (!threat && curD > 5 && randInt(100) < 8) {
    const pxs = [0, 0], pys = [0, 0];
    if (S.lastDx !== 0) { pys[0] = 1; pys[1] = -1; }
    else { pxs[0] = 1; pxs[1] = -1; }
    const start = randInt(2);
    for (let i = 0; i < 2; i++) {
      const k = (start + i) % 2;
      const nx = S.heroX + pxs[k], ny = S.heroY + pys[k];
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
      if (S.distMap[ny][nx] >= INF) continue;
      if (cellDangerTruth(nx, ny, attackIdx) > 0) continue;
      if (tryMove(pxs[k], pys[k])) return;
    }
  }
  const cand = [];
  for (const [dx, dy] of DIRS) {
    const nx = S.heroX + dx, ny = S.heroY + dy;
    if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
    if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
    if (S.distMap[ny][nx] !== curD - 1) continue;
    cand.push({ dx, dy, danger: cellDangerTruth(nx, ny, attackIdx) });
  }
  if (cand.length === 0) return;
  let minD = cand[0].danger;
  for (const c of cand) if (c.danger < minD) minD = c.danger;
  const keep = cand.filter(c => c.danger === minD);
  let pick = keep.findIndex(c => c.dx === S.lastDx && c.dy === S.lastDy);
  if (pick < 0 || randInt(100) < 30) pick = randInt(keep.length);
  tryMove(keep[pick].dx, keep[pick].dy);
}

function roomHasContent(r, c) {
  const rm = S.rooms[r][c];
  if (rm.hasChest) return true;
  if (rm.hasPotion && S.heroHP < S.cfg.startHp) return true;
  for (const m of rm.monsters) if (m.alive) return true;
  return false;
}

function supermanPickTarget() {
  const vis = new Uint8Array(GRID_H * GRID_W);
  const parR = new Int32Array(GRID_H * GRID_W).fill(-2);
  const parC = new Int32Array(GRID_H * GRID_W).fill(-2);
  const q = [[S.heroR, S.heroC]];
  vis[S.heroR * GRID_W + S.heroC] = 1;
  parR[S.heroR * GRID_W + S.heroC] = -1; parC[S.heroR * GRID_W + S.heroC] = -1;
  let fbR = -1, fbC = -1;
  let qh = 0;
  while (qh < q.length) {
    const [r, c] = q[qh++];
    if (!(r === S.heroR && c === S.heroC) && roomHasContent(r, c)) {
      let stepR = r, stepC = c;
      let pR = parR[stepR * GRID_W + stepC], pC = parC[stepR * GRID_W + stepC];
      while (pR !== -1 && !(pR === S.heroR && pC === S.heroC)) {
        stepR = pR; stepC = pC;
        pR = parR[stepR * GRID_W + stepC]; pC = parC[stepR * GRID_W + stepC];
      }
      if (stepR !== S.prevRoomR || stepC !== S.prevRoomC) {
        S.targetRoomR = r; S.targetRoomC = c; return;
      }
      if (fbR < 0) { fbR = r; fbC = c; }
    }
    const rm = S.rooms[r][c];
    const nrs = [r - 1, r + 1, r, r];
    const ncs = [c, c, c - 1, c + 1];
    const vals = [rm.connectN, rm.connectS, rm.connectW, rm.connectE];
    for (let k = 0; k < 4; k++) {
      if (!vals[k]) continue;
      const nr = nrs[k], nc = ncs[k];
      if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
      if (vis[nr * GRID_W + nc]) continue;
      vis[nr * GRID_W + nc] = 1;
      parR[nr * GRID_W + nc] = r; parC[nr * GRID_W + nc] = c;
      q.push([nr, nc]);
    }
  }
  if (fbR >= 0) { S.targetRoomR = fbR; S.targetRoomC = fbC; }
  else { S.targetRoomR = -1; S.targetRoomC = -1; }
}

function supermanTick() {
  const rm = S.rooms[S.heroR][S.heroC];
  if (rm.hasChest) { supermanMove(rm.chestX, rm.chestY); return; }
  if (rm.hasPotion && S.heroHP < S.cfg.startHp) { supermanMove(rm.potionX, rm.potionY); return; }
  let mi = -1, mdist = INF;
  for (let i = 0; i < rm.monsters.length; i++) {
    if (!rm.monsters[i].alive) continue;
    if (S.heroHP <= 1 && rm.monsters[i].type === 'D') continue;
    const d = Math.abs(rm.monsters[i].x - S.heroX) + Math.abs(rm.monsters[i].y - S.heroY);
    if (d < mdist) { mdist = d; mi = i; }
  }
  if (mi >= 0) { supermanMove(rm.monsters[mi].x, rm.monsters[mi].y); return; }

  if (S.targetRoomR < 0 || !roomHasContent(S.targetRoomR, S.targetRoomC) ||
      (S.targetRoomR === S.heroR && S.targetRoomC === S.heroC)) {
    supermanPickTarget();
  }
  if (S.targetRoomR < 0) return;
  const next = bfsRoomNextStep(S.heroR, S.heroC, S.targetRoomR, S.targetRoomC);
  if (!next) return;
  const [nr, nc] = next;
  let innerX = -1, innerY = -1, ddx = 0, ddy = 0;
  if (nr === S.heroR - 1 && rm.connectN) { innerX = rm.doorN; innerY = 1; ddx = 0; ddy = -1; }
  else if (nr === S.heroR + 1 && rm.connectS) { innerX = rm.doorS; innerY = ROOM_H - 2; ddx = 0; ddy = 1; }
  else if (nc === S.heroC - 1 && rm.connectW) { innerX = 1; innerY = rm.doorW; ddx = -1; ddy = 0; }
  else if (nc === S.heroC + 1 && rm.connectE) { innerX = ROOM_W - 2; innerY = rm.doorE; ddx = 1; ddy = 0; }
  else return;
  if (S.heroX === innerX && S.heroY === innerY) heroStep(ddx, ddy);
  else supermanMove(innerX, innerY);
}

// =============================================================
// Jones AI
// =============================================================
function cellDangerKnown(x, y, skipX, skipY) {
  let dg = 0;
  for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++) {
    const md = Math.abs(dx) + Math.abs(dy);
    if (md > 2 || md === 0) continue;
    const cx = x + dx, cy = y + dy;
    if (cx < 1 || cx >= ROOM_W - 1 || cy < 1 || cy >= ROOM_H - 1) continue;
    if (cx === skipX && cy === skipY) continue;
    if (S.knownThreat[S.heroR][S.heroC][cy][cx]) dg += (3 - md);
  }
  return dg;
}

function jonesMove(tx, ty) {
  computeDistMapJones(S.heroR, S.heroC, tx, ty);
  const curD = S.distMap[S.heroY][S.heroX];
  if (curD >= INF || curD === 0) return false;

  let threat = 0;
  for (let dy = -3; dy <= 3 && !threat; dy++) for (let dx = -3; dx <= 3; dx++) {
    const md = Math.abs(dx) + Math.abs(dy);
    if (md === 0 || md >= 4) continue;
    const cx = S.heroX + dx, cy = S.heroY + dy;
    if (cx < 1 || cx >= ROOM_W - 1 || cy < 1 || cy >= ROOM_H - 1) continue;
    if (S.knownThreat[S.heroR][S.heroC][cy][cx]) { threat = 1; break; }
  }
  if (!threat && curD > 3 && randInt(100) < 3) return false;
  if (!threat && curD > 5 && randInt(100) < 8) {
    const pxs = [0, 0], pys = [0, 0];
    if (S.lastDx !== 0) { pys[0] = 1; pys[1] = -1; }
    else { pxs[0] = 1; pxs[1] = -1; }
    const start = randInt(2);
    for (let i = 0; i < 2; i++) {
      const k = (start + i) % 2;
      const nx = S.heroX + pxs[k], ny = S.heroY + pys[k];
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (!S.seen[S.heroR][S.heroC][ny][nx]) continue;
      if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
      if (S.distMap[ny][nx] >= INF) continue;
      if (cellDangerKnown(nx, ny, tx, ty) > 0) continue;
      if (tryMove(pxs[k], pys[k])) return true;
    }
  }
  const cand = [];
  for (const [dx, dy] of DIRS) {
    const nx = S.heroX + dx, ny = S.heroY + dy;
    if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
    if (!S.seen[S.heroR][S.heroC][ny][nx]) continue;
    if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
    if (S.distMap[ny][nx] !== curD - 1) continue;
    cand.push({ dx, dy, danger: cellDangerKnown(nx, ny, tx, ty) });
  }
  if (cand.length === 0) return false;
  let minD = cand[0].danger;
  for (const c of cand) if (c.danger < minD) minD = c.danger;
  const keep = cand.filter(c => c.danger === minD);
  let pick = keep.findIndex(c => c.dx === S.lastDx && c.dy === S.lastDy);
  if (pick < 0 || randInt(100) < 30) pick = randInt(keep.length);
  return tryMove(keep[pick].dx, keep[pick].dy);
}

function jonesRoomHasPotential(r, c) {
  const rm = S.rooms[r][c];
  if (!rm.visited) return true;
  if (rm.hasChest && S.seen[r][c][rm.chestY][rm.chestX]) return true;
  if (rm.hasPotion && S.heroHP < S.cfg.startHp && S.seen[r][c][rm.potionY][rm.potionX]) return true;
  for (let y = 1; y < ROOM_H - 1; y++)
    for (let x = 1; x < ROOM_W - 1; x++) {
      if (!S.seen[r][c][y][x]) return true;
      if (S.knownThreat[r][c][y][x]) return true;
    }
  return false;
}

function jonesPickTarget() {
  const vis = new Uint8Array(GRID_H * GRID_W);
  const parR = new Int32Array(GRID_H * GRID_W).fill(-2);
  const parC = new Int32Array(GRID_H * GRID_W).fill(-2);
  const q = [[S.heroR, S.heroC]];
  vis[S.heroR * GRID_W + S.heroC] = 1;
  parR[S.heroR * GRID_W + S.heroC] = -1; parC[S.heroR * GRID_W + S.heroC] = -1;
  let fbR = -1, fbC = -1;
  let qh = 0;
  while (qh < q.length) {
    const [r, c] = q[qh++];
    if (!(r === S.heroR && c === S.heroC) && jonesRoomHasPotential(r, c)) {
      let stepR = r, stepC = c;
      let pR = parR[stepR * GRID_W + stepC], pC = parC[stepR * GRID_W + stepC];
      while (pR !== -1 && !(pR === S.heroR && pC === S.heroC)) {
        stepR = pR; stepC = pC;
        pR = parR[stepR * GRID_W + stepC]; pC = parC[stepR * GRID_W + stepC];
      }
      if (stepR !== S.prevRoomR || stepC !== S.prevRoomC) {
        S.targetRoomR = r; S.targetRoomC = c; return;
      }
      if (fbR < 0) { fbR = r; fbC = c; }
    }
    const rm = S.rooms[r][c];
    const nrs = [r - 1, r + 1, r, r];
    const ncs = [c, c, c - 1, c + 1];
    const vals = [rm.connectN, rm.connectS, rm.connectW, rm.connectE];
    for (let k = 0; k < 4; k++) {
      if (!vals[k]) continue;
      const nr = nrs[k], nc = ncs[k];
      if (nr < 0 || nr >= GRID_H || nc < 0 || nc >= GRID_W) continue;
      if (vis[nr * GRID_W + nc]) continue;
      vis[nr * GRID_W + nc] = 1;
      parR[nr * GRID_W + nc] = r; parC[nr * GRID_W + nc] = c;
      q.push([nr, nc]);
    }
  }
  if (fbR >= 0) { S.targetRoomR = fbR; S.targetRoomC = fbC; }
  else { S.targetRoomR = -1; S.targetRoomC = -1; }
}

function jonesStepTowardUnknown() {
  const dist = [], par = [];
  for (let y = 0; y < ROOM_H; y++) {
    dist[y] = new Int32Array(ROOM_W).fill(-1);
    par[y] = new Array(ROOM_W).fill(null);
  }
  const q = [];
  for (let y = 1; y < ROOM_H - 1; y++) for (let x = 1; x < ROOM_W - 1; x++) {
    if (S.seen[S.heroR][S.heroC][y][x]) continue;
    let adj = false;
    for (const [dx, dy] of DIRS) {
      const nx = x + dx, ny = y + dy;
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (S.seen[S.heroR][S.heroC][ny][nx] && !S.roomWalls[S.heroR][S.heroC][ny][nx]) {
        adj = true; break;
      }
    }
    if (adj) { dist[y][x] = 0; par[y][x] = [-1, -1]; q.push([x, y]); }
  }
  if (q.length === 0) return null;
  let qh = 0;
  while (qh < q.length) {
    const [cx, cy] = q[qh++];
    for (const [dx, dy] of DIRS) {
      const nx = cx + dx, ny = cy + dy;
      if (nx < 1 || nx >= ROOM_W - 1 || ny < 1 || ny >= ROOM_H - 1) continue;
      if (dist[ny][nx] !== -1) continue;
      if (!S.seen[S.heroR][S.heroC][ny][nx]) continue;
      if (S.roomWalls[S.heroR][S.heroC][ny][nx]) continue;
      dist[ny][nx] = dist[cy][cx] + 1;
      par[ny][nx] = [cx, cy];
      q.push([nx, ny]);
    }
  }
  if (dist[S.heroY][S.heroX] < 0) return null;
  const first = par[S.heroY][S.heroX];
  if (!first || first[0] === -1) return null;
  return [first[0] - S.heroX, first[1] - S.heroY];
}

function jonesTick() {
  const rm = S.rooms[S.heroR][S.heroC];
  if (rm.hasChest && S.seen[S.heroR][S.heroC][rm.chestY][rm.chestX]) {
    if (jonesMove(rm.chestX, rm.chestY)) return;
  }
  if (rm.hasPotion && S.heroHP < S.cfg.startHp &&
      S.seen[S.heroR][S.heroC][rm.potionY][rm.potionX]) {
    if (jonesMove(rm.potionX, rm.potionY)) return;
  }
  let mi = -1, mdist = INF;
  for (let i = 0; i < rm.monsters.length; i++) {
    const m = rm.monsters[i];
    if (!m.alive) continue;
    if (!S.seen[S.heroR][S.heroC][m.y][m.x]) continue;
    if (S.heroHP <= 1 && m.type === 'D') continue;
    const d = Math.abs(m.x - S.heroX) + Math.abs(m.y - S.heroY);
    if (d < mdist) { mdist = d; mi = i; }
  }
  if (mi >= 0) {
    if (jonesMove(rm.monsters[mi].x, rm.monsters[mi].y)) return;
  }
  if (S.targetRoomR < 0 ||
      (S.targetRoomR === S.heroR && S.targetRoomC === S.heroC) ||
      !jonesRoomHasPotential(S.targetRoomR, S.targetRoomC)) {
    jonesPickTarget();
  }
  if (S.targetRoomR < 0) {
    const adjR = [], adjC = [];
    if (rm.connectN) { adjR.push(S.heroR - 1); adjC.push(S.heroC); }
    if (rm.connectS) { adjR.push(S.heroR + 1); adjC.push(S.heroC); }
    if (rm.connectW) { adjR.push(S.heroR); adjC.push(S.heroC - 1); }
    if (rm.connectE) { adjR.push(S.heroR); adjC.push(S.heroC + 1); }
    let found = -1;
    for (let i = 0; i < adjR.length; i++)
      if (adjR[i] !== S.prevRoomR || adjC[i] !== S.prevRoomC) { found = i; break; }
    if (found < 0 && adjR.length > 0) found = 0;
    if (found >= 0) { S.targetRoomR = adjR[found]; S.targetRoomC = adjC[found]; }
  }
  if (S.targetRoomR >= 0) {
    const next = bfsRoomNextStep(S.heroR, S.heroC, S.targetRoomR, S.targetRoomC);
    if (next) {
      const [nr, nc] = next;
      let innerX = -1, innerY = -1, ddx = 0, ddy = 0;
      if (nr === S.heroR - 1 && rm.connectN) { innerX = rm.doorN; innerY = 1; ddx = 0; ddy = -1; }
      else if (nr === S.heroR + 1 && rm.connectS) { innerX = rm.doorS; innerY = ROOM_H - 2; ddx = 0; ddy = 1; }
      else if (nc === S.heroC - 1 && rm.connectW) { innerX = 1; innerY = rm.doorW; ddx = -1; ddy = 0; }
      else if (nc === S.heroC + 1 && rm.connectE) { innerX = ROOM_W - 2; innerY = rm.doorE; ddx = 1; ddy = 0; }
      if (innerX >= 0) {
        if (S.heroX === innerX && S.heroY === innerY) { heroStep(ddx, ddy); return; }
        if (jonesMove(innerX, innerY)) return;
      }
    }
  }
  const step = jonesStepTowardUnknown();
  if (step) tryMove(step[0], step[1]);
}

function heroTickAI() {
  if (S.cfg.aiPersonality === AI_SUPERMAN) supermanTick();
  else jonesTick();
}

// =============================================================
// Rendering
// =============================================================
const COLORS = {
  hero: '#ffffff',
  grunt: '#00ff00',   // Pure lime green
  bat: '#00ffff',     // Pure cyan
  dragon: '#ff0000',  // Pure red
  chest: '#ffff00',   // Pure yellow
  potion: '#ff00ff',  //  Magenta
  empty: '#aaaaaa',   //  Gray
  treasure: '#ffcc00',//  Gold
  den: '#ff3333',     //  Red
  maze: '#6699ff',    //  Blue
};

function roomTint(type) {
  switch (type) {
    case ROOM_TREASURE: return COLORS.treasure;
    case ROOM_DEN: return COLORS.den;
    case ROOM_MAZE: return COLORS.maze;
    default: return COLORS.empty;
  }
}
function roomTypeTag(type) {
  switch (type) {
    case ROOM_TREASURE: return '[$]';
    case ROOM_DEN: return '[!]';
    case ROOM_MAZE: return '[~]';
    default: return '[ ]';
  }
}
function dim(hex, f = 0.4) {
  let r = 0, g = 0, b = 0;
  if (hex.length === 4) {
    r = parseInt(hex[1] + hex[1], 16);
    g = parseInt(hex[2] + hex[2], 16);
    b = parseInt(hex[3] + hex[3], 16);
  } else {
    r = parseInt(hex.slice(1, 3), 16);
    g = parseInt(hex.slice(3, 5), 16);
    b = parseInt(hex.slice(5, 7), 16);
  }
  return `rgb(${Math.round(r*f)},${Math.round(g*f)},${Math.round(b*f)})`;
}

let canvas, ctx, minimap, mmCtx;

function bindCanvases() {
  canvas = document.getElementById('playfield');
  minimap = document.getElementById('minimap');
  ctx = canvas.getContext('2d');
  mmCtx = minimap.getContext('2d');
}

function sizeCanvases() {
  const dpr = window.devicePixelRatio || 1;

  {
    const rect = canvas.getBoundingClientRect();
    const cssW = Math.max(1, Math.round(rect.width));
    const cssH = Math.max(1, Math.round(rect.height));
    canvas.width = cssW * dpr;
    canvas.height = cssH * dpr;
    const sx = canvas.width / (ROOM_W * CELL);
    const sy = canvas.height / (ROOM_H * CELL);
    ctx.setTransform(sx, 0, 0, sy, 0, 0);
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
  }

  {
    const rect = minimap.getBoundingClientRect();
    const cssW = Math.max(1, Math.round(rect.width));
    const cssH = Math.max(1, Math.round(rect.height));
    minimap.width = cssW * dpr;
    minimap.height = cssH * dpr;
    const sx = minimap.width / ((2 * GRID_W + 1) * CELL);
    const sy = minimap.height / ((2 * GRID_H + 1) * CELL);
    mmCtx.setTransform(sx, 0, 0, sy, 0, 0);
    mmCtx.textAlign = 'center';
    mmCtx.textBaseline = 'middle';
  }
}

let pendingResize = false;
function onResize() {
  if (pendingResize) return;
  pendingResize = true;
  requestAnimationFrame(() => {
    pendingResize = false;
    sizeCanvases();
    render();
  });
}

function drawChar(x, y, ch, color, bold) {
  ctx.font = (bold ? 'bold ' : '') + '14px "Courier New", monospace';
  ctx.fillStyle = color;
  ctx.fillText(ch, x * CELL + CELL / 2, y * CELL + CELL / 2 + 1);
}

function computeVisibility() {
  const torch = S.cfg.torchEnabled;
  const r2 = S.cfg.torchRadius * S.cfg.torchRadius;
  const vis = [];
  for (let y = 0; y < ROOM_H; y++) {
    vis[y] = new Uint8Array(ROOM_W);
    for (let x = 0; x < ROOM_W; x++) {
      if (!hasLOS(S.heroX, S.heroY, x, y, S.heroR, S.heroC)) continue;
      if (torch) {
        const dx = x - S.heroX, dy = y - S.heroY;
        if (dx * dx + dy * dy > r2) continue;
      }
      vis[y][x] = 1;
    }
  }
  return vis;
}

function render() {
  const rm = S.rooms[S.heroR][S.heroC];
  const tint = roomTint(rm.type);
  // Drop the multiplier dramatically so "dim" is extremely dark, just like A_DIM
  const dimTint = dim(tint, 0.40);

  const vis = computeVisibility();

  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, ROOM_W * CELL, ROOM_H * CELL);

  for (let y = 0; y < ROOM_H; y++) {
    for (let x = 0; x < ROOM_W; x++) {
      const visible = vis[y][x];
      const wasSeen = S.seen[S.heroR][S.heroC][y][x];

      if (isWall(x, y)) {
        const isDoor = (rm.connectN && x === rm.doorN && y === 0) ||
                       (rm.connectS && x === rm.doorS && y === ROOM_H - 1) ||
                       (rm.connectW && x === 0 && y === rm.doorW) ||
                       (rm.connectE && x === ROOM_W - 1 && y === rm.doorE);
        if (visible) {
          if (!isDoor) drawChar(x, y, '#', tint, true);
        } else if (wasSeen) {
          if (!isDoor) drawChar(x, y, '#', dimTint, false);
        } else {
          drawChar(x, y, '#', '#222222', false);
        }
      } else if (S.roomWalls[S.heroR][S.heroC][y][x]) {
        if (visible) drawChar(x, y, '#', tint, true);
        else if (wasSeen) drawChar(x, y, '#', dimTint, false);
      } else {
        if (visible) drawChar(x, y, '.', tint, false);
        else if (wasSeen) drawChar(x, y, '.', dimTint, false);
      }
    }
  }

  if (rm.hasChest) {
    const v = vis[rm.chestY][rm.chestX], ws = S.seen[S.heroR][S.heroC][rm.chestY][rm.chestX];
    if (v) drawChar(rm.chestX, rm.chestY, '$', COLORS.chest, true);
    else if (ws) drawChar(rm.chestX, rm.chestY, '$', dim(COLORS.chest, 0.15), false);
  }
  if (rm.hasPotion) {
    const v = vis[rm.potionY][rm.potionX], ws = S.seen[S.heroR][S.heroC][rm.potionY][rm.potionX];
    if (v) drawChar(rm.potionX, rm.potionY, '+', COLORS.potion, true);
    else if (ws) drawChar(rm.potionX, rm.potionY, '+', dim(COLORS.potion, 0.15), false);
  }
  for (const m of rm.monsters) {
    if (!m.alive) continue;
    if (!vis[m.y][m.x]) continue;
    const color = m.type === 'G' ? COLORS.grunt : m.type === 'B' ? COLORS.bat : COLORS.dragon;
    drawChar(m.x, m.y, m.type, color, true);
  }
  drawChar(S.heroX, S.heroY, '1', COLORS.hero, true);

  renderMinimap();
  renderHUD();
  updateButtons();
}

function renderMinimap() {
  const w = (2 * GRID_W + 1) * CELL, h = (2 * GRID_H + 1) * CELL;
  mmCtx.fillStyle = '#000';
  mmCtx.fillRect(0, 0, w, h);
  mmCtx.font = 'bold 14px "Courier New", monospace';
  mmCtx.fillStyle = '#666666';

  for (let row = 0; row <= GRID_H; row++) for (let col = 0; col <= GRID_W; col++) {
    mmCtx.fillText('+', col * 2 * CELL + CELL / 2, row * 2 * CELL + CELL / 2 + 1);
  }
  for (let row = 0; row <= GRID_H; row++) for (let col = 0; col < GRID_W; col++) {
    mmCtx.fillText('-', (col * 2 + 1) * CELL + CELL / 2, row * 2 * CELL + CELL / 2 + 1);
  }
  for (let row = 0; row < GRID_H; row++) for (let col = 0; col <= GRID_W; col++) {
    mmCtx.fillText('|', col * 2 * CELL + CELL / 2, (row * 2 + 1) * CELL + CELL / 2 + 1);
  }
  for (let row = 0; row < GRID_H; row++) for (let col = 0; col < GRID_W; col++) {
    const px = (col * 2 + 1) * CELL + CELL / 2;
    const py = (row * 2 + 1) * CELL + CELL / 2 + 1;
    const rm = S.rooms[row][col];
    const tp = roomTint(rm.type);

    if (row === S.heroR && col === S.heroC) {
      mmCtx.fillStyle = tp;
      mmCtx.fillRect((col * 2 + 1) * CELL, (row * 2 + 1) * CELL, CELL, CELL);
      mmCtx.fillStyle = '#000000';
      mmCtx.fillText('@', px, py);
    } else if (!rm.visited) {
      // blank
    } else if (rm.hasChest && S.seen[row][col][rm.chestY][rm.chestX]) {
      mmCtx.fillStyle = COLORS.chest;
      mmCtx.fillText('$', px, py);
    } else if (rm.monsters.some(m => m.alive)) {
      mmCtx.fillStyle = '#ff5555';
      mmCtx.fillText('!', px, py);
    } else {
      mmCtx.fillStyle = dim(tp, 0.25); // Very dark visited rooms
      mmCtx.fillText('.', px, py);
    }
  }
}

function renderHUD() {
  const rm = S.rooms[S.heroR][S.heroC];
  const tag = roomTypeTag(rm.type);
  const tint = roomTint(rm.type);

  let line1 = `<span style="color:${tint}">${tag}</span> Lv ${S.level} | Score ${S.score}`;
  if (S.bestScore > S.score) line1 += ` | Best ${S.bestScore}`;
  line1 += ` | Chests ${S.chestsCollected}/${S.totalChests} | Kills ${S.kills} | HP: `;
  for (let i = 0; i < S.heroHP; i++)
    line1 += `<span class="${S.damageFlash > 0 ? 'hp flash' : 'hp'}">&lt;3</span>`;

  document.getElementById('line1').innerHTML = line1;

  const aiName = S.paused ? '[PAUSED]' :
                 S.aiMode ? (S.cfg.aiPersonality === AI_SUPERMAN ? '[S]' : '[J]') : '[M]';
  document.getElementById('line2').textContent =
    `${aiName} torch:${S.cfg.torchEnabled ? 'on' : 'off'} seed:${S.seedUsed}`;
}

function updateButtons() {
  const aiBtn = document.getElementById('btn-ai-toggle');
  aiBtn.textContent = S.aiMode ? 'AI: ON' : 'AI: OFF';
  aiBtn.classList.toggle('active', S.aiMode);

  const cycleBtn = document.getElementById('btn-ai-cycle');
  cycleBtn.textContent = S.cfg.aiPersonality === AI_SUPERMAN ? '→ Jones' : '→ Superman';

  const pauseBtn = document.getElementById('btn-pause');
  pauseBtn.textContent = S.paused ? 'Resume' : 'Pause';
  pauseBtn.classList.toggle('active', S.paused);

  const torchBtn = document.getElementById('btn-torch');
  torchBtn.textContent = S.cfg.torchEnabled ? 'Torch: On' : 'Torch: Off';
  torchBtn.classList.toggle('active', S.cfg.torchEnabled);

  document.getElementById('speed-label').textContent =
    Math.round(S.cfg.aiTickUs / 1000) + 'ms';
}

function drawBanner(text) {
  const w = ROOM_W * CELL, h = ROOM_H * CELL;
  ctx.fillStyle = 'rgba(0,0,0,0.85)';
  ctx.fillRect(w * 0.15, h * 0.4, w * 0.7, h * 0.2);
  ctx.strokeStyle = '#888888';
  ctx.lineWidth = 2;
  ctx.strokeRect(w * 0.15, h * 0.4, w * 0.7, h * 0.2);
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 22px "Courier New", monospace';
  ctx.fillText(text, w / 2, h / 2);
}

// =============================================================
// Game loop
// =============================================================
let loopHandle = null;

function startLoop() {
  stopLoop();
  if (S.paused || S.ended || !S.aiMode) return;
  const interval = Math.max(20, Math.floor(S.cfg.aiTickUs / 1000));
  loopHandle = setInterval(aiTick, interval);
}
function stopLoop() {
  if (loopHandle) { clearInterval(loopHandle); loopHandle = null; }
}

function afterTick() {
  collectChest();
  collectPotion();
  checkCollisions();
  moveMonsters();
  checkCollisions();
  updateSeen();

  if (S.score > S.bestScore) {
    S.bestScore = S.score;
    try { localStorage.setItem('dungeon-best', String(S.bestScore)); } catch(e) {}
  }
  if (S.ended) return;

  if (S.chestsCollected >= S.totalChests || S.heroHP <= 0) {
    handleLevelEnd();
    return;
  }
  render();
  if (S.damageFlash > 0) S.damageFlash--;
}

function aiTick() {
  if (S.paused || S.ended || !S.aiMode) return;
  S.tick++;
  heroTickAI();
  afterTick();
}

function handleLevelEnd() {
  const completed = S.chestsCollected >= S.totalChests;
  const dead = S.heroHP <= 0;
  if (completed) S.score += 500;
  if (S.score > S.bestScore) {
    S.bestScore = S.score;
    try { localStorage.setItem('dungeon-best', String(S.bestScore)); } catch(e) {}
  }
  S.ended = true;
  render();
  drawBanner(dead ? 'YOU DIED' : 'LEVEL COMPLETE');

  setTimeout(() => {
    S.level++;
    S.tick = 0;
    S.prevRoomR = -1; S.prevRoomC = -1;
    S.targetRoomR = -1; S.targetRoomC = -1;
    generateDungeon();
    spawnHero();
    S.ended = false;
    render();
    startLoop();
  }, 1200);
}

// =============================================================
// Shared actions
// =============================================================
function updateURLParam(key, value) {
  try {
    const params = new URLSearchParams(location.search);
    if (value === null || value === '') params.delete(key);
    else params.set(key, value);
    history.replaceState(null, '', '?' + params.toString());
  } catch (e) { /* file:// or sandboxed iframe */ }
}

function actionToggleAI() {
  S.aiMode = !S.aiMode;
  if (S.aiMode) startLoop(); else stopLoop();
  render();
}
function actionCycleAI() {
  S.cfg.aiPersonality = S.cfg.aiPersonality === AI_JONES ? AI_SUPERMAN : AI_JONES;
  S.aiMode = true;
  updateURLParam('ai', S.cfg.aiPersonality === AI_SUPERMAN ? 'superman' : 'jones');
  startLoop();
  render();
}
function actionTogglePause() {
  S.paused = !S.paused;
  if (S.paused) stopLoop(); else startLoop();
  render();
}
function actionToggleTorch() {
  S.cfg.torchEnabled = !S.cfg.torchEnabled;
  render();
}
function actionSpeedUp() {
  S.cfg.aiTickUs = Math.max(20000, S.cfg.aiTickUs - 20000);
  if (S.aiMode && !S.paused) startLoop();
  updateButtons();
}
function actionSpeedDown() {
  S.cfg.aiTickUs = Math.min(400000, S.cfg.aiTickUs + 20000);
  if (S.aiMode && !S.paused) startLoop();
  updateButtons();
}
function actionRestart() {
  window.location.reload();
}
function actionNewRun() {
  const seed = Math.floor(Date.now() / 1000) % 1000000000;
  updateURLParam('seed', String(seed));
  window.location.reload();
}
function actionApplySeed() {
  const raw = document.getElementById('seed-input').value.trim();
  const n = parseInt(raw, 10);
  if (isNaN(n) || n <= 0) {
    updateURLParam('seed', null);
  } else {
    updateURLParam('seed', String(n));
  }
  window.location.reload();
}
function actionToggleFullscreen() {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen().catch(() => {});
  } else {
    document.exitFullscreen();
  }
}

// =============================================================
// Fullscreen scale
// =============================================================
function updateFullscreenScale() {
  const html = document.documentElement;
  const container = document.getElementById('game-container');
  if (!document.fullscreenElement) {
    html.style.setProperty('--fs-scale', '1');
    return;
  }
  html.style.setProperty('--fs-scale', '1');
  const rect = container.getBoundingClientRect();
  const nativeW = rect.width || 800;
  const nativeH = rect.height || 600;
  const vw = window.innerWidth;
  const vh = window.innerHeight;
  const scale = Math.min(vw / nativeW, vh / nativeH, 3);
  html.style.setProperty('--fs-scale', scale.toFixed(3));
}

// =============================================================
// Input
// =============================================================
function onKeyDown(e) {
  const tag = e.target.tagName;
  if (tag === 'INPUT' || tag === 'TEXTAREA') {
    if (e.key === 'Escape') e.target.blur();
    if (e.key === 'Enter' && e.target.id === 'seed-input') {
      e.preventDefault();
      actionApplySeed();
    }
    return;
  }

  const k = e.key;
  if (k === 'q' || k === 'Q') { actionRestart(); return; }
  if (S.ended) return;
  if (k === 'i' || k === 'I') { actionToggleAI(); return; }
  if (k === 'c' || k === 'C') { actionCycleAI(); return; }
  if (k === 'p' || k === 'P') { actionTogglePause(); return; }
  if (k === 't' || k === 'T') { actionToggleTorch(); return; }
  if (k === '+' || k === '=') { actionSpeedUp(); return; }
  if (k === '-' || k === '_') { actionSpeedDown(); return; }

  if (S.paused || S.aiMode) return;

  let dx = 0, dy = 0;
  if (k === 'ArrowLeft' || k === 'a' || k === 'A') dx = -1;
  else if (k === 'ArrowRight' || k === 'd' || k === 'D') dx = 1;
  else if (k === 'ArrowUp' || k === 'w' || k === 'W') dy = -1;
  else if (k === 'ArrowDown' || k === 's' || k === 'S') dy = 1;

  if (dx || dy) {
    e.preventDefault();
    S.tick++;
    heroStep(dx, dy);
    afterTick();
  }
}

// =============================================================
// Auto-pause on tab blur
// =============================================================
let autoPausedByBlur = false;
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    if (!S.paused && S.aiMode && !S.ended) {
      autoPausedByBlur = true;
      S.paused = true;
      stopLoop();
      render();
    }
  } else if (autoPausedByBlur) {
    autoPausedByBlur = false;
    S.paused = false;
    startLoop();
    render();
  }
});

// =============================================================
// Init
// =============================================================
function init() {
  const params = new URLSearchParams(location.search);

  let seed = parseInt(params.get('seed'), 10);
  if (!seed || seed <= 0) {
    seed = Math.floor(Date.now() / 1000) % 1000000000;
  }
  S.rngState = (seed | 0) || 1;
  S.seedUsed = seed;

  const aiParam = params.get('ai');
  if (aiParam === 'superman') S.cfg.aiPersonality = AI_SUPERMAN;
  else if (aiParam === 'jones') S.cfg.aiPersonality = AI_JONES;

  updateURLParam('seed', String(seed));
  updateURLParam('ai', S.cfg.aiPersonality === AI_SUPERMAN ? 'superman' : 'jones');

  try {
    const b = localStorage.getItem('dungeon-best');
    if (b) S.bestScore = parseInt(b, 10) || 0;
  } catch (e) {}

  bindCanvases();

  document.getElementById('seed-input').value = String(seed);

  document.getElementById('btn-ai-toggle').addEventListener('click', actionToggleAI);
  document.getElementById('btn-ai-cycle').addEventListener('click', actionCycleAI);
  document.getElementById('btn-pause').addEventListener('click', actionTogglePause);
  document.getElementById('btn-torch').addEventListener('click', actionToggleTorch);
  document.getElementById('btn-speed-down').addEventListener('click', actionSpeedDown);
  document.getElementById('btn-speed-up').addEventListener('click', actionSpeedUp);
  document.getElementById('btn-seed-apply').addEventListener('click', actionApplySeed);
  document.getElementById('btn-new-run').addEventListener('click', actionNewRun);
  document.getElementById('btn-fullscreen').addEventListener('click', actionToggleFullscreen);
  document.getElementById('btn-restart').addEventListener('click', actionRestart);

  document.getElementById('seed-input').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      actionApplySeed();
    }
  });

  window.addEventListener('keydown', onKeyDown);

  document.addEventListener('fullscreenchange', () => {
    const btn = document.getElementById('btn-fullscreen');
    if (document.fullscreenElement) {
      btn.textContent = 'Exit Fullscreen';
      btn.classList.add('active');
    } else {
      btn.textContent = 'Fullscreen';
      btn.classList.remove('active');
    }
    requestAnimationFrame(() => {
      sizeCanvases();
      updateFullscreenScale();
      render();
    });
  });

  window.addEventListener('resize', () => {
    onResize();
    updateFullscreenScale();
  });
  window.addEventListener('orientationchange', onResize);

  // Initial sizing after layout settles
  requestAnimationFrame(() => {
    sizeCanvases();
    generateDungeon();
    spawnHero();
    render();
    startLoop();
  });
}

init();
