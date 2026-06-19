#include <stdint.h>
#include "ui.h"
#include "ui.h"

void error_page_display(const char *msg)//统一的报错模板（各种问题导致的欢迎页面无法显示）
{
		const uint16_t color_bg = mkcolor(255, 255, 255);
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(0,0, &image_error);
		ui_write_string(40, 240, (char *)msg, mkcolor(0, 0, 0), color_bg, &font_16);
}
