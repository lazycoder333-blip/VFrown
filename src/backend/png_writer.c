#include "png_writer.h"

#include <stdint.h>

#define PNG_COLOR_TYPE_RGBA 6

static void Png_WriteU32(FILE* f, uint32_t value) {
  unsigned char bytes[4];
  bytes[0] = (unsigned char)((value >> 24) & 0xff);
  bytes[1] = (unsigned char)((value >> 16) & 0xff);
  bytes[2] = (unsigned char)((value >> 8) & 0xff);
  bytes[3] = (unsigned char)(value & 0xff);
  fwrite(bytes, 1, 4, f);
}

static uint32_t Png_Crc32(const unsigned char* data, size_t length) {
  static uint32_t table[256];
  static bool tableInit = false;
  if (!tableInit) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (uint32_t k = 0; k < 8; k++) {
        if (c & 1) {
          c = 0xedb88320u ^ (c >> 1);
        } else {
          c >>= 1;
        }
      }
      table[i] = c;
    }
    tableInit = true;
  }

  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; i++) {
    crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
  }
  return crc ^ 0xffffffffu;
}

static uint32_t Png_Adler32(const unsigned char* data, size_t length) {
  const uint32_t mod = 65521;
  uint32_t a = 1;
  uint32_t b = 0;
  for (size_t i = 0; i < length; i++) {
    a = (a + data[i]) % mod;
    b = (b + a) % mod;
  }
  return (b << 16) | a;
}

static bool Png_WriteChunk(FILE* f, const char* type, const unsigned char* data, uint32_t length) {
  unsigned char typeBytes[4];
  memcpy(typeBytes, type, 4);

  Png_WriteU32(f, length);
  fwrite(typeBytes, 1, 4, f);
  if (length > 0 && data) {
    fwrite(data, 1, length, f);
  }

  uint32_t crcLen = 4 + length;
  unsigned char* crcData = (unsigned char*)malloc(crcLen);
  if (!crcData) {
    return false;
  }

  memcpy(crcData, typeBytes, 4);
  if (length > 0 && data) {
    memcpy(crcData + 4, data, length);
  }

  uint32_t crc = Png_Crc32(crcData, crcLen);
  free(crcData);
  Png_WriteU32(f, crc);
  return true;
}

static bool Png_BuildZlib(const unsigned char* data, size_t dataLen, unsigned char** outData, size_t* outLen) {
  size_t blockCount = (dataLen + 65535u - 1u) / 65535u;
  size_t zlibSize = 2 + blockCount * 5 + dataLen + 4;
  unsigned char* zlib = (unsigned char*)malloc(zlibSize);
  if (!zlib) {
    return false;
  }

  unsigned char* p = zlib;
  *p++ = 0x78;
  *p++ = 0x01;

  size_t remaining = dataLen;
  size_t offset = 0;
  while (remaining > 0) {
    uint16_t blockLen = (remaining > 65535u) ? 65535u : (uint16_t)remaining;
    unsigned char finalBlock = (remaining <= 65535u) ? 1 : 0;
    *p++ = finalBlock;
    *p++ = (unsigned char)(blockLen & 0xff);
    *p++ = (unsigned char)((blockLen >> 8) & 0xff);
    uint16_t nlen = (uint16_t)~blockLen;
    *p++ = (unsigned char)(nlen & 0xff);
    *p++ = (unsigned char)((nlen >> 8) & 0xff);
    memcpy(p, data + offset, blockLen);
    p += blockLen;
    offset += blockLen;
    remaining -= blockLen;
  }

  uint32_t adler = Png_Adler32(data, dataLen);
  *p++ = (unsigned char)((adler >> 24) & 0xff);
  *p++ = (unsigned char)((adler >> 16) & 0xff);
  *p++ = (unsigned char)((adler >> 8) & 0xff);
  *p++ = (unsigned char)(adler & 0xff);

  *outData = zlib;
  *outLen = (size_t)(p - zlib);
  return true;
}

bool Png_WriteRGBA(const char* path, int width, int height, const uint32_t* pixels) {
  if (!path || !pixels || width <= 0 || height <= 0) {
    return false;
  }

  size_t rowStride = (size_t)width * 4;
  size_t rawStride = rowStride + 1;
  size_t rawSize = (size_t)height * rawStride;

  unsigned char* raw = (unsigned char*)malloc(rawSize);
  if (!raw) {
    return false;
  }

  for (int y = 0; y < height; y++) {
    size_t rowOffset = (size_t)y * rawStride;
    raw[rowOffset] = 0;
    size_t pixelOffset = rowOffset + 1;
    for (int x = 0; x < width; x++) {
      uint32_t color = pixels[(size_t)y * width + x];
      raw[pixelOffset++] = (unsigned char)(color & 0xff);
      raw[pixelOffset++] = (unsigned char)((color >> 8) & 0xff);
      raw[pixelOffset++] = (unsigned char)((color >> 16) & 0xff);
      raw[pixelOffset++] = (unsigned char)((color >> 24) & 0xff);
    }
  }

  unsigned char* zlib = NULL;
  size_t zlibLen = 0;
  bool ok = Png_BuildZlib(raw, rawSize, &zlib, &zlibLen);
  free(raw);
  if (!ok || !zlib) {
    free(zlib);
    return false;
  }

  FILE* f = fopen(path, "wb");
  if (!f) {
    free(zlib);
    return false;
  }

  static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
  fwrite(signature, 1, sizeof(signature), f);

  unsigned char ihdr[13];
  ihdr[0] = (unsigned char)((width >> 24) & 0xff);
  ihdr[1] = (unsigned char)((width >> 16) & 0xff);
  ihdr[2] = (unsigned char)((width >> 8) & 0xff);
  ihdr[3] = (unsigned char)(width & 0xff);
  ihdr[4] = (unsigned char)((height >> 24) & 0xff);
  ihdr[5] = (unsigned char)((height >> 16) & 0xff);
  ihdr[6] = (unsigned char)((height >> 8) & 0xff);
  ihdr[7] = (unsigned char)(height & 0xff);
  ihdr[8] = 8;
  ihdr[9] = PNG_COLOR_TYPE_RGBA;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;

  ok = Png_WriteChunk(f, "IHDR", ihdr, sizeof(ihdr));
  if (ok) {
    ok = Png_WriteChunk(f, "IDAT", zlib, (uint32_t)zlibLen);
  }
  if (ok) {
    ok = Png_WriteChunk(f, "IEND", NULL, 0);
  }

  fclose(f);
  free(zlib);
  return ok;
}
