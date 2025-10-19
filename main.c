// port_scanner_ncurses.c
// Compile: gcc -o main  main.c -lncurses -lpthread
// Usage: run the program, enter target and optional range in UI, press Enter to start scanning.
// Controls: 'q' to quit

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <pthread.h>
#include <ncurses.h>
#include <stdatomic.h>

#define DEFAULT_START_PORT 1
#define DEFAULT_END_PORT 1024
#define MAX_THREADS 200
#define CONNECT_TIMEOUT_MS 500

typedef enum { ST_UNKNOWN=0, ST_OPEN, ST_CLOSED, ST_TIMEOUT, ST_ERROR } port_status_t;

typedef struct {
    int port;
    port_status_t status;
} port_result_t;

static port_result_t *results = NULL;
static int total_ports = 0;
static atomic_int next_port;
static atomic_int ports_done;
static atomic_int stop_flag = 0;

static struct addrinfo *target_ai = NULL;
static char target_str[256] = {0};

static pthread_t *threads = NULL;
static int thread_count = 50;

static pthread_mutex_t draw_mutex = PTHREAD_MUTEX_INITIALIZER;

static time_t start_time;

static WINDOW *win_header, *win_list, *win_footer;

static void draw_ui_header();
static void draw_ui_list(int highlight_offset);
static void draw_ui_footer();

static void cleanup_and_exit(int code) {
    if (target_ai) freeaddrinfo(target_ai);
    if (results) free(results);
    if (threads) free(threads);

    endwin();
    exit(code);
}

static void sigint_handler(int sig) {
    (void)sig;
    atomic_store(&stop_flag, 1);
}

// non-blocking connect with timeout in milliseconds
static int try_connect_addr(struct sockaddr *sa, socklen_t salen, int timeout_ms) {
    int s = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return -1;
    // set non-blocking
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    int res = connect(s, sa, salen);
    if (res == 0) {
        // connected immediately
        fcntl(s, F_SETFL, flags);
        close(s);
        return 0;
    }

    if (errno != EINPROGRESS) {
        close(s);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    res = select(s+1, NULL, &wfds, NULL, &tv);
    if (res <= 0) {
        // timeout or error
        close(s);
        if (res == 0) return 1; // indicate timeout
        return -1;
    }

    // check socket error
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
        close(s);
        return -1;
    }

    close(s);
    if (so_error == 0) return 0;
    if (so_error == EINPROGRESS) return 1;
    return -1;
}

static port_status_t scan_port(int port) {
    if (!target_ai) return ST_ERROR;
    struct addrinfo *ai;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    // Try each resolved addr for the target. If any connects -> OPEN
    for (ai = target_ai; ai != NULL; ai = ai->ai_next) {
        // copy sockaddr and set port
        if (ai->ai_family == AF_INET) {
            struct sockaddr_in sa4;
            memcpy(&sa4, ai->ai_addr, sizeof(sa4));
            sa4.sin_port = htons((uint16_t)port);
            int r = try_connect_addr((struct sockaddr*)&sa4, sizeof(sa4), CONNECT_TIMEOUT_MS);
            if (r == 0) return ST_OPEN;
            if (r == 1) continue; // timeout -> treat as filtered/timeouts; continue trying other addrs
        } else if (ai->ai_family == AF_INET6) {
            struct sockaddr_in6 sa6;
            memcpy(&sa6, ai->ai_addr, sizeof(sa6));
            sa6.sin6_port = htons((uint16_t)port);
            int r = try_connect_addr((struct sockaddr*)&sa6, sizeof(sa6), CONNECT_TIMEOUT_MS);
            if (r == 0) return ST_OPEN;
            if (r == 1) continue;
        }
    }

    // If none succeeded, best guess: closed or timeout. We check the last try: if it timed out, mark TIMEOUT.
    // Simpler logic: we treated timeouts as non-zero; we'll mark CLOSED for now if no open found.
    return ST_CLOSED;
}

