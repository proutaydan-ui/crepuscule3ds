// Crépuscule - portage Nintendo 3DS (libctru + citro2d)
// Écran du haut : jeu (800x448 logiques -> 400x224, échelle 0.5)
// Écran du bas  : section triche (tactile / stylet)
#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------- constantes
static const int   T = 32, VW = 800, VH = 448, ROWS = 14, MAXW = 128;
static const float S = 0.5f;   // échelle monde -> écran
static const float OY = 8.0f;  // (240 - 224) / 2

// ---------------------------------------------------------------- niveaux
struct Level {
    int w; bool boss;
    std::vector<std::array<int,2>> gaps;
    std::vector<std::array<int,3>> plats;
    std::vector<int> spikes, enemies;
    int flag;
};

static const Level LEVELS[] = {
 {60,false,{{12,14},{26,28},{40,42}},{{16,9,4},{22,8,3},{32,9,3},{36,7,4},{46,9,3},{50,7,3}},{20,34},{24,38,52},57},
 {75,false,{{10,12},{22,24},{34,36},{46,48},{58,60}},{{14,9,3},{18,7,3},{26,9,3},{29,7,3},{38,9,2},{41,7,3},{50,9,3},{54,7,3},{62,9,3}},{16,30,44,52},{20,32,40,56,66},71},
 {90,false,{{10,12},{20,22},{30,32},{40,42},{52,54},{64,66},{76,78}},{{14,9,3},{16,6,3},{24,9,2},{26,7,3},{34,9,3},{36,6,3},{44,9,3},{46,7,2},{56,9,3},{58,6,3},{68,9,3},{70,7,3},{80,9,3}},{15,28,38,48,60,72,82},{17,27,35,46,57,69,84},87},
 {80,false,{{10,12},{24,26},{38,40},{52,54},{66,68}},{{14,9,3},{18,8,3},{28,9,3},{32,7,3},{42,9,3},{46,7,3},{56,9,3},{60,7,3},{70,9,3}},{16,21,30,35,44,58,62},{20,31,43,57,63,74},77},
 {85,false,{{9,11},{19,21},{29,31},{39,41},{49,51},{59,61},{69,71}},{{13,9,2},{16,7,3},{23,9,3},{33,9,2},{35,7,3},{43,9,3},{46,6,3},{53,9,2},{55,7,3},{63,9,3},{66,7,2},{73,9,3}},{14,17,25,27,36,45,47,57,65,75},{15,24,26,34,44,46,56,64,76,80},82},
 {95,false,{{10,12},{19,21},{28,30},{37,39},{46,48},{55,57},{64,66},{73,75},{82,84}},{{14,9,3},{23,9,3},{32,9,3},{41,9,3},{50,9,3},{59,9,3},{68,9,3},{77,9,3}},{15,24,33,42,51,60,69,78},{16,25,34,43,52,61,70,79,88},90},
 {100,false,{{10,12},{17,19},{24,26},{31,33},{38,40},{45,47},{52,54},{59,61},{66,68},{73,75},{80,82}},{{14,9,2},{28,9,2},{42,9,2},{56,9,2},{70,9,2}},{21,35,49,63,77,88,90},{14,28,42,56,70,86,92},96},
 {105,false,{{10,12},{15,17},{20,22},{25,27},{36,38},{41,43},{46,48},{58,60},{63,65},{68,70},{73,75},{86,88}},{{31,9,3},{52,9,3},{79,9,3},{93,9,3}},{30,33,52,55,79,82,92,96},{29,34,50,56,78,84,93,99},101},
 {115,false,{{10,12},{16,18},{22,24},{28,30},{34,36},{40,42},{46,48},{52,54},{58,60},{64,66},{70,72},{76,78},{82,84},{88,90}},{{17,9,2},{29,9,2},{41,9,2},{53,9,2},{65,9,2},{77,9,2},{89,9,2}},{95,98,101,104},{20,32,44,56,68,80,86,93,97,100,103,107},111},
 {115,true,{{10,12},{15,17},{20,22},{25,27},{30,32},{40,42},{45,47},{50,52},{55,57},{65,67},{70,72},{75,77},{80,82}},{{34,9,3},{59,9,3},{96,9,3},{104,9,3}},{35,37,60,62,86,89},{34,38,59,63,85,88},-1}
};
static const int NLEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);

