#include "tas.h"

#include <errno.h>

#define TAS_MAGIC 0x53415456u
#define TAS_VERSION 1u

static void TAS_FreeStates(TAS_t* tas) {
  if (!tas) return;
  for (uint32_t i = 0; i < tas->stateCount; i++) {
    free(tas->states[i].name);
    free(tas->states[i].data);
  }
  free(tas->states);
  tas->states = NULL;
  tas->stateCount = 0;
  tas->stateCap = 0;
}

static void TAS_FreeFrames(TAS_t* tas) {
  if (!tas) return;
  free(tas->frames);
  tas->frames = NULL;
  tas->frameCount = 0;
  tas->frameCap = 0;
}

static void TAS_FreeMarkers(TAS_t* tas) {
  if (!tas) return;
  free(tas->markers);
  tas->markers = NULL;
  tas->markerCount = 0;
  tas->markerCap = 0;
}

bool TAS_Init(TAS_t* tas) {
  if (!tas) return false;
  memset(tas, 0, sizeof(*tas));
  return true;
}

void TAS_Free(TAS_t* tas) {
  if (!tas) return;
  TAS_FreeFrames(tas);
  TAS_FreeStates(tas);
  TAS_FreeMarkers(tas);
}

void TAS_Clear(TAS_t* tas) {
  if (!tas) return;
  TAS_FreeFrames(tas);
  TAS_FreeStates(tas);
  TAS_FreeMarkers(tas);
}

static bool TAS_ReserveFrames(TAS_t* tas, uint32_t needed) {
  if (!tas) return false;
  if (needed <= tas->frameCap) return true;
  uint32_t newCap = tas->frameCap ? tas->frameCap : 256;
  while (newCap < needed) newCap *= 2;
  TAS_Frame_t* newFrames = realloc(tas->frames, sizeof(TAS_Frame_t) * newCap);
  if (!newFrames) return false;
  tas->frames = newFrames;
  tas->frameCap = newCap;
  return true;
}

static bool TAS_ReserveStates(TAS_t* tas, uint32_t needed) {
  if (!tas) return false;
  if (needed <= tas->stateCap) return true;
  uint32_t newCap = tas->stateCap ? tas->stateCap : 8;
  while (newCap < needed) newCap *= 2;
  TAS_SaveState_t* newStates = realloc(tas->states, sizeof(TAS_SaveState_t) * newCap);
  if (!newStates) return false;
  tas->states = newStates;
  tas->stateCap = newCap;
  return true;
}

static bool TAS_ReserveMarkers(TAS_t* tas, uint32_t needed) {
  if (!tas) return false;
  if (needed <= tas->markerCap) return true;
  uint32_t newCap = tas->markerCap ? tas->markerCap : 8;
  while (newCap < needed) newCap *= 2;
  TAS_StateMarker_t* newMarkers = realloc(tas->markers, sizeof(TAS_StateMarker_t) * newCap);
  if (!newMarkers) return false;
  tas->markers = newMarkers;
  tas->markerCap = newCap;
  return true;
}

bool TAS_AddFrame(TAS_t* tas, uint32_t buttons0, uint32_t buttons1) {
  if (!tas) return false;
  if (!TAS_ReserveFrames(tas, tas->frameCount + 1)) return false;
  tas->frames[tas->frameCount++] = (TAS_Frame_t){ .buttons0 = buttons0, .buttons1 = buttons1 };
  return true;
}

bool TAS_SetFrame(TAS_t* tas, uint32_t index, uint32_t buttons0, uint32_t buttons1) {
  if (!tas) return false;
  if (index >= tas->frameCount) return false;
  tas->frames[index].buttons0 = buttons0;
  tas->frames[index].buttons1 = buttons1;
  return true;
}

bool TAS_GetFrame(const TAS_t* tas, uint32_t index, TAS_Frame_t* outFrame) {
  if (!tas || !outFrame) return false;
  if (index >= tas->frameCount) return false;
  *outFrame = tas->frames[index];
  return true;
}

