// ============================================================================
// platform.h — Cross-platform abstraction layer
// ============================================================================
// All platform-specific code (Windows vs Linux) lives behind this interface.
// The rest of the program uses these functions and types exclusively, making
// it compile on both OSes without changes.
// ============================================================================

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

// ============================================================================
// COLOR PALETTE (platform-indepedent indices)
// ============================================================================
// Each platform maps these indices to its own colour system (ANSI escapes on
// Linux, console attributes on Windows).

typedef int color_t;
enum {
    C_DEFAULT,
    C_OPEN,
    C_CLOSED,
    C_PENDING,
    C_ACCENT,
    C_TITLE,
    C_PROGRESS,
    C_PAUSED,
    C_DIMTEXT,
    C_BOLD,
    COLOR_COUNT
};

// ============================================================================
// UTF-8 BOX-DRAWING CHARACTERS
// ============================================================================
// These work on both platforms when the terminal supports UTF-8 (modern
// terminals on both Windows and Linux do).

#define BOX_TL  "\xe2\x95\x94"
#define BOX_TR  "\xe2\x95\x97"
#define BOX_BL  "\xe2\x95\x9a"
#define BOX_BR  "\xe2\x95\x9d"
#define BOX_H   "\xe2\x95\x90"
#define BOX_V   "\xe2\x95\x91"
#define BOX_TLJ "\xe2\x95\xa0"
#define BOX_TRJ "\xe2\x95\xa3"
#define BAR_FILL   "\xe2\x96\x88"
#define BAR_EMPTY  "\xe2\x96\x91"

// ============================================================================
// CONSOLE LIFE-CYCLE
// ============================================================================

// Enters "UI mode": saves terminal state, switches to raw input, enables
// UTF-8 output, hides the cursor.
void plat_enter_ui(void);

// Restores the terminal to its original state.
void plat_exit_ui(void);

// ============================================================================
// CONSOLE OUTPUT
// ============================================================================

// Writes a string at position (y, x) with the given colour index.
void plat_puts(int y, int x, color_t color, const char *s);

// printf-style variant of plat_puts.
void plat_printf(int y, int x, color_t color, const char *fmt, ...);

// Fills a rectangular region with spaces (clears it).
void plat_clear_rect(int y, int x, int h, int w);

// Returns the current terminal dimensions (rows, columns).
void plat_get_term_size(int *rows, int *cols);

// Shows (1) or hides (0) the terminal cursor.
void plat_show_cursor(int visible);

// ============================================================================
// INPUT
// ============================================================================

// Returns non-zero if a key press is available without blocking.
int plat_kbhit(void);

// Reads a single key press. Blocks until one is available.
// Returns a character code or K_xxx for arrow / special keys.
int plat_getch(void);

// ============================================================================
// TIMING
// ============================================================================

// Sleeps for the given number of milliseconds.
void plat_sleep(int ms);

// ============================================================================
// MUTEX
// ============================================================================

typedef struct {
    void *impl[4];   // Opaque storage (fits CRITICAL_SECTION / pthread_mutex_t)
} plat_mutex_t;

void plat_mutex_init(plat_mutex_t *m);
void plat_mutex_lock(plat_mutex_t *m);
void plat_mutex_unlock(plat_mutex_t *m);
void plat_mutex_destroy(plat_mutex_t *m);

// ============================================================================
// THREAD
// ============================================================================

typedef struct {
    void *impl[2];   // Opaque storage (fits HANDLE / pthread_t)
} plat_thread_t;

// Spawns a thread that calls fn(arg). fn must have C calling convention.
void plat_thread_spawn(plat_thread_t *t, void (*fn)(void*), void *arg);

// Waits for all threads in the array to finish, then cleans them up.
void plat_thread_wait_all(plat_thread_t *threads, int count);

// ============================================================================
// SOCKETS (minimal — Winsock init / cleanup only)
// ============================================================================
// Individual socket calls in scanner.c use #ifdef _WIN32 because the BSD
// and Winsock APIs are nearly identical apart from a few calls.

int  plat_socket_init(void);
void plat_socket_cleanup(void);
void plat_socket_close(int fd);

#endif // PLATFORM_H