// ---------------------------------------------------------------- triches
enum { C_GOD, C_LIVES, C_JUMP, C_SUPER, C_SPEED, C_MOON, C_FLY, C_MAGNET, NCHEAT };
struct CheatDef { const char* code; const char* name; };
static const CheatDef CH[NCHEAT] = {
 {"god",   "Invincibilité"},
 {"lives", "Vies infinies"},
 {"jump",  "Sauts infinis"},
 {"super", "Super saut"},
 {"speed", "Vitesse x1,8"},
 {"moon",  "Gravité lunaire"},
 {"fly",   "Vol"},
 {"magnet","Aimant à pièces"}
};
static bool cheat[NCHEAT];

// ---------------------------------------------------------------- état du jeu
struct Coin   { float x, y; bool got; };
struct Spike  { float x, y, w, h; };
struct Enemy  { float x, y, w, h; int dir; int dead; };
struct Player { float x, y, w, h, vx, vy; bool on; int coy, face, inv; };
struct Boss   { bool active; float x, y, w, h, vy; int dir, hp, inv; std::string say; int sayT, dead, jt, on; };

static uint8_t grid[ROWS][MAXW];
static int gridW = 0;
static std::vector<Coin> coins;
static std::vector<Spike> spikes;
static std::vector<Enemy> enemies;
static Boss boss;
static Player P;
static int L = 0, lives = 3, score = 0, tick = 0, toastT = 0;
static float flagX = -9999, camX = 0;
static bool winState = false;
static std::string toastS, msgS;

static bool inLeft, inRight, inUp, inDown, jumpQ, jumpHeld;

static inline int fl(float v) { return (int)floorf(v); }
static void say(const std::string& t) { toastS = t; toastT = 150; msgS = t; }

