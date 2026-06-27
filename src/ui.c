// ============================================================================
// ui.c — Terminal User Interface (platform-independent)
// ============================================================================
// All rendering goes through the platform abstraction layer so the same
// code works on both Windows (Win32 Console API) and Linux (ANSI escapes).
// ============================================================================

#include "ui.h"
#include "utils.h"
#include "scanner.h"

// ============================================================================
// GLOBALS
// ============================================================================

plat_mutex_t draw_mutex;
int filter_open = 0;
int scroll_off = 0;

int con_rows;
int con_cols;
int inner_w;

int hdr_h = 3;
int list_hdr_h = 1;
int info_h = 1;
int ftr_h = 1;
int list_items_h;

int hdr_y0;
int div1_y;
int list_y0;
int list_data_y0;
int info_y;
int div2_y;
int ftr_y0;
int div3_y;
int bot_y;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static int open_port_at(int n) {
    int c = 0;
    for (int i = 0; i < total_ports; i++)
        if (results[i].status == ST_OPEN && c++ == n)
            return i;
    return -1;
}

static int vis_total(void) {
    return filter_open ? (int)atomic_load(&open_count) : total_ports;
}

static color_t status_color(port_status_t st) {
    switch (st) {
        case ST_OPEN:   return C_OPEN;
        case ST_CLOSED: return C_CLOSED;
        default:        return C_PENDING;
    }
}

