// ============================================================================
// platform_win32.c — Windows implementation of the platform abstraction
// ============================================================================
// Uses the Win32 Console API for UI, _beginthreadex for threads, and
// CRITICAL_SECTION for mutexes. Winsock for sockets.
// ============================================================================

#define _WIN32_WINNT 0x0600

#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <winsock2.h>

// ============================================================================
// COLOUR MAPPING
// ============================================================================

static const WORD color_map[COLOR_COUNT] = {
    [C_DEFAULT] = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    [C_OPEN]    = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    [C_CLOSED]  = FOREGROUND_RED,
    [C_PENDING] = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    [C_ACCENT]  = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    [C_TITLE]   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    [C_PROGRESS]= FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    [C_PAUSED]  = FOREGROUND_RED | FOREGROUND_INTENSITY,
    [C_DIMTEXT] = FOREGROUND_INTENSITY,
    [C_BOLD]    = FOREGROUND_INTENSITY,
};

// ============================================================================
// GLOBALS
// ============================================================================

static HANDLE hOut, hIn;
static CONSOLE_SCREEN_BUFFER_INFO orig_csbi;
static WORD default_attr;
static DWORD orig_in_mode, orig_out_mode;

// ============================================================================
// CONSOLE LIFE-CYCLE
// ============================================================================

void plat_enter_ui(void) {
    SetConsoleOutputCP(CP_UTF8);

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn  = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleScreenBufferInfo(hOut, &orig_csbi);
    default_attr = orig_csbi.wAttributes;

    GetConsoleMode(hIn,  &orig_in_mode);
    GetConsoleMode(hOut, &orig_out_mode);

    SetConsoleMode(hIn, 0);   // raw input

    plat_show_cursor(0);
}

void plat_exit_ui(void) {
    plat_show_cursor(1);
    SetConsoleTextAttribute(hOut, default_attr);
    SetConsoleCursorPosition(hOut, orig_csbi.dwCursorPosition);
    SetConsoleMode(hIn,  orig_in_mode);
    SetConsoleMode(hOut, orig_out_mode);
}

// ============================================================================
// CONSOLE OUTPUT
// ============================================================================

void plat_puts(int y, int x, color_t color, const char *s) {
    COORD c = { (SHORT)x, (SHORT)y };
    DWORD n;
    SetConsoleCursorPosition(hOut, c);
    SetConsoleTextAttribute(hOut, color_map[color]);
    WriteConsoleA(hOut, s, (DWORD)strlen(s), &n, NULL);
}

void plat_printf(int y, int x, color_t color, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    plat_puts(y, x, color, buf);
}

void plat_clear_rect(int y, int x, int h, int w) {
    DWORD n;
    for (int i = 0; i < h; i++) {
        COORD c = { (SHORT)x, (SHORT)(y + i) };
        SetConsoleCursorPosition(hOut, c);
        FillConsoleOutputCharacterA(hOut, ' ', (DWORD)w, c, &n);
        FillConsoleOutputAttribute(hOut, default_attr, (DWORD)w, c, &n);
    }
}

void plat_get_term_size(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    *cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
}

void plat_show_cursor(int visible) {
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(hOut, &ci);
}

// ============================================================================
// INPUT
// ============================================================================

int plat_kbhit(void) {
    return _kbhit();
}

int plat_getch(void) {
    int c = _getch();
    if (c == 0x00 || c == 0xE0) {
        c = _getch();
        switch (c) {
            case 72: return 0x101;   // K_UP
            case 80: return 0x102;   // K_DOWN
            case 73: return 0x103;   // K_PGUP
            case 81: return 0x104;   // K_PGDN
            case 71: return 0x105;   // K_HOME
            case 79: return 0x106;   // K_END
            default: return c | 0x200;
        }
    }
    return c;
}

// ============================================================================
// TIMING
// ============================================================================

void plat_sleep(int ms) {
    Sleep((DWORD)ms);
}

// ============================================================================
// MUTEX
// ============================================================================

void plat_mutex_init(plat_mutex_t *m) {
    InitializeCriticalSection((CRITICAL_SECTION *)m->impl);
}

void plat_mutex_lock(plat_mutex_t *m) {
    EnterCriticalSection((CRITICAL_SECTION *)m->impl);
}

void plat_mutex_unlock(plat_mutex_t *m) {
    LeaveCriticalSection((CRITICAL_SECTION *)m->impl);
}

void plat_mutex_destroy(plat_mutex_t *m) {
    DeleteCriticalSection((CRITICAL_SECTION *)m->impl);
}

// ============================================================================
// THREAD
// ============================================================================

struct thread_start {
    void (*fn)(void*);
    void *arg;
};

static unsigned __stdcall thread_wrapper(void *arg) {
    struct thread_start *ts = (struct thread_start *)arg;
    ts->fn(ts->arg);
    free(ts);
    return 0;
}

void plat_thread_spawn(plat_thread_t *t, void (*fn)(void*), void *arg) {
    struct thread_start *ts = malloc(sizeof(*ts));
    ts->fn = fn;
    ts->arg = arg;
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, thread_wrapper, ts, 0, NULL);
    memcpy(t->impl, &h, sizeof(h));
}

void plat_thread_wait_all(plat_thread_t *threads, int count) {
    if (count == 0) return;
    HANDLE handles[256];
    for (int i = 0; i < count; i++)
        memcpy(&handles[i], threads[i].impl, sizeof(HANDLE));
    WaitForMultipleObjects((DWORD)count, handles, TRUE, INFINITE);
    for (int i = 0; i < count; i++)
        CloseHandle(handles[i]);
}

// ============================================================================
// SOCKETS
// ============================================================================

int plat_socket_init(void) {
    WSADATA wsad;
    return WSAStartup(MAKEWORD(2, 2), &wsad);
}

void plat_socket_cleanup(void) {
    WSACleanup();
}

void plat_socket_close(int fd) {
    closesocket((SOCKET)fd);
}