static bool solid(int c, int r) {
    if (c < 0 || c >= gridW) return true;
    if (r < 0 || r >= ROWS) return false;
    return grid[r][c] == 1;
}
template<class A, class B>
static bool hit(const A& a, const B& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static void respawn() {
    P = {(float)T, (float)(10 * T), 22, 28, 0, 0, false, 0, 1, 60};
    camX = 0;
}

static void loadLevel(int i) {
    L = i; const Level& d = LEVELS[i];
    gridW = d.w;
    memset(grid, 0, sizeof(grid));
    auto gap = [&](int x) { for (auto& g : d.gaps) if (x >= g[0] && x <= g[1]) return true; return false; };
    for (int x = 0; x < d.w; x++) if (!gap(x)) { grid[12][x] = 1; grid[13][x] = 1; }
    coins.clear(); spikes.clear(); enemies.clear();
    for (int x : d.spikes) spikes.push_back({(float)(x * T), (float)(11 * T + 14), (float)T, 18.f});
    for (auto& p : d.plats)
        for (int k = 0; k < p[2]; k++) {
            grid[p[1]][p[0] + k] = 1;
            coins.push_back({(float)((p[0] + k) * T + 16), (float)((p[1] - 1) * T + 16), false});
        }
    for (int x = 3; x < d.w - 4; x += 5)
        if (!gap(x) && !gap(x + 1) && std::find(d.spikes.begin(), d.spikes.end(), x) == d.spikes.end())
            coins.push_back({(float)(x * T + 16), (float)(11 * T + 14), false});
    for (int x : d.enemies) enemies.push_back({(float)(x * T + 4), (float)(11 * T + 6), 24, 26, -1, 0});
    flagX = d.flag >= 0 ? (float)(d.flag * T) : -9999.f;
    boss = {d.boss, (float)(100 * T), (float)(12 * T - 56), 56, 56, 0, -1, 3, 0, "", 0, 0, 90, 0};
    respawn();
}

static void die() {
    if (cheat[C_GOD]) { respawn(); return; }
    if (!cheat[C_LIVES]) lives--;
    if (lives <= 0) { say("Plus de vies : retour au niveau 1"); lives = 3; score = 0; loadLevel(0); return; }
    say("Aïe ! Tu repars du début du niveau."); respawn();
}

static void nextLevel() {
    if (L >= NLEVELS - 1) {
        if (boss.active && !boss.dead) { P.x = 92 * T; P.y = 10 * T; P.vy = 0; say("Le boss t'attend !"); return; }
        winState = true; return;
    }
    say("Niveau " + std::to_string(L + 2)); loadLevel(L + 1);
}

// ---------------------------------------------------------------- mise à jour
static void update() {
    tick++;
    if (winState) { if (jumpQ) { jumpQ = false; winState = false; lives = 3; score = 0; loadLevel(0); } return; }

    float sp = 3.f * (cheat[C_SPEED] ? 1.8f : 1.f);
    P.vx = ((inRight ? 1 : 0) - (inLeft ? 1 : 0)) * sp;
    if (P.vx != 0) P.face = P.vx > 0 ? 1 : -1;
    if (P.inv > 0) P.inv--;

    if (cheat[C_FLY]) {
        P.vy = ((inDown ? 1 : 0) - (inUp ? 1 : 0)) * sp;
        P.x += P.vx; P.y += P.vy; P.on = false; jumpQ = false;
        P.x = std::max(0.f, P.x);
        if (P.y < -40) P.y = -40;
        if (P.y > ROWS * T) P.y = 10 * T;
    } else {
        float g = .55f * (cheat[C_MOON] ? .4f : 1.f);
        P.coy = P.on ? 6 : P.coy - 1;
        if (jumpQ && (P.coy > 0 || cheat[C_JUMP])) { P.vy = -11.f * (cheat[C_SUPER] ? 1.5f : 1.f); P.on = false; P.coy = 0; }
        jumpQ = false;
        if (!jumpHeld && P.vy < -4 && !cheat[C_SUPER]) P.vy *= .6f;
        P.vy = std::min(P.vy + g, 12.f);

        P.x += P.vx;
        int c0 = fl(P.x / T), c1 = fl((P.x + P.w - 1) / T), r0 = fl(P.y / T), r1 = fl((P.y + P.h - 1) / T);
        for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) if (solid(c, r)) {
            if (P.vx > 0) P.x = c * T - P.w; else if (P.vx < 0) P.x = (c + 1) * T;
            c0 = fl(P.x / T); c1 = fl((P.x + P.w - 1) / T);
        }
        P.y += P.vy; P.on = false;
        c0 = fl(P.x / T); c1 = fl((P.x + P.w - 1) / T); r0 = fl(P.y / T); r1 = fl((P.y + P.h - 1) / T);
        for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) if (solid(c, r)) {
            if (P.vy > 0) { P.y = r * T - P.h; P.on = true; } else if (P.vy < 0) P.y = (r + 1) * T;
            P.vy = 0; r0 = fl(P.y / T); r1 = fl((P.y + P.h - 1) / T);
        }
        if (P.y > ROWS * T + 60) { die(); return; }
    }

    // pièces
    for (auto& k : coins) {
        if (k.got) continue;
        if (cheat[C_MAGNET]) {
            float dx = P.x + 11 - k.x, dy = P.y + 14 - k.y, d = hypotf(dx, dy);
            if (d < 110 && d > 0.001f) { k.x += dx / d * 5; k.y += dy / d * 5; }
        }
        if (hypotf(P.x + 11 - k.x, P.y + 14 - k.y) < 20) { k.got = true; score += 10; }
    }

    // ennemis
    for (auto& e : enemies) {
        if (e.dead) continue;
        float ahead = e.dir > 0 ? e.x + e.w + 2 : e.x - 2;
        int col = fl(ahead / T);
        if (solid(col, fl((e.y + e.h / 2) / T)) || !solid(col, fl((e.y + e.h + 2) / T))) e.dir *= -1;
        e.x += e.dir * 1.1f;
        if (hit(P, e)) {
            if (P.vy > 0 && P.y + P.h - P.vy < e.y + 10) { e.dead = 1; P.vy = -8; score += 100; }
            else if (P.inv <= 0 && !cheat[C_FLY]) die();
        }
    }
    if (P.inv <= 0 && !cheat[C_FLY] && !cheat[C_GOD])
        for (auto& s : spikes) if (hit(P, s)) { die(); return; }

    // boss
    if (boss.active) {
        Boss& b = boss;
        if (b.dead) { b.dead++; if (b.dead > 240) winState = true; }
        else {
            if (P.x > 90 * T && !b.on) { b.on = 1; b.say = "Tu comptes tricher ?"; b.sayT = 150; }
            if (b.on) {
                float bs = 1.6f + (3 - b.hp) * .9f;
                b.x += b.dir * bs;
                if (b.x < 93 * T) { b.x = 93 * T; b.dir = 1; }
                if (b.x + b.w > 109 * T) { b.x = 109 * T - b.w; b.dir = -1; }
                b.jt--;
                if (b.jt <= 0 && b.vy == 0) { b.vy = -10; b.jt = 60 + rand() % 50; }
                b.vy += .5f; b.y += b.vy;
                if (b.y >= 12 * T - b.h) { b.y = 12 * T - b.h; b.vy = 0; }
                if (b.inv > 0) b.inv--;
                if (b.sayT > 0) b.sayT--;
                if (b.inv <= 0 && hit(P, b)) {
                    if (P.vy > 0 && P.y + P.h - P.vy < b.y + 14) {
                        b.hp--; b.inv = 70; P.vy = -11; score += 500;
                        if (b.hp <= 0) { b.dead = 1; b.say = "Tout tricher, c'est ça ? :("; b.sayT = 99999; }
                        else { b.say = b.hp == 2 ? "Aïe ! Tu triches, hein ?" : "Arrête de tricher !"; b.sayT = 120; }
                    } else if (P.inv <= 0 && !cheat[C_FLY]) { die(); return; }
                }
            }
        }
    }
    if (P.x + P.w > flagX && P.x < flagX + T) nextLevel();
    camX = std::max(0.f, std::min((float)(gridW * T - VW), P.x - VW / 2 + 11));
}

