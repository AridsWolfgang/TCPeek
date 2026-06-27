// ============================================================================
// config.h — Shared types, constants, and extern declarations
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

// Target Vista+ for modern Winsock APIs (getaddrinfo, AI_ADDRCONFIG, etc.)
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0600
#endif

// ============================================================================
// PLATFORM ABSTRACTION
// ============================================================================

#include "platform.h"

// ============================================================================
// STANDARD LIBRARY
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <stdatomic.h>

// ============================================================================
// NETWORKING HEADERS (included here so all modules can use socket types)
// ============================================================================

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0x0400
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

// ============================================================================
// USER CONFIGURATION
// ============================================================================

#define DEFAULT_START   1
#define DEFAULT_END     1024
#define MAX_THREADS     300
#define TIMEOUT_MS      500
#define UI_REFRESH_MS   200

// ============================================================================
// VIRTUAL KEY CODES
// ============================================================================

#define K_UP    0x101
#define K_DOWN  0x102
#define K_PGUP  0x103
#define K_PGDN  0x104
#define K_HOME  0x105
#define K_END   0x106
#define K_ESC   27

// ============================================================================
// DATA TYPES
// ============================================================================

typedef enum {
    ST_UNKNOWN,
    ST_OPEN,
    ST_CLOSED
} port_status_t;

typedef struct {
    int port;
    port_status_t status;
} port_result_t;

// ============================================================================
// EXTERN GLOBALS
// ============================================================================

// scanner.c
extern port_result_t *results;
extern int total_ports;
extern atomic_int next_port;
extern atomic_int ports_done;
extern atomic_int stop_flag;
extern atomic_int pause_flag;
extern atomic_int open_count;
extern struct addrinfo *target_ai;
extern char target_str[256];
extern int thread_count;
extern time_t start_time;
extern double smooth_speed;
extern time_t last_speed_time;
extern int last_speed_done;

// ui.c
extern plat_mutex_t draw_mutex;
extern int filter_open;
extern int scroll_off;
extern int con_rows;
extern int con_cols;
extern int inner_w;
extern int hdr_h;
extern int list_hdr_h;
extern int info_h;
extern int ftr_h;
extern int list_items_h;
extern int hdr_y0;
extern int div1_y;
extern int list_y0;
extern int list_data_y0;
extern int info_y;
extern int div2_y;
extern int ftr_y0;
extern int div3_y;
extern int bot_y;

#endif // CONFIG_H
