// ============================================================================
// ui.h — Terminal UI module interface
// ============================================================================

#ifndef UI_H
#define UI_H

#include "config.h"

void ui_init(void);
void ui_cleanup(void);
void ui_calc_layout(void);
void ui_draw_frame(void);
void ui_draw_header(void);
void ui_draw_list(void);
void ui_draw_footer(void);
void ui_redraw_all(void);

int  ui_get_key(void);
void ui_read_line(int y, int x, char *buf, int max);

#endif // UI_H