// ---------------------------------------------------------------- rendu : helpers
static C3D_RenderTarget *topT, *botT;
static C2D_TextBuf tbuf;
static float gz = 0, camR = 0;
static inline float Z() { gz += 0.0001f; return gz; }
static inline u32 C(u32 hex, u8 a = 255) { return C2D_Color32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, a); }

// monde (coordonnées logiques 800x448, caméra appliquée)
static void R(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid((x - camR) * S, y * S + OY, Z(), w * S, h * S, c); }
static void TRI(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) {
    C2D_DrawTriangle((x0 - camR) * S, y0 * S + OY, c, (x1 - camR) * S, y1 * S + OY, c, (x2 - camR) * S, y2 * S + OY, c, Z());
}
static void ELL(float cx, float cy, float rx, float ry, u32 c) {
    C2D_DrawEllipseSolid((cx - rx - camR) * S, (cy - ry) * S + OY, Z(), rx * 2 * S, ry * 2 * S, c);
}
// texte en coordonnées écran
static float textWidth(const std::string& s, float sc) {
    C2D_Text t; C2D_TextParse(&t, tbuf, s.c_str()); C2D_TextOptimize(&t);
    float w, h; C2D_TextGetDimensions(&t, sc, sc, &w, &h); return w;
}
static void TX(const std::string& s, float x, float y, float sc, u32 c, int al = 0) {
    C2D_Text t; C2D_TextParse(&t, tbuf, s.c_str()); C2D_TextOptimize(&t);
    float w, h; C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    if (al == 1) x -= w / 2; else if (al == 2) x -= w;
    C2D_DrawText(&t, C2D_WithColor, x, y, Z(), sc, sc, c);
}