static const char *status_label(port_status_t st) {
    switch (st) {
        case ST_OPEN:   return "OPEN";
        case ST_CLOSED: return "CLOSED";
        default:        return "PENDING";
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ui_init(void) {
    plat_enter_ui();
    plat_mutex_init(&draw_mutex);
    ui_calc_layout();
}

void ui_cleanup(void) {
    plat_mutex_destroy(&draw_mutex);
    plat_exit_ui();
}

// ============================================================================
// LAYOUT
// ============================================================================

void ui_calc_layout(void) {
    plat_get_term_size(&con_rows, &con_cols);
    inner_w = con_cols - 2;

    list_items_h = con_rows - hdr_h - list_hdr_h - info_h - ftr_h - 5;
    if (list_items_h < 1) list_items_h = 1;

    hdr_y0       = 1;
    div1_y       = hdr_y0 + hdr_h;
    list_y0      = div1_y + 1;
    list_data_y0 = list_y0 + list_hdr_h;
    info_y       = list_data_y0 + list_items_h;
    div2_y       = info_y + info_h;
    ftr_y0       = div2_y + 1;
    div3_y       = ftr_y0 + ftr_h;
    bot_y        = div3_y + 1;
}

// ============================================================================
// FRAME (borders)
// ============================================================================

static void draw_hline(int y, const char *left, const char *mid, const char *right) {
    char buf[1024];
    int n = 0;
    while (left[n])  buf[n] = left[n], n++;
    for (int i = 0; i < inner_w; i++) buf[n++] = mid[0];
    int r = 0;
    while (right[r]) buf[n++] = right[r], r++;
    buf[n] = 0;
    plat_puts(y, 0, C_DEFAULT, buf);
}

static void draw_row_bg(int y) {
    char buf[1024];
    int n = 0;
    const char *v = BOX_V;
    int vlen = (int)strlen(v);
    for (int j = 0; j < vlen; j++) buf[n++] = v[j];
    for (int i = 0; i < inner_w; i++) buf[n++] = ' ';
    for (int j = 0; j < vlen; j++) buf[n++] = v[j];
    buf[n] = 0;
    plat_puts(y, 0, C_DEFAULT, buf);
}

void ui_draw_frame(void) {
    plat_clear_rect(0, 0, con_rows, con_cols);
    draw_hline(0,     BOX_TL, BOX_H, BOX_TR);
    for (int y = 1; y < bot_y; y++) draw_row_bg(y);
    draw_hline(div1_y, BOX_TLJ, BOX_H, BOX_TRJ);
    draw_hline(div2_y, BOX_TLJ, BOX_H, BOX_TRJ);
    draw_hline(div3_y, BOX_TLJ, BOX_H, BOX_TRJ);
    draw_hline(bot_y,  BOX_BL,  BOX_H, BOX_BR);
}

// ============================================================================
// HEADER
// ============================================================================

static void draw_progress_bar(int y, int bar_w, int done, int total) {
    if (bar_w < 4) bar_w = 4;
    int fill = total > 0 ? (done * bar_w) / total : 0;
    char buf[1024];
    int n = 0;
    for (int i = 0; i < bar_w; i++) {
        const char *ch = i < fill ? BAR_FILL : BAR_EMPTY;
        int len = (int)strlen(ch);
        for (int j = 0; j < len; j++) buf[n++] = ch[j];
    }
    buf[n] = 0;
    plat_puts(y, 2, C_PROGRESS, buf);
    plat_printf(y, 2 + (bar_w * (int)strlen(BAR_FILL)) + 1, C_DIMTEXT,
                "%3d%%", total > 0 ? done * 100 / total : 0);
}

static void update_speed(void) {
    time_t now = time(NULL);
    int d = (int)atomic_load(&ports_done);
    double elapsed = difftime(now, last_speed_time);
    if (elapsed >= 1.0) {
        double raw = (d - last_speed_done) / elapsed;
        if (smooth_speed == 0.0)
            smooth_speed = raw;
        else
            smooth_speed = 0.7 * smooth_speed + 0.3 * raw;
        last_speed_time = now;
        last_speed_done = d;
    }
}

void ui_draw_header(void) {
    plat_clear_rect(hdr_y0, 1, hdr_h, inner_w);

    int d = (int)atomic_load(&ports_done);
    int t = total_ports;

    int secs = (int)difftime(time(NULL), start_time);
    int mins = secs / 60; secs %= 60;

    plat_printf(hdr_y0, 2, C_TITLE, "TCPeek  |  %s", target_str);
    plat_printf(hdr_y0, con_cols - 16, C_DIMTEXT, "Time  %02d:%02d", mins, secs);

    char buf_done[64], buf_total[64];
    fmt_int(buf_done,  d, sizeof(buf_done));
    fmt_int(buf_total, t, sizeof(buf_total));

    int oc = (int)atomic_load(&open_count);
    plat_printf(hdr_y0 + 1, 2,  C_DIMTEXT, "Threads: %d", thread_count);
    plat_printf(hdr_y0 + 1, 14, C_DEFAULT,  "%s / %s", buf_done, buf_total);

    int col_x = 14 + (int)strlen(buf_done) + (int)strlen(buf_total) + 3;
    plat_printf(hdr_y0 + 1, col_x, C_DIMTEXT, "Open:");
    plat_printf(hdr_y0 + 1, col_x + 5, oc > 0 ? C_OPEN : C_DIMTEXT, "%d", oc);

    update_speed();
    plat_printf(hdr_y0 + 1, con_cols - 28, C_DIMTEXT, "Speed: %.0f/s", smooth_speed);

    if (atomic_load(&pause_flag)) {
        plat_printf(hdr_y0 + 1, con_cols - 12, C_PAUSED, " [PAUSED]");
    } else if (atomic_load(&stop_flag)) {
        plat_printf(hdr_y0 + 1, con_cols - 12, C_CLOSED, " STOPPING");
    } else if (smooth_speed > 0.0) {
        int rem = t - d;
        int eta_secs = (int)((double)rem / smooth_speed);
        plat_printf(hdr_y0 + 1, con_cols - 18, C_DIMTEXT,
                    "ETA %02d:%02d", eta_secs / 60, eta_secs % 60);
    }

    int bar_w = inner_w - 10;
    if (bar_w < 5) bar_w = 5;
    draw_progress_bar(hdr_y0 + 2, bar_w, d, t);
}

// ============================================================================
// LIST
// ============================================================================

void ui_draw_list(void) {
    plat_clear_rect(list_y0, 1, (bot_y - 1) - list_y0, inner_w);

    plat_printf(list_y0, 2, C_BOLD, "PORT      STATUS     SERVICE");

    int total_vis = vis_total();
    if (total_vis == 0) {
        const char *msg = filter_open ? "No open ports found" : "No ports to scan";
        plat_puts(list_data_y0, 2, C_DIMTEXT, msg);
        return;
    }

    if (scroll_off + list_items_h > total_vis)
        scroll_off = total_vis - list_items_h;
    if (scroll_off < 0) scroll_off = 0;

    for (int i = 0; i < list_items_h; i++) {
        int idx = scroll_off + i;
        if (idx >= total_vis) break;
        int ri = filter_open ? open_port_at(idx) : idx;
        if (ri < 0) break;
        int y = list_data_y0 + i;
        color_t attr = status_color(results[ri].status);
        const char *svc = get_service(results[ri].port);
        plat_printf(y, 2, attr, "%-6d  %-7s  %s",
                    results[ri].port, status_label(results[ri].status), svc);
    }

    char idx_fmt[64];
    fmt_int(idx_fmt, total_vis, sizeof(idx_fmt));
    int end_i = scroll_off + list_items_h > total_vis ? total_vis : scroll_off + list_items_h;

    if (filter_open)
        plat_printf(info_y, 2, C_DIMTEXT, "%d-%d of %s  [FILTER: OPEN ONLY]",
                    scroll_off + 1, end_i, idx_fmt);
    else
        plat_printf(info_y, 2, C_DIMTEXT, "%d-%d of %s",
                    scroll_off + 1, end_i, idx_fmt);

    int pct = total_vis > 0 ? (scroll_off * 100) / total_vis : 0;
    plat_printf(info_y, con_cols - 8, C_DIMTEXT, "%d%%", pct);
}

// ============================================================================
// FOOTER
// ============================================================================

void ui_draw_footer(void) {
    plat_clear_rect(ftr_y0, 1, ftr_h, inner_w);
    plat_puts(ftr_y0, 2, C_DIMTEXT,
        "[Q]uit  [\x18\x19]Scroll  [\x1b\x1a]Page  [Home][End]  "
        "[O]Filter  [S]ave  [P]ause");
}

// ============================================================================
// REDRAW ALL
// ============================================================================

void ui_redraw_all(void) {
    ui_draw_frame();
    ui_draw_header();
    ui_draw_list();
    ui_draw_footer();
}

// ============================================================================
// INPUT
// ============================================================================

int ui_get_key(void) {
    if (!plat_kbhit()) return -1;
    int c = plat_getch();
    if (c == 3) atomic_store(&stop_flag, 1);
    return c;
}

void ui_read_line(int y, int x, char *buf, int max) {
    plat_clear_rect(y, x, 1, con_cols - x);
    int pos = 0;
    buf[0] = 0;
    while (1) {
        plat_printf(y, x, C_DEFAULT, "%s%*c", buf, max - (int)strlen(buf) - 1, ' ');
        int ch = plat_getch();
        if (ch == '\r' || ch == '\n') break;
        if (ch == 3)  { buf[0] = 0; return; }
        if (ch == '\b' || ch == 127) {   // backspace on both platforms
            if (pos > 0) { pos--; buf[pos] = 0; }
        }
        else if (ch >= 32 && ch < 127 && pos < max - 1) {
            buf[pos++] = (char)ch;
            buf[pos] = 0;
        }
    }
}
