#include "cheats.h"
#include "bus.h"
#include "../backend/input.h"
#include "../backend/lib/ini.h"

#define MAX_CHEATS 256

static CheatEntry_t cheats[MAX_CHEATS];
static int32_t cheatCount = 0;
static char cheatFilePath[256];
static bool cheatsEnabled = true;

static void Cheats_Clear() {
  memset(cheats, 0, sizeof(cheats));
  cheatCount = 0;
}

static const char* Cheats_GetConditionName(CheatCondition_t condition) {
  switch (condition) {
  case CHEAT_CONDITION_HINT: return "hint";
  case CHEAT_CONDITION_BUTTON: return "button";
  default: return "none";
  }
}

static const char* Cheats_GetButtonName(uint8_t button) {
  switch (button) {
  case INPUT_UP: return "up";
  case INPUT_DOWN: return "down";
  case INPUT_LEFT: return "left";
  case INPUT_RIGHT: return "right";
  case INPUT_RED: return "red";
  case INPUT_YELLOW: return "yellow";
  case INPUT_BLUE: return "blue";
  case INPUT_GREEN: return "green";
  case INPUT_ENTER: return "enter";
  case INPUT_HELP: return "help";
  case INPUT_EXIT: return "exit";
  case INPUT_ABC: return "abc";
  default: return "";
  }
}

static bool Cheats_ParseButton(const char* value, uint8_t* outButton) {
  if (!value || !outButton) return false;
  if (strequ(value, "up")) { *outButton = INPUT_UP; return true; }
  if (strequ(value, "down")) { *outButton = INPUT_DOWN; return true; }
  if (strequ(value, "left")) { *outButton = INPUT_LEFT; return true; }
  if (strequ(value, "right")) { *outButton = INPUT_RIGHT; return true; }
  if (strequ(value, "red")) { *outButton = INPUT_RED; return true; }
  if (strequ(value, "yellow")) { *outButton = INPUT_YELLOW; return true; }
  if (strequ(value, "blue")) { *outButton = INPUT_BLUE; return true; }
  if (strequ(value, "green")) { *outButton = INPUT_GREEN; return true; }
  if (strequ(value, "enter")) { *outButton = INPUT_ENTER; return true; }
  if (strequ(value, "help") || strequ(value, "hint")) { *outButton = INPUT_HELP; return true; }
  if (strequ(value, "exit")) { *outButton = INPUT_EXIT; return true; }
  if (strequ(value, "abc")) { *outButton = INPUT_ABC; return true; }
  return false;
}

static CheatCondition_t Cheats_ParseCondition(const char* value) {
  if (!value) return CHEAT_CONDITION_NONE;
  if (strequ(value, "hint")) return CHEAT_CONDITION_HINT;
  if (strequ(value, "button")) return CHEAT_CONDITION_BUTTON;
  return CHEAT_CONDITION_NONE;
}

static bool Cheats_ReadBool(const char* value, bool defaultValue) {
  if (!value) return defaultValue;
  if (strequ(value, "1") || strequ(value, "true") || strequ(value, "yes")) return true;
  if (strequ(value, "0") || strequ(value, "false") || strequ(value, "no")) return false;
  return defaultValue;
}

static CheatMode_t Cheats_ParseMode(const char* value) {
  if (!value) return CHEAT_MODE_SET;
  if (strequ(value, "inc") || strequ(value, "increment")) return CHEAT_MODE_INC;
  if (strequ(value, "dec") || strequ(value, "decrement")) return CHEAT_MODE_DEC;
  return CHEAT_MODE_SET;
}

static const char* Cheats_GetModeName(CheatMode_t mode) {
  switch (mode) {
  case CHEAT_MODE_INC: return "inc";
  case CHEAT_MODE_DEC: return "dec";
  default: return "set";
  }
}

static uint32_t Cheats_ReadHex(const char* value, uint32_t defaultValue) {
  if (!value) return defaultValue;
  char* end = NULL;
  uint32_t parsed = (uint32_t)strtoul(value, &end, 0);
  if (!end || end == value) return defaultValue;
  return parsed;
}