// ---------------------------------------------------------------- rendu : écran du haut
static void drawTop() {
    gz = 0;
    C2D_TargetClear(topT, C(0x000000));
    C2D_SceneBegin(topT);
    camR = 0;

    // ciel (dégradé en deux bandes)
    float h1 = VH * .6f * S, h2 = VH * S - h1;
    C2D_DrawRectangle(0, OY, Z(), 400, h1, C(0x2a2466), C(0x2a2466), C(0xc4507e), C(0xc4507e));
    C2D_DrawRectangle(0, OY + h1, Z(), 400, h2, C(0xc4507e), C(0xc4507e), C(0xffb36b), C(0xffb36b));
    for (int i = 0; i < 30; i++) R((float)((i * 137) % VW), (float)((i * 53) % 200), 2, 2, C(0xffffff, 136));
    C2D_DrawCircleSolid(640 * S, 290 * S + OY, Z(), 46 * S, C(0xffe9b0));

    // montagnes (parallaxe)
    for (int l = 0; l < 2; l++) {
        u32 col = l ? C(0x3b2a6b) : C(0x54356f);
        float o = fmodf(camX * (.2f + l * .2f), 120.f);
        float px = 0, py = 0;
        for (int x = 0; x <= VW + 60; x += 60) {
            float nx = x - o, ny = (float)(300 + l * 40 - ((x * 7 + l * 50) % 5) * 22);
            if (x > 0) {
                TRI(px, py, nx, ny, nx, VH, col);
                TRI(px, py, nx, VH, px, VH, col);
            }
            px = nx; py = ny;
        }
    }

    // monde
    camR = roundf(camX);
    int c0 = fl(camX / T), c1 = std::min(gridW - 1, c0 + (VW + T - 1) / T + 1);
    for (int r = 0; r < ROWS; r++) for (int c = std::max(0, c0); c <= c1; c++) if (grid[r][c] == 1) {
        R(c * T, r * T, T, T, C(0x4a2f5a));
        R(c * T, r * T + T - 3, T, 3, C(0x2d1b3d));
        if (!solid(c, r - 1)) { R(c * T, r * T, T, 7, C(0x3ddbb0)); R(c * T, r * T + 7, T, 3, C(0x2aa383)); }
    }
    for (auto& s : spikes) {
        if (s.x + s.w < camR || s.x > camR + VW) continue;
        for (int i = 0; i < 3; i++)
            TRI(s.x + i * 11, s.y + s.h, s.x + i * 11 + 5.5f, s.y, s.x + i * 11 + 11, s.y + s.h, C(0xe8e3f5));
    }
    for (auto& k : coins) {
        if (k.got) continue;
        float w = fabsf(sinf((tick + k.x) / 14.f)) * 6 + 2;
        ELL(k.x, k.y, w + 1, 9, C(0xb8860b)); ELL(k.x, k.y, w, 8, C(0xffc857));
    }
    for (auto& e : enemies) {
        if (e.dead) continue;
        ELL(e.x + 12, e.y + 12, 12, 12, C(0xff7a90)); R(e.x, e.y + 12, e.w, e.h - 12, C(0xff7a90));
        R(e.x + 5, e.y + 7, 5, 6, C(0xffffff)); R(e.x + 14, e.y + 7, 5, 6, C(0xffffff));
        R(e.x + (e.dir > 0 ? 7 : 5), e.y + 9, 3, 3, C(0x1b1838)); R(e.x + (e.dir > 0 ? 16 : 14), e.y + 9, 3, 3, C(0x1b1838));
    }
    if (flagX >= 0) {
        R(flagX + 14, 8 * T, 4, 4 * T, C(0xf6efe6));
        TRI(flagX + 18, 8 * T, flagX + 18 + 26 + sinf(tick / 10.f) * 3, 8 * T + 13, flagX + 18, 8 * T + 26, C(0xffc857));
    }

    // boss
    if (boss.active) {
        Boss& b = boss; bool dd = b.dead > 0;
        if (!(b.inv > 0 && tick % 6 < 3)) {
            u32 body = dd ? C(0x6a5f9a) : C(0x7a2fbf);
            ELL(b.x + 28, b.y + 22, 28, 22, body); R(b.x, b.y + 22, b.w, b.h - 22, body);
            R(b.x + 6, b.y, 44, 6, C(0xffc857));
            TRI(b.x + 6, b.y + 4, b.x + 10, b.y - 16, b.x + 20, b.y + 2, C(0xffc857));
            TRI(b.x + 20, b.y + 2, b.x + 28, b.y - 20, b.x + 36, b.y + 2, C(0xffc857));
            TRI(b.x + 36, b.y + 2, b.x + 46, b.y - 16, b.x + 50, b.y + 4, C(0xffc857));
            R(b.x + 12, b.y + 18, 12, 12, C(0xffffff)); R(b.x + 32, b.y + 18, 12, 12, C(0xffffff));
            R(b.x + (b.dir > 0 ? 18 : 14), b.y + 22, 6, 6, C(0x1b1838)); R(b.x + (b.dir > 0 ? 38 : 34), b.y + 22, 6, 6, C(0x1b1838));
            if (dd) {
                R(b.x + 22, b.y + 41, 12, 3, C(0x1b1838)); R(b.x + 18, b.y + 44, 4, 3, C(0x1b1838)); R(b.x + 34, b.y + 44, 4, 3, C(0x1b1838));
                R(b.x + 16, b.y + 47, 3, 3, C(0x1b1838)); R(b.x + 37, b.y + 47, 3, 3, C(0x1b1838));
            } else R(b.x + 16, b.y + 41, 24, 3, C(0x1b1838));
        }
        if (b.sayT > 0 && !b.say.empty()) {
            float sc = 0.4f;
            float wl = textWidth(b.say, sc) / S + 24;
            float bx = std::min(b.x + b.w / 2 - wl / 2, camR + VW - wl - 8);
            bx = std::max(bx, camR + 8);
            float by = b.y - 62;
            R(bx, by, wl, 30, C(0xf6efe6));
            TX(b.say, (bx - camR) * S + 6, by * S + OY + 3, sc, C(0x1b1838));
        }
    }

    // joueur
    if (!(P.inv > 0 && tick % 6 < 3)) {
        u8 a = cheat[C_FLY] ? 178 : 255;
        R(P.x, P.y + 6, P.w, P.h - 6, cheat[C_GOD] ? C(0xffe9b0, a) : C(0xf6efe6, a));
        R(P.x - 1, P.y, P.w + 2, 10, C(0xffc857, a));
        R(P.x + (P.face > 0 ? 12 : -5), P.y + 7, 12, 3, C(0xffc857, a));
        R(P.x + (P.face > 0 ? 13 : 4), P.y + 13, 4, 5, C(0x1b1838, a));
    }

    // HUD (coordonnées écran)
    camR = 0;
    C2D_DrawRectSolid(0, 0, Z(), 400, 15, C(0x1b1838, 204));
    TX("Niveau " + std::to_string(L + 1) + "/" + std::to_string(NLEVELS), 4, 0, 0.4f, C(0xf6efe6));
    TX(std::string("Vies : ") + (cheat[C_LIVES] ? "inf" : std::to_string(lives)), 90, 0, 0.4f, C(0xf6efe6));
    TX("Score : " + std::to_string(score), 150, 0, 0.4f, C(0xf6efe6));
    if (boss.active && boss.on && !boss.dead)
        TX("Boss : " + std::to_string(boss.hp) + "/3", 240, 0, 0.4f, C(0xff7a90));
    std::string act;
    for (int i = 0; i < NCHEAT; i++) if (cheat[i]) {
        std::string n = CH[i].name; n = n.substr(0, n.find(' '));
        act += (act.empty() ? "" : ", ") + n;
    }
    if (!act.empty()) {
        C2D_DrawRectSolid(0, 15, Z(), 400, 13, C(0x1b1838, 160));
        TX("Triche : " + act, 4, 14, 0.35f, C(0x3ddbb0));
    }
    if (toastT > 0) {
        toastT--;
        float w = textWidth(toastS, 0.4f) + 14;
        C2D_DrawRectSolid(200 - w / 2, 240 - 30, Z(), w, 18, C(0x1b1838, 221));
        TX(toastS, 200, 240 - 29, 0.4f, C(0xf6efe6), 1);
    }
    if (winState) {
        C2D_DrawRectSolid(0, 0, Z(), 400, 240, C(0x1b1838, 238));
        TX("Tu as vu la nuit tomber. Bravo !", 200, 85, 0.8f, C(0xffc857), 1);
        TX("Score final : " + std::to_string(score), 200, 120, 0.6f, C(0xf6efe6), 1);
        TX("Appuie sur A pour rejouer", 200, 142, 0.5f, C(0xf6efe6), 1);
    }
}

