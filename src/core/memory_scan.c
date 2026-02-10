#include "memory_scan.h"
#include "bus.h"

#define MAX_SCAN_RESULTS 65536

static uint32_t results[MAX_SCAN_RESULTS];
static int32_t resultCount = 0;
static bool scanActive = false;
static uint8_t* prevRam = NULL;
static uint32_t prevRamSize = 0;

static uint32_t ReadU16(const uint8_t* data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8);
}

static uint32_t ReadU32(const uint8_t* data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool ParseUint32(const char* text, bool hex, uint32_t* outValue) {
  if (!text || text[0] == '\0') return false;
  char* end = NULL;
  int base = hex ? 16 : 10;
  uint32_t value = (uint32_t)strtoul(text, &end, base);
  if (!end || end == text) return false;
  *outValue = value;
  return true;
}

static bool ParseInt32(const char* text, bool hex, int32_t* outValue) {
  if (!text || text[0] == '\0') return false;
  char* end = NULL;
  if (hex) {
    uint32_t value = (uint32_t)strtoul(text, &end, 16);
    if (!end || end == text) return false;
    *outValue = (int32_t)value;
    return true;
  }

  long value = strtol(text, &end, 10);
  if (!end || end == text) return false;
  *outValue = (int32_t)value;
  return true;
}

static uint32_t ToLowerAscii(uint32_t c) {
  if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
  return c;
}

static bool Utf8Decode(const char* text, size_t length, size_t* index, uint32_t* codepoint) {
  if (*index >= length) return false;

  uint8_t c0 = (uint8_t)text[*index];
  if (c0 < 0x80) {
    *codepoint = c0;
    (*index)++;
    return true;
  }

  if ((c0 & 0xE0) == 0xC0 && (*index + 1) < length) {
    uint8_t c1 = (uint8_t)text[*index + 1];
    if ((c1 & 0xC0) != 0x80) return false;
    *codepoint = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
    (*index) += 2;
    return true;
  }

  if ((c0 & 0xF0) == 0xE0 && (*index + 2) < length) {
    uint8_t c1 = (uint8_t)text[*index + 1];
    uint8_t c2 = (uint8_t)text[*index + 2];
    if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;
    *codepoint = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
    (*index) += 3;
    return true;
  }

  if ((c0 & 0xF8) == 0xF0 && (*index + 3) < length) {
    uint8_t c1 = (uint8_t)text[*index + 1];
    uint8_t c2 = (uint8_t)text[*index + 2];
    uint8_t c3 = (uint8_t)text[*index + 3];
    if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
    *codepoint = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
    (*index) += 4;
    return true;
  }

  return false;
}

static bool Utf8ToUtf16(const char* text, uint8_t** outBuffer, size_t* outSize, bool matchCase) {
  if (!text || !outBuffer || !outSize) return false;

  size_t length = strlen(text);
  if (length == 0) return false;

  size_t maxUnits = length * 2;
  uint8_t* buffer = malloc(maxUnits * 2);
  if (!buffer) return false;

  size_t outIndex = 0;
  size_t index = 0;
  while (index < length) {
    uint32_t cp = 0;
    if (!Utf8Decode(text, length, &index, &cp)) {
      free(buffer);
      return false;
    }

    if (!matchCase && cp <= 0x7F) {
      cp = ToLowerAscii(cp);
    }

    if (cp <= 0xFFFF) {
      buffer[outIndex++] = (uint8_t)(cp & 0xFF);
      buffer[outIndex++] = (uint8_t)((cp >> 8) & 0xFF);
    } else {
      cp -= 0x10000;
      uint16_t high = (uint16_t)(0xD800 | ((cp >> 10) & 0x3FF));
      uint16_t low = (uint16_t)(0xDC00 | (cp & 0x3FF));
      buffer[outIndex++] = (uint8_t)(high & 0xFF);
      buffer[outIndex++] = (uint8_t)((high >> 8) & 0xFF);
      buffer[outIndex++] = (uint8_t)(low & 0xFF);
      buffer[outIndex++] = (uint8_t)((low >> 8) & 0xFF);
    }
  }

  *outBuffer = buffer;
  *outSize = outIndex;
  return true;
}

static void ScanReset() {
  resultCount = 0;
  scanActive = false;
}

static bool EnsureSnapshotSize(uint32_t size) {
  if (size == 0) return false;
  if (prevRam && prevRamSize == size) return true;

  uint8_t* newBuffer = realloc(prevRam, size);
  if (!newBuffer) return false;
  prevRam = newBuffer;
  prevRamSize = size;
  return true;
}

static void CaptureSnapshot(const uint8_t* ram, uint32_t size) {
  if (!ram || size == 0) return;
  if (!EnsureSnapshotSize(size)) return;
  memcpy(prevRam, ram, size);
}

static bool IsChangeCompare(MemoryScanCompare_t compare) {
  return compare == MEMORY_SCAN_COMPARE_CHANGED ||
         compare == MEMORY_SCAN_COMPARE_UNCHANGED ||
         compare == MEMORY_SCAN_COMPARE_INCREASED ||
         compare == MEMORY_SCAN_COMPARE_DECREASED;
}

void MemoryScan_Reset() {
  ScanReset();
}

static bool CompareUnsigned(uint32_t current, uint32_t value, MemoryScanCompare_t compare) {
  switch (compare) {
  case MEMORY_SCAN_COMPARE_NOT_EQUAL: return current != value;
  case MEMORY_SCAN_COMPARE_GREATER: return current > value;
  case MEMORY_SCAN_COMPARE_LESS: return current < value;
  default: return current == value;
  }
}

static bool CompareSigned(int32_t current, int32_t value, MemoryScanCompare_t compare) {
  switch (compare) {
  case MEMORY_SCAN_COMPARE_NOT_EQUAL: return current != value;
  case MEMORY_SCAN_COMPARE_GREATER: return current > value;
  case MEMORY_SCAN_COMPARE_LESS: return current < value;
  default: return current == value;
  }
}

static bool CompareChangeUnsigned(uint32_t current, uint32_t previous, MemoryScanCompare_t compare) {
  switch (compare) {
  case MEMORY_SCAN_COMPARE_CHANGED: return current != previous;
  case MEMORY_SCAN_COMPARE_UNCHANGED: return current == previous;
  case MEMORY_SCAN_COMPARE_INCREASED: return current > previous;
  case MEMORY_SCAN_COMPARE_DECREASED: return current < previous;
  default: return current == previous;
  }
}

static bool CompareChangeSigned(int32_t current, int32_t previous, MemoryScanCompare_t compare) {
  switch (compare) {
  case MEMORY_SCAN_COMPARE_CHANGED: return current != previous;
  case MEMORY_SCAN_COMPARE_UNCHANGED: return current == previous;
  case MEMORY_SCAN_COMPARE_INCREASED: return current > previous;
  case MEMORY_SCAN_COMPARE_DECREASED: return current < previous;
  default: return current == previous;
  }
}

static void ScanInt8(const uint8_t* ram, size_t size, uint8_t value, bool signedMode, MemoryScanCompare_t compare) {
  resultCount = 0;
  for (size_t i = 0; i < size && resultCount < MAX_SCAN_RESULTS; i++) {
    bool match = signedMode
      ? CompareSigned((int8_t)ram[i], (int8_t)value, compare)
      : CompareUnsigned(ram[i], value, compare);
    if (match) results[resultCount++] = (uint32_t)i;
  }
}

static void ScanInt16(const uint8_t* ram, size_t size, uint16_t value, bool signedMode, MemoryScanCompare_t compare) {
  resultCount = 0;
  for (size_t i = 0; i + 1 < size && resultCount < MAX_SCAN_RESULTS; i++) {
    uint16_t current = (uint16_t)ReadU16(ram + i);
    bool match = signedMode
      ? CompareSigned((int16_t)current, (int16_t)value, compare)
      : CompareUnsigned(current, value, compare);
    if (match) results[resultCount++] = (uint32_t)i;
  }
}

static void ScanInt32(const uint8_t* ram, size_t size, uint32_t value, bool signedMode, MemoryScanCompare_t compare) {
  resultCount = 0;
  for (size_t i = 0; i + 3 < size && resultCount < MAX_SCAN_RESULTS; i++) {
    uint32_t current = ReadU32(ram + i);
    bool match = signedMode
      ? CompareSigned((int32_t)current, (int32_t)value, compare)
      : CompareUnsigned(current, value, compare);
    if (match) results[resultCount++] = (uint32_t)i;
  }
}

static void ScanAllInt8(size_t size) {
  resultCount = 0;
  for (size_t i = 0; i < size && resultCount < MAX_SCAN_RESULTS; i++) {
    results[resultCount++] = (uint32_t)i;
  }
}

static void ScanAllInt16(size_t size) {
  resultCount = 0;
  for (size_t i = 0; i + 1 < size && resultCount < MAX_SCAN_RESULTS; i++) {
    results[resultCount++] = (uint32_t)i;
  }
}

static void ScanAllInt32(size_t size) {
  resultCount = 0;
  for (size_t i = 0; i + 3 < size && resultCount < MAX_SCAN_RESULTS; i++) {
    results[resultCount++] = (uint32_t)i;
  }
}

static void ScanStringUtf8(const uint8_t* ram, size_t size, const char* text, bool matchCase) {
  resultCount = 0;
  size_t textLen = strlen(text);
  if (textLen == 0) return;

  for (size_t i = 0; i + textLen <= size && resultCount < MAX_SCAN_RESULTS; i++) {
    bool match = true;
    for (size_t j = 0; j < textLen; j++) {
      uint8_t a = ram[i + j];
      uint8_t b = (uint8_t)text[j];
      if (!matchCase) {
        a = (uint8_t)ToLowerAscii(a);
        b = (uint8_t)ToLowerAscii(b);
      }
      if (a != b) {
        match = false;
        break;
      }
    }
    if (match) {
      results[resultCount++] = (uint32_t)i;
    }
  }
}

static void ScanStringUtf16(const uint8_t* ram, size_t size, const char* text, bool matchCase) {
  resultCount = 0;
  uint8_t* utf16 = NULL;
  size_t utf16Size = 0;
  if (!Utf8ToUtf16(text, &utf16, &utf16Size, matchCase)) return;

  for (size_t i = 0; i + utf16Size <= size && resultCount < MAX_SCAN_RESULTS; i++) {
    bool match = true;
    for (size_t j = 0; j < utf16Size; j++) {
      if (ram[i + j] != utf16[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      results[resultCount++] = (uint32_t)i;
    }
  }

  free(utf16);
}

void MemoryScan_New(MemoryScanType_t type, const char* valueText, bool hex, bool matchCase, bool signedMode, MemoryScanCompare_t compare) {
  const uint8_t* ram = Bus_GetRamBytes();
  uint32_t ramSize = Bus_GetRamByteSize();
  if (!ram || ramSize == 0) {
    ScanReset();
    return;
  }

  scanActive = true;
  if (IsChangeCompare(compare)) {
    CaptureSnapshot(ram, ramSize);
  }

  switch (type) {
  case MEMORY_SCAN_INT8: {
    if (IsChangeCompare(compare)) {
      ScanAllInt8(ramSize);
    } else {
      uint32_t uvalue = 0;
      int32_t svalue = 0;
      if (signedMode) {
        if (!ParseInt32(valueText, hex, &svalue)) {
          ScanReset();
          return;
        }
        uvalue = (uint32_t)(uint8_t)svalue;
      } else if (!ParseUint32(valueText, hex, &uvalue)) {
        ScanReset();
        return;
      }
      ScanInt8(ram, ramSize, (uint8_t)(uvalue & 0xFF), signedMode, compare);
    }
  } break;
  case MEMORY_SCAN_INT16: {
    if (IsChangeCompare(compare)) {
      ScanAllInt16(ramSize);
    } else {
      uint32_t uvalue = 0;
      int32_t svalue = 0;
      if (signedMode) {
        if (!ParseInt32(valueText, hex, &svalue)) {
          ScanReset();
          return;
        }
        uvalue = (uint32_t)(uint16_t)svalue;
      } else if (!ParseUint32(valueText, hex, &uvalue)) {
        ScanReset();
        return;
      }
      ScanInt16(ram, ramSize, (uint16_t)(uvalue & 0xFFFF), signedMode, compare);
    }
  } break;
  case MEMORY_SCAN_INT32: {
    if (IsChangeCompare(compare)) {
      ScanAllInt32(ramSize);
    } else {
      uint32_t uvalue = 0;
      int32_t svalue = 0;
      if (signedMode) {
        if (!ParseInt32(valueText, hex, &svalue)) {
          ScanReset();
          return;
        }
        uvalue = (uint32_t)svalue;
      } else if (!ParseUint32(valueText, hex, &uvalue)) {
        ScanReset();
        return;
      }
      ScanInt32(ram, ramSize, uvalue, signedMode, compare);
    }
  } break;
  case MEMORY_SCAN_STR_UTF8:
    ScanStringUtf8(ram, ramSize, valueText ? valueText : "", matchCase);
    break;
  case MEMORY_SCAN_STR_UTF16:
    ScanStringUtf16(ram, ramSize, valueText ? valueText : "", matchCase);
    break;
  }
}

void MemoryScan_Refine(MemoryScanType_t type, const char* valueText, bool hex, bool matchCase, bool signedMode, MemoryScanCompare_t compare) {
  if (!scanActive || resultCount == 0) {
    MemoryScan_New(type, valueText, hex, matchCase, signedMode, compare);
    return;
  }

  const uint8_t* ram = Bus_GetRamBytes();
  uint32_t ramSize = Bus_GetRamByteSize();
  if (!ram || ramSize == 0) {
    ScanReset();
    return;
  }

  if (IsChangeCompare(compare) && (!prevRam || prevRamSize != ramSize)) {
    CaptureSnapshot(ram, ramSize);
  }

  uint32_t filtered[MAX_SCAN_RESULTS];
  int32_t filteredCount = 0;

  if (type == MEMORY_SCAN_INT8 || type == MEMORY_SCAN_INT16 || type == MEMORY_SCAN_INT32) {
    uint32_t uvalue = 0;
    int32_t svalue = 0;
    if (!IsChangeCompare(compare)) {
      if (signedMode) {
        if (!ParseInt32(valueText, hex, &svalue)) {
          ScanReset();
          return;
        }
        uvalue = (uint32_t)svalue;
      } else if (!ParseUint32(valueText, hex, &uvalue)) {
        ScanReset();
        return;
      }
    }

    for (int32_t i = 0; i < resultCount && filteredCount < MAX_SCAN_RESULTS; i++) {
      uint32_t offset = results[i];
      if (offset >= ramSize) continue;

      bool matches = false;
      if (type == MEMORY_SCAN_INT8) {
        uint8_t current = ram[offset];
        if (IsChangeCompare(compare) && prevRam) {
          uint8_t prev = prevRam[offset];
          matches = signedMode
            ? CompareChangeSigned((int8_t)current, (int8_t)prev, compare)
            : CompareChangeUnsigned(current, prev, compare);
        } else {
          uint8_t value = (uint8_t)(uvalue & 0xFF);
          matches = signedMode
            ? CompareSigned((int8_t)current, (int8_t)value, compare)
            : CompareUnsigned(current, value, compare);
        }
      } else if (type == MEMORY_SCAN_INT16 && offset + 1 < ramSize) {
        uint16_t current = (uint16_t)ReadU16(ram + offset);
        if (IsChangeCompare(compare) && prevRam) {
          uint16_t prev = (uint16_t)ReadU16(prevRam + offset);
          matches = signedMode
            ? CompareChangeSigned((int16_t)current, (int16_t)prev, compare)
            : CompareChangeUnsigned(current, prev, compare);
        } else {
          uint16_t value = (uint16_t)(uvalue & 0xFFFF);
          matches = signedMode
            ? CompareSigned((int16_t)current, (int16_t)value, compare)
            : CompareUnsigned(current, value, compare);
        }
      } else if (type == MEMORY_SCAN_INT32 && offset + 3 < ramSize) {
        uint32_t current = ReadU32(ram + offset);
        if (IsChangeCompare(compare) && prevRam) {
          uint32_t prev = ReadU32(prevRam + offset);
          matches = signedMode
            ? CompareChangeSigned((int32_t)current, (int32_t)prev, compare)
            : CompareChangeUnsigned(current, prev, compare);
        } else {
          uint32_t value = uvalue;
          matches = signedMode
            ? CompareSigned((int32_t)current, (int32_t)value, compare)
            : CompareUnsigned(current, value, compare);
        }
      }

      if (matches) {
        filtered[filteredCount++] = offset;
      }
    }
  } else {
    uint8_t* textBytes = NULL;
    size_t textSize = 0;

    if (type == MEMORY_SCAN_STR_UTF8) {
      if (!valueText || valueText[0] == '\0') {
        ScanReset();
        return;
      }
      textBytes = (uint8_t*)valueText;
      textSize = strlen(valueText);
    } else {
      if (!Utf8ToUtf16(valueText ? valueText : "", &textBytes, &textSize, matchCase)) {
        ScanReset();
        return;
      }
    }

    for (int32_t i = 0; i < resultCount && filteredCount < MAX_SCAN_RESULTS; i++) {
      uint32_t offset = results[i];
      if (offset + textSize > ramSize) continue;

      bool match = true;
      for (size_t j = 0; j < textSize; j++) {
        uint8_t a = ram[offset + j];
        uint8_t b = textBytes[j];
        if (type == MEMORY_SCAN_STR_UTF8 && !matchCase) {
          a = (uint8_t)ToLowerAscii(a);
          b = (uint8_t)ToLowerAscii(b);
        }
        if (a != b) {
          match = false;
          break;
        }
      }

      if (match) {
        filtered[filteredCount++] = offset;
      }
    }

    if (type == MEMORY_SCAN_STR_UTF16) {
      free(textBytes);
    }
  }

  memcpy(results, filtered, sizeof(uint32_t) * filteredCount);
  resultCount = filteredCount;

  if (IsChangeCompare(compare)) {
    CaptureSnapshot(ram, ramSize);
  }
}

int32_t MemoryScan_GetResultCount() {
  return resultCount;
}

uint32_t MemoryScan_GetResultOffset(int32_t index) {
  if (index < 0 || index >= resultCount) return 0;
  return results[index];
}
