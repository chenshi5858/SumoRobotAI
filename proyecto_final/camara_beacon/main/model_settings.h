#ifndef MODEL_SETTINGS_H_
#define MODEL_SETTINGS_H_

#include <stdint.h>

constexpr int kImageWidth = 96;
constexpr int kImageHeight = 96;
constexpr int kImageChannels = 1;
constexpr int kImageElementCount = kImageWidth * kImageHeight * kImageChannels;
constexpr int kClassCount = 2;

constexpr int kAbsentClass = 0;
constexpr int kPresentClass = 1;

const char* IdentifierClassName(int class_id);

#endif  // MODEL_SETTINGS_H_
