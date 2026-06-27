// ============================================================================
// scanner.c — Multi-threaded TCP port scanning engine
// ============================================================================

#include "scanner.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

port_result_t *results = NULL;
int total_ports = 0;

atomic_int next_port;
atomic_int ports_done;
atomic_int stop_flag;
atomic_int pause_flag;
atomic_int open_count;

struct addrinfo *target_ai = NULL;
char target_str[256];

int thread_count = 50;

time_t start_time;
double smooth_speed = 0.0;
time_t last_speed_time;
int last_speed_done;

// Thread handles (opaque storage for plat_thread_t)
static plat_thread_t *thread_handles = NULL;
static int thread_handle_count = 0;

// ============================================================================
// INTERNAL: non-blocking connect
// ============================================================================

static int try_connect(struct sockaddr *sa, socklen_t sl, int ms) {
    int s = (int)socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return -1;

    // Set non-blocking
#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    int r = connect(s, sa, sl);
    if (r == 0) {
#ifdef _WIN32
        u_long block = 0;
        ioctlsocket(s, FIONBIO, &block);
#endif
        plat_socket_close(s);
        return 0;
    }

#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK) { plat_socket_close(s); return -1; }
#else
    if (errno != EINPROGRESS) { plat_socket_close(s); return -1; }
#endif

    fd_set wf;
    FD_ZERO(&wf);
    FD_SET(s, &wf);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };

#ifdef _WIN32
    r = select(0, NULL, &wf, NULL, &tv);
#else
    r = select(s + 1, NULL, &wf, NULL, &tv);
#endif

    if (r <= 0) {
        plat_socket_close(s);
        return r == 0 ? 1 : -1;
    }

    int err = 0;
    socklen_t el = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &el);
    plat_socket_close(s);
    return err == 0 ? 0 : -1;
}

// ============================================================================
// PUBLIC API
// ============================================================================

int scanner_init(void) {
    return plat_socket_init();
}

void scanner_cleanup(void) {
    plat_socket_cleanup();
}

int scanner_resolve(const char *hostname) {
    struct addrinfo hints = {0};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    return getaddrinfo(hostname, NULL, &hints, &target_ai);
}

void scanner_setup_range(int start, int end) {
    total_ports = end - start + 1;
    results = (port_result_t *)calloc((size_t)total_ports, sizeof(port_result_t));
    for (int i = 0; i < total_ports; i++) {
        results[i].port = start + i;
        results[i].status = ST_UNKNOWN;
    }

    atomic_store(&next_port, 0);
    atomic_store(&ports_done, 0);
    atomic_store(&stop_flag, 0);
    atomic_store(&pause_flag, 0);
    atomic_store(&open_count, 0);

    start_time = time(NULL);
    last_speed_time = start_time;
    last_speed_done = 0;
    smooth_speed = 0.0;
}

void scanner_start_workers(void) {
    thread_handles = (plat_thread_t *)malloc((size_t)thread_count * sizeof(plat_thread_t));
    thread_handle_count = thread_count;
    for (int i = 0; i < thread_count; i++)
        plat_thread_spawn(&thread_handles[i], worker_thread, NULL);
}

void scanner_wait_for_completion(void) {
    if (thread_handles && thread_handle_count > 0) {
        plat_thread_wait_all(thread_handles, thread_handle_count);
        thread_handle_count = 0;
    }
}

void scanner_free(void) {
    free(results);
    results = NULL;
    free(thread_handles);
    thread_handles = NULL;
}

// ============================================================================
// WORKER THREAD
// ============================================================================

void worker_thread(void *arg) {
    (void)arg;
    while (!atomic_load(&stop_flag)) {
        if (atomic_load(&pause_flag)) {
            plat_sleep(200);
            continue;
        }
        int p = atomic_fetch_add(&next_port, 1);
        if (p >= total_ports) break;
        results[p].status = scan_port(results[p].port);
        if (results[p].status == ST_OPEN)
            atomic_fetch_add(&open_count, 1);
        atomic_fetch_add(&ports_done, 1);
    }
}

// ============================================================================
// PORT SCANNING
// ============================================================================

port_status_t scan_port(int port) {
    if (!target_ai) return ST_CLOSED;
    for (struct addrinfo *ai = target_ai; ai; ai = ai->ai_next) {
        if (ai->ai_family == AF_INET) {
            struct sockaddr_in sa;
            memcpy(&sa, ai->ai_addr, sizeof(sa));
            sa.sin_port = htons((uint16_t)port);
            int r = try_connect((struct sockaddr *)&sa, sizeof(sa), TIMEOUT_MS);
            if (r == 0) return ST_OPEN;
        } else if (ai->ai_family == AF_INET6) {
            struct sockaddr_in6 sa;
            memcpy(&sa, ai->ai_addr, sizeof(sa));
            sa.sin6_port = htons((uint16_t)port);
            int r = try_connect((struct sockaddr *)&sa, sizeof(sa), TIMEOUT_MS);
            if (r == 0) return ST_OPEN;
        }
    }
    return ST_CLOSED;
}