static bool Cheats_LoadIni(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) return false;

  fseek(file, 0, SEEK_END);
  uint32_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* buffer = malloc(size + 1);
  if (!buffer) {
    fclose(file);
    return false;
  }

  fread(buffer, size, sizeof(char), file);
  buffer[size] = '\0';
  fclose(file);

  ini_t* ini = ini_load(buffer, NULL);
  free(buffer);
  if (!ini) return false;

  Cheats_Clear();

  int sectionCount = ini_section_count(ini);
  for (int section = 0; section < sectionCount; section++) {
    if (cheatCount >= MAX_CHEATS) break;

    const char* sectionName = ini_section_name(ini, section);
    if (!sectionName || sectionName[0] == '\0') continue;

    CheatEntry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, sectionName, CHEATS_MAX_NAME - 1);
    entry.name[CHEATS_MAX_NAME - 1] = '\0';
    entry.enabled = true;
    entry.persistent = true;
    entry.watchEnabled = false;
    entry.watchHex = false;
    entry.stepValue = 1;
    entry.minValue = 0;
    entry.maxValue = 0xFFFF;
    entry.limitEnabled = true;
    entry.mode = CHEAT_MODE_SET;
    entry.mask = 0xFFFF;
    entry.condition = CHEAT_CONDITION_NONE;
    entry.conditionButton = INPUT_HELP;

    int propCount = ini_property_count(ini, section);
    for (int prop = 0; prop < propCount; prop++) {
      const char* propName = ini_property_name(ini, section, prop);
      const char* propValue = ini_property_value(ini, section, prop);
      if (!propName) continue;

      if (strequ(propName, "addr")) {
        entry.addr = Cheats_ReadHex(propValue, entry.addr);
      } else if (strequ(propName, "value")) {
        entry.value = (uint16_t)(Cheats_ReadHex(propValue, entry.value) & 0xFFFF);
      } else if (strequ(propName, "mask")) {
        entry.mask = (uint16_t)(Cheats_ReadHex(propValue, entry.mask) & 0xFFFF);
      } else if (strequ(propName, "enabled")) {
        entry.enabled = Cheats_ReadBool(propValue, entry.enabled);
      } else if (strequ(propName, "persistent")) {
        entry.persistent = Cheats_ReadBool(propValue, entry.persistent);
      } else if (strequ(propName, "watch")) {
        entry.watchEnabled = Cheats_ReadBool(propValue, entry.watchEnabled);
      } else if (strequ(propName, "watchHex")) {
        entry.watchHex = Cheats_ReadBool(propValue, entry.watchHex);
      } else if (strequ(propName, "step")) {
        entry.stepValue = (uint16_t)(Cheats_ReadHex(propValue, entry.stepValue) & 0xFFFF);
      } else if (strequ(propName, "min")) {
        entry.minValue = (uint16_t)(Cheats_ReadHex(propValue, entry.minValue) & 0xFFFF);
      } else if (strequ(propName, "max")) {
        entry.maxValue = (uint16_t)(Cheats_ReadHex(propValue, entry.maxValue) & 0xFFFF);
      } else if (strequ(propName, "limit")) {
        entry.limitEnabled = Cheats_ReadBool(propValue, entry.limitEnabled);
      } else if (strequ(propName, "mode")) {
        entry.mode = Cheats_ParseMode(propValue);
      } else if (strequ(propName, "condition")) {
        entry.condition = Cheats_ParseCondition(propValue);
      } else if (strequ(propName, "button")) {
        Cheats_ParseButton(propValue, &entry.conditionButton);
      }
    }

    if (entry.condition == CHEAT_CONDITION_HINT) {
      entry.conditionButton = INPUT_HELP;
    }

    cheats[cheatCount++] = entry;
  }

  ini_destroy(ini);
  return true;
}

void Cheats_Reset() {
  Cheats_Clear();
  cheatFilePath[0] = '\0';
}

bool Cheats_LoadForRom(const char* romTitle) {
  Cheats_Clear();
  if (!romTitle || romTitle[0] == '\0') {
    cheatFilePath[0] = '\0';
    return false;
  }

  EnsureDirectoryExists("cheats");
  snprintf(cheatFilePath, sizeof(cheatFilePath), "cheats/%s.ini", romTitle);
  cheatFilePath[sizeof(cheatFilePath) - 1] = '\0';

  return Cheats_LoadIni(cheatFilePath);
}

