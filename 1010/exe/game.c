#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// --- 전역 변수 선언 ---
BOOL g_switchToPiano = FALSE;
#define NUM_TOTAL_KEYS 13
int key_visual_state[NUM_TOTAL_KEYS] = {0};
#define IDT_KEY_TIMER_BASE 1000
int base_octave = 4;

// --- 인트로 화면 관련 상수 ---
#define IDT_INTRO_TIMER 1
#define INTRO_DURATION_MS 2500
#define IDT_FADE_TIMER 2
#define FADE_STEP_MS 50
#define FADE_STEPS (INTRO_DURATION_MS / FADE_STEP_MS)
int g_fade_alpha = 0;

// --- 녹음 및 재생 관련 ---
#define MAX_NOTES 1024
#define IDT_PLAYBACK_TIMER 3

typedef struct {
    int key_index;
    int octave;
    DWORD timestamp;
} NoteEvent;

NoteEvent g_recordedNotes[MAX_NOTES];
int g_noteCount = 0;
BOOL g_isRecording = FALSE;
BOOL g_isPlaying = FALSE;
DWORD g_startTime = 0;
DWORD g_playbackStartTime = 0;
int g_playbackIndex = 0;


// --- 함수 프로토타입 선언 ---
LRESULT CALLBACK IntroWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK PianoWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void DrawIntro(HDC hdc, RECT clientRect);
void DrawPiano(HDC hdc, RECT clientRect);
void PlayNote(int key_index, int octave_override);

