#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "rtc.h"
#include "ui.h"
#include "app.h"
#include "page.h"


static const uint16_t color_bg_white = mkcolor(240, 240, 240);
static const uint16_t color_bg_khaki = mkcolor(225, 212, 180);
static const uint16_t color_bg_blue = mkcolor(111, 134, 148);
static const uint16_t color_black = mkcolor(0, 0, 0);
static const uint16_t color_white = mkcolor(255, 255, 255);
static const uint16_t color_grey = mkcolor(85, 85, 85);

void main_page_display()
{
    rtc_date_time_t current_time;
    rtc_get_time(&current_time);

    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_white);
    do
    {
        ui_fill_color(0, 0, 239, 150, color_bg_white);
        ui_draw_image(15, 10, &icon_wifi);
        main_page_redraw_wifi_ssid( WIFI_SSID );

        main_page_redraw_time(&current_time);

        main_page_redraw_date(&current_time);
    } while (0);

    do
    {
        ui_fill_color(0, 150, 120, 319, color_bg_khaki);
        ui_write_string(40, 165, "室内", color_black, color_bg_khaki, &font_20);
        main_page_redraw_inner_temperature(25);
        ui_write_string(76, 198, "。", color_black, color_bg_khaki, &font_20);
        main_page_redraw_inner_humidity(48);
        ui_write_string(76, 286, "%", color_black, color_bg_khaki, &font_16);
    } while (0);

    do
    {
        ui_fill_color(120, 150, 239, 319, color_bg_blue);

        main_page_redraw_outdoor_city("西安");
        main_page_redraw_outdoor_weather_icon(0);
        main_page_redraw_outdoor_temperature(32.0f);
        ui_write_string(194, 258, "。", color_black, color_bg_blue, &font_20);
    } while (0);
}

void main_page_redraw_wifi_ssid(const char *ssid)
{
    if (ssid == NULL)
        return;
    uint16_t ssid_pixel = strlen(ssid) * (font_16.size / 2); 
    uint16_t right_margin = 220;                             
    uint16_t left_margin = 50;                               
    uint16_t start_point;                                    

    if (ssid_pixel < (right_margin - left_margin))
    {
        start_point = right_margin - ssid_pixel;
    }
    else
    {
        start_point = left_margin;
    }
    ui_fill_color(left_margin, 17, right_margin, 17 + font_16.size - 1, color_bg_white);
    ui_write_string(start_point, 17, (char *)ssid, color_grey, color_bg_white, &font_16);
}

void main_page_redraw_time(rtc_date_time_t *time) 
{
		main_page_redraw_hour(time->hour);
    main_page_redraw_colon(time->second); 
    main_page_redraw_minute(time->minute);
}

void main_page_redraw_date(rtc_date_time_t *date)
{
    char date_str[16];
    char week_str[16];
    const char *week_map[] = {"", "一", "二", "三", "四", "五", "六", "日"};

    snprintf(date_str, sizeof(date_str), "%04u.%02u.%02u", date->year, date->month, date->day);
    snprintf(week_str, sizeof(week_str), "星期%s", week_map[date->weekday]);

    ui_write_string(30, 120, date_str, color_grey, color_bg_white, &font_16);
    ui_write_string(165, 120, week_str, color_grey, color_bg_white, &font_16);
}


void main_page_redraw_colon(uint8_t second) // 专门控制心跳冒号
{
    if (second % 2 == 0) 
        ui_write_string(115, 45, ":", color_black, color_bg_white, &font_64);
    else 
        ui_write_string(115, 45, " ", color_black, color_bg_white, &font_64); 
}


void main_page_redraw_minute(uint8_t minute) // 专门刷新分钟
{
    char min_str[3];
    snprintf(min_str, sizeof(min_str), "%02u", minute);
    ui_write_string(150, 45, min_str, color_black, color_bg_white, &font_64);
}


void main_page_redraw_hour(uint8_t hour) // 专门刷新小时
{
    char hour_str[3];
    snprintf(hour_str, sizeof(hour_str), "%02u", hour);
    ui_write_string(30, 45, hour_str, color_black, color_bg_white, &font_64);
}


void main_page_redraw_inner_temperature(int temp) 
{
    char str[3] = {'-', '-', '\0'};

    if (temp >= -9 && temp <= 99) 
    {
        if (temp >= 0)
        {
            str[0] = (temp >= 10) ? ('0' + temp / 10) : ' '; 
            str[1] = '0' + (temp % 10);                      
        }
        else
        {
            str[0] = '-';
            str[1] = '0' + (-temp);
        }
    }
    ui_write_string(44, 210, str, color_black, color_bg_khaki, &font_32);
}

void main_page_redraw_inner_humidity(int humidity)
{
    char str[3] = {'-', '-', '\0'};

    if (humidity >= 0 && humidity <= 99)
    {
        str[0] = (humidity >= 10) ? ('0' + humidity / 10) : ' ';
        str[1] = '0' + (humidity % 10);
    }

    ui_fill_color(44, 270, 44 + 32, 270 + 32 - 1, color_bg_khaki);
    ui_write_string(44, 270, str, color_black, color_bg_khaki, &font_32);
}

void main_page_redraw_outdoor_city(const char *city)
{
    char str[9];
    snprintf(str, sizeof(str), "%s", city);
    ui_write_string(160, 165, str, color_black, color_bg_blue, &font_20);
}

void main_page_redraw_outdoor_temperature(float temperature)
{
    char str[3] = {'-', '-', '\0'};
    if (temperature > -10.0f && temperature <= 100.0f)
    {
        snprintf(str, sizeof(str), "%2.0f", temperature);
    }
    ui_write_string(164, 270, str, color_black, color_bg_blue, &font_32);
}

void main_page_redraw_outdoor_weather_icon(const int code)
{
    const image_t *icon = NULL;
    if (code == 0 || code == 1 || code == 2 || code == 3 || code == 38)
    {
        icon = &icon_sunny;
    }
    else if (code >= 4 && code <= 9)
    {
        icon = &icon_cloudy;
    }
    else if (code == 11 || code == 12)
    {
        icon = &icon_thunder;
    }
    else if (code == 10 || (code >= 13 && code <= 19))
    {
        icon = &icon_rainy;
    }
    else if (code >= 20 && code <= 25)
    {
        icon = &icon_snowy;
    }
    else if (code >= 26 && code <= 32)
    {
        icon = &icon_windy;
    }
    else
    {
        icon = &icon_NA;
    }
    ui_draw_image(150, 190, icon);
}
