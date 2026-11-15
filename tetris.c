/*
 * IceTetris
 *
 * A small portable Tetris clone in plain C89. It needs nothing beyond
 * the standard library plus the platform's console helpers, so it
 * builds with most C compilers (Turbo C / Borland, MSVC, DJGPP,
 * Watcom, gcc).
 *
 * Controls:
 *   a / left    move left
 *   d / right   move right
 *   w / up      rotate
 *   f / down    soft drop
 *   s / space   instant drop
 *   p           pause
 *   q           quit
 */

/* Feature test macros must come before any system header. */
#if !defined(_WIN32) && !defined(_WIN64)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 199309L
#  endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define BH        20          /* board height (rows)    */
#define BW        10          /* board width  (columns) */
#define FRAME_MS  10          /* loop tick in ms        */

#define C_DEFAULT 0
#define C_GHOST   8

#define K_UP      1001
#define K_DOWN    1002
#define K_LEFT    1003
#define K_RIGHT   1004

/* ---------------- platform detection ---------------- */
#if defined(_WIN32) || defined(_WIN64)
#  define PLAT_WIN  1
#  define PLAT_DOS  0
#  define PLAT_UNIX 0
#elif defined(__DJGPP__) || defined(__TURBOC__) || defined(__BORLANDC__) || \
      defined(__WATCOMC__) || defined(MSDOS) || defined(__MSDOS__)
#  define PLAT_WIN  0
#  define PLAT_DOS  1
#  define PLAT_UNIX 0
#else
#  define PLAT_WIN  0
#  define PLAT_DOS  0
#  define PLAT_UNIX 1
#endif

#if PLAT_WIN
#  include <windows.h>
#  include <conio.h>
#elif PLAT_DOS
#  include <conio.h>
#  include <dos.h>
#else
#  include <termios.h>
#  include <unistd.h>
#  include <sys/select.h>
#endif

/* Colours: 0 = default, 1..7 = pieces, 8 = ghost. */
#if PLAT_UNIX
static const char *COL[9] = {
    "\x1b[0m",  "\x1b[96m", "\x1b[93m", "\x1b[95m", "\x1b[92m",
    "\x1b[91m", "\x1b[94m", "\x1b[97m", "\x1b[90m"
};
#else
static const int COL[9] = { 7, 11, 14, 13, 10, 12, 9, 15, 8 };
#endif

/* ---------------- platform layer ---------------- */
#if PLAT_UNIX
static struct termios g_old;
static int g_raw = 0;
static int g_done = 0;

static void term_init(void)
{
    struct termios t;
    if (tcgetattr(0, &g_old) == 0) {
        t = g_old;
        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &t);
        g_raw = 1;
    }
    fputs("\x1b[?25l", stdout);
    fflush(stdout);
}
static void term_done(void)
{
    if (g_done) return;
    g_done = 1;
    fputs("\x1b[0m\x1b[?25h", stdout);
    fflush(stdout);
    if (g_raw) tcsetattr(0, TCSANOW, &g_old);
}
static void term_clear(void) { fputs("\x1b[2J\x1b[H", stdout); fflush(stdout); }
static void term_gotoxy(int x, int y) { printf("\x1b[%d;%dH", y, x); }
static void term_color(int c) { fputs(COL[c], stdout); }
static int key_hit(void)
{
    fd_set r;
    struct timeval tv;
    FD_ZERO(&r); FD_SET(0, &r);
    tv.tv_sec = 0; tv.tv_usec = 0;
    return select(1, &r, NULL, NULL, &tv) > 0;
}
static int read_byte(void)
{
    unsigned char c;
    if (read(0, &c, 1) == 1) return (int)c;
    return -1;
}
static int read_key(void)
{
    int c = read_byte();
    if (c == 27) {                       /* possibly an arrow sequence */
        fd_set r;
        struct timeval tv;
        FD_ZERO(&r); FD_SET(0, &r);
        tv.tv_sec = 0; tv.tv_usec = 40000;
        if (select(1, &r, NULL, NULL, &tv) > 0) {
            int a = read_byte();
            if (a == '[' || a == 'O') {
                int b = read_byte();
                if (b == 'A') return K_UP;
                if (b == 'B') return K_DOWN;
                if (b == 'C') return K_RIGHT;
                if (b == 'D') return K_LEFT;
            }
        }
        return 0;                        /* lone ESC: ignore */
    }
    return c;
}
static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