static void *worker_thread(void *arg) {
    (void)arg;
    while (!atomic_load(&stop_flag)) {
        int p = atomic_fetch_add(&next_port, 1);
        if (p >= total_ports) break;
        int portnum = results[p].port;
        port_status_t st = scan_port(portnum);
        results[p].status = st;

        atomic_fetch_add(&ports_done, 1);

        // update UI: we keep it simple and let main thread redraw periodically;
        // but to keep UI responsive we can signal by drawing here (protected by mutex)
        pthread_mutex_lock(&draw_mutex);
        draw_ui_list(0);
        pthread_mutex_unlock(&draw_mutex);
    }
    return NULL;
}

static void draw_ui_header() {
    werase(win_header);
    box(win_header, 0, 0);

    mvwprintw(win_header, 1, 2, "Port Scanner UI (ncurses) - Target: %s", target_str);
    int done = atomic_load(&ports_done);
    mvwprintw(win_header, 2, 2, "Threads: %d  Progress: %d/%d", thread_count, done, total_ports);

    time_t now = time(NULL);
    int elapsed = (int)difftime(now, start_time);
    mvwprintw(win_header, 1, COLS - 30, "Elapsed: %02d:%02d", elapsed/60, elapsed%60);

    wrefresh(win_header);
}

static void draw_ui_list(int highlight_offset) {
    // highlight_offset is reserved for future use (scroll)
    werase(win_list);
    box(win_list, 0, 0);
    int rows, cols;
    getmaxyx(win_list, rows, cols);
    // header row
    mvwprintw(win_list, 1, 2, "%-8s %-10s", "Port", "Status");
    int startrow = 2;
    int visible = rows - startrow - 1;
    if (visible <= 0) return;

    // Show as many as fit; basic paged display from 0..total_ports
    int perpage = visible;
    static int page = 0;
    int pages = (total_ports + perpage - 1) / perpage;
    if (page >= pages) page = pages - 1;
    int base = page * perpage;

    for (int i = 0; i < perpage; ++i) {
        int idx = base + i;
        int y = startrow + 1 + i;
        if (idx >= total_ports) break;
        char status_s[16];
        switch (results[idx].status) {
            case ST_UNKNOWN: strcpy(status_s, "PENDING"); break;
            case ST_OPEN: strcpy(status_s, "OPEN"); break;
            case ST_CLOSED: strcpy(status_s, "CLOSED"); break;
            case ST_TIMEOUT: strcpy(status_s, "TIMEOUT"); break;
            case ST_ERROR: strcpy(status_s, "ERROR"); break;
            default: strcpy(status_s, "UNKNOWN"); break;
        }
        mvwprintw(win_list, y, 2, "%-8d %-10s", results[idx].port, status_s);
    }

    // footer showing page info
    mvwprintw(win_list, rows-2, 2, "Page %d/%d  (Use q to quit)", page+1, pages);
    wrefresh(win_list);
}

static void draw_ui_footer() {
    werase(win_footer);
    box(win_footer, 0, 0);
    mvwprintw(win_footer, 1, 2, "Controls: q = quit");
    mvwprintw(win_footer, 1, 25, "Ports done: %d/%d", atomic_load(&ports_done), total_ports);
    wrefresh(win_footer);
}

