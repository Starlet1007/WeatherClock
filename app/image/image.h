#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>

typedef struct
{
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
} image_t;

extern const image_t image_xiaoxing;
extern const image_t image_error;
extern const image_t image_wifi;
extern const image_t icon_wifi;
extern const image_t icon_sunny;
extern const image_t icon_cloudy;
extern const image_t icon_thunder;
extern const image_t icon_rainy;
extern const image_t icon_snowy;
extern const image_t icon_windy;
extern const image_t icon_NA;

#endif /* __IMAGE_H__ */