// ---------------------------------------------------------------- écran du bas (tactile)
struct Btn { int x, y, w, h; };
static Btn tBtn[NCHEAT], aBtn[4], codeBtn;
static const char* ACT_LABEL[4] = {"+5 vies", "Niveau suivant", "Aller au drapeau", "Toutes les pièces"};

static void layoutButtons() {
    for (int i = 0; i < NCHEAT; i++) tBtn[i] = {4 + (i % 2) * 160, 22 + (i / 2) * 29, 152, 26};
    for (int j = 0; j < 4; j++)      aBtn[j] = {4 + (j % 2) * 160, 140 + (j / 2) * 29, 152, 26};
    codeBtn = {4, 198, 312, 26};
}
static void drawBtn(const Btn& b, const char* label, bool on, bool toggle) {
    u32 fill = toggle ? (on ? C(0x3ddbb0) : C(0x3a3470)) : C(0x4a4390);
    C2D_DrawRectSolid(b.x - 1, b.y - 1, Z(), b.w + 2, b.h + 2, C(0x4a4390));
    C2D_DrawRectSolid(b.x, b.y, Z(), b.w, b.h, fill);
    u32 tc = (toggle && on) ? C(0x1b1838) : C(0xf6efe6);
    if (toggle) {
        TX(label, b.x + 7, b.y + 5, 0.45f, tc);
        TX(on ? "ON" : "OFF", b.x + b.w - 6, b.y + 7, 0.35f, tc, 2);
    } else TX(label, b.x + b.w / 2, b.y + 5, 0.45f, tc, 1);
}
static void drawBottom() {
    gz = 0;
    C2D_TargetClear(botT, C(0x1b1838));
    C2D_SceneBegin(botT);
    TX("Section triche", 6, 1, 0.55f, C(0xffc857));
    TX("X : reset   Y : god   START : quitter", 314, 7, 0.3f, C(0xa9a3d1), 2);
    for (int i = 0; i < NCHEAT; i++) drawBtn(tBtn[i], CH[i].name, cheat[i], true);
    for (int j = 0; j < 4; j++) drawBtn(aBtn[j], ACT_LABEL[j], false, false);
    drawBtn(codeBtn, "Saisir un code (clavier)", false, false);
    TX(msgS, 6, 226, 0.4f, C(0x3ddbb0));
}