#if PLAT_WIN
static HANDLE g_out;
static int g_done = 0;

static void term_init(void)
{
    CONSOLE_CURSOR_INFO ci;
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    ci.dwSize = 1;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(g_out, &ci);
}
static void term_done(void)
{
    CONSOLE_CURSOR_INFO ci;
    if (g_done) return;
    g_done = 1;
    SetConsoleTextAttribute(g_out, 7);
    ci.dwSize = 25;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(g_out, &ci);
}
static void term_clear(void)
{
    CONSOLE_SCREEN_BUFFER_INFO si;
    DWORD n;
    COORD home;
    home.X = 0; home.Y = 0;
    GetConsoleScreenBufferInfo(g_out, &si);
    FillConsoleOutputCharacterA(g_out, ' ',
        (DWORD)si.dwSize.X * si.dwSize.Y, home, &n);
    FillConsoleOutputAttribute(g_out, si.wAttributes,
        (DWORD)si.dwSize.X * si.dwSize.Y, home, &n);
    SetConsoleCursorPosition(g_out, home);
}
static void term_gotoxy(int x, int y)
{
    COORD p;
    p.X = (SHORT)(x - 1);
    p.Y = (SHORT)(y - 1);
    SetConsoleCursorPosition(g_out, p);
}
static void term_color(int c) { SetConsoleTextAttribute(g_out, (WORD)COL[c]); }
static int key_hit(void) { return _kbhit(); }
static int read_key(void)
{
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int s = _getch();
        if (s == 72) return K_UP;
        if (s == 80) return K_DOWN;
        if (s == 75) return K_LEFT;
        if (s == 77) return K_RIGHT;
        return 0;
    }
    return c;
}
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#endif

#if PLAT_DOS
static int g_done = 0;

static void term_init(void) { textcolor(7); clrscr(); }
static void term_done(void)
{
    if (g_done) return;
    g_done = 1;
    textcolor(7);
}
static void term_clear(void) { clrscr(); }
static void term_gotoxy(int x, int y) { gotoxy(x, y); }
static void term_color(int c) { textcolor(COL[c]); }
static int key_hit(void) { return kbhit(); }
static int read_key(void)
{
    int c = getch();
    if (c == 0) {
        int s = getch();
        if (s == 72) return K_UP;
        if (s == 80) return K_DOWN;
        if (s == 75) return K_LEFT;
        if (s == 77) return K_RIGHT;
        return 0;
    }
    return c;
}
static void sleep_ms(int ms) { delay(ms); }
#endif

/* ---------------- game state ---------------- */
static int board[BH][BW];
static int px, py, prot, ptype;
static int nexttype;
static int score, lines, level;
static int gameover, paused;
static int drop_frames, frame;

/* The 7 tetrominoes, 4 rotations each, as 4 rows of 4 chars. */
static const char *SH[7][4][4] = {
    /* I */
    { { "....", "XXXX", "....", "...." }, { "..X.", "..X.", "..X.", "..X." },
      { "....", "....", "XXXX", "...." }, { ".X..", ".X..", ".X..", ".X.." } },
    /* O */
    { { ".XX.", ".XX.", "....", "...." }, { ".XX.", ".XX.", "....", "...." },
      { ".XX.", ".XX.", "....", "...." }, { ".XX.", ".XX.", "....", "...." } },
    /* T */
    { { ".X..", "XXX.", "....", "...." }, { ".X..", ".XX.", ".X..", "...." },
      { "....", "XXX.", ".X..", "...." }, { ".X..", "XX..", ".X..", "...." } },
    /* S */
    { { ".XX.", "XX..", "....", "...." }, { ".X..", ".XX.", "..X.", "...." },
      { "....", ".XX.", "XX..", "...." }, { "X...", "XX..", ".X..", "...." } },
    /* Z */
    { { "XX..", ".XX.", "....", "...." }, { "..X.", ".XX.", ".X..", "...." },
      { "....", "XX..", ".XX.", "...." }, { ".X..", "XX..", "X...", "...." } },
    /* J */
    { { "X...", "XXX.", "....", "...." }, { ".XX.", ".X..", ".X..", "...." },
      { "....", "XXX.", "..X.", "...." }, { ".X..", ".X..", "XX..", "...." } },
    /* L */
    { { "..X.", "XXX.", "....", "...." }, { ".X..", ".X..", ".XX.", "...." },
      { "....", "XXX.", "X...", "...." }, { "XX..", ".X..", ".X..", "...." } }
};

