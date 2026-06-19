#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

typedef struct
{
    const char *name;
    const uint8_t *model;
} font_chinese_t;

typedef struct
{
    const uint8_t *ascii_model;
    const char *ascii_map;
    const font_chinese_t *chinese;
    uint16_t size;
} font_t;

extern const font_t font_16;
extern const font_t font_20;
extern const font_t font_32;
extern const font_t font_36;
extern const font_t font_64;

#endif /* __FONT_H */
