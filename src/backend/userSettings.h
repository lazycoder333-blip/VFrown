#ifndef USERSETTINGS_H
#define USERSETTINGS_H

#include "../common.h"
#include "../core/vsmile.h"
#include "backend.h"
#include "libs.h"
#include "lib/ini.h"

typedef struct {
  ini_t* ini;
} UserSettings_t;

bool UserSettings_Init();
void UserSettings_Cleanup();

bool UserSettings_ReadString(const char* name, char* value, uint32_t bufferSize);

void UserSettings_WriteString(const char* name, char* value, uint32_t size);
bool UserSettings_ReadInt(const char* name, int* outValue);
void UserSettings_WriteInt(const char* name, int value);
bool UserSettings_ReadBool(const char* name, bool* outValue);
void UserSettings_WriteBool(const char* name, bool value);

#endif // USERSETTINGS_H