static int cell(int p, int r, int i, int j) { return SH[p][r][i][j] == 'X'; }

static int collides(int p, int r, int bx, int by)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (cell(p, r, i, j)) {
                int x = bx + i, y = by + j;
                if (y < 0 || y >= BW || x >= BH) return 1;
                if (x >= 0 && board[x][y]) return 1;
            }
    return 0;
}

static void lock_piece(void)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (cell(ptype, prot, i, j)) {
                int x = px + i, y = py + j;
                if (x >= 0 && x < BH && y >= 0 && y < BW)
                    board[x][y] = ptype + 1;
            }
}

static void clear_lines(void)
{
    static const int pts[5] = { 0, 100, 300, 500, 800 };
    int r, c, rr, full, got = 0;
    for (r = BH - 1; r >= 0; r--) {
        full = 1;
        for (c = 0; c < BW; c++)
            if (!board[r][c]) { full = 0; break; }
        if (full) {
            for (rr = r; rr > 0; rr--)
                for (c = 0; c < BW; c++)
                    board[rr][c] = board[rr - 1][c];
            for (c = 0; c < BW; c++) board[0][c] = 0;
            got++;
            r++;                         /* re-check the row we just pulled down */
        }
    }
    if (got) {
        lines += got;
        score += pts[got] * level;
        level = 1 + lines / 10;
        drop_frames = 42 - (level - 1) * 3;
        if (drop_frames < 4) drop_frames = 4;
    }
}

static void spawn(void)
{
    ptype = nexttype;
    nexttype = rand() % 7;
    prot = 0;
    px = 0;
    py = BW / 2 - 2;
    if (collides(ptype, prot, px, py)) gameover = 1;
}

static void start_game(void)
{
    int r, c;
    for (r = 0; r < BH; r++)
        for (c = 0; c < BW; c++) board[r][c] = 0;
    score = 0; lines = 0; level = 1;
    gameover = 0; paused = 0; frame = 0;
    drop_frames = 42;
    nexttype = rand() % 7;
    spawn();
}

static void step(void)
{
    if (!collides(ptype, prot, px + 1, py)) {
        px++;
    } else {
        lock_piece();
        clear_lines();
        spawn();
    }
}

static void handle_key(int k)
{
    if (k == 'q' || k == 'Q') { gameover = 1; return; }
    if (k == 'p' || k == 'P') { paused = !paused; return; }
    if (paused || gameover) return;

    if (k == K_LEFT || k == 'a' || k == 'A') {
        if (!collides(ptype, prot, px, py - 1)) py--;
    } else if (k == K_RIGHT || k == 'd' || k == 'D') {
        if (!collides(ptype, prot, px, py + 1)) py++;
    } else if (k == K_UP || k == 'w' || k == 'W') {
        int nr = (prot + 1) % 4;
        if (!collides(ptype, nr, px, py)) prot = nr;
    } else if (k == K_DOWN || k == 'f' || k == 'F') {
        if (!collides(ptype, prot, px + 1, py)) { px++; score++; }
    } else if (k == 's' || k == 'S' || k == ' ') {
        while (!collides(ptype, prot, px + 1, py)) { px++; score += 2; }
        lock_piece();
        clear_lines();
        spawn();
    }
}