bool TAS_AddSavestateData(TAS_t* tas, const char* name, const uint8_t* data, uint32_t size) {
  if (!tas || !name || !data || size == 0) return false;
  if (!TAS_ReserveStates(tas, tas->stateCount + 1)) return false;

  TAS_SaveState_t state;
  memset(&state, 0, sizeof(state));

  size_t nameLen = strlen(name);
  state.name = malloc(nameLen + 1);
  if (!state.name) return false;
  memcpy(state.name, name, nameLen + 1);

  state.data = malloc(size);
  if (!state.data) {
    free(state.name);
    return false;
  }
  memcpy(state.data, data, size);
  state.size = size;

  tas->states[tas->stateCount++] = state;
  return true;
}

bool TAS_AddSavestateFromFile(TAS_t* tas, const char* name, const char* path) {
  if (!tas || !name || !path) return false;

  FILE* file = fopen(path, "rb");
  if (!file) return false;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (size <= 0) {
    fclose(file);
    return false;
  }

  uint8_t* buffer = malloc((size_t)size);
  if (!buffer) {
    fclose(file);
    return false;
  }

  size_t readSize = fread(buffer, 1, (size_t)size, file);
  fclose(file);
  if (readSize != (size_t)size) {
    free(buffer);
    return false;
  }

  bool result = TAS_AddSavestateData(tas, name, buffer, (uint32_t)size);
  free(buffer);
  return result;
}

bool TAS_AddMarker(TAS_t* tas, uint32_t frameIndex, uint32_t stateIndex) {
  if (!tas) return false;
  if (stateIndex >= tas->stateCount) return false;
  if (!TAS_ReserveMarkers(tas, tas->markerCount + 1)) return false;
  tas->markers[tas->markerCount++] = (TAS_StateMarker_t){
    .frameIndex = frameIndex,
    .stateIndex = stateIndex
  };
  for (uint32_t i = tas->markerCount - 1; i > 0; i--) {
    TAS_StateMarker_t* a = &tas->markers[i - 1];
    TAS_StateMarker_t* b = &tas->markers[i];
    if (a->frameIndex < b->frameIndex) break;
    if (a->frameIndex == b->frameIndex && a->stateIndex <= b->stateIndex) break;
    TAS_StateMarker_t tmp = *a;
    *a = *b;
    *b = tmp;
  }
  return true;
}

static bool TAS_WriteU32(FILE* file, uint32_t value) {
  return fwrite(&value, sizeof(value), 1, file) == 1;
}

static bool TAS_ReadU32(FILE* file, uint32_t* outValue) {
  return fread(outValue, sizeof(*outValue), 1, file) == 1;
}

bool TAS_SaveToFile(const TAS_t* tas, const char* path) {
  if (!tas || !path) return false;
  FILE* file = fopen(path, "wb");
  if (!file) return false;

  bool ok = true;
  ok = ok && TAS_WriteU32(file, TAS_MAGIC);
  ok = ok && TAS_WriteU32(file, TAS_VERSION);
  ok = ok && TAS_WriteU32(file, tas->frameCount);
  ok = ok && TAS_WriteU32(file, tas->stateCount);
  ok = ok && TAS_WriteU32(file, tas->markerCount);

  for (uint32_t i = 0; ok && i < tas->frameCount; i++) {
    ok = ok && TAS_WriteU32(file, tas->frames[i].buttons0);
    ok = ok && TAS_WriteU32(file, tas->frames[i].buttons1);
  }

  for (uint32_t i = 0; ok && i < tas->stateCount; i++) {
    TAS_SaveState_t* state = &tas->states[i];
    uint32_t nameLen = state->name ? (uint32_t)strlen(state->name) : 0;
    ok = ok && TAS_WriteU32(file, nameLen);
    ok = ok && TAS_WriteU32(file, state->size);
    if (ok && nameLen > 0) {
      ok = fwrite(state->name, 1, nameLen, file) == nameLen;
    }
    if (ok && state->size > 0) {
      ok = fwrite(state->data, 1, state->size, file) == state->size;
    }
  }

  for (uint32_t i = 0; ok && i < tas->markerCount; i++) {
    ok = ok && TAS_WriteU32(file, tas->markers[i].frameIndex);
    ok = ok && TAS_WriteU32(file, tas->markers[i].stateIndex);
  }

  if (fclose(file) != 0) ok = false;
  if (!ok) {
    remove(path);
  }
  return ok;
}

