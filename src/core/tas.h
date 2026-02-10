#ifndef TAS_H
#define TAS_H

#include "../common.h"

typedef struct {
  uint32_t buttons0;
  uint32_t buttons1;
} TAS_Frame_t;

typedef struct {
  char* name;
  uint8_t* data;
  uint32_t size;
} TAS_SaveState_t;

typedef struct {
  uint32_t frameIndex;
  uint32_t stateIndex;
} TAS_StateMarker_t;

typedef struct {
  TAS_Frame_t* frames;
  uint32_t frameCount;
  uint32_t frameCap;

  TAS_SaveState_t* states;
  uint32_t stateCount;
  uint32_t stateCap;

  TAS_StateMarker_t* markers;
  uint32_t markerCount;
  uint32_t markerCap;
} TAS_t;

bool TAS_Init(TAS_t* tas);
void TAS_Free(TAS_t* tas);
void TAS_Clear(TAS_t* tas);

bool TAS_AddFrame(TAS_t* tas, uint32_t buttons0, uint32_t buttons1);
bool TAS_SetFrame(TAS_t* tas, uint32_t index, uint32_t buttons0, uint32_t buttons1);
bool TAS_GetFrame(const TAS_t* tas, uint32_t index, TAS_Frame_t* outFrame);

bool TAS_AddSavestateData(TAS_t* tas, const char* name, const uint8_t* data, uint32_t size);
bool TAS_AddSavestateFromFile(TAS_t* tas, const char* name, const char* path);

bool TAS_AddMarker(TAS_t* tas, uint32_t frameIndex, uint32_t stateIndex);

bool TAS_SaveToFile(const TAS_t* tas, const char* path);
bool TAS_LoadFromFile(TAS_t* tas, const char* path);

#endif // TAS_H