// --- WinMain: 프로그램의 시작점 ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const char* fontFileName = "MaruBuri-Regular.ttf";
    if (AddFontResourceExA(fontFileName, FR_PRIVATE, NULL) == 0)
    {
        MessageBoxA(NULL, "폰트 파일을 로드할 수 없습니다!\n'MaruBuri-Regular.ttf' 파일이 실행 파일과 같은 위치에 있는지 확인하세요.", "폰트 오류", MB_OK | MB_ICONERROR);
    }
    SendMessageA(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);

    const char INTRO_CLASS_NAME[] = "IntroWindowClass";
    WNDCLASS wcIntro = {0};
    wcIntro.lpfnWndProc   = IntroWindowProc;
    wcIntro.hInstance     = hInstance;
    wcIntro.lpszClassName = INTRO_CLASS_NAME;
    wcIntro.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcIntro.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcIntro);

    const char PIANO_CLASS_NAME[] = "PianoWindowClass";
    WNDCLASS wcPiano = {0};
    wcPiano.lpfnWndProc   = PianoWindowProc;
    wcPiano.hInstance     = hInstance;
    wcPiano.lpszClassName = PIANO_CLASS_NAME;
    wcPiano.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcPiano.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcPiano);

    HWND hwndIntro = CreateWindowExA(0, INTRO_CLASS_NAME, "Intro", WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 300,
        NULL, NULL, hInstance, NULL);

    if (hwndIntro == NULL) return 0;

    RECT screenRect;
    GetClientRect(GetDesktopWindow(), &screenRect);
    RECT introRect;
    GetWindowRect(hwndIntro, &introRect);
    SetWindowPos(hwndIntro, NULL,
        (screenRect.right - (introRect.right - introRect.left)) / 2,
        (screenRect.bottom - (introRect.bottom - introRect.top)) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwndIntro, nCmdShow);
    UpdateWindow(hwndIntro);

    PlaySoundA("sound/intro.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    SetTimer(hwndIntro, IDT_INTRO_TIMER, INTRO_DURATION_MS, NULL);
    SetTimer(hwndIntro, IDT_FADE_TIMER, FADE_STEP_MS, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_switchToPiano)
    {
        HWND hwndPiano = CreateWindowExA(0, PIANO_CLASS_NAME, "게임프로그래밍 9_1_2", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 900, 500,
            NULL, NULL, hInstance, NULL);

        if (hwndPiano == NULL) return 0;

        ShowWindow(hwndPiano, nCmdShow);
        UpdateWindow(hwndPiano);

        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    RemoveFontResourceExA(fontFileName, FR_PRIVATE, NULL);
    return (int) msg.wParam;
}

LRESULT CALLBACK IntroWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        DrawIntro(hdc, r);
        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_TIMER:
        if (wParam == IDT_INTRO_TIMER)
        {
            PlaySound(NULL, NULL, 0);
            g_switchToPiano = TRUE;
            KillTimer(hwnd, IDT_INTRO_TIMER);
            KillTimer(hwnd, IDT_FADE_TIMER);
            DestroyWindow(hwnd);
        }
        else if (wParam == IDT_FADE_TIMER)
        {
            g_fade_alpha += (255 / FADE_STEPS);
            if (g_fade_alpha > 255) g_fade_alpha = 255;
            InvalidateRect(hwnd, NULL, TRUE);
            if (g_fade_alpha == 255) KillTimer(hwnd, IDT_FADE_TIMER);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        PlaySound(NULL, NULL, 0);
        g_switchToPiano = TRUE;
        KillTimer(hwnd, IDT_INTRO_TIMER);
        KillTimer(hwnd, IDT_FADE_TIMER);
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT CALLBACK PianoWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const char* myFontName = "MaruBuri-Regular";

    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        DrawPiano(hdc, clientRect);

        char statusText[256];
        if (g_isRecording)
        {
            sprintf(statusText, "[녹음 중...] | Z키를 다시 눌러 녹음을 중지하세요.");
        }
        else if (g_isPlaying)
        {
            sprintf(statusText, "[재생 중...]");
        }
        else
        {
            sprintf(statusText, "현재 옥타브: %d | 연주: 1 - 8, QWETY | 조작: ↑ ↓ (옥타브), Z (녹음), X (재생) | 종료: ESC", base_octave);
        }

        // [글자 크기 변경] 12pt -> 15pt
        HFONT hFont = CreateFontA(-MulDiv(15, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, 0, 0, 0,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, myFontName);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        TextOutA(hdc, 10, 10, statusText, strlen(statusText));

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_KEYDOWN:
    {
        if (lParam & (1 << 30)) return 0;

        int key_code = -1;
        if (wParam >= '1' && wParam <= '8') { key_code = wParam - '1'; }
        else {
            switch(wParam) {
                case 'Q': key_code = 8; break; case 'W': key_code = 9; break;
                case 'E': key_code = 10; break; case 'T': key_code = 11; break;
                case 'Y': key_code = 12; break;
            }
        }

        if (key_code != -1)
        {
            key_visual_state[key_code] = 1;
            PlayNote(key_code, base_octave);
            InvalidateRect(hwnd, NULL, FALSE);

            if (g_isRecording && g_noteCount < MAX_NOTES)
            {
                g_recordedNotes[g_noteCount].key_index = key_code;
                g_recordedNotes[g_noteCount].octave = base_octave;
                g_recordedNotes[g_noteCount].timestamp = GetTickCount() - g_startTime;
                g_noteCount++;
            }
        }
        else
        {
            switch (wParam)
            {
                case VK_UP:
                    if (base_octave < 5) base_octave++;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case VK_DOWN:
                    if (base_octave > 3) base_octave--;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 'Z':
                    if (!g_isPlaying) {
                        g_isRecording = !g_isRecording;
                        if (g_isRecording) {
                            g_noteCount = 0; g_startTime = GetTickCount();
                        }
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case 'X':
                    if (!g_isRecording && !g_isPlaying && g_noteCount > 0) {
                        g_isPlaying = TRUE; g_playbackIndex = 0; g_playbackStartTime = GetTickCount();
                        SetTimer(hwnd, IDT_PLAYBACK_TIMER, 10, NULL);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
            }
        }
    }
    return 0;

    case WM_KEYUP:
    {
        int key_code = -1;
        if (wParam >= '1' && wParam <= '8') { key_code = wParam - '1'; }
        else {
            switch(wParam) {
                case 'Q': key_code = 8; break; case 'W': key_code = 9; break;
                case 'E': key_code = 10; break; case 'T': key_code = 11; break;
                case 'Y': key_code = 12; break;
            }
        }
        if (key_code != -1) { SetTimer(hwnd, IDT_KEY_TIMER_BASE + key_code, 100, NULL); }
    }
    return 0;

    case WM_TIMER:
    {
        if (wParam >= IDT_KEY_TIMER_BASE && wParam < IDT_KEY_TIMER_BASE + NUM_TOTAL_KEYS)
        {
            int key_code = wParam - IDT_KEY_TIMER_BASE;
            key_visual_state[key_code] = 0;
            KillTimer(hwnd, IDT_KEY_TIMER_BASE + key_code);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == IDT_PLAYBACK_TIMER)
        {
            if (g_isPlaying)
            {
                DWORD elapsedTime = GetTickCount() - g_playbackStartTime;
                if (g_playbackIndex < g_noteCount && elapsedTime >= g_recordedNotes[g_playbackIndex].timestamp)
                {
                    NoteEvent note = g_recordedNotes[g_playbackIndex];
                    PlayNote(note.key_index, note.octave);
                    key_visual_state[note.key_index] = 1;
                    SetTimer(hwnd, IDT_KEY_TIMER_BASE + note.key_index, 100, NULL);
                    InvalidateRect(hwnd, NULL, FALSE);
                    g_playbackIndex++;
                }

                if (g_playbackIndex >= g_noteCount)
                {
                    g_isPlaying = FALSE;
                    KillTimer(hwnd, IDT_PLAYBACK_TIMER);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
        }
    }
    return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DrawIntro(HDC hdc, RECT clientRect)
{
    const char* myFontName = "MaruBuri-Regular";

    TRIVERTEX vertices[2];
    GRADIENT_RECT gradientRect;
    vertices[0] = (TRIVERTEX){0, 0, 0x0000, 0x8000, 0xFFFF, 0x0000};
    vertices[1] = (TRIVERTEX){clientRect.right, clientRect.bottom, 0x0000, 0x0000, 0x8000, 0x0000};
    gradientRect = (GRADIENT_RECT){0, 1};
    GradientFill(hdc, vertices, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);

    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    HPEN noPen = (HPEN)GetStockObject(NULL_PEN);
    SelectObject(hdc, noPen);

    int iconWidth = 200, iconHeight = 80;
    int iconX = (clientRect.right - iconWidth) / 2;
    int iconY = (clientRect.bottom / 2) - iconHeight - 40;

    SelectObject(hdc, whiteBrush);
    for (int i = 0; i < 4; i++)
    {
        Rectangle(hdc, iconX + i * (iconWidth / 4), iconY, iconX + (i + 1) * (iconWidth / 4) - 2, iconY + iconHeight);
    }

    SelectObject(hdc, blackBrush);
    Rectangle(hdc, iconX + (iconWidth / 4) - 15, iconY, iconX + (iconWidth / 4) + 15, iconY + iconHeight / 2);
    Rectangle(hdc, iconX + 2 * (iconWidth / 4) + 5, iconY, iconX + 2 * (iconWidth / 4) + 35, iconY + iconHeight / 2);
    Rectangle(hdc, iconX + 3 * (iconWidth / 4) + 5, iconY, iconX + 3 * (iconWidth / 4) + 35, iconY + iconHeight / 2);

    DeleteObject(whiteBrush);
    DeleteObject(blackBrush);

    COLORREF fadedTextColor = RGB(255 * g_fade_alpha / 255, 255 * g_fade_alpha / 255, 255 * g_fade_alpha / 255);
    SetTextColor(hdc, fadedTextColor);
    SetBkMode(hdc, TRANSPARENT);

    // [글자 크기 변경] 50pt -> 52pt
    HFONT hFontLarge = CreateFontA(-MulDiv(52, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, myFontName);

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFontLarge);
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT shadowRect = clientRect;
    shadowRect.left += 3;
    shadowRect.top += 3;
    DrawTextA(hdc, "real 피아노", -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, fadedTextColor);
    DrawTextA(hdc, "real 피아노", -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFontLarge);

    // [글자 크기 변경] 18pt -> 22pt
    HFONT hFontSmall = CreateFontA(-MulDiv(22, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, myFontName);

    SelectObject(hdc, hFontSmall);
    RECT subTextRect = clientRect;
    subTextRect.top = clientRect.bottom / 2 + 50;
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT subShadowRect = subTextRect;
    subShadowRect.left += 2;
    subShadowRect.top += 2;
    DrawTextA(hdc, "2023864032 양예진", -1, &subShadowRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    SetTextColor(hdc, fadedTextColor);
    DrawTextA(hdc, "2023864032 양예진", -1, &subTextRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFontSmall);
}

void PlayNote(int key_index, int octave_override)
{
    const char* sound_files[3][NUM_TOTAL_KEYS] =
    {
        {
            "sound/FX_piano09.wav", "sound/FX_piano10.wav", "sound/FX_piano11.wav", "sound/FX_piano12.wav",
            "sound/FX_piano13.wav", "sound/FX_piano14.wav", "sound/FX_piano15.wav", "sound/FX_piano16.wav",
            "sound/FX_piano_b06.wav", "sound/FX_piano_b07.wav", "sound/FX_piano_b08.wav",
            "sound/FX_piano_b09.wav", "sound/FX_piano_b10.wav"
        },
        {
            "sound/FX_piano01.wav", "sound/FX_piano02.wav", "sound/FX_piano03.wav", "sound/FX_piano04.wav",
            "sound/FX_piano05.wav", "sound/FX_piano06.wav", "sound/FX_piano07.wav", "sound/FX_piano08.wav",
            "sound/C4.wav", "sound/D4.wav", "sound/F4.wav",
            "sound/G4.wav", "sound/A4.wav"
        },
        {
            "sound/C5.wav", "sound/FX_piano18.wav", "sound/FX_piano19.wav", "sound/FX_piano20.wav",
            "sound/FX_piano21.wav", "sound/FX_piano22.wav", "sound/FX_piano23.wav", "sound/FX_piano24.wav",
            "sound/FX_piano_b11.wav", "sound/FX_piano_b12.wav", "sound/FX_piano_b13.wav",
            "sound/FX_piano_b14.wav", "sound/FX_piano_b15.wav"
        }
    };

    int octave_index = octave_override - 3;
    if (octave_index >= 0 && octave_index < 3 && key_index >= 0 && key_index < NUM_TOTAL_KEYS)
    {
        PlaySoundA(sound_files[octave_index][key_index], NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

void DrawPiano(HDC hdc, RECT clientRect)
{
    const char* myFontName = "MaruBuri-Regular";

    int num_white_keys = 8;
    int white_key_width = (clientRect.right - 20) / num_white_keys;
    int white_key_height = clientRect.bottom - 50;

    HBRUSH white_brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH black_brush = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH pressed_brush = CreateSolidBrush(RGB(173, 216, 230));
    HBRUSH black_pressed_brush = CreateSolidBrush(RGB(100, 150, 180));
    HPEN black_pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    SelectObject(hdc, black_pen);

    const char* white_key_labels[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    const char* note_names[] = {"도", "레", "미", "파", "솔", "라", "시", "도"};
    const char* black_key_labels[] = {"Q", "W", "E", "T", "Y"};

    // [글자 크기 변경] 14pt -> 18pt
    HFONT hKeyFont = CreateFontA(-MulDiv(18, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, 0, 0, 0, HANGEUL_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, myFontName);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hKeyFont);

    for (int i = 0; i < num_white_keys; i++)
    {
        int x1 = 10 + i * white_key_width, y1 = 40, x2 = x1 + white_key_width, y2 = y1 + white_key_height;
        SelectObject(hdc, (key_visual_state[i] == 1) ? pressed_brush : white_brush);
        Rectangle(hdc, x1, y1, x2, y2);

        RECT textRect = {x1, y2 - 60, x2, y2};
        char label[20];
        sprintf(label, "%s\n(%s)", white_key_labels[i], note_names[i]);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        DrawTextA(hdc, label, -1, &textRect, DT_CENTER | DT_WORDBREAK);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hKeyFont);

    // [글자 크기 변경] 11pt -> 14pt
    hKeyFont = CreateFontA(-MulDiv(14, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, myFontName);
    SelectObject(hdc, hKeyFont);

    int black_key_width = white_key_width * 0.6;
    int black_key_height = white_key_height * 0.6;
    int black_key_indices[] = {8, 9, 10, 11, 12};
    int black_key_pos[] = {0, 1, 3, 4, 5};
    for (int i = 0; i < 5; i++)
    {
        int key_idx = black_key_indices[i], pos_idx = black_key_pos[i];
        int x1 = 10 + (pos_idx + 1) * white_key_width - (black_key_width / 2);
        int y1 = 40, x2 = x1 + black_key_width, y2 = y1 + black_key_height;
        SelectObject(hdc, (key_visual_state[key_idx] == 1) ? black_pressed_brush : black_brush);
        Rectangle(hdc, x1, y1, x2, y2);

        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect = {x1, y1 + 10, x2, y2};
        DrawTextA(hdc, black_key_labels[i], -1, &textRect, DT_CENTER | DT_TOP);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hKeyFont);
    DeleteObject(white_brush);
    DeleteObject(black_brush);
    DeleteObject(pressed_brush);
    DeleteObject(black_pressed_brush);
    DeleteObject(black_pen);
}