// ---------------------------------------------------------------- actions / codes
static void setCheat(int k, bool v) { cheat[k] = v; }

static void doAction(int a) {
    if (a == 0) { lives += 5; say("+5 vies"); }
    else if (a == 1) nextLevel();
    else if (a == 2) {
        P.x = flagX > 0 ? flagX - 60 : 92 * T; P.y = 10 * T; P.vy = 0;
        say(flagX > 0 ? "Téléportation au drapeau" : "Téléportation devant le boss");
    } else if (a == 3) {
        int n = 0; for (auto& k : coins) if (!k.got) { k.got = true; n++; score += 10; }
        say(std::to_string(n) + " pièces récupérées");
    }
}

static void runCode(std::string c) {
    while (!c.empty() && isspace((unsigned char)c.back())) c.pop_back();
    while (!c.empty() && isspace((unsigned char)c.front())) c.erase(c.begin());
    for (auto& ch : c) ch = (char)tolower((unsigned char)ch);
    for (int i = 0; i < NCHEAT; i++) if (c == CH[i].code) {
        setCheat(i, !cheat[i]);
        say(std::string("Code accepté : ") + c + (cheat[i] ? " activé" : " désactivé"));
        return;
    }
    if (c == "skip") doAction(1);
    else if (c == "end") doAction(2);
    else if (c == "coins") doAction(3);
    else if (c == "1up") doAction(0);
    else say("Code inconnu : « " + c + " »");
}