bool TAS_LoadFromFile(TAS_t* tas, const char* path) {
  if (!tas || !path) return false;
  FILE* file = fopen(path, "rb");
  if (!file) return false;

  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t frameCount = 0;
  uint32_t stateCount = 0;
  uint32_t markerCount = 0;

  bool ok = true;
  ok = ok && TAS_ReadU32(file, &magic);
  ok = ok && TAS_ReadU32(file, &version);
  ok = ok && TAS_ReadU32(file, &frameCount);
  ok = ok && TAS_ReadU32(file, &stateCount);
  ok = ok && TAS_ReadU32(file, &markerCount);

  if (!ok || magic != TAS_MAGIC || version != TAS_VERSION) {
    fclose(file);
    return false;
  }

  TAS_Clear(tas);

  if (!TAS_ReserveFrames(tas, frameCount)) {
    fclose(file);
    return false;
  }
  tas->frameCount = frameCount;
  for (uint32_t i = 0; i < frameCount; i++) {
    uint32_t buttons0 = 0;
    uint32_t buttons1 = 0;
    ok = ok && TAS_ReadU32(file, &buttons0);
    ok = ok && TAS_ReadU32(file, &buttons1);
    tas->frames[i].buttons0 = buttons0;
    tas->frames[i].buttons1 = buttons1;
  }

  if (ok && stateCount > 0) {
    if (!TAS_ReserveStates(tas, stateCount)) ok = false;
  }
  if (ok) {
    for (uint32_t i = 0; i < stateCount; i++) {
      uint32_t nameLen = 0;
      uint32_t size = 0;
      ok = ok && TAS_ReadU32(file, &nameLen);
      ok = ok && TAS_ReadU32(file, &size);
      if (!ok) break;

      char* name = NULL;
      uint8_t* data = NULL;
      if (nameLen > 0) {
        name = malloc(nameLen + 1);
        if (!name) { ok = false; break; }
        if (fread(name, 1, nameLen, file) != nameLen) {
          free(name);
          ok = false;
          break;
        }
        name[nameLen] = '\0';
      }

      if (size > 0) {
        data = malloc(size);
        if (!data) {
          free(name);
          ok = false;
          break;
        }
        if (fread(data, 1, size, file) != size) {
          free(name);
          free(data);
          ok = false;
          break;
        }
      }

      if (!TAS_ReserveStates(tas, tas->stateCount + 1)) {
        free(name);
        free(data);
        ok = false;
        break;
      }

      tas->states[tas->stateCount++] = (TAS_SaveState_t){
        .name = name,
        .data = data,
        .size = size
      };
    }
  }

  if (ok && markerCount > 0) {
    if (!TAS_ReserveMarkers(tas, markerCount)) ok = false;
  }
  if (ok) {
    tas->markerCount = markerCount;
    for (uint32_t i = 0; i < markerCount; i++) {
      ok = ok && TAS_ReadU32(file, &tas->markers[i].frameIndex);
      ok = ok && TAS_ReadU32(file, &tas->markers[i].stateIndex);
    }
  }

  if (ok && tas->markerCount > 1) {
    for (uint32_t i = 1; i < tas->markerCount; i++) {
      for (uint32_t j = i; j > 0; j--) {
        TAS_StateMarker_t* a = &tas->markers[j - 1];
        TAS_StateMarker_t* b = &tas->markers[j];
        if (a->frameIndex < b->frameIndex) break;
        if (a->frameIndex == b->frameIndex && a->stateIndex <= b->stateIndex) break;
        TAS_StateMarker_t tmp = *a;
        *a = *b;
        *b = tmp;
      }
    }
  }

  fclose(file);
  if (!ok) {
    TAS_Clear(tas);
  }

  return ok;
}