bool Cheats_Save() {
  if (cheatFilePath[0] == '\0') return false;

  ini_t* ini = ini_create(NULL);
  if (!ini) return false;

  for (int32_t i = 0; i < cheatCount; i++) {
    CheatEntry_t* entry = &cheats[i];
    int section = ini_section_add(ini, entry->name, 0);
    if (section == INI_NOT_FOUND) continue;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "0x%06x", entry->addr);
    ini_property_add(ini, section, "addr", 0, buffer, 0);

    snprintf(buffer, sizeof(buffer), "0x%04x", entry->value);
    ini_property_add(ini, section, "value", 0, buffer, 0);

    snprintf(buffer, sizeof(buffer), "0x%04x", entry->mask);
    ini_property_add(ini, section, "mask", 0, buffer, 0);

    ini_property_add(ini, section, "enabled", 0, entry->enabled ? "1" : "0", 0);
    ini_property_add(ini, section, "persistent", 0, entry->persistent ? "1" : "0", 0);
    ini_property_add(ini, section, "watch", 0, entry->watchEnabled ? "1" : "0", 0);
    ini_property_add(ini, section, "watchHex", 0, entry->watchHex ? "1" : "0", 0);
    snprintf(buffer, sizeof(buffer), "0x%04x", entry->stepValue);
    ini_property_add(ini, section, "step", 0, buffer, 0);
    snprintf(buffer, sizeof(buffer), "0x%04x", entry->minValue);
    ini_property_add(ini, section, "min", 0, buffer, 0);
    snprintf(buffer, sizeof(buffer), "0x%04x", entry->maxValue);
    ini_property_add(ini, section, "max", 0, buffer, 0);
    ini_property_add(ini, section, "limit", 0, entry->limitEnabled ? "1" : "0", 0);
    ini_property_add(ini, section, "mode", 0, Cheats_GetModeName(entry->mode), 0);
    ini_property_add(ini, section, "condition", 0, Cheats_GetConditionName(entry->condition), 0);
    if (entry->condition == CHEAT_CONDITION_BUTTON) {
      const char* buttonName = Cheats_GetButtonName(entry->conditionButton);
      if (buttonName[0] != '\0') {
        ini_property_add(ini, section, "button", 0, buttonName, 0);
      }
    }
  }

  int size = ini_save(ini, NULL, 0);
  char* data = malloc(size);
  if (!data) {
    ini_destroy(ini);
    return false;
  }

  size = ini_save(ini, data, size);
  ini_destroy(ini);

  FILE* file = fopen(cheatFilePath, "w");
  if (!file) {
    free(data);
    return false;
  }

  fwrite(data, size - 1, sizeof(uint8_t), file);
  fclose(file);
  free(data);
  return true;
}

static bool Cheats_IsConditionActive(CheatEntry_t* entry) {
  if (entry->condition == CHEAT_CONDITION_HINT) {
    Mapping_t mapping;
    if (Input_GetMapping(0, INPUT_HELP, &mapping) && mapping.deviceType == INPUT_DEVICETYPE_KEYBOARD) {
      return Input_IsKeyDown(mapping.inputID);
    }
  }

  switch (entry->condition) {
  case CHEAT_CONDITION_HINT:
    return ((Input_GetButtons(0) | Input_GetButtons(1)) & (1 << INPUT_HELP)) != 0;
  case CHEAT_CONDITION_BUTTON:
    return ((Input_GetButtons(0) | Input_GetButtons(1)) & (1 << entry->conditionButton)) != 0;
  default:
    return true;
  }
}

static uint16_t Cheats_ClampValue(CheatEntry_t* entry, int32_t value) {
  if (!entry->limitEnabled) {
    return (uint16_t)(value & 0xFFFF);
  }

  int32_t minValue = entry->minValue;
  int32_t maxValue = entry->maxValue;
  if (minValue > maxValue) {
    int32_t tmp = minValue;
    minValue = maxValue;
    maxValue = tmp;
  }
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  return (uint16_t)value;
}

