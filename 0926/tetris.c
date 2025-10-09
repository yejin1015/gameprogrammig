#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>

#define W 10
#define H 20
#define BORDER_LEFT 4
#define TICK_BASE 550 /* ms (레벨 올라갈수록 감소) */
#define CELL_FILLED "[]"
#define CELL_EMPTY "  "

/* 콘솔 커서 이동/숨김 */
static void gotoxy(int x, int y)
{
    COORD c;
    c.X = (SHORT)x;
    c.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}
static void hide_cursor(int hide)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci;
    GetConsoleCursorInfo(hOut, &cci);
    cci.bVisible = hide ? FALSE : TRUE;
    SetConsoleCursorInfo(hOut, &cci);
}

static int board[H][W]; /* 0: 빈칸, 1+: 고정 블록 */
static int score = 0, lines_cleared = 0, level = 1;

typedef struct
{
    int x, y, r, type;
} Piece;

/* 7개 테트로미노(I O T S Z J L), 각 4회전, 4x4 */
static const int TET[7][4][4][4] = {
    /* I */
    {
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}}},
    /* O */
    {
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    /* T */
    {
        {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    /* S */
    {
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}}},
    /* Z */
    {
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    /* J */
    {
        {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}}},
    /* L */
    {
        {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}}};

static Piece cur, nxt;

/* 랜덤 */
static int rnd(int n) { return rand() % n; }

/* 충돌 검사 (경계/바닥/고정 블록) */
static int hit(Piece p)
{
    int yy, xx, x, y;
    for (yy = 0; yy < 4; ++yy)
        for (xx = 0; xx < 4; ++xx)
        {
            if (!TET[p.type][p.r][yy][xx])
                continue;
            x = p.x + xx;
            y = p.y + yy;
            if (x < 0 || x >= W || y >= H)
                return 1; // y<0 은 허용
            if (y >= 0 && board[y][x])
                return 1; // 보드 접근은 y>=0일 때만
        }
    return 0;
}

/* 보드에 고정 */
static void lock_piece(Piece p)
{
    int yy, xx, x, y;
    for (yy = 0; yy < 4; ++yy)
        for (xx = 0; xx < 4; ++xx)
        {
            if (TET[p.type][p.r][yy][xx])
            {
                x = p.x + xx;
                y = p.y + yy;
                if (y >= 0 && y < H && x >= 0 && x < W)
                    board[y][x] = p.type + 1;
            }
        }
}

/* 라인 클리어 */
static int clear_lines(void)
{
    int cleared = 0;
    int y, x, yy, full;
    for (y = H - 1; y >= 0; --y)
    {
        full = 1;
        for (x = 0; x < W; ++x)
        {
            if (!board[y][x])
            {
                full = 0;
                break;
            }
        }
        if (full)
        {
            ++cleared;
            for (yy = y; yy > 0; --yy)
                for (x = 0; x < W; ++x)
                    board[yy][x] = board[yy - 1][x];
            for (x = 0; x < W; ++x)
                board[0][x] = 0;
            ++y; /* 같은 y 재검사(위에서 내려왔으므로) */
        }
    }
    if (cleared)
    {
        static const int add[5] = {0, 100, 300, 500, 800};
        lines_cleared += cleared;
        score += add[cleared] * level;
        level = 1 + lines_cleared / 10;
    }
    return cleared;
}

/* 다음 조각으로 교체 / 새 스폰 */
static void spawn(void)
{
    cur = nxt;
    cur.x = (W / 2) - 2;
    cur.y = -1; /* 위에서 등장 */
    cur.r = 0;
    nxt.type = rnd(7);
    nxt.r = 0;
    nxt.x = 0;
    nxt.y = 0;
}

/* 그리기 */
static void draw_frame(void)
{
    int i, y, x;
    /* 보드 외곽 */
    gotoxy(BORDER_LEFT - 2, 1);
    printf("┌");
    for (i = 0; i < W; i++)
        printf("──");
    printf("┐");

    for (y = 0; y < H; ++y)
    {
        gotoxy(BORDER_LEFT - 2, 2 + y);
        printf("│");
        for (x = 0; x < W; ++x)
            printf("  ");
        printf("│");
    }
    gotoxy(BORDER_LEFT - 2, 2 + H);
    printf("└");
    for (i = 0; i < W; i++)
        printf("──");
    printf("┘");
}