int main(void) {
    signal(SIGINT, sigint_handler);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    int h = LINES;
    int w = COLS;

    int header_h = 4;
    int footer_h = 3;
    int list_h = h - header_h - footer_h;

    win_header = newwin(header_h, w, 0, 0);
    win_list = newwin(list_h, w, header_h, 0);
    win_footer = newwin(footer_h, w, header_h + list_h, 0);

    // Input UI: ask for target and optional range
    mvwprintw(win_header, 1, 2, "Enter target hostname or IP: ");
    wrefresh(win_header);

    echo();
    char input_target[256] = {0};
    wgetnstr(win_header, input_target, sizeof(input_target)-1);
    noecho();
    if (strlen(input_target) == 0) {
        mvwprintw(win_header, 2, 2, "No target entered. Exiting...");
        wrefresh(win_header);
        sleep(2);
        cleanup_and_exit(1);
    }
    strncpy(target_str, input_target, sizeof(target_str)-1);

    // Ask for port range
    mvwprintw(win_header, 2, 2, "Enter port range (start-end) or leave empty for 1-1024: ");
    wrefresh(win_header);
    echo();
    char range_input[64] = {0};
    wgetnstr(win_header, range_input, sizeof(range_input)-1);
    noecho();

    int start_port = DEFAULT_START_PORT, end_port = DEFAULT_END_PORT;
    if (strlen(range_input) > 0) {
        if (sscanf(range_input, "%d-%d", &start_port, &end_port) != 2) {
            start_port = DEFAULT_START_PORT;
            end_port = DEFAULT_END_PORT;
        }
    }
    if (start_port < 1) start_port = 1;
    if (end_port > 65535) end_port = 65535;
    if (end_port < start_port) { int t = start_port; start_port = end_port; end_port = t; }

    // Ask for thread count
    mvwprintw(win_header, 3, 2, "Concurrent threads (suggest 10-200) [default 50]: ");
    wrefresh(win_header);
    echo();
    char threads_input[16] = {0};
    wgetnstr(win_header, threads_input, sizeof(threads_input)-1);
    noecho();
    if (strlen(threads_input) > 0) {
        int tc = atoi(threads_input);
        if (tc < 1) tc = 1;
        if (tc > MAX_THREADS) tc = MAX_THREADS;
        thread_count = tc;
    }

    // Resolve target
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;

    int gaierr = getaddrinfo(target_str, NULL, &hints, &target_ai);
    if (gaierr != 0) {
        mvwprintw(win_header, 1, 2, "getaddrinfo error: %s", gai_strerror(gaierr));
        wrefresh(win_header);
        sleep(2);
        cleanup_and_exit(1);
    }

    // prepare results
    total_ports = end_port - start_port + 1;
    results = calloc(total_ports, sizeof(port_result_t));
    if (!results) {
        mvwprintw(win_header, 1, 2, "Allocation failed.");
        wrefresh(win_header);
        sleep(2);
        cleanup_and_exit(1);
    }
    for (int i = 0; i < total_ports; ++i) {
        results[i].port = start_port + i;
        results[i].status = ST_UNKNOWN;
    }

    atomic_store(&next_port, 0);
    atomic_store(&ports_done, 0);
    atomic_store(&stop_flag, 0);

    // start threads
    threads = malloc(sizeof(pthread_t) * thread_count);
    if (!threads) {
        mvwprintw(win_header, 1, 2, "Thread allocation failed.");
        wrefresh(win_header);
        sleep(2);
        cleanup_and_exit(1);
    }

    start_time = time(NULL);

    // initial draw
    pthread_mutex_lock(&draw_mutex);
    draw_ui_header();
    draw_ui_list(0);
    draw_ui_footer();
    pthread_mutex_unlock(&draw_mutex);

    for (int i = 0; i < thread_count; ++i) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // main loop: accept 'q' to quit, and periodically redraw UI
    nodelay(stdscr, TRUE);
    curs_set(0);

    while (1) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            atomic_store(&stop_flag, 1);
            break;
        }
        // if all done, break
        if (atomic_load(&ports_done) >= total_ports) break;

        // redraw UI periodically
        pthread_mutex_lock(&draw_mutex);
        draw_ui_header();
        draw_ui_list(0);
        draw_ui_footer();
        pthread_mutex_unlock(&draw_mutex);

        struct timespec ts = {0, 200 * 1000000}; // 200 ms
        nanosleep(&ts, NULL);
    }

    // wait for threads to finish
    for (int i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    // final draw
    pthread_mutex_lock(&draw_mutex);
    draw_ui_header();
    draw_ui_list(0);
    draw_ui_footer();
    pthread_mutex_unlock(&draw_mutex);

    // show summary in footer for a moment
    int open_count = 0;
    for (int i = 0; i < total_ports; ++i) if (results[i].status == ST_OPEN) open_count++;

    mvwprintw(win_footer, 1, 40, "Open ports: %d", open_count);
    wrefresh(win_footer);

    // wait for user key to exit
    nodelay(stdscr, FALSE);
    mvwprintw(win_footer, 1, 2, "Scan complete. Press any key to exit.");
    wrefresh(win_footer);
    getch();

    cleanup_and_exit(0);
    return 0;
}

