#ifndef STATIC_IMAGE_DATA_H_
#define STATIC_IMAGE_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include "model_settings.h"

extern const uint8_t g_static_images[][kImageElementCount];
extern const char* g_static_image_names[];
extern const size_t g_static_image_count;

#endif  // STATIC_IMAGE_DATA_H_