static void draw_board_and_piece(void)
{
    int y, x, yy, xx;
    /* 보드 */
    for (y = 0; y < H; ++y)
    {
        gotoxy(BORDER_LEFT, 2 + y);
        for (x = 0; x < W; ++x)
        {
            if (board[y][x])
                printf(CELL_FILLED);
            else
                printf(CELL_EMPTY);
        }
    }
    /* 현재 조각 */
    for (yy = 0; yy < 4; ++yy)
        for (xx = 0; xx < 4; ++xx)
        {
            if (!TET[cur.type][cur.r][yy][xx])
                continue;
            x = cur.x + xx;
            y = cur.y + yy;
            if (y >= 0)
            {
                gotoxy(BORDER_LEFT + x * 2, 2 + y);
                printf("■");
            }
        }

    /* 사이드 정보 */
    gotoxy(BORDER_LEFT + W * 2 + 3, 2);
    printf("[NEXT]");
    for (yy = 0; yy < 4; ++yy)
    {
        gotoxy(BORDER_LEFT + W * 2 + 3, 3 + yy);
        for (xx = 0; xx < 4; ++xx)
        {
            if (TET[nxt.type][0][yy][xx])
                printf(CELL_FILLED);
            else
                printf(CELL_EMPTY);
        }
    }
    gotoxy(BORDER_LEFT + W * 2 + 3, 8);
    printf("Score : %6d", score);
    gotoxy(BORDER_LEFT + W * 2 + 3, 9);
    printf("Lines : %6d", lines_cleared);
    gotoxy(BORDER_LEFT + W * 2 + 3, 10);
    printf("Level : %6d", level);
    gotoxy(BORDER_LEFT + W * 2 + 3, 12);
    printf("Ctrl  : ←→↓, SPACE=Rotate");
    gotoxy(BORDER_LEFT + W * 2 + 3, 13);
    printf("        P=Pause, ESC=Quit");
}

/* 하강 틱(ms) */
static int fall_tick(void)
{
    int t = TICK_BASE - (level - 1) * 45;
    if (t < 90)
        t = 90;
    return t;
}

int main(void)
{
    int y, x;
    DWORD lastFall;
    int paused;
    Piece tmp;
    srand((unsigned)time(NULL));

    /* 초기화 */
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            board[y][x] = 0;

    nxt.type = rnd(7);
    nxt.r = 0;
    nxt.x = 0;
    nxt.y = 0;
    spawn();

    system("cls");
    hide_cursor(1);
    draw_frame();

    lastFall = GetTickCount();
    paused = 0;

    while (1)
    {
        int has_key = 0;
        int k = 0, ext = 0;
        DWORD now;

        /* 입력 처리 (논블로킹) */
        if (_kbhit())
        {
            has_key = 1;
            k = _getch();
        }
        if (has_key)
        {
            if (k == 0 || k == 224)
            {
                /* 확장키 */
                ext = _getch();
                if (!paused)
                {
                    if (ext == 75)
                    { /* ← */
                        tmp = cur;
                        tmp.x--;
                        if (!hit(tmp))
                            cur = tmp;
                    }
                    else if (ext == 77)
                    { /* → */
                        tmp = cur;
                        tmp.x++;
                        if (!hit(tmp))
                            cur = tmp;
                    }
                    else if (ext == 80)
                    { /* ↓ */
                        tmp = cur;
                        tmp.y++;
                        if (!hit(tmp))
                            cur = tmp;
                    }
                    else if (ext == 72)
                    { /* ↑ (하드드롭) */
                        tmp = cur;
                        do
                        {
                            cur = tmp;
                            tmp.y++;
                        } while (!hit(tmp));
                    }
                }
            }
            else
            {
                if (k == 27)
                    break; /* ESC */
                if (k == 'p' || k == 'P')
                {
                    paused = !paused;
                }
                else if (k == ' ' && !paused)
                { /* 회전(간단 킥) */
                    tmp = cur;
                    tmp.r = (tmp.r + 1) & 3;
                    if (!hit(tmp))
                        cur = tmp;
                    else
                    { /* 벽 킥 대충 */
                        tmp.x = cur.x - 1;
                        if (!hit(tmp))
                        {
                            cur = tmp;
                        }
                        else
                        {
                            tmp.x = cur.x + 1;
                            if (!hit(tmp))
                                cur = tmp;
                            else
                            {
                                tmp.x = cur.x;
                                tmp.y = cur.y - 1;
                                if (!hit(tmp))
                                    cur = tmp;
                            }
                        }
                    }
                }
            }
        }

        /* 로직 업데이트 */
        now = GetTickCount();
        if (!paused && now - lastFall >= (DWORD)fall_tick())
        {
            tmp = cur;
            tmp.y++;
            lastFall = now;
            if (!hit(tmp))
            {
                cur = tmp;
            }
            else
            {
                /* 바닥에 닿음 → 고정 */
                lock_piece(cur);
                clear_lines();
                spawn();
                if (hit(cur))
                {
                    /* 스폰 즉시 충돌 → 게임오버 */
                    gotoxy(BORDER_LEFT, 2 + H / 2);
                    printf("   G A M E   O V E R   ");
                    gotoxy(BORDER_LEFT, 3 + H / 2);
                    printf("   Score: %d            ", score);
                    gotoxy(0, 2 + H + 3);
                    hide_cursor(0);
                    _getch();
                    return 0;
                }
            }
        }

        /* 렌더(프레임 단위) */
        draw_board_and_piece();
        Sleep(10);
    }

    hide_cursor(0);
    system("cls");
    return 0;
}