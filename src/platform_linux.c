// ============================================================================
// platform_linux.c — Linux implementation of the platform abstraction
// ============================================================================
// Uses ANSI escape sequences for UI, termios for raw keyboard, pthreads for
// threading, POSIX sockets for networking.
// ============================================================================

#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>

// ============================================================================
// ANSI COLOUR MAPPING
// ============================================================================
// Each colour index maps to an ANSI SGR (Select Graphic Rendition) code.

static const char *ansi_colors[COLOR_COUNT] = {
    [C_DEFAULT] = "0",        // reset
    [C_OPEN]    = "32;1",     // bold green
    [C_CLOSED]  = "31",       // red
    [C_PENDING] = "33;1",     // bold yellow
    [C_ACCENT]  = "36;1",     // bold cyan
    [C_TITLE]   = "37;1",     // bold white
    [C_PROGRESS]= "32;1",     // bold green
    [C_PAUSED]  = "31;1",     // bold red
    [C_DIMTEXT] = "90",       // bright black (gray)
    [C_BOLD]    = "1",        // bold / intensity
};

// ============================================================================
// GLOBALS
// ============================================================================

static struct termios orig_termios;
static int termios_saved = 0;

// ============================================================================
// CONSOLE LIFE-CYCLE
// ============================================================================

void plat_enter_ui(void) {
    // Save terminal attributes
    tcgetattr(STDIN_FILENO, &orig_termios);
    termios_saved = 1;

    // Set raw mode: no echo, no canonical processing, no signals
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN]  = 1;     // read at least 1 char
    raw.c_cc[VTIME] = 0;     // no timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    // Enable UTF-8 via locale
    setenv("LC_ALL", "en_US.UTF-8", 1);

    // Hide cursor
    plat_show_cursor(0);

    // Clear screen
    write(STDOUT_FILENO, "\033[2J", 4);
}

void plat_exit_ui(void) {
    plat_show_cursor(1);
    write(STDOUT_FILENO, "\033[0m\033[2J\033[H", 10);  // reset, clear, home
    if (termios_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// ============================================================================
// CONSOLE OUTPUT
// ============================================================================

void plat_puts(int y, int x, color_t color, const char *s) {
    // \033[y;xH  — cursor to row y, col x (1-based)
    // \033[<n>m — set colour
    // s         — the text
    // \033[0m   — reset
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH\033[%sm%s\033[0m",
                     y + 1, x + 1, ansi_colors[color], s);
    write(STDOUT_FILENO, buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
}

void plat_printf(int y, int x, color_t color, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    plat_puts(y, x, color, buf);
}

void plat_clear_rect(int y, int x, int h, int w) {
    char buf[1024];
    for (int i = 0; i < h; i++) {
        // Clear each row by overwriting with spaces
        int n = snprintf(buf, sizeof(buf), "\033[%d;%dH%*s",
                         y + 1 + i, x + 1, w, "");
        write(STDOUT_FILENO, buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf)));
    }
}

void plat_get_term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

void plat_show_cursor(int visible) {
    if (visible)
        write(STDOUT_FILENO, "\033[?25h", 6);   // show
    else
        write(STDOUT_FILENO, "\033[?25l", 6);   // hide
}

// ============================================================================
// INPUT
// ============================================================================

int plat_kbhit(void) {
    struct timeval tv = { 0, 0 };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int plat_getch(void) {
    unsigned char c;
    while (read(STDIN_FILENO, &c, 1) != 1);

    // Escape sequence detection
    if (c == '\033') {
        struct timeval tv = { 0, 20000 };   // 20 ms to collect remaining bytes
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        char seq[8];
        int n = 0;

        // Read as many bytes as arrive within the timeout
        while (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0 && n < 6) {
            unsigned char b;
            if (read(STDIN_FILENO, &b, 1) == 1)
                seq[n++] = (char)b;
            tv.tv_sec = 0; tv.tv_usec = 20000;
        }
        seq[n] = 0;

        // Recognised sequences
        if (n >= 2 && seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 0x101;   // K_UP
                case 'B': return 0x102;   // K_DOWN
                case 'C': return 0x107;   // K_RIGHT (not used)
                case 'D': return 0x108;   // K_LEFT  (not used)
                case 'H': return 0x105;   // K_HOME
                case 'F': return 0x106;   // K_END
                case '5': return 0x103;   // K_PGUP  (\033[5~)
                case '6': return 0x104;   // K_PGDN  (\033[6~)
            }
        }
        // If it was just a bare Escape, return it
        if (n == 0) return 27;  // K_ESC
        return c | 0x200;       // unknown escape sequence
    }

    // Ctrl+C — signal stop (the UI loop checks stop_flag)
    if (c == 3) return 3;
    return (int)c;
}

// ============================================================================
// TIMING
// ============================================================================

void plat_sleep(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// ============================================================================
// MUTEX
// ============================================================================

void plat_mutex_init(plat_mutex_t *m) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    // Use NORMAL to match Win32 CRITICAL_SECTION semantics
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init((pthread_mutex_t *)m->impl, &attr);
    pthread_mutexattr_destroy(&attr);
}

void plat_mutex_lock(plat_mutex_t *m) {
    pthread_mutex_lock((pthread_mutex_t *)m->impl);
}

void plat_mutex_unlock(plat_mutex_t *m) {
    pthread_mutex_unlock((pthread_mutex_t *)m->impl);
}

void plat_mutex_destroy(plat_mutex_t *m) {
    pthread_mutex_destroy((pthread_mutex_t *)m->impl);
}

// ============================================================================
// THREAD
// ============================================================================

struct thread_start {
    void (*fn)(void*);
    void *arg;
};

static void *thread_wrapper(void *arg) {
    struct thread_start *ts = (struct thread_start *)arg;
    ts->fn(ts->arg);
    free(ts);
    return NULL;
}

void plat_thread_spawn(plat_thread_t *t, void (*fn)(void*), void *arg) {
    struct thread_start *ts = malloc(sizeof(*ts));
    ts->fn = fn;
    ts->arg = arg;
    pthread_t pt;
    pthread_create(&pt, NULL, thread_wrapper, ts);
    memcpy(t->impl, &pt, sizeof(pt));
}

void plat_thread_wait_all(plat_thread_t *threads, int count) {
    for (int i = 0; i < count; i++) {
        pthread_t pt;
        memcpy(&pt, threads[i].impl, sizeof(pt));
        pthread_join(pt, NULL);
    }
}

// ============================================================================
// SOCKETS (no init / cleanup needed on POSIX)
// ============================================================================

int plat_socket_init(void) {
    return 0;
}

void plat_socket_cleanup(void) {
}

void plat_socket_close(int fd) {
    close(fd);
}
