// ============================================================================
// main.c — Program entry point and top-level orchestration
// ============================================================================

#include "config.h"
#include "scanner.h"
#include "ui.h"
#include "utils.h"

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

static void handle_sigint(int signo) {
    (void)signo;
    atomic_store(&stop_flag, 1);
}

// ============================================================================
// SAVE RESULTS
// ============================================================================

static void save_results(void) {
    char fname[512];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(fname, sizeof(fname), "portscan_%Y%m%d_%H%M%S.txt", tm);

    FILE *fp = fopen(fname, "w");
    if (!fp) return;

    fprintf(fp, "Port Scan Results for %s\n", target_str);
    fprintf(fp, "Scanned: %d ports\n\n", total_ports);
    fprintf(fp, "%-6s  %-7s  %s\n", "PORT", "STATUS", "SERVICE");
    fprintf(fp, "------  -------  -------\n");

    int oc = 0;
    for (int i = 0; i < total_ports; i++) {
        if (results[i].status == ST_OPEN) {
            fprintf(fp, "  %-4d  OPEN     %s\n",
                    results[i].port, get_service(results[i].port));
            oc++;
        }
    }

    fprintf(fp, "\n%d open ports found.\n", oc);
    fclose(fp);

    plat_mutex_lock(&draw_mutex);
    plat_printf(ftr_y0, 2, C_OPEN, "Saved to %s", fname);
    plat_mutex_unlock(&draw_mutex);

    plat_sleep(1500);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    // --- Initialise modules ---
    if (scanner_init() != 0) {
        fprintf(stderr, "Failed to initialise networking.\n");
        return 1;
    }

    ui_init();
    signal(SIGINT, handle_sigint);

    // ========================================================================
    // PROMPT: target hostname
    // ========================================================================
    char input[256] = {0};
    plat_clear_rect(0, 0, 5, con_cols);
    plat_puts(1, 0, C_DIMTEXT, "Enter target hostname or IP: ");
    ui_read_line(1, 30, input, sizeof(input));

    if (strlen(input) == 0) {
        plat_puts(2, 0, C_CLOSED, "No target specified.");
        plat_sleep(1500);
        ui_cleanup();
        scanner_cleanup();
        return 1;
    }
    strncpy(target_str, input, sizeof(target_str) - 1);

    // ========================================================================
    // PROMPT: port range
    // ========================================================================
    int sp = DEFAULT_START, ep = DEFAULT_END;
    plat_clear_rect(0, 0, 5, con_cols);
    plat_puts(1, 0, C_DIMTEXT, "Port range [1-65535, e.g. 1-1000] (default 1-1024): ");

    char rbuf[64] = {0};
    ui_read_line(1, 52, rbuf, sizeof(rbuf));
    if (strlen(rbuf) > 0) sscanf(rbuf, "%d-%d", &sp, &ep);

    if (sp < 1)     sp = 1;
    if (ep > 65535) ep = 65535;
    if (ep < sp)   { int t = sp; sp = ep; ep = t; }

    // ========================================================================
    // PROMPT: thread count
    // ========================================================================
    plat_clear_rect(0, 0, 5, con_cols);
    plat_printf(1, 0, C_DIMTEXT, "Threads [1-%d] (default 50): ", MAX_THREADS);

    char tbuf[16] = {0};
    ui_read_line(1, 30, tbuf, sizeof(tbuf));
    if (strlen(tbuf) > 0) {
        int tc = atoi(tbuf);
        if (tc < 1)        tc = 1;
        if (tc > MAX_THREADS) tc = MAX_THREADS;
        thread_count = tc;
    }

    // ========================================================================
    // RESOLVE TARGET
    // ========================================================================
    int gerr = scanner_resolve(target_str);
    if (gerr != 0) {
        plat_clear_rect(0, 0, 3, con_cols);
        plat_printf(1, 0, C_CLOSED, "DNS resolution failed: %s",
                    gai_strerror(gerr));
        plat_sleep(2000);
        ui_cleanup();
        scanner_cleanup();
        return 1;
    }

    // ========================================================================
    // SET UP SCAN
    // ========================================================================
    scanner_setup_range(sp, ep);

    // ========================================================================
    // DRAW INITIAL UI
    // ========================================================================
    ui_calc_layout();
    plat_mutex_lock(&draw_mutex);
    ui_redraw_all();
    plat_mutex_unlock(&draw_mutex);

    // ========================================================================
    // LAUNCH WORKERS
    // ========================================================================
    scanner_start_workers();

    // ========================================================================
    // MAIN EVENT LOOP
    // ========================================================================
    int prev_rows = 0, prev_cols = 0;

    while (1) {
        int ch = ui_get_key();

        ui_calc_layout();
        if (con_rows != prev_rows || con_cols != prev_cols) {
            plat_mutex_lock(&draw_mutex);
            ui_redraw_all();
            plat_mutex_unlock(&draw_mutex);
            prev_rows = con_rows;
            prev_cols = con_cols;
        }

        if (ch == 'q' || ch == 'Q' || ch == K_ESC) {
            atomic_store(&stop_flag, 1);
            break;
        }

        if (ch == 'p' || ch == 'P') {
            int new_val = !atomic_load(&pause_flag);
            atomic_store(&pause_flag, new_val);
            plat_mutex_lock(&draw_mutex);
            ui_draw_header();
            ui_draw_footer();
            plat_mutex_unlock(&draw_mutex);
        }

        int total_vis = filter_open
                        ? (int)atomic_load(&open_count)
                        : total_ports;
        int page = list_items_h > 1 ? list_items_h : 1;

        switch (ch) {
            case K_DOWN:
                if (scroll_off + 1 < total_vis) {
                    scroll_off++;
                    plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                }
                break;
            case K_UP:
                if (scroll_off > 0) {
                    scroll_off--;
                    plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                }
                break;
            case K_PGDN:
                scroll_off += page;
                if (scroll_off >= total_vis)
                    scroll_off = total_vis > 0 ? total_vis - 1 : 0;
                plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                break;
            case K_PGUP:
                scroll_off -= page;
                if (scroll_off < 0) scroll_off = 0;
                plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                break;
            case K_HOME:
                scroll_off = 0;
                plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                break;
            case K_END:
                scroll_off = total_vis > 1 ? total_vis - 1 : 0;
                plat_mutex_lock(&draw_mutex); ui_draw_list(); plat_mutex_unlock(&draw_mutex);
                break;
            case 'o': case 'O':
                filter_open = !filter_open;
                scroll_off = 0;
                plat_mutex_lock(&draw_mutex); ui_draw_list(); ui_draw_footer(); plat_mutex_unlock(&draw_mutex);
                break;
            case 's': case 'S':
                save_results();
                break;
        }

        if (atomic_load(&ports_done) >= total_ports) break;

        plat_mutex_lock(&draw_mutex);
        ui_draw_header();
        ui_draw_list();
        ui_draw_footer();
        plat_mutex_unlock(&draw_mutex);

        plat_sleep(UI_REFRESH_MS);
    }

    // ========================================================================
    // WAIT & EXIT
    // ========================================================================
    scanner_wait_for_completion();

    ui_calc_layout();
    plat_mutex_lock(&draw_mutex);
    ui_redraw_all();
    int oc = (int)atomic_load(&open_count);
    if (oc > 0)
        plat_printf(ftr_y0, 2, C_OPEN,
                    "Scan complete! %d open port(s) found. Press any key to exit.", oc);
    else
        plat_puts(ftr_y0, 2, C_DIMTEXT,
                  "Scan complete. No open ports found. Press any key to exit.");
    plat_mutex_unlock(&draw_mutex);

    while (plat_kbhit()) plat_getch();
    plat_getch();

    scanner_free();
    ui_cleanup();
    scanner_cleanup();
    return 0;
}
