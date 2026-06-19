#include "ui.h"

void welcome_page_display(void)
{
		const uint16_t color_bg = mkcolor(100, 126, 99);
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(0,0, &image_xiaoxing);
		ui_write_string(20, 235, "–°–«÷«ƒ‹ ±÷”", mkcolor(255, 255, 255), color_bg, &font_32);
		ui_write_string(150, 285, "loading...", mkcolor(255, 255, 255), color_bg, &font_16);
}
