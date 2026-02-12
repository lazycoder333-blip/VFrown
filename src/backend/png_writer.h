#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include "../common.h"

bool Png_WriteRGBA(const char* path, int width, int height, const uint32_t* pixels);

#endif // PNG_WRITER_H
