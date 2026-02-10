#ifndef CHEATS_H
#define CHEATS_H

#include "../common.h"

#define CHEATS_MAX_NAME 64

typedef enum {
	CHEAT_CONDITION_NONE = 0,
	CHEAT_CONDITION_HINT = 1,
	CHEAT_CONDITION_BUTTON = 2
} CheatCondition_t;

typedef enum {
	CHEAT_MODE_SET = 0,
	CHEAT_MODE_INC = 1,
	CHEAT_MODE_DEC = 2
} CheatMode_t;

typedef struct {
	char name[CHEATS_MAX_NAME];
	uint32_t addr;
	uint16_t value;
	uint16_t mask;
	bool enabled;
	bool persistent;
	bool watchEnabled;
	bool watchHex;
	uint16_t stepValue;
	uint16_t minValue;
	uint16_t maxValue;
	bool limitEnabled;
	CheatMode_t mode;
	CheatCondition_t condition;
	uint8_t conditionButton;
	bool prevCondition;
} CheatEntry_t;

void Cheats_Reset();
bool Cheats_LoadForRom(const char* romTitle);
bool Cheats_Save();
void Cheats_Apply();
void Cheats_SetGlobalEnabled(bool enabled);
bool Cheats_GetGlobalEnabled();

int32_t Cheats_GetCount();
const CheatEntry_t* Cheats_GetCheat(int32_t index);
void Cheats_SetEnabled(int32_t index, bool enabled);
void Cheats_SetPersistent(int32_t index, bool persistent);
void Cheats_SetWatchEnabled(int32_t index, bool enabled);
void Cheats_SetWatchHex(int32_t index, bool enabled);
void Cheats_AdjustValue(int32_t index, int32_t delta);
bool Cheats_AddCheat(const CheatEntry_t* entry);
bool Cheats_UpdateCheat(int32_t index, const CheatEntry_t* entry);
bool Cheats_RemoveCheat(int32_t index);

#endif // CHEATS_H