/* ---------------- drawing ---------------- */
static void put(int x, int y, int col, const char *s)
{
    term_gotoxy(x, y);
    term_color(col);
    fputs(s, stdout);
}

static void draw(void)
{
    int disp[BH][BW];
    int r, c, i, j, x, y, gr;
    char buf[24];

    for (r = 0; r < BH; r++)
        for (c = 0; c < BW; c++)
            disp[r][c] = board[r][c];

    if (!gameover) {
        gr = px;
        while (!collides(ptype, prot, gr + 1, py)) gr++;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                if (cell(ptype, prot, i, j)) {
                    int bx = gr + i, by = py + j;
                    if (bx >= 0 && bx < BH && by >= 0 && by < BW && !disp[bx][by])
                        disp[bx][by] = C_GHOST;
                }
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                if (cell(ptype, prot, i, j)) {
                    int bx = px + i, by = py + j;
                    if (bx >= 0 && bx < BH && by >= 0 && by < BW)
                        disp[bx][by] = ptype + 1;
                }
    }

    put(2, 1, C_DEFAULT, "ICE TETRIS");

    put(2, 2, C_DEFAULT, "+------------------+");
    for (r = 0; r < BH; r++) {
        put(2, 3 + r, C_DEFAULT, "|");
        put(23, 3 + r, C_DEFAULT, "|");
    }
    put(2, 23, C_DEFAULT, "+------------------+");

    for (r = 0; r < BH; r++)
        for (c = 0; c < BW; c++) {
            x = 3 + c * 2;
            y = 3 + r;
            if (disp[r][c] == 0) put(x, y, C_DEFAULT, "  ");
            else put(x, y, disp[r][c], "[]");
        }

    put(26, 2, C_DEFAULT, "SCORE");
    sprintf(buf, "%-9d", score);
    put(26, 3, C_DEFAULT, buf);
    put(26, 5, C_DEFAULT, "LINES");
    sprintf(buf, "%-9d", lines);
    put(26, 6, C_DEFAULT, buf);
    put(26, 8, C_DEFAULT, "LEVEL");
    sprintf(buf, "%-9d", level);
    put(26, 9, C_DEFAULT, buf);

    put(26, 11, C_DEFAULT, "NEXT");
    put(26, 12, C_DEFAULT, "+--------+");
    for (i = 0; i < 4; i++) {
        put(26, 13 + i, C_DEFAULT, "|");
        put(35, 13 + i, C_DEFAULT, "|");
    }
    put(26, 17, C_DEFAULT, "+--------+");
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            x = 27 + j * 2;
            y = 13 + i;
            if (SH[nexttype][0][i][j] == 'X') put(x, y, nexttype + 1, "[]");
            else put(x, y, C_DEFAULT, "  ");
        }

    put(2, 24, C_DEFAULT,
        "a/d move   w rotate   f soft drop   s instant drop   p pause   q quit");

    if (paused) put(7, 12, C_DEFAULT, " PAUSED ");

    term_color(C_DEFAULT);
    fflush(stdout);
}

static void on_signal(int s)
{
    (void)s;
    term_done();
    exit(1);
}

int main(void)
{
    int dirty = 1, k;

    srand((unsigned)time(NULL));
    term_init();
    atexit(term_done);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    term_clear();
    start_game();

    while (!gameover) {
        while (key_hit()) {
            k = read_key();
            if (k < 0) { gameover = 1; break; }
            handle_key(k);
            dirty = 1;
        }
        if (gameover) break;
        if (!paused) {
            frame++;
            if (frame >= drop_frames) { frame = 0; step(); dirty = 1; }
        }
        if (dirty) { draw(); dirty = 0; }
        sleep_ms(FRAME_MS);
    }

    draw();
    put(7, 12, C_DEFAULT, "GAME OVER");
    term_color(C_DEFAULT);
    fflush(stdout);
    sleep_ms(1200);
    term_done();
    printf("Final score: %d\n", score);
    return 0;
}


