#include <stdint.h>
#include "ui.h"
#include "ui.h"

void wifi_page_display()
{
		const uint16_t color_bg = mkcolor(255, 255, 255);
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(0,0, &image_wifi);
		ui_write_string(55, 200, "[iPhone]", mkcolor(0, 0, 0), color_bg, &font_32);
		ui_write_string(70, 270, "Connecting...", mkcolor(0, 0, 0), color_bg, &font_16);
}