void Cheats_Apply() {
  if (!cheatsEnabled)
    return;
  for (int32_t i = 0; i < cheatCount; i++) {
    CheatEntry_t* entry = &cheats[i];
    if (!entry->enabled) continue;
    if (entry->watchEnabled) continue;

    bool conditionActive = Cheats_IsConditionActive(entry);
    bool shouldApply = true;

    if (entry->condition != CHEAT_CONDITION_NONE) {
      if (entry->persistent) {
        shouldApply = conditionActive;
      } else {
        shouldApply = conditionActive && !entry->prevCondition;
      }
    }

    entry->prevCondition = conditionActive;

    if (!shouldApply) continue;

    uint16_t current = Bus_Load(entry->addr);
    if (entry->mode == CHEAT_MODE_SET) {
      uint16_t masked = (uint16_t)((current & ~entry->mask) | (entry->value & entry->mask));
      if (masked != current) {
        Bus_Store(entry->addr, masked);
      }
    } else {
      int32_t delta = (entry->mode == CHEAT_MODE_INC) ? (int32_t)entry->stepValue : -(int32_t)entry->stepValue;
      uint16_t target = current;
      if (entry->mask != 0xFFFF) {
        uint16_t maskedCurrent = (uint16_t)(current & entry->mask);
        uint16_t maskedTarget = Cheats_ClampValue(entry, (int32_t)maskedCurrent + delta);
        target = (uint16_t)((current & ~entry->mask) | (maskedTarget & entry->mask));
      } else {
        target = Cheats_ClampValue(entry, (int32_t)current + delta);
      }
      if (target != current) {
        Bus_Store(entry->addr, target);
      }
    }
  }
}


void Cheats_SetWatchEnabled(int32_t index, bool enabled) {
  if (index < 0 || index >= cheatCount) return;
  cheats[index].watchEnabled = enabled;
}


void Cheats_SetWatchHex(int32_t index, bool enabled) {
  if (index < 0 || index >= cheatCount) return;
  cheats[index].watchHex = enabled;
}


void Cheats_AdjustValue(int32_t index, int32_t delta) {
  if (index < 0 || index >= cheatCount) return;
  CheatEntry_t* entry = &cheats[index];
  entry->value = Cheats_ClampValue(entry, (int32_t)entry->value + delta);
}


void Cheats_SetGlobalEnabled(bool enabled) {
  cheatsEnabled = enabled;
}


bool Cheats_GetGlobalEnabled() {
  return cheatsEnabled;
}

int32_t Cheats_GetCount() {
  return cheatCount;
}

const CheatEntry_t* Cheats_GetCheat(int32_t index) {
  if (index < 0 || index >= cheatCount) return NULL;
  return &cheats[index];
}


void Cheats_SetEnabled(int32_t index, bool enabled) {
  if (index < 0 || index >= cheatCount) return;
  cheats[index].enabled = enabled;
}


void Cheats_SetPersistent(int32_t index, bool persistent) {
  if (index < 0 || index >= cheatCount) return;
  cheats[index].persistent = persistent;
}


bool Cheats_AddCheat(const CheatEntry_t* entry) {
  if (!entry) return false;
  if (cheatCount >= MAX_CHEATS) return false;
  if (entry->name[0] == '\0') return false;

  CheatEntry_t newEntry = *entry;
  newEntry.prevCondition = false;
  cheats[cheatCount++] = newEntry;
  return true;
}


bool Cheats_UpdateCheat(int32_t index, const CheatEntry_t* entry) {
  if (!entry) return false;
  if (index < 0 || index >= cheatCount) return false;
  if (entry->name[0] == '\0') return false;

  CheatEntry_t updated = *entry;
  updated.prevCondition = cheats[index].prevCondition;
  cheats[index] = updated;
  return true;
}


bool Cheats_RemoveCheat(int32_t index) {
  if (index < 0 || index >= cheatCount) return false;
  for (int32_t i = index; i < cheatCount - 1; i++) {
    cheats[i] = cheats[i + 1];
  }
  cheatCount--;
  return true;
}
