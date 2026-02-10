#ifndef MEMORY_SCAN_H
#define MEMORY_SCAN_H

#include "../common.h"

typedef enum {
  MEMORY_SCAN_INT8 = 0,
  MEMORY_SCAN_INT16,
  MEMORY_SCAN_INT32,
  MEMORY_SCAN_STR_UTF8,
  MEMORY_SCAN_STR_UTF16
} MemoryScanType_t;

typedef enum {
  MEMORY_SCAN_COMPARE_EQUAL = 0,
  MEMORY_SCAN_COMPARE_NOT_EQUAL,
  MEMORY_SCAN_COMPARE_GREATER,
  MEMORY_SCAN_COMPARE_LESS,
  MEMORY_SCAN_COMPARE_CHANGED,
  MEMORY_SCAN_COMPARE_UNCHANGED,
  MEMORY_SCAN_COMPARE_INCREASED,
  MEMORY_SCAN_COMPARE_DECREASED
} MemoryScanCompare_t;

void MemoryScan_Reset();
void MemoryScan_New(MemoryScanType_t type, const char* valueText, bool hex, bool matchCase, bool signedMode, MemoryScanCompare_t compare);
void MemoryScan_Refine(MemoryScanType_t type, const char* valueText, bool hex, bool matchCase, bool signedMode, MemoryScanCompare_t compare);

int32_t MemoryScan_GetResultCount();
uint32_t MemoryScan_GetResultOffset(int32_t index);

#endif // MEMORY_SCAN_H