static void askCode() {
    SwkbdState kb; char buf[40] = "";
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 20);
    swkbdSetHintText(&kb, "god, fly, moon, speed, jump, magnet...");
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "Annuler", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "Valider", true);
    if (swkbdInputText(&kb, buf, sizeof(buf)) == SWKBD_BUTTON_RIGHT) runCode(buf);
}

static bool inBtn(const Btn& b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

// ---------------------------------------------------------------- main
int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topT = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    tbuf = C2D_TextBufNew(4096);
    srand((unsigned)osGetTime());
    layoutButtons();
    loadLevel(0);
    msgS = "Touche un bouton pour tricher.";

    while (aptMainLoop()) {
        hidScanInput();
        u32 kd = hidKeysDown(), kh = hidKeysHeld();
        if (kd & KEY_START) break;

        circlePosition cp; hidCircleRead(&cp);
        const u32 JUMP = KEY_A | KEY_B | KEY_DUP;
        inLeft  = (kh & KEY_DLEFT)  || cp.dx < -40;
        inRight = (kh & KEY_DRIGHT) || cp.dx >  40;
        inUp    = (kh & JUMP)       || cp.dy >  40;
        inDown  = (kh & KEY_DDOWN)  || cp.dy < -40;
        if (kd & JUMP) jumpQ = true;
        jumpHeld = (kh & JUMP) != 0;

        if (kd & KEY_X) { lives = std::max(lives, 1); loadLevel(L); }
        if (kd & KEY_Y) { setCheat(C_GOD, !cheat[C_GOD]); say(std::string("Invincibilité") + (cheat[C_GOD] ? " activée" : " désactivée")); }

        bool wantKb = false;
        if (kd & KEY_TOUCH) {
            touchPosition tp; hidTouchRead(&tp);
            for (int i = 0; i < NCHEAT; i++) if (inBtn(tBtn[i], tp.px, tp.py)) {
                setCheat(i, !cheat[i]);
                say(std::string(CH[i].name) + (cheat[i] ? " activé" : " désactivé"));
            }
            for (int j = 0; j < 4; j++) if (inBtn(aBtn[j], tp.px, tp.py)) doAction(j);
            if (inBtn(codeBtn, tp.px, tp.py)) wantKb = true;
        }
        if (wantKb) { askCode(); continue; }

        update();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(tbuf);
        drawTop();
        drawBottom();
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(tbuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
