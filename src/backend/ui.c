#include "ui.h"
#include "backend.h"
#include "input.h"
#include "../core/cheats.h"
#include "../core/memory_scan.h"
#include "../core/bus.h"

#include <sys/stat.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

// static UI_t this;

#define GET_NKCOLOR(r, g, b, a) (struct nk_color){ r, g, b, a }

enum {
  THEME_TEXT,
  THEME_HEADER,
  THEME_BORDER,
  THEME_WINDOW,
  THEME_BUTTON,
};

// UI Style //
static struct nk_color theme[NK_COLOR_COUNT] = {
  [NK_COLOR_TEXT]                    = GET_NKCOLOR(210, 210, 210, 255),
  [NK_COLOR_WINDOW]                  = GET_NKCOLOR( 57,  67,  71, 255),
  [NK_COLOR_HEADER]                  = GET_NKCOLOR( 51,  51,  56, 255),
  [NK_COLOR_BORDER]                  = GET_NKCOLOR( 46,  46,  46, 255),
  [NK_COLOR_BUTTON]                  = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_BUTTON_HOVER]            = GET_NKCOLOR( 58,  93, 121, 255),
  [NK_COLOR_BUTTON_ACTIVE]           = GET_NKCOLOR( 63,  98, 126, 255),
  [NK_COLOR_TOGGLE]                  = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_TOGGLE_HOVER]            = GET_NKCOLOR( 45,  53,  56, 255),
  [NK_COLOR_TOGGLE_CURSOR]           = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_SELECT]                  = GET_NKCOLOR( 57,  67,  61, 255),
  [NK_COLOR_SELECT_ACTIVE]           = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_SLIDER]                  = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_SLIDER_CURSOR]           = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_SLIDER_CURSOR_HOVER]     = GET_NKCOLOR( 53,  88, 116, 255),
  [NK_COLOR_SLIDER_CURSOR_ACTIVE]    = GET_NKCOLOR( 58,  93, 121, 255),
  [NK_COLOR_PROPERTY]                = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_EDIT]                    = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_EDIT_CURSOR]             = GET_NKCOLOR(210, 210, 210, 255),
  [NK_COLOR_COMBO]                   = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_CHART]                   = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_CHART_COLOR]             = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_CHART_COLOR_HIGHLIGHT]   = GET_NKCOLOR(255,   0,   0, 255),
  [NK_COLOR_SCROLLBAR]               = GET_NKCOLOR( 50,  58,  61, 255),
  [NK_COLOR_SCROLLBAR_CURSOR]        = GET_NKCOLOR( 48,  83, 111, 255),
  [NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = GET_NKCOLOR( 53,  88, 116, 255),
  [NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = GET_NKCOLOR( 58,  93, 121, 255),
  [NK_COLOR_TAB_HEADER]              = GET_NKCOLOR( 48,  83, 111, 255),
};
static bool themeUpdated = false;

// UI variables //
static struct nk_context* ctx;
static nk_bool showUI          = true;
static nk_bool showAbout       = false;
static nk_bool showEmuSettings = false;
static nk_bool showRegisters   = false;
static nk_bool showMemory      = false;
static nk_bool showMemWatch    = false;
static nk_bool showCPU         = false;
static nk_bool showCheats      = false;
static nk_bool showMemoryScan  = false;
static nk_bool showSaveStates  = false;
static nk_bool showHotkeys     = false;
static nk_bool showControllerConfig = false;
static nk_bool showTASEditor   = false;
static nk_bool showRomBrowser  = false;

#define ROM_LIST_MAX 512
#define ROM_NAME_MAX 256
static nk_bool romListDirty = true;
static int romListCount = 0;
static char romList[ROM_LIST_MAX][ROM_NAME_MAX];

static int scanType = 0;
static int scanCompare = 0;
static nk_bool scanHex = false;
static nk_bool scanSigned = false;
static nk_bool scanMatchCase = false;
static char scanValueText[64];
static int scanValueTextLen;
static int scanSelectedIndex = -1;
static int manageSelectedIndex = -1;
static nk_bool showAddCheat = false;
static nk_bool showEditCheat = false;
static int editCheatIndex = -1;
static char addCheatName[CHEATS_MAX_NAME];
static int addCheatNameLen;
static char addCheatValue[8];
static int addCheatValueLen;
static char addCheatMask[8];
static int addCheatMaskLen;
static nk_bool addCheatEnabled = true;
static nk_bool addCheatPersistent = true;
static nk_bool addCheatWatch = false;
static nk_bool addCheatWatchHex = false;
static int addCheatMode = 0;
static char addCheatStep[8];
static int addCheatStepLen;
static char addCheatMin[8];
static int addCheatMinLen;
static char addCheatMax[8];
static int addCheatMaxLen;
static nk_bool addCheatLimitEnabled = true;
static int addCheatCondition = 0;
static uint32_t addCheatAddr = 0;
static nk_bool showScanContext = false;
static int scanContextIndex = -1;
static struct nk_vec2 scanContextPos;
static nk_bool showCheatContext = false;
static int cheatContextIndex = -1;
static struct nk_vec2 cheatContextPos;
static nk_bool showDeleteCheatConfirm = false;

static nk_bool showIntro = true;
static nk_bool showLeds  = false;
static uint8_t currRegion = 0;
static uint8_t filterMode = SCREENFILTER_NEAREST;
static nk_bool keepAR;
static nk_bool pixelPerfectScale;
static nk_bool showLayer[3] = { true, true, true };
static nk_bool showSpriteOutlines = false;
static nk_bool showSpriteFlip = false;

static int hotkeyCaptureIndex = -1;
static int mapCaptureCtrl = -1;
static int mapCaptureInput = -1;
static uint8_t controllerConfigIndex = 0;
static int tasEditorStartFrame = 0;

static char jumpAddressText[7];
static int  jumpAddressTextLen;

// static MemWatchValue_t* memWatches = NULL;


static const uint8_t regionValues[] = {
  REGION_US,         REGION_UK,
  REGION_FRENCH,     REGION_SPANISH,
  REGION_GERMAN,     REGION_ITALIAN,
  REGION_DUTCH,      REGION_POLISH,
  REGION_PORTUGUESE, REGION_CHINESE
};

static const char* regionText[] = {
  "US",         "UK",
  "French",     "Spanish",
  "German",     "Italian",
  "Dutch",      "Polish",
  "Portuguese", "Chinese"
};

static const char* filterText[] = {
  "Nearest", "Linear"
};

static const char* scanTypeText[] = {
  "Int 8-bit", "Int 16-bit", "Int 32-bit", "String UTF-8", "String UTF-16"
};

static const char* scanCompareText[] = {
  "Equal", "Not Equal", "Greater", "Less", "Changed", "Unchanged", "Increased", "Decreased"
};

static const char* cheatConditionText[] = {
  "None", "Hint (Exit)", "Enter", "Help", "ABC", "Up", "Down", "Left", "Right", "Red", "Yellow", "Green", "Blue"
};

static const char* hotkeyActionText[] = {
  "Toggle UI",
  "Toggle Fullscreen",
  "Reset",
  "Pause",
  "Step",
  "Save State",
  "Load State",
  "Fast Forward"
};

static const char* controllerLabelText[] = { "Player 1", "Player 2" };

static const char* inputNameText[] = {
  "Up", "Dn", "Lt", "Rt",
  "R", "Y", "G", "B",
  "Ent", "Exit", "Help", "ABC"
};

static const uint8_t tasInputOrder[] = {
  INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
  INPUT_RED, INPUT_YELLOW, INPUT_GREEN, INPUT_BLUE,
  INPUT_ENTER, INPUT_EXIT, INPUT_HELP, INPUT_ABC
};

static const char* cheatModeText[] = {
  "Set",
  "Increment",
  "Decrement"
};

static void FormatKeyName(int32_t keycode, char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  if (keycode == 0) {
    snprintf(outText, outSize, "None");
    return;
  }

  if (keycode >= SAPP_KEYCODE_A && keycode <= SAPP_KEYCODE_Z) {
    char letter = (char)('A' + (keycode - SAPP_KEYCODE_A));
    snprintf(outText, outSize, "%c", letter);
    return;
  }

  if (keycode >= SAPP_KEYCODE_0 && keycode <= SAPP_KEYCODE_9) {
    char digit = (char)('0' + (keycode - SAPP_KEYCODE_0));
    snprintf(outText, outSize, "%c", digit);
    return;
  }

  switch (keycode) {
  case SAPP_KEYCODE_SPACE: snprintf(outText, outSize, "Space"); break;
  case SAPP_KEYCODE_ENTER: snprintf(outText, outSize, "Enter"); break;
  case SAPP_KEYCODE_TAB: snprintf(outText, outSize, "Tab"); break;
  case SAPP_KEYCODE_ESCAPE: snprintf(outText, outSize, "Esc"); break;
  case SAPP_KEYCODE_UP: snprintf(outText, outSize, "Up"); break;
  case SAPP_KEYCODE_DOWN: snprintf(outText, outSize, "Down"); break;
  case SAPP_KEYCODE_LEFT: snprintf(outText, outSize, "Left"); break;
  case SAPP_KEYCODE_RIGHT: snprintf(outText, outSize, "Right"); break;
  case SAPP_KEYCODE_BACKSPACE: snprintf(outText, outSize, "Backspace"); break;
  case SAPP_KEYCODE_F1: snprintf(outText, outSize, "F1"); break;
  case SAPP_KEYCODE_F2: snprintf(outText, outSize, "F2"); break;
  case SAPP_KEYCODE_F3: snprintf(outText, outSize, "F3"); break;
  case SAPP_KEYCODE_F4: snprintf(outText, outSize, "F4"); break;
  case SAPP_KEYCODE_F5: snprintf(outText, outSize, "F5"); break;
  case SAPP_KEYCODE_F6: snprintf(outText, outSize, "F6"); break;
  case SAPP_KEYCODE_F7: snprintf(outText, outSize, "F7"); break;
  case SAPP_KEYCODE_F8: snprintf(outText, outSize, "F8"); break;
  case SAPP_KEYCODE_F9: snprintf(outText, outSize, "F9"); break;
  case SAPP_KEYCODE_F10: snprintf(outText, outSize, "F10"); break;
  case SAPP_KEYCODE_F11: snprintf(outText, outSize, "F11"); break;
  case SAPP_KEYCODE_F12: snprintf(outText, outSize, "F12"); break;
  default:
    snprintf(outText, outSize, "Key %d", keycode);
    break;
  }
}

static void FormatMappingText(Mapping_t mapping, char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  if (mapping.raw == 0) {
    snprintf(outText, outSize, "Unbound");
    return;
  }

  if (mapping.deviceType == INPUT_DEVICETYPE_KEYBOARD && mapping.inputType == INPUT_INPUTTYPE_BUTTON) {
    char keyName[32];
    FormatKeyName(mapping.inputID, keyName, sizeof(keyName));
    snprintf(outText, outSize, "Key %s", keyName);
    return;
  }

  if (mapping.deviceType == INPUT_DEVICETYPE_MOUSE) {
    if (mapping.inputType == INPUT_INPUTTYPE_BUTTON) {
      snprintf(outText, outSize, "Mouse Btn %d", mapping.inputID);
    } else if (mapping.inputType == INPUT_INPUTTYPE_MOTION) {
      snprintf(outText, outSize, "Mouse Axis %d", mapping.inputID);
    } else {
      snprintf(outText, outSize, "Mouse Input %d", mapping.inputID);
    }
    return;
  }

  if (mapping.deviceType == INPUT_DEVICETYPE_CONTROLLER) {
    if (mapping.inputType == INPUT_INPUTTYPE_BUTTON) {
      snprintf(outText, outSize, "Pad%d Btn %d", mapping.deviceID + 1, mapping.inputID);
    } else if (mapping.inputType == INPUT_INPUTTYPE_AXIS) {
      snprintf(outText, outSize, "Pad%d Axis %d", mapping.deviceID + 1, mapping.inputID);
    } else {
      snprintf(outText, outSize, "Pad%d Input %d", mapping.deviceID + 1, mapping.inputID);
    }
    return;
  }

  snprintf(outText, outSize, "Unbound");
}

static void BuildHotkeyLabel(HotkeyAction_t action, const char* name, char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  int32_t keycode = Backend_GetHotkey(action);
  if (keycode != 0) {
    char keyName[32];
    FormatKeyName(keycode, keyName, sizeof(keyName));
    snprintf(outText, outSize, "[%s] %s", keyName, name);
  } else {
    snprintf(outText, outSize, "%s", name);
  }
}

static void FormatScanResultValue(uint32_t offset, char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  const uint8_t* ram = Bus_GetRamBytes();
  uint32_t ramSize = Bus_GetRamByteSize();
  if (!ram || offset >= ramSize) {
    snprintf(outText, outSize, "N/A");
    return;
  }

  if (scanType == MEMORY_SCAN_INT8) {
    uint8_t value = ram[offset];
    if (scanHex) {
      snprintf(outText, outSize, "0x%02x", value);
    } else if (scanSigned) {
      snprintf(outText, outSize, "%d", (int8_t)value);
    } else {
      snprintf(outText, outSize, "%u", (uint8_t)value);
    }
    return;
  }

  if (scanType == MEMORY_SCAN_INT16) {
    if (offset + 1 >= ramSize) {
      snprintf(outText, outSize, "N/A");
      return;
    }
    uint16_t value = (uint16_t)ram[offset] | ((uint16_t)ram[offset + 1] << 8);
    if (scanHex) {
      snprintf(outText, outSize, "0x%04x", value);
    } else if (scanSigned) {
      snprintf(outText, outSize, "%d", (int16_t)value);
    } else {
      snprintf(outText, outSize, "%u", (uint16_t)value);
    }
    return;
  }

  if (scanType == MEMORY_SCAN_INT32) {
    if (offset + 3 >= ramSize) {
      snprintf(outText, outSize, "N/A");
      return;
    }
    uint32_t value = (uint32_t)ram[offset]
      | ((uint32_t)ram[offset + 1] << 8)
      | ((uint32_t)ram[offset + 2] << 16)
      | ((uint32_t)ram[offset + 3] << 24);
    if (scanHex) {
      snprintf(outText, outSize, "0x%08x", value);
    } else if (scanSigned) {
      snprintf(outText, outSize, "%d", (int32_t)value);
    } else {
      snprintf(outText, outSize, "%u", value);
    }
    return;
  }

  if (scanType == MEMORY_SCAN_STR_UTF8) {
    size_t outPos = 0;
    while (offset < ramSize && outPos + 1 < outSize) {
      uint8_t c = ram[offset++];
      if (c == 0) break;
      if (c >= 32 && c <= 126) {
        outText[outPos++] = (char)c;
      } else {
        outText[outPos++] = '.';
      }
      if (outPos >= 16) break;
    }
    outText[outPos] = '\0';
    if (outPos == 0) snprintf(outText, outSize, "(empty)");
    return;
  }

  if (scanType == MEMORY_SCAN_STR_UTF16) {
    size_t outPos = 0;
    while (offset + 1 < ramSize && outPos + 1 < outSize) {
      uint8_t lo = ram[offset];
      uint8_t hi = ram[offset + 1];
      offset += 2;
      if (lo == 0 && hi == 0) break;
      if (hi == 0 && lo >= 32 && lo <= 126) {
        outText[outPos++] = (char)lo;
      } else {
        outText[outPos++] = '.';
      }
      if (outPos >= 16) break;
    }
    outText[outPos] = '\0';
    if (outPos == 0) snprintf(outText, outSize, "(empty)");
    return;
  }

  snprintf(outText, outSize, "N/A");
}

static bool GetLocalTimeSafe(const time_t* timeValue, struct tm* outTime) {
  if (!timeValue || !outTime) return false;
#if defined(_WIN32)
  return localtime_s(outTime, timeValue) == 0;
#else
  return localtime_r(timeValue, outTime) != NULL;
#endif
}

static void FormatSavestateTimestamp(int32_t slot, char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  char path[256];
  if (!Backend_GetStatePath(slot, path, sizeof(path)))
    return;

  struct stat st;
  if (stat(path, &st) != 0)
    return;

  struct tm timeInfo;
  if (!GetLocalTimeSafe(&st.st_mtime, &timeInfo))
    return;

  strftime(outText, outSize, "%Y-%m-%d %H:%M", &timeInfo);
}

static bool HasBinExtension(const char* name) {
  if (!name) return false;
  const char* dot = strrchr(name, '.');
  if (!dot || dot == name) return false;
  if (strlen(dot) != 4) return false;
  return (tolower((unsigned char)dot[1]) == 'b'
    && tolower((unsigned char)dot[2]) == 'i'
    && tolower((unsigned char)dot[3]) == 'n');
}

static void RefreshRomList(void) {
  romListCount = 0;
#ifdef _WIN32
  WIN32_FIND_DATAA findData;
  HANDLE handle = FindFirstFileA("roms\\*", &findData);
  if (handle == INVALID_HANDLE_VALUE) return;
  do {
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    if (!HasBinExtension(findData.cFileName)) continue;
    if (romListCount >= ROM_LIST_MAX) break;
    strncpy(romList[romListCount], findData.cFileName, ROM_NAME_MAX);
    romList[romListCount][ROM_NAME_MAX - 1] = '\0';
    romListCount++;
  } while (FindNextFileA(handle, &findData));
  FindClose(handle);
#else
  DIR* dir = opendir("roms");
  if (!dir) return;
  struct dirent* entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') continue;
    if (!HasBinExtension(entry->d_name)) continue;
    if (romListCount >= ROM_LIST_MAX) break;

    char path[512];
    snprintf(path, sizeof(path), "roms%c%s", PATH_CHAR, entry->d_name);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

    strncpy(romList[romListCount], entry->d_name, ROM_NAME_MAX);
    romList[romListCount][ROM_NAME_MAX - 1] = '\0';
    romListCount++;
  }
  closedir(dir);
#endif
}

static void BuildCheatInputStatus(char* outText, size_t outSize) {
  if (!outText || outSize == 0) return;
  outText[0] = '\0';

  uint32_t buttons = Input_GetButtons(0) | Input_GetButtons(1);
  if (buttons == 0) {
    snprintf(outText, outSize, "Buttons: (none)");
    return;
  }

  snprintf(outText, outSize, "Buttons:");
  if (buttons & (1 << INPUT_HELP)) strncat(outText, " Exit", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_EXIT)) strncat(outText, " Help", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_ENTER)) strncat(outText, " Enter", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_ABC)) strncat(outText, " ABC", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_UP)) strncat(outText, " Up", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_DOWN)) strncat(outText, " Down", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_LEFT)) strncat(outText, " Left", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_RIGHT)) strncat(outText, " Right", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_RED)) strncat(outText, " Red", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_YELLOW)) strncat(outText, " Yellow", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_BLUE)) strncat(outText, " Blue", outSize - strlen(outText) - 1);
  if (buttons & (1 << INPUT_GREEN)) strncat(outText, " Green", outSize - strlen(outText) - 1);
}

static void InitAddCheatFromOffset(uint32_t byteOffset) {
  addCheatAddr = byteOffset / 2;
  addCheatNameLen = 0;
  memset(addCheatName, 0, sizeof(addCheatName));

  uint16_t mask = 0xFFFF;
  uint16_t value = Bus_Load(addCheatAddr);

  const uint8_t* ram = Bus_GetRamBytes();
  uint32_t ramSize = Bus_GetRamByteSize();
  if (ram && byteOffset < ramSize) {
    uint8_t byteValue = ram[byteOffset];
    if (scanType == MEMORY_SCAN_INT8) {
      if ((byteOffset & 1) == 0) {
        mask = 0x00FF;
        value = (uint16_t)byteValue;
      } else {
        mask = 0xFF00;
        value = (uint16_t)(byteValue << 8);
      }
    } else if (scanType == MEMORY_SCAN_INT16 || scanType == MEMORY_SCAN_INT32) {
      mask = 0xFFFF;
    } else {
      mask = 0xFFFF;
    }
  }

  snprintf(addCheatValue, sizeof(addCheatValue), "%04x", value);
  addCheatValueLen = (int)strlen(addCheatValue);
  snprintf(addCheatMask, sizeof(addCheatMask), "%04x", mask);
  addCheatMaskLen = (int)strlen(addCheatMask);

  addCheatEnabled = true;
  addCheatPersistent = true;
  addCheatWatch = false;
  addCheatWatchHex = false;
  addCheatMode = 0;
  snprintf(addCheatStep, sizeof(addCheatStep), "%04x", 1);
  addCheatStepLen = (int)strlen(addCheatStep);
  snprintf(addCheatMin, sizeof(addCheatMin), "%04x", 0);
  addCheatMinLen = (int)strlen(addCheatMin);
  snprintf(addCheatMax, sizeof(addCheatMax), "%04x", 0xFFFF);
  addCheatMaxLen = (int)strlen(addCheatMax);
  addCheatLimitEnabled = true;
  addCheatCondition = 0;
}

static void InitEditCheat(int32_t index) {
  const CheatEntry_t* entry = Cheats_GetCheat(index);
  if (!entry) return;

  editCheatIndex = index;
  addCheatAddr = entry->addr;
  addCheatEnabled = entry->enabled ? 1 : 0;
  addCheatPersistent = entry->persistent ? 1 : 0;
  addCheatWatch = entry->watchEnabled ? 1 : 0;
  addCheatWatchHex = entry->watchHex ? 1 : 0;
  addCheatMode = (int)entry->mode;

  strncpy(addCheatName, entry->name, CHEATS_MAX_NAME - 1);
  addCheatName[CHEATS_MAX_NAME - 1] = '\0';
  addCheatNameLen = (int)strlen(addCheatName);

  snprintf(addCheatValue, sizeof(addCheatValue), "%04x", entry->value);
  addCheatValueLen = (int)strlen(addCheatValue);
  snprintf(addCheatMask, sizeof(addCheatMask), "%04x", entry->mask);
  addCheatMaskLen = (int)strlen(addCheatMask);
  snprintf(addCheatStep, sizeof(addCheatStep), "%04x", entry->stepValue);
  addCheatStepLen = (int)strlen(addCheatStep);
  snprintf(addCheatMin, sizeof(addCheatMin), "%04x", entry->minValue);
  addCheatMinLen = (int)strlen(addCheatMin);
  snprintf(addCheatMax, sizeof(addCheatMax), "%04x", entry->maxValue);
  addCheatMaxLen = (int)strlen(addCheatMax);
  addCheatLimitEnabled = entry->limitEnabled ? 1 : 0;

  if (entry->condition == CHEAT_CONDITION_HINT) {
    addCheatCondition = 1;
  } else if (entry->condition == CHEAT_CONDITION_BUTTON) {
    switch (entry->conditionButton) {
    case INPUT_ENTER: addCheatCondition = 2; break;
    case INPUT_EXIT: addCheatCondition = 3; break;
    case INPUT_ABC: addCheatCondition = 4; break;
    case INPUT_UP: addCheatCondition = 5; break;
    case INPUT_DOWN: addCheatCondition = 6; break;
    case INPUT_LEFT: addCheatCondition = 7; break;
    case INPUT_RIGHT: addCheatCondition = 8; break;
    case INPUT_RED: addCheatCondition = 9; break;
    case INPUT_YELLOW: addCheatCondition = 10; break;
    case INPUT_BLUE: addCheatCondition = 11; break;
    case INPUT_GREEN: addCheatCondition = 12; break;
    default: addCheatCondition = 0; break;
    }
  } else {
    addCheatCondition = 0;
  }
}

static void BuildCheatFromFields(CheatEntry_t* entry) {
  if (!entry) return;
  memset(entry, 0, sizeof(*entry));

  strncpy(entry->name, addCheatName, CHEATS_MAX_NAME - 1);
  entry->name[CHEATS_MAX_NAME - 1] = '\0';
  entry->addr = addCheatAddr;
  entry->enabled = addCheatEnabled != 0;
  entry->persistent = addCheatPersistent != 0;
  entry->watchEnabled = addCheatWatch != 0;
  entry->watchHex = addCheatWatchHex != 0;
  entry->mode = (CheatMode_t)addCheatMode;
  entry->stepValue = (uint16_t)strtoul(addCheatStep, NULL, 16);
  entry->minValue = (uint16_t)strtoul(addCheatMin, NULL, 16);
  entry->maxValue = (uint16_t)strtoul(addCheatMax, NULL, 16);
  entry->limitEnabled = addCheatLimitEnabled != 0;
  entry->value = (uint16_t)strtoul(addCheatValue, NULL, 16);
  entry->mask = (uint16_t)strtoul(addCheatMask, NULL, 16);

  if (addCheatCondition == 1) {
    entry->condition = CHEAT_CONDITION_HINT;
    entry->conditionButton = INPUT_HELP;
  } else if (addCheatCondition >= 2) {
    entry->condition = CHEAT_CONDITION_BUTTON;
    switch (addCheatCondition) {
    case 2: entry->conditionButton = INPUT_ENTER; break;
    case 3: entry->conditionButton = INPUT_EXIT; break;
    case 4: entry->conditionButton = INPUT_ABC; break;
    case 5: entry->conditionButton = INPUT_UP; break;
    case 6: entry->conditionButton = INPUT_DOWN; break;
    case 7: entry->conditionButton = INPUT_LEFT; break;
    case 8: entry->conditionButton = INPUT_RIGHT; break;
    case 9: entry->conditionButton = INPUT_RED; break;
    case 10: entry->conditionButton = INPUT_YELLOW; break;
    case 11: entry->conditionButton = INPUT_BLUE; break;
    case 12: entry->conditionButton = INPUT_GREEN; break;
    default: entry->condition = CHEAT_CONDITION_NONE; entry->conditionButton = INPUT_HELP; break;
    }
  } else {
    entry->condition = CHEAT_CONDITION_NONE;
    entry->conditionButton = INPUT_HELP;
  }
}


// Helper functions //

void fPercentSlider(const char* name, float min, float max, float step, float* value) {
  char text[256];
  float percent = ((*value) / max) * (max * 100.0f);
  snprintf((char*)&text, 256, "%s (%d%%)", name, (int)percent);
  nk_label(ctx, text, NK_TEXT_LEFT);
  nk_slider_float(ctx, min, value, max, step);
}


static char     updateText[5];
static int      updateTextLen;
static uint32_t holdAddr = 0xffffffff;
void IOReg(const char* name, uint16_t addr) {
  if (name)
    nk_label(ctx, name, NK_TEXT_LEFT);

  if (addr == holdAddr) {
    nk_flags flags = nk_edit_string(ctx, NK_EDIT_SIMPLE, updateText, &updateTextLen, 5, nk_filter_hex);
    if (flags & NK_EDIT_ACTIVATED) {
      holdAddr = addr;
      updateTextLen = 0;
      memset(updateText, 0, 5);
      Input_SetControlsEnable(false);
    }
    else if (flags & (NK_EDIT_COMMITED | NK_EDIT_DEACTIVATED)) {
      if (updateTextLen > 0)
        Bus_Store(addr, strtol(updateText, NULL, 16));
      holdAddr = 0xffffffff;
      Input_SetControlsEnable(true);
    }
  } else {
    char text[5];
    uint16_t value = Bus_Load(addr);
    snprintf((char*)&text, 5, "%04x", value);

    if (nk_button_label(ctx, text)) {
      if (holdAddr != 0xffffffff && updateTextLen > 0)
        Bus_Store(holdAddr, strtol(updateText, NULL, 16));
      holdAddr = addr;
      updateTextLen = 0;
      memset(updateText, 0, 5);
      Backend_SetControlsEnable(false);
    }

  }
}


void controllerMapping(const char* buttonName, char* mappingText, int* mappingLen) {
  nk_label(ctx, buttonName, NK_TEXT_LEFT);
  // nk_flags flags = nk_edit_string(ctx, NK_EDIT_SIMPLE, mappingText, mappingLen, 16, nk_filter_hex);
  // if (flags & (NK_EDIT_COMMITED | NK_EDIT_DEACTIVATED)) {
  //   Backend_UpdateButtonMapping(buttonName, mappingText, (*mappingLen));
}


static nk_bool openColorPicker = false;
static struct nk_colorf pickedColor;
static uint8_t colorToPick;
void chooseColor(struct nk_context* ctx, const char* name, uint8_t type) {
  struct nk_color color;
  switch (type) {
  case THEME_TEXT:   color = theme[NK_COLOR_TEXT];   break;
  case THEME_HEADER: color = theme[NK_COLOR_HEADER]; break;
  case THEME_BORDER: color = theme[NK_COLOR_BORDER]; break;
  case THEME_WINDOW: color = theme[NK_COLOR_WINDOW]; break;
  case THEME_BUTTON: color = theme[NK_COLOR_BUTTON]; break;
  default:           color = theme[NK_COLOR_TEXT];   break;
  }

  nk_layout_row_dynamic(ctx, 25, 3);
  nk_label(ctx, name, NK_TEXT_LEFT);
  nk_button_color(ctx, color);
  if (nk_button_label(ctx, "Choose...")) {
    openColorPicker = true;
    colorToPick = type;
    pickedColor.r = color.r / 255.0f;
    pickedColor.g = color.g / 255.0f;
    pickedColor.b = color.b / 255.0f;
    pickedColor.a = 1.0f;
  }
}


void updateColor(uint8_t type, struct nk_color chosen) {
  // Adjust click and highlight color
  struct nk_color hover  = nk_rgba_u32(((nk_color_u32(chosen) & 0xefefefef) << 1) | 0xff010101);
  struct nk_color active = nk_rgba_u32(((nk_color_u32(chosen) & 0x3f3f3f3f) << 2) | 0xff030303);
  struct nk_color blank  = nk_rgba_u32(((nk_color_u32(chosen) & 0xfefefefe) >> 1) | 0xff000000);

  switch (type) {
  case THEME_TEXT:   theme[NK_COLOR_TEXT]   = chosen; break;
  case THEME_HEADER: theme[NK_COLOR_HEADER] = chosen; break;
  case THEME_BORDER: theme[NK_COLOR_BORDER] = chosen; break;

  case THEME_BUTTON: {
    theme[NK_COLOR_BUTTON]        = chosen;
    theme[NK_COLOR_BUTTON_HOVER]  = hover;
    theme[NK_COLOR_BUTTON_ACTIVE] = active;

    theme[NK_COLOR_TOGGLE]        = chosen;
    theme[NK_COLOR_TOGGLE_HOVER]  = hover;
    theme[NK_COLOR_TOGGLE_CURSOR] = active;

    theme[NK_COLOR_SLIDER]               = blank;
    theme[NK_COLOR_SLIDER_CURSOR]        = chosen;
    theme[NK_COLOR_SLIDER_CURSOR_HOVER]  = hover;
    theme[NK_COLOR_SLIDER_CURSOR_ACTIVE] = active;

    theme[NK_COLOR_SCROLLBAR]               = blank;
    theme[NK_COLOR_SCROLLBAR_CURSOR]        = hover;
    theme[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = active;
    theme[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = hover;
  } break;

  case THEME_WINDOW: {
    theme[NK_COLOR_WINDOW]      = chosen;
    theme[NK_COLOR_COMBO]       = blank;
    theme[NK_COLOR_EDIT]        = blank;
    theme[NK_COLOR_EDIT_CURSOR] = chosen;
  } break;

  }
}


// UI functions //

bool UI_Init() {
  // memset(&this, 0, sizeof(UI_t));

  snk_setup(&(snk_desc_t){
    .dpi_scale = sapp_dpi_scale()
  });

  char hex[8];
  struct nk_color color;

  if (UserSettings_ReadString("textColor", hex, 8)) {
    color = nk_rgb_hex(hex);
    updateColor(THEME_TEXT, color);
  }

  if (UserSettings_ReadString("headerColor", hex, 8)) {
    color = nk_rgb_hex(hex);
    updateColor(THEME_HEADER, color);
  }

  if (UserSettings_ReadString("borderColor", hex, 8)) {
    color = nk_rgb_hex(hex);
    updateColor(THEME_BORDER, color);
  }

  if (UserSettings_ReadString("windowColor", hex, 8)) {
    color = nk_rgb_hex(hex);
    updateColor(THEME_WINDOW, color);
  }

  if (UserSettings_ReadString("buttonColor", hex, 8)) {
    color = nk_rgb_hex(hex);
    updateColor(THEME_BUTTON, color);
  }

  bool storedKeepAR = false;
  if (UserSettings_ReadBool("keepAspectRatio", &storedKeepAR)) {
    keepAR = storedKeepAR;
  }
  Backend_SetKeepAspectRatio(keepAR != 0);

  bool storedPixelPerfect = false;
  if (UserSettings_ReadBool("pixelPerfectScale", &storedPixelPerfect)) {
    pixelPerfectScale = storedPixelPerfect;
  }
  Backend_SetPixelPerfectScale(pixelPerfectScale != 0);


  return true;
}


void UI_Cleanup() {
  snk_shutdown();
}


void UI_HandleEvent(sapp_event* event) {
  snk_handle_event(event);
}


void UI_StartFrame() {
  ctx = snk_new_frame();
}


void UI_RunFrame() {
  if (!showUI)
    return;

  if (!themeUpdated) {
    nk_style_from_table(ctx, theme);
    themeUpdated = true;
  }

  if (hotkeyCaptureIndex >= 0) {
    Mapping_t lastEvent;
    if (Input_GetLastEvent(&lastEvent)) {
      if (lastEvent.deviceType == INPUT_DEVICETYPE_KEYBOARD && lastEvent.inputType == INPUT_INPUTTYPE_BUTTON && lastEvent.value != 0) {
        if (lastEvent.inputID == SAPP_KEYCODE_ESCAPE) {
          hotkeyCaptureIndex = -1;
        } else {
          Backend_SetHotkey((HotkeyAction_t)hotkeyCaptureIndex, lastEvent.inputID);
          hotkeyCaptureIndex = -1;
        }
        Input_ClearLastEvent();
        Input_SetControlsEnable(true);
      }
    }
  }

  if (mapCaptureCtrl >= 0 && mapCaptureInput >= 0) {
    Mapping_t lastEvent;
    if (Input_GetLastEvent(&lastEvent)) {
      if (lastEvent.deviceType == INPUT_DEVICETYPE_KEYBOARD && lastEvent.inputType == INPUT_INPUTTYPE_BUTTON && lastEvent.inputID == SAPP_KEYCODE_ESCAPE) {
        mapCaptureCtrl = -1;
        mapCaptureInput = -1;
        Input_ClearLastEvent();
        Input_SetControlsEnable(true);
      } else {
        bool validDevice =
          lastEvent.deviceType == INPUT_DEVICETYPE_KEYBOARD ||
          lastEvent.deviceType == INPUT_DEVICETYPE_MOUSE ||
          lastEvent.deviceType == INPUT_DEVICETYPE_CONTROLLER;
        bool validType = lastEvent.inputType == INPUT_INPUTTYPE_BUTTON || lastEvent.inputType == INPUT_INPUTTYPE_AXIS;
        if (validDevice && validType && lastEvent.value != 0) {
          Mapping_t mapping = lastEvent;
          if (lastEvent.inputType == INPUT_INPUTTYPE_AXIS) {
            mapping.value = (lastEvent.value >= 0) ? 0x4000 : (int16_t)-0x4000;
          } else {
            mapping.value = 0x7fff;
          }
          Input_SetMapping((uint8_t)mapCaptureCtrl, (uint8_t)mapCaptureInput, mapping);
          Input_SaveMappings();
          mapCaptureCtrl = -1;
          mapCaptureInput = -1;
          Input_ClearLastEvent();
          Input_SetControlsEnable(true);
        }
      }
    }
  }

  const float width  = sapp_widthf();
  const float height = sapp_heightf();

  char* romPath = NULL;
  char quickSaveLabel[64];
  char quickLoadLabel[64];
  char pauseLabel[64];
  char stepLabel[64];
  char resetLabel[64];
  char toggleUiLabel[64];
  char toggleFullscreenLabel[64];
  int quickSaveSlot = Backend_GetQuickSaveSlot();
  int quickLoadSlot = Backend_GetQuickLoadSlot();
  char saveSlotLabel[48];
  char loadSlotLabel[48];
  snprintf(saveSlotLabel, sizeof(saveSlotLabel), "Save State (Slot %02d)", quickSaveSlot);
  snprintf(loadSlotLabel, sizeof(loadSlotLabel), "Load State (Slot %02d)", quickLoadSlot);
  BuildHotkeyLabel(HOTKEY_SAVE_STATE, saveSlotLabel, quickSaveLabel, sizeof(quickSaveLabel));
  BuildHotkeyLabel(HOTKEY_LOAD_STATE, loadSlotLabel, quickLoadLabel, sizeof(quickLoadLabel));
  BuildHotkeyLabel(HOTKEY_PAUSE, "Pause", pauseLabel, sizeof(pauseLabel));
  BuildHotkeyLabel(HOTKEY_STEP, "Step", stepLabel, sizeof(stepLabel));
  BuildHotkeyLabel(HOTKEY_RESET, "Reset", resetLabel, sizeof(resetLabel));
  BuildHotkeyLabel(HOTKEY_TOGGLE_UI, "Toggle UI", toggleUiLabel, sizeof(toggleUiLabel));
  BuildHotkeyLabel(HOTKEY_TOGGLE_FULLSCREEN, "Toggle Fullscreen", toggleFullscreenLabel, sizeof(toggleFullscreenLabel));

  // Toolbar //
  if (nk_begin(ctx, "Toolbar", nk_rect(0, 0, width, 32), NK_WINDOW_NO_SCROLLBAR)) {
    nk_menubar_begin(ctx);
    nk_layout_row_begin(ctx, NK_STATIC, 24, 5);

    // Menu
    nk_layout_row_push(ctx, 48);
    if (nk_menu_begin_label(ctx, " Menu", NK_TEXT_LEFT, nk_vec2(220, 220))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "      Open ROM...",       NK_TEXT_LEFT)) romPath = (char*)Backend_OpenFileDialog("Please Select a ROM...");
      if (nk_menu_item_label(ctx, "      ROM Browser",        NK_TEXT_LEFT)) {
        showRomBrowser = true;
        romListDirty = true;
      }
      if (nk_menu_item_label(ctx, toggleFullscreenLabel, NK_TEXT_LEFT)) sapp_toggle_fullscreen();
      if (nk_menu_item_label(ctx, toggleUiLabel,         NK_TEXT_LEFT)) showUI = false;
      if (nk_menu_item_label(ctx, "      About",             NK_TEXT_LEFT)) showAbout = true;
      if (nk_menu_item_label(ctx, "[ESC] Quit",              NK_TEXT_LEFT)) sapp_request_quit();
      nk_menu_end(ctx);
    }

    // System
    nk_layout_row_push(ctx, 48);
    if (nk_menu_begin_label(ctx, "System", NK_TEXT_LEFT, nk_vec2(240, 280))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, pauseLabel,       NK_TEXT_LEFT)) VSmile_SetPause(!VSmile_GetPaused());
      if (nk_menu_item_label(ctx, stepLabel,        NK_TEXT_LEFT)) VSmile_Step();
      if (nk_menu_item_label(ctx, resetLabel,       NK_TEXT_LEFT)) VSmile_Reset();
      if (nk_menu_item_label(ctx, quickSaveLabel,   NK_TEXT_LEFT)) Backend_SaveStateSlot(quickSaveSlot);
      if (nk_menu_item_label(ctx, quickLoadLabel,   NK_TEXT_LEFT)) Backend_LoadStateSlot(quickLoadSlot);
      if (nk_menu_item_label(ctx, "    Savestates", NK_TEXT_LEFT)) showSaveStates = true;
      if (nk_menu_item_label(ctx, "    Hotkeys",    NK_TEXT_LEFT)) showHotkeys = true;
      if (nk_menu_item_label(ctx, "    Controller Config", NK_TEXT_LEFT)) showControllerConfig = true;
      if (nk_menu_item_label(ctx, "    Settings",    NK_TEXT_LEFT)) showEmuSettings = true;
      nk_menu_end(ctx);
    }

    // Debug //
    nk_layout_row_push(ctx, 56);
    if (nk_menu_begin_label(ctx, " Debug", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "    Registers", NK_TEXT_LEFT)) showRegisters = true;
      if (nk_menu_item_label(ctx, "    Memory",    NK_TEXT_LEFT)) showMemory    = true;
      if (nk_menu_item_label(ctx, "    CPU",       NK_TEXT_LEFT)) showCPU       = true;
      nk_menu_end(ctx);
    }

    // TAS
    nk_layout_row_push(ctx, 48);
    if (nk_menu_begin_label(ctx, " TAS", NK_TEXT_LEFT, nk_vec2(240, 380))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (!Backend_TAS_IsRecording()) {
        if (nk_menu_item_label(ctx, "    Start Recording", NK_TEXT_LEFT)) {
          Backend_TAS_Start();
        }
      } else {
        if (nk_menu_item_label(ctx, "    Stop Recording", NK_TEXT_LEFT)) {
          Backend_TAS_Stop();
        }
      }

      if (nk_menu_item_label(ctx, "    Load TAS...", NK_TEXT_LEFT)) {
        const char* path = Backend_OpenFileDialog("Open TAS Recording");
        if (path && path[0]) {
          Backend_TAS_Load(path);
        }
      }

      if (!Backend_TAS_IsPlaying()) {
        if (nk_menu_item_label(ctx, "    Start Playback", NK_TEXT_LEFT)) {
          Backend_TAS_StartPlayback();
        }
      } else {
        if (nk_menu_item_label(ctx, "    Stop Playback", NK_TEXT_LEFT)) {
          Backend_TAS_StopPlayback();
        }
      }

      char frameLabel[64];
      snprintf(frameLabel, sizeof(frameLabel), "Frames: %u", Backend_TAS_GetFrameCount());
      nk_label(ctx, frameLabel, NK_TEXT_LEFT);

      char playbackLabel[64];
      snprintf(playbackLabel, sizeof(playbackLabel), "Playback: %u/%u", Backend_TAS_GetPlaybackFrame(), Backend_TAS_GetPlaybackLength());
      nk_label(ctx, playbackLabel, NK_TEXT_LEFT);

      if (nk_menu_item_label(ctx, "    Save Recording...", NK_TEXT_LEFT)) {
        char defaultPath[256];
        const char* romTitle = Backend_GetRomTitle();
        if (romTitle && romTitle[0]) {
          snprintf(defaultPath, sizeof(defaultPath), "tas/%s.vtas", romTitle);
        } else {
          snprintf(defaultPath, sizeof(defaultPath), "tas/recording.vtas");
        }
        const char* path = Backend_SaveFileDialog("Save TAS Recording", defaultPath);
        if (path && path[0]) {
          Backend_TAS_Save(path);
        }
      }

      if (nk_menu_item_label(ctx, "    Clear Recording", NK_TEXT_LEFT)) {
        Backend_TAS_Clear();
      }

      if (nk_menu_item_label(ctx, "    Input Editor", NK_TEXT_LEFT)) {
        showTASEditor = true;
      }

      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, "Emulation Speed", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ctx, 24, 1);
      float tasSpeed = Backend_GetSpeed();
      nk_slider_float(ctx, 0.1f, &tasSpeed, 2.0f, 0.1f);
      Backend_SetSpeed(tasSpeed);

      nk_layout_row_dynamic(ctx, 25, 1);
      nk_bool startFromBoot = Backend_TAS_GetStartFromBoot();
      if (nk_checkbox_label(ctx, "Start From Boot", &startFromBoot)) {
        Backend_TAS_SetStartFromBoot(startFromBoot != 0);
      }

      nk_menu_end(ctx);
    }

    // Cheats
    nk_layout_row_push(ctx, 132);
    bool cheatsEnabledMenu = Cheats_GetGlobalEnabled();
    if (nk_menu_begin_label(ctx, cheatsEnabledMenu ? "Cheats" : "Cheats (disabled)", NK_TEXT_LEFT, nk_vec2(240, 140))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (!cheatsEnabledMenu) {
        nk_label(ctx, "Cheats are disabled", NK_TEXT_LEFT);
      } else {
        if (nk_menu_item_label(ctx, "    Manage Cheats", NK_TEXT_LEFT)) showCheats = true;
        if (nk_menu_item_label(ctx, "    Memory Scan", NK_TEXT_LEFT)) showMemoryScan = true;
      }
      nk_menu_end(ctx);
    }


    nk_menubar_end(ctx);

    nk_end(ctx);

    if (romPath != NULL) {
      VSmile_LoadROM(romPath);
      VSmile_Reset();
      VSmile_SetPause(false);
    }

  }


  // ROM Browser //
  if (showRomBrowser) {
    if (romListDirty) {
      RefreshRomList();
      romListDirty = false;
    }
    if (nk_begin(ctx, "ROM Browser", nk_rect(width*0.5f - 260, height*0.5f - 220, 520, 440), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Folder: roms", NK_TEXT_LEFT);
      if (nk_button_label(ctx, "Refresh")) {
        romListDirty = true;
      }

      char countLabel[64];
      snprintf(countLabel, sizeof(countLabel), "ROMs found: %d", romListCount);
      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, countLabel, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 340, 1);
      if (nk_group_begin(ctx, "RomList", NK_WINDOW_BORDER)) {
        if (romListCount == 0) {
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_label(ctx, "No V.Smile ROMs (.bin) found in the roms folder.", NK_TEXT_LEFT);
        }
        for (int i = 0; i < romListCount; i++) {
          nk_layout_row_begin(ctx, NK_STATIC, 24, 2);
          nk_layout_row_push(ctx, 360);
          nk_label(ctx, romList[i], NK_TEXT_LEFT);
          nk_layout_row_push(ctx, 100);
          if (nk_button_label(ctx, "Load")) {
            char path[512];
            snprintf(path, sizeof(path), "roms%c%s", PATH_CHAR, romList[i]);
            VSmile_LoadROM(path);
            VSmile_Reset();
            VSmile_SetPause(false);
          }
          nk_layout_row_end(ctx);
        }
        nk_group_end(ctx);
      }
    } else {
      showRomBrowser = false;
    }
    nk_end(ctx);
  }

  // Emulation Settings //
  if (showEmuSettings) {
    if (nk_begin(ctx, "Emulation Settings", nk_rect(width*0.5f - 600*0.5f, height*0.5f - 400*0.5f, 600, 400), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {

      if (nk_tree_push(ctx, NK_TREE_NODE, "System", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 3);

        // Emulation speed
        float emulationSpeed = Backend_GetSpeed();
        fPercentSlider("Emulation Speed", 0.1f, 2.0f, 0.1f, &emulationSpeed);
        if (nk_button_label(ctx, "Set to 100%")) emulationSpeed = 1.0f;
        Backend_SetSpeed(emulationSpeed);

        nk_layout_row_dynamic(ctx, 25, 2);

        // Set Region
        nk_label(ctx, "Region: ", NK_TEXT_LEFT);
        currRegion = nk_combo(ctx, regionText, NK_LEN(regionText), currRegion, 25, nk_vec2(200,200));
        VSmile_SetRegion(regionValues[currRegion]);

        nk_layout_row_dynamic(ctx, 25, 1);

        // Show Boot Screen
        nk_checkbox_label(ctx, "Show Boot Screen", &showIntro);
        VSmile_SetIntroEnable(showIntro);

        // Show LEDs
        nk_checkbox_label(ctx, "Show LEDs", &showLeds);
        Backend_ShowLeds(showLeds);

        nk_layout_row_dynamic(ctx, 25, 1);
        bool warningsEnabled = VSmile_GetWarningsEnabled();
        if (nk_checkbox_label(ctx, "Show Warnings (Non-fatal)", &warningsEnabled)) {
          VSmile_SetWarningsEnabled(warningsEnabled);
          UserSettings_WriteBool("warningsEnabled", warningsEnabled);
        }

        bool cheatsEnabled = Cheats_GetGlobalEnabled();
        if (nk_checkbox_label(ctx, "Enable Cheats", &cheatsEnabled)) {
          Cheats_SetGlobalEnabled(cheatsEnabled);
          UserSettings_WriteBool("cheatsEnabled", cheatsEnabled);
          if (!cheatsEnabled) {
            showCheats = false;
            showMemoryScan = false;
          }
        }

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "Graphics", NK_MINIMIZED)) {
        // Screen Filter
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Screen Filter", NK_TEXT_LEFT);
        filterMode = nk_combo(ctx, filterText, NK_LEN(filterText), filterMode, 25, nk_vec2(200,200));
        Backend_SetScreenFilter(filterMode);

        if (nk_checkbox_label(ctx, "Maintain Aspect Ratio", &keepAR)) {
          Backend_SetKeepAspectRatio(keepAR != 0);
          UserSettings_WriteBool("keepAspectRatio", keepAR != 0);
        }

        if (nk_checkbox_label(ctx, "Pixel Perfect Scale", &pixelPerfectScale)) {
          Backend_SetPixelPerfectScale(pixelPerfectScale != 0);
          UserSettings_WriteBool("pixelPerfectScale", pixelPerfectScale != 0);
        }

        // Layer Visibility
        nk_label(ctx, "Layer Visibility", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 3);
        nk_checkbox_label(ctx, "Show Layer 0", &showLayer[0]);
        PPU_SetLayerVisibility(0, showLayer[0]);
        nk_checkbox_label(ctx, "Show Layer 1", &showLayer[1]);
        PPU_SetLayerVisibility(1, showLayer[1]);
        nk_checkbox_label(ctx, "Show Sprites", &showLayer[2]);
        PPU_SetLayerVisibility(2, showLayer[2]);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Sprite Visualization", NK_LEFT);
        nk_layout_row_dynamic(ctx, 25, 2);

        // Sprite Outlines
        nk_checkbox_label(ctx, "Sprite Outlines", &showSpriteOutlines);
        PPU_SetSpriteOutlines(showSpriteOutlines);
        // Sprite Flip
        nk_checkbox_label(ctx, "Sprite Flip", &showSpriteFlip);
        PPU_SetFlipVisual(showSpriteFlip);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "Input Bindings", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Configure controller inputs", NK_LEFT);
        if (nk_button_label(ctx, "Open Controller Config")) {
          showControllerConfig = true;
        }

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "UI Theme", NK_MINIMIZED)) {
        chooseColor(ctx, "Text",    THEME_TEXT);
        chooseColor(ctx, "Header",  THEME_HEADER);
        chooseColor(ctx, "Border",  THEME_BORDER);
        chooseColor(ctx, "Window",  THEME_WINDOW);
        chooseColor(ctx, "Buttons", THEME_BUTTON);
        if (nk_button_label(ctx, "Save colors to profile")) {
          char hexColor[7];

          nk_color_hex_rgb(hexColor, theme[NK_COLOR_TEXT]);
          UserSettings_WriteString("textColor", hexColor, 7);
          nk_color_hex_rgb(hexColor, theme[NK_COLOR_HEADER]);
          UserSettings_WriteString("headerColor", hexColor, 7);
          nk_color_hex_rgb(hexColor, theme[NK_COLOR_BORDER]);
          UserSettings_WriteString("borderColor", hexColor, 7);
          nk_color_hex_rgb(hexColor, theme[NK_COLOR_WINDOW]);
          UserSettings_WriteString("windowColor", hexColor, 7);
          nk_color_hex_rgb(hexColor, theme[NK_COLOR_BUTTON]);
          UserSettings_WriteString("buttonColor", hexColor, 7);
        }

        nk_tree_pop(ctx);
      }


    } else showEmuSettings = false;
    nk_end(ctx);
  }


  // About //
  if (showAbout) {
    if (nk_begin(ctx, "About", nk_rect(width/2 - 480/2, height/2 - 360/2, 480, 360), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "V.Frown - The Experimental V.Smile Emulator", NK_TEXT_LEFT);
        nk_label(ctx, "Created by Schnert0 and contributors", NK_TEXT_LEFT);
        nk_label(ctx, "V.Frown emulator core is licensed under the MIT license.",  NK_TEXT_LEFT);
        nk_label(ctx, "nuklear UI library is licensed under the public domain license.",  NK_TEXT_LEFT);
        nk_label(ctx, "Sokol libraries are licensed under the zlib/libpng license.",  NK_TEXT_LEFT);
        nk_label(ctx, "ini.h is dual licensed under the MIT and public domain licenses.",  NK_TEXT_LEFT);
        nk_label(ctx, "for source code and a full list of contributors, go to:", NK_TEXT_LEFT);
        nk_label(ctx, "https://github.com/Schnert0/VFrown", NK_TEXT_LEFT);
    } else showAbout = false;
    nk_end(ctx);
  }

  // Hotkeys //
  if (showHotkeys) {
    if (nk_begin(ctx, "Hotkeys", nk_rect(width/2 - 320, height/2 - 220, 640, 440), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 24, 1);
      if (hotkeyCaptureIndex >= 0) {
        nk_label(ctx, "Press a key to bind. Esc to cancel.", NK_TEXT_LEFT);
      } else {
        nk_label(ctx, "Select Set to bind a hotkey.", NK_TEXT_LEFT);
      }

      nk_layout_row_dynamic(ctx, 24, 4);
      for (int32_t i = 0; i < HOTKEY_MAX; i++) {
        char keyName[32];
        FormatKeyName(Backend_GetHotkey((HotkeyAction_t)i), keyName, sizeof(keyName));
        nk_label(ctx, hotkeyActionText[i], NK_TEXT_LEFT);
        nk_label(ctx, keyName, NK_TEXT_LEFT);

        if (nk_button_label(ctx, "Set")) {
          hotkeyCaptureIndex = i;
          Input_ClearLastEvent();
          Input_SetControlsEnable(false);
        }

        if (nk_button_label(ctx, "Clear")) {
          Backend_SetHotkey((HotkeyAction_t)i, 0);
        }
      }
    } else {
      showHotkeys = false;
      hotkeyCaptureIndex = -1;
      Input_SetControlsEnable(true);
    }
    nk_end(ctx);
  }

  // Controller Config //
  if (showControllerConfig) {
    if (nk_begin(ctx, "Controller Config", nk_rect(width/2 - 360, height/2 - 260, 720, 520), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Controller", NK_TEXT_LEFT);
      controllerConfigIndex = nk_combo(ctx, controllerLabelText, NK_LEN(controllerLabelText), controllerConfigIndex, 24, nk_vec2(200, 120));

      nk_layout_row_dynamic(ctx, 20, 1);
      if (mapCaptureCtrl >= 0) {
        nk_label(ctx, "Press a button/axis to bind. Esc to cancel.", NK_TEXT_LEFT);
      } else {
        nk_label(ctx, "Select Set to bind an input.", NK_TEXT_LEFT);
      }

      nk_layout_row_dynamic(ctx, 360, 1);
      if (nk_group_begin(ctx, "ControllerBindings", NK_WINDOW_BORDER)) {
        for (int32_t i = 0; i < NUM_INPUTS; i++) {
          Mapping_t mapping;
          mapping.raw = 0;
          Input_GetMapping(controllerConfigIndex, (uint8_t)i, &mapping);

          char mappingText[64];
          FormatMappingText(mapping, mappingText, sizeof(mappingText));

          nk_layout_row_dynamic(ctx, 24, 4);
          nk_label(ctx, inputNameText[i], NK_TEXT_LEFT);
          nk_label(ctx, mappingText, NK_TEXT_LEFT);
          if (nk_button_label(ctx, "Set")) {
            mapCaptureCtrl = controllerConfigIndex;
            mapCaptureInput = i;
            Input_ClearLastEvent();
            Input_SetControlsEnable(false);
          }
          if (nk_button_label(ctx, "Clear")) {
            Mapping_t cleared;
            cleared.raw = 0;
            Input_SetMapping(controllerConfigIndex, (uint8_t)i, cleared);
            Input_SaveMappings();
          }
        }
        nk_group_end(ctx);
      }
    } else {
      showControllerConfig = false;
      mapCaptureCtrl = -1;
      mapCaptureInput = -1;
      Input_SetControlsEnable(true);
    }
    nk_end(ctx);
  }

  // Savestate Manager //
  if (showSaveStates) {
    if (nk_begin(ctx, "Savestates", nk_rect(width/2 - 320, height/2 - 260, 640, 520), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      const char* romTitle = Backend_GetRomTitle();
      bool hasRom = (romTitle && romTitle[0]);

      nk_layout_row_dynamic(ctx, 24, 1);
      if (hasRom) {
        char romLabel[300];
        snprintf(romLabel, sizeof(romLabel), "ROM: %s", romTitle);
        nk_label(ctx, romLabel, NK_TEXT_LEFT);
      } else {
        nk_label(ctx, "No ROM loaded. Savestates are disabled.", NK_TEXT_LEFT);
      }

      int quickSaveSlot = Backend_GetQuickSaveSlot();
      int quickLoadSlot = Backend_GetQuickLoadSlot();
      nk_layout_row_dynamic(ctx, 24, 4);
      nk_label(ctx, "Quick Save Slot", NK_TEXT_LEFT);
      nk_property_int(ctx, "##quick_save", 0, &quickSaveSlot, 99, 1, 1);
      nk_label(ctx, "Quick Load Slot", NK_TEXT_LEFT);
      nk_property_int(ctx, "##quick_load", 0, &quickLoadSlot, 99, 1, 1);
      Backend_SetQuickSaveSlot(quickSaveSlot);
      Backend_SetQuickLoadSlot(quickLoadSlot);

      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, "Tip: J/K hotkeys use the quick slots above.", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 24, 1);
      nk_label(ctx, "Slots", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 360, 1);
      if (nk_group_begin(ctx, "SavestateList", NK_WINDOW_BORDER)) {
        for (int32_t slot = 0; slot < 100; slot++) {
          bool exists = Backend_StateSlotExists(slot);
          char slotLabel[16];
          snprintf(slotLabel, sizeof(slotLabel), "Slot %02d", slot);

          char timestampText[32];
          if (exists) {
            FormatSavestateTimestamp(slot, timestampText, sizeof(timestampText));
            if (timestampText[0] == '\0') {
              snprintf(timestampText, sizeof(timestampText), "Unknown");
            }
          } else {
            timestampText[0] = '\0';
          }

          nk_layout_row_begin(ctx, NK_STATIC, 24, 6);
          nk_layout_row_push(ctx, 70);
          nk_label(ctx, slotLabel, NK_TEXT_LEFT);
          nk_layout_row_push(ctx, 60);
          nk_label(ctx, exists ? "Ready" : "Empty", NK_TEXT_LEFT);
          nk_layout_row_push(ctx, 190);
          nk_label(ctx, exists ? timestampText : "", NK_TEXT_LEFT);
          nk_layout_row_push(ctx, 60);
          if (nk_button_label(ctx, "Save")) {
            if (hasRom) Backend_SaveStateSlot(slot);
          }
          nk_layout_row_push(ctx, 60);
          if (exists) {
            if (nk_button_label(ctx, "Load")) {
              if (hasRom) Backend_LoadStateSlot(slot);
            }
          } else {
            nk_label(ctx, "", NK_TEXT_LEFT);
          }
          nk_layout_row_push(ctx, 60);
          if (exists) {
            if (nk_button_label(ctx, "Delete")) {
              if (hasRom) Backend_DeleteStateSlot(slot);
            }
          } else {
            nk_label(ctx, "", NK_TEXT_LEFT);
          }
          nk_layout_row_end(ctx);
        }
        nk_group_end(ctx);
      }
    } else {
      showSaveStates = false;
    }
    nk_end(ctx);
  }

  // Manage Cheats //
  if (showCheats) {
    if (nk_begin(ctx, "Manage Cheats", nk_rect(width/2 - 320, height/2 - 240, 640, 480), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 24, 1);
      if (Cheats_GetCount() == 0) {
        nk_label(ctx, "No cheats loaded for this ROM.", NK_TEXT_LEFT);
      } else {
        nk_layout_row_begin(ctx, NK_STATIC, 24, 4);
        nk_layout_row_push(ctx, 64);
        nk_label(ctx, "Enabled", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 320);
        nk_label(ctx, "Name", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 100);
        nk_label(ctx, "Persistent", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 60);
        nk_label(ctx, "Watch", NK_TEXT_LEFT);
        nk_layout_row_end(ctx);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Tip: select a cheat and press Enter to edit.", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 380, 1);
        if (nk_group_begin(ctx, "CheatList", NK_WINDOW_BORDER)) {
          for (int32_t i = 0; i < Cheats_GetCount(); i++) {
            const CheatEntry_t* entry = Cheats_GetCheat(i);
            if (!entry) continue;

            nk_layout_row_begin(ctx, NK_STATIC, 24, 4);
            nk_layout_row_push(ctx, 64);
            nk_bool enabled = entry->enabled ? 1 : 0;
            if (nk_checkbox_label(ctx, "", &enabled)) {
              Cheats_SetEnabled(i, enabled != 0);
              Cheats_Save();
            }

            nk_layout_row_push(ctx, 320);
            nk_bool selected = (manageSelectedIndex == i);
            if (nk_selectable_label(ctx, entry->name, NK_TEXT_LEFT, &selected)) {
              manageSelectedIndex = selected ? i : -1;
            }

            struct nk_rect bounds = nk_widget_bounds(ctx);
            if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, bounds)) {
              manageSelectedIndex = i;
              cheatContextIndex = i;
              cheatContextPos = ctx->input.mouse.pos;
              showCheatContext = true;
            }

            nk_layout_row_push(ctx, 100);
            nk_bool persistent = entry->persistent ? 1 : 0;
            if (nk_checkbox_label(ctx, "", &persistent)) {
              Cheats_SetPersistent(i, persistent != 0);
              Cheats_Save();
            }

            nk_layout_row_push(ctx, 60);
            nk_bool watchEnabled = entry->watchEnabled ? 1 : 0;
            if (nk_checkbox_label(ctx, "", &watchEnabled)) {
              Cheats_SetWatchEnabled(i, watchEnabled != 0);
              Cheats_Save();
            }
            nk_layout_row_end(ctx);
          }
          nk_group_end(ctx);
        }

        if (manageSelectedIndex >= 0 && nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
          InitEditCheat(manageSelectedIndex);
          showEditCheat = true;
          showCheatContext = false;
        }

        if (showCheatContext && cheatContextIndex >= 0) {
          struct nk_rect winBounds = nk_window_get_bounds(ctx);
          float popupX = cheatContextPos.x - winBounds.x;
          float popupY = cheatContextPos.y - winBounds.y;
          float popupW = 180.0f;
          float popupH = 100.0f;

          if (popupX < 0.0f) popupX = 0.0f;
          if (popupY < 0.0f) popupY = 0.0f;
          if (popupX + popupW > winBounds.w) popupX = winBounds.w - popupW;
          if (popupY + popupH > winBounds.h) popupY = winBounds.h - popupH;

          struct nk_rect popupBounds = nk_rect(popupX, popupY, popupW, popupH);
          if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "CheatContext", NK_WINDOW_NO_SCROLLBAR, popupBounds)) {
            nk_layout_row_dynamic(ctx, 24, 1);
            if (nk_button_label(ctx, "Edit")) {
              InitEditCheat(cheatContextIndex);
              showEditCheat = true;
              showCheatContext = false;
              nk_popup_close(ctx);
            }
            if (nk_button_label(ctx, "Close")) {
              showCheatContext = false;
              nk_popup_close(ctx);
            }
            nk_popup_end(ctx);
          } else {
            showCheatContext = false;
          }
        }
      }
    } else {
      showCheats = false;
      Cheats_Save();
    }
    nk_end(ctx);
  }

  // Memory Scan //
  if (showMemoryScan) {
    if (nk_begin(ctx, "Memory Scan", nk_rect(width/2 - 320, height/2 - 240, 640, 480), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Scan Type", NK_TEXT_LEFT);
      scanType = nk_combo(ctx, scanTypeText, NK_LEN(scanTypeText), scanType, 24, nk_vec2(200, 200));

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Compare", NK_TEXT_LEFT);
      scanCompare = nk_combo(ctx, scanCompareText, NK_LEN(scanCompareText), scanCompare, 24, nk_vec2(200, 200));

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Value", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, scanValueText, &scanValueTextLen, 63, nk_filter_default);
      if (scanValueTextLen < (int)sizeof(scanValueText)) {
        scanValueText[scanValueTextLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_checkbox_label(ctx, "Hex", &scanHex);
      nk_checkbox_label(ctx, "Signed", &scanSigned);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "", NK_TEXT_LEFT);
      nk_checkbox_label(ctx, "Match Case", &scanMatchCase);

      nk_layout_row_dynamic(ctx, 24, 3);
      if (nk_button_label(ctx, "New Scan")) {
        MemoryScan_New((MemoryScanType_t)scanType, scanValueText, scanHex, scanMatchCase, scanSigned, (MemoryScanCompare_t)scanCompare);
      }
      if (nk_button_label(ctx, "Refine Scan")) {
        MemoryScan_Refine((MemoryScanType_t)scanType, scanValueText, scanHex, scanMatchCase, scanSigned, (MemoryScanCompare_t)scanCompare);
      }
      if (nk_button_label(ctx, "Reset")) {
        MemoryScan_Reset();
      }

      nk_layout_row_dynamic(ctx, 24, 1);
      char resultsLabel[64];
      snprintf(resultsLabel, sizeof(resultsLabel), "Results (%d)", MemoryScan_GetResultCount());
      nk_label(ctx, resultsLabel, NK_TEXT_LEFT);
      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, "Tip: select a result and press Enter to add a cheat.", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 320, 1);
      if (nk_group_begin(ctx, "ScanResults", NK_WINDOW_BORDER)) {
        int32_t count = MemoryScan_GetResultCount();
        if (count == 0) {
          nk_layout_row_dynamic(ctx, 22, 1);
          nk_label(ctx, "No results yet.", NK_TEXT_LEFT);
        } else {
          int32_t showCount = count;
          if (showCount > 128) showCount = 128;
          nk_layout_row_dynamic(ctx, 22, 2);
          for (int32_t i = 0; i < showCount; i++) {
            char entryText[32];
            char valueText[64];
            uint32_t offset = MemoryScan_GetResultOffset(i);
            snprintf(entryText, sizeof(entryText), "0x%04x", offset);
            FormatScanResultValue(offset, valueText, sizeof(valueText));

            nk_bool selected = (scanSelectedIndex == i);
            if (nk_selectable_label(ctx, entryText, NK_TEXT_LEFT, &selected)) {
              scanSelectedIndex = selected ? i : -1;
            }

            struct nk_rect bounds = nk_widget_bounds(ctx);
            if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, bounds)) {
              scanSelectedIndex = i;
              scanContextIndex = i;
              scanContextPos = ctx->input.mouse.pos;
              showScanContext = true;
            }

            nk_label(ctx, valueText, NK_TEXT_LEFT);
          }
        }
        nk_group_end(ctx);
      }

      if (scanSelectedIndex >= 0 && nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
        uint32_t offset = MemoryScan_GetResultOffset(scanSelectedIndex);
        InitAddCheatFromOffset(offset);
        showAddCheat = true;
        showScanContext = false;
      }

      if (showScanContext && scanContextIndex >= 0) {
        struct nk_rect winBounds = nk_window_get_bounds(ctx);
        float popupX = scanContextPos.x - winBounds.x;
        float popupY = scanContextPos.y - winBounds.y;
        float popupW = 180.0f;
        float popupH = 80.0f;

        if (popupX < 0.0f) popupX = 0.0f;
        if (popupY < 0.0f) popupY = 0.0f;
        if (popupX + popupW > winBounds.w) popupX = winBounds.w - popupW;
        if (popupY + popupH > winBounds.h) popupY = winBounds.h - popupH;

        struct nk_rect popupBounds = nk_rect(popupX, popupY, popupW, popupH);
        if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "ScanContext", NK_WINDOW_NO_SCROLLBAR, popupBounds)) {
          nk_layout_row_dynamic(ctx, 24, 1);
          if (nk_button_label(ctx, "Add custom")) {
            uint32_t offset = MemoryScan_GetResultOffset(scanContextIndex);
            InitAddCheatFromOffset(offset);
            showAddCheat = true;
            showScanContext = false;
            nk_popup_close(ctx);
          }
          if (nk_button_label(ctx, "Close")) {
            showScanContext = false;
            nk_popup_close(ctx);
          }
          nk_popup_end(ctx);
        } else {
          showScanContext = false;
        }
      }
    } else {
      showMemoryScan = false;
    }
    nk_end(ctx);
  }

  // Add Cheat //
  if (showAddCheat) {
    if (nk_begin(ctx, "Add Cheat", nk_rect(width/2 - 260, height/2 - 180, 520, 360), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE)) {
      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Address", NK_TEXT_LEFT);
      char addrText[16];
      snprintf(addrText, sizeof(addrText), "0x%04x", addCheatAddr);
      nk_label(ctx, addrText, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Name", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatName, &addCheatNameLen, CHEATS_MAX_NAME - 1, nk_filter_default);
      if (addCheatNameLen < (int)sizeof(addCheatName)) {
        addCheatName[addCheatNameLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Value", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatValue, &addCheatValueLen, 7, nk_filter_hex);
      if (addCheatValueLen < (int)sizeof(addCheatValue)) {
        addCheatValue[addCheatValueLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Mask", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMask, &addCheatMaskLen, 7, nk_filter_hex);
      if (addCheatMaskLen < (int)sizeof(addCheatMask)) {
        addCheatMask[addCheatMaskLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Mode", NK_TEXT_LEFT);
      addCheatMode = nk_combo(ctx, cheatModeText, NK_LEN(cheatModeText), addCheatMode, 24, nk_vec2(200, 120));

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Condition", NK_TEXT_LEFT);
      addCheatCondition = nk_combo(ctx, cheatConditionText, NK_LEN(cheatConditionText), addCheatCondition, 24, nk_vec2(200, 120));

      nk_layout_row_dynamic(ctx, 20, 1);
      char inputStatus[256];
      BuildCheatInputStatus(inputStatus, sizeof(inputStatus));
      nk_label(ctx, inputStatus, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_checkbox_label(ctx, "Enabled", &addCheatEnabled);
      nk_checkbox_label(ctx, "Persistent", &addCheatPersistent);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_checkbox_label(ctx, "Watch", &addCheatWatch);
      nk_checkbox_label(ctx, "Watch Hex", &addCheatWatchHex);

      if (addCheatMode != 0) {
        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Step", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatStep, &addCheatStepLen, 7, nk_filter_hex);
        if (addCheatStepLen < (int)sizeof(addCheatStep)) {
          addCheatStep[addCheatStepLen] = '\0';
        }

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_checkbox_label(ctx, "Enable Limits", &addCheatLimitEnabled);
        nk_label(ctx, "", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Min", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMin, &addCheatMinLen, 7, nk_filter_hex);
        if (addCheatMinLen < (int)sizeof(addCheatMin)) {
          addCheatMin[addCheatMinLen] = '\0';
        }

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Max", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMax, &addCheatMaxLen, 7, nk_filter_hex);
        if (addCheatMaxLen < (int)sizeof(addCheatMax)) {
          addCheatMax[addCheatMaxLen] = '\0';
        }
      }

      nk_layout_row_dynamic(ctx, 28, 2);
      if (nk_button_label(ctx, "Add")) {
        if (addCheatNameLen > 0) {
          CheatEntry_t entry;
          BuildCheatFromFields(&entry);
          if (Cheats_AddCheat(&entry)) {
            Cheats_Save();
            showAddCheat = false;
          }
        }
      }
      if (nk_button_label(ctx, "Cancel")) {
        showAddCheat = false;
      }
    } else {
      showAddCheat = false;
    }
    nk_end(ctx);
  }

  // Edit Cheat //
  if (showEditCheat && editCheatIndex >= 0) {
    if (nk_begin(ctx, "Edit Cheat", nk_rect(width/2 - 260, height/2 - 180, 520, 360), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE)) {
      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Address", NK_TEXT_LEFT);
      char addrText[16];
      snprintf(addrText, sizeof(addrText), "0x%04x", addCheatAddr);
      nk_label(ctx, addrText, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Name", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatName, &addCheatNameLen, CHEATS_MAX_NAME - 1, nk_filter_default);
      if (addCheatNameLen < (int)sizeof(addCheatName)) {
        addCheatName[addCheatNameLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Value", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatValue, &addCheatValueLen, 7, nk_filter_hex);
      if (addCheatValueLen < (int)sizeof(addCheatValue)) {
        addCheatValue[addCheatValueLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Mask", NK_TEXT_LEFT);
      nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMask, &addCheatMaskLen, 7, nk_filter_hex);
      if (addCheatMaskLen < (int)sizeof(addCheatMask)) {
        addCheatMask[addCheatMaskLen] = '\0';
      }

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Mode", NK_TEXT_LEFT);
      addCheatMode = nk_combo(ctx, cheatModeText, NK_LEN(cheatModeText), addCheatMode, 24, nk_vec2(200, 120));

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_label(ctx, "Condition", NK_TEXT_LEFT);
      addCheatCondition = nk_combo(ctx, cheatConditionText, NK_LEN(cheatConditionText), addCheatCondition, 24, nk_vec2(200, 120));

      nk_layout_row_dynamic(ctx, 20, 1);
      char inputStatus[256];
      BuildCheatInputStatus(inputStatus, sizeof(inputStatus));
      nk_label(ctx, inputStatus, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_checkbox_label(ctx, "Enabled", &addCheatEnabled);
      nk_checkbox_label(ctx, "Persistent", &addCheatPersistent);

      nk_layout_row_dynamic(ctx, 24, 2);
      nk_checkbox_label(ctx, "Watch", &addCheatWatch);
      nk_checkbox_label(ctx, "Watch Hex", &addCheatWatchHex);

      if (addCheatMode != 0) {
        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Step", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatStep, &addCheatStepLen, 7, nk_filter_hex);
        if (addCheatStepLen < (int)sizeof(addCheatStep)) {
          addCheatStep[addCheatStepLen] = '\0';
        }

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_checkbox_label(ctx, "Enable Limits", &addCheatLimitEnabled);
        nk_label(ctx, "", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Min", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMin, &addCheatMinLen, 7, nk_filter_hex);
        if (addCheatMinLen < (int)sizeof(addCheatMin)) {
          addCheatMin[addCheatMinLen] = '\0';
        }

        nk_layout_row_dynamic(ctx, 24, 2);
        nk_label(ctx, "Max", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, addCheatMax, &addCheatMaxLen, 7, nk_filter_hex);
        if (addCheatMaxLen < (int)sizeof(addCheatMax)) {
          addCheatMax[addCheatMaxLen] = '\0';
        }
      }

      nk_layout_row_dynamic(ctx, 28, 3);
      if (nk_button_label(ctx, "Save")) {
        if (addCheatNameLen > 0) {
          CheatEntry_t entry;
          BuildCheatFromFields(&entry);
          if (Cheats_UpdateCheat(editCheatIndex, &entry)) {
            Cheats_Save();
            showEditCheat = false;
            editCheatIndex = -1;
          }
        }
      }
      if (nk_button_label(ctx, "Delete")) {
        showDeleteCheatConfirm = true;
      }
      if (nk_button_label(ctx, "Cancel")) {
        showEditCheat = false;
        editCheatIndex = -1;
      }
    } else {
      showEditCheat = false;
      editCheatIndex = -1;
    }
    nk_end(ctx);
  }

  // Delete Cheat Confirmation //
  if (showDeleteCheatConfirm && editCheatIndex >= 0) {
    if (nk_begin(ctx, "Delete Cheat?", nk_rect(width/2 - 200, height/2 - 80, 400, 160), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE)) {
      nk_layout_row_dynamic(ctx, 24, 1);
      nk_label(ctx, "This will permanently delete the cheat.", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ctx, 28, 2);
      if (nk_button_label(ctx, "Delete")) {
        Cheats_RemoveCheat(editCheatIndex);
        Cheats_Save();
        showDeleteCheatConfirm = false;
        showEditCheat = false;
        editCheatIndex = -1;
      }
      if (nk_button_label(ctx, "Cancel")) {
        showDeleteCheatConfirm = false;
      }
    } else {
      showDeleteCheatConfirm = false;
    }
    nk_end(ctx);
  }

  // Cheat Watch Overlay //
  int watchCount = 0;
  for (int32_t i = 0; i < Cheats_GetCount(); i++) {
    const CheatEntry_t* entry = Cheats_GetCheat(i);
    if (entry && entry->watchEnabled) {
      watchCount++;
    }
  }

  if (watchCount > 0) {
    float maxWatchHeight = height * 0.5f;
    if (maxWatchHeight < 140.0f) maxWatchHeight = 140.0f;
    if (maxWatchHeight > 360.0f) maxWatchHeight = 360.0f;

    float watchHeight = 26.0f + (float)watchCount * 20.0f;
    if (watchHeight > maxWatchHeight) watchHeight = maxWatchHeight;

    if (nk_begin(ctx, "Cheat Watch", nk_rect(10, 40, 260, watchHeight), NK_WINDOW_MOVABLE | NK_WINDOW_BORDER)) {
      nk_layout_row_dynamic(ctx, 20, 1);
      for (int32_t i = 0; i < Cheats_GetCount(); i++) {
        const CheatEntry_t* entry = Cheats_GetCheat(i);
        if (!entry || !entry->watchEnabled) continue;

        uint16_t value = Bus_Load(entry->addr);
        char nameText[CHEATS_MAX_NAME + 8];
        const char* label = entry->name[0] ? entry->name : "(unnamed)";
        if (entry->watchHex) {
          snprintf(nameText, sizeof(nameText), "%s: 0x%04x", label, value);
        } else {
          snprintf(nameText, sizeof(nameText), "%s: %u", label, (unsigned)value);
        }
        nk_label(ctx, nameText, NK_TEXT_LEFT);
      }
    }
    nk_end(ctx);
  }

  // TAS Input Editor //
  if (showTASEditor) {
    if (nk_begin(ctx, "TAS Input Editor", nk_rect(width/2 - 365, height/2 - 320, 730, 640), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      const int visibleFrames = 120;
      nk_layout_row_begin(ctx, NK_STATIC, 24, 4);
      nk_layout_row_push(ctx, 90);
      nk_label(ctx, "Page Start", NK_TEXT_LEFT);
      nk_layout_row_push(ctx, 220);
      nk_property_int(ctx, "##tas_start", 0, &tasEditorStartFrame, 1000000, visibleFrames, visibleFrames);
      nk_layout_row_push(ctx, 60);
      if (nk_button_label(ctx, "Prev")) {
        tasEditorStartFrame -= visibleFrames;
        if (tasEditorStartFrame < 0) tasEditorStartFrame = 0;
      }
      nk_layout_row_push(ctx, 60);
      if (nk_button_label(ctx, "Next")) {
        tasEditorStartFrame += visibleFrames;
      }
      nk_layout_row_end(ctx);

      if (Backend_TAS_IsRecording() || Backend_TAS_IsPlaying()) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Stop recording/playback to edit inputs.", NK_TEXT_LEFT);
      }

      nk_layout_row_dynamic(ctx, 520, 1);
      if (nk_group_begin(ctx, "TASEditorGrid", NK_WINDOW_BORDER)) {
        const float rowHeight = 20.0f;

        nk_layout_row_begin(ctx, NK_STATIC, rowHeight, 13);
        nk_layout_row_push(ctx, 44);
        nk_label(ctx, "Frame", NK_TEXT_LEFT);
        for (int i = 0; i < 12; i++) {
          nk_layout_row_push(ctx, 42);
          nk_label(ctx, inputNameText[i], NK_TEXT_LEFT);
        }
        nk_layout_row_end(ctx);

        for (int row = 0; row < visibleFrames; row++) {
          int frameIndex = tasEditorStartFrame + row;
          uint32_t buttons0 = 0;
          uint32_t buttons1 = 0;
          Backend_TAS_GetFrame((uint32_t)frameIndex, &buttons0, &buttons1);

          nk_layout_row_begin(ctx, NK_STATIC, rowHeight, 13);
          nk_layout_row_push(ctx, 44);
          char frameLabel[16];
          snprintf(frameLabel, sizeof(frameLabel), "%d", frameIndex);
          nk_label(ctx, frameLabel, NK_TEXT_LEFT);

          for (int col = 0; col < 12; col++) {
            nk_layout_row_push(ctx, 42);
            uint32_t mask = 1u << tasInputOrder[col];
            nk_bool pressed = (buttons0 & mask) != 0;
            if (nk_checkbox_label(ctx, "", &pressed)) {
              if (!(Backend_TAS_IsRecording() || Backend_TAS_IsPlaying())) {
                if (pressed) {
                  buttons0 |= mask;
                } else {
                  buttons0 &= ~mask;
                }
                Backend_TAS_SetFrame((uint32_t)frameIndex, buttons0, buttons1, true);
              }
            }
          }
          nk_layout_row_end(ctx);
        }

        nk_group_end(ctx);
      }
    } else {
      showTASEditor = false;
    }
    nk_end(ctx);
  }

  // Color Picker //
  if (openColorPicker) {
    const float width  = sapp_widthf();
    const float height = sapp_heightf();
    if (nk_begin(
      ctx, "Choose a color...",
      nk_rect(width*0.5f - 600*0.5f, height*0.5f - 400*0.5f, 600, 400),
      NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE
    )) {

      static char hexText[8];
      static int hexTextLen = 0;
      static bool updatingColor = false;

      nk_layout_row_static(ctx, 256, 256, 1);
      nk_color_pick(ctx, &pickedColor, NK_RGB);
      struct nk_color preview = {
        .r = pickedColor.r*255.0f,
        .g = pickedColor.g*255.0f,
        .b = pickedColor.b*255.0f,
        .a = pickedColor.a*255.0f,
      };

      nk_layout_row_dynamic(ctx, 25, 3);
      if (nk_button_label(ctx, "Confirm")) {
        if (updatingColor) {
          updatingColor = false;
          preview = nk_rgb_hex(hexText);
          pickedColor.r = preview.r/255.0f;
          pickedColor.g = preview.g/255.0f;
          pickedColor.b = preview.b/255.0f;
          pickedColor.a = preview.a/255.0f;
        }

        updateColor(colorToPick, preview);

        themeUpdated = false;
        openColorPicker = false;
      }

      nk_flags flags = nk_edit_string(ctx, NK_EDIT_SIMPLE, hexText, &hexTextLen, 8, nk_filter_hex);
      if (flags & NK_EDIT_ACTIVATED) {
        updatingColor = true;
      }
      else if (flags & (NK_EDIT_DEACTIVATED | NK_EDIT_COMMITED)) {
        updatingColor = false;
        preview = nk_rgb_hex(hexText);
        pickedColor.r = preview.r/255.0f;
        pickedColor.g = preview.g/255.0f;
        pickedColor.b = preview.b/255.0f;
        pickedColor.a = preview.a/255.0f;
      }

      if (!updatingColor) {
        nk_color_hex_rgb(hexText, preview);
        hexTextLen = strlen(hexText);
      }
      nk_button_color(ctx, preview);

    } else openColorPicker = false;
    nk_end(ctx);
  }

  // Registers //
  if (showRegisters) {
    if (nk_begin(ctx, "Registers", nk_rect(width/2 - 480/2, height/2 - 270/2, 480, 270), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      if (nk_tree_push(ctx, NK_TREE_NODE, "GPIO", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Config", 0x3d00);

        // Port A
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Port A", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Data", 0x3d01);
        IOReg("Buffer", 0x3d02);
        IOReg("Direction", 0x3d03);
        IOReg("Attributes", 0x3d04);
        IOReg("Mask", 0x3d05);


        // Port B
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Port B", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Data", 0x3d06);
        IOReg("Buffer", 0x3d07);
        IOReg("Direction", 0x3d08);
        IOReg("Attributes", 0x3d09);
        IOReg("Mask", 0x3d0a);

        // Port C
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Port C", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Data", 0x3d0b);
        IOReg("Buffer", 0x3d0c);
        IOReg("Direction", 0x3d0d);
        IOReg("Attributes", 0x3d0e);
        IOReg("Mask", 0x3d0f);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "Timers", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("TmBase set", 0x3d10);
        // IOReg("Timebase Clear", 0x3d11);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Timer A", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Data", 0x3d12);
        IOReg("Ctrl", 0x3d13);
        IOReg("Enable", 0x3d14);
        // IOReg("IRQ Clear", 0x3d15);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Timer B", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Data", 0x3d16);
        IOReg("Ctrl", 0x3d17);
        IOReg("Enable", 0x3d18);
        // IOReg("IRQ Clear", 0x3d19);
        IOReg("Scanline", 0x3d1c);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "Misc.", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 6);
        IOReg("Sys Ctrl", 0x3d20);
        IOReg("Int Ctrl", 0x3d21);
        IOReg("Int Stat", 0x3d22);
        IOReg("ExMem Ctrl", 0x3d23);
        // IOReg("Wtchdg Clr", 0x3d24);
        IOReg("ADC Ctrl", 0x3d25);
        IOReg("ADC Data", 0x3d27);
        // IOReg("Sleep Mode", 0x3d28);
        IOReg("Wkup Src", 0x3d29);
        IOReg("Wkup Time", 0x3d2a);
        IOReg("TV System", 0x3d2b);
        IOReg("PRNG 1", 0x3d2c);
        IOReg("PRNG 2", 0x3d2d);
        IOReg("FIQ Sel", 0x3d2e);
        IOReg("DS Reg", 0x3d2f);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "UART", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 6);

        IOReg("Ctrl", 0x3d30);
        IOReg("Stat", 0x3d31);
        IOReg("Baud Lo", 0x3d33);
        IOReg("Baud Hi", 0x3d34);
        IOReg("Tx", 0x3d35);
        IOReg("Rx", 0x3d36);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "SPI", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 1);

        nk_label(ctx, "SPI is currently not implemented", NK_TEXT_LEFT);

        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_NODE, "DMA", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 25, 6);

        IOReg("Src Lo", 0x3e00);
        IOReg("Src Hi", 0x3e01);
        IOReg("Length", 0x3e02);
        IOReg("Dst", 0x3e03);

        nk_tree_pop(ctx);
      }

    } else {
      showRegisters = false;
      Backend_SetControlsEnable(true);
    }
    nk_end(ctx);
  }

  if (showMemory) {
    if (nk_begin(ctx, "Memory", nk_rect(width/2 - 480/2, height/2 - 270/2, 480, 270), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {

      // Jump to Address
      nk_menubar_begin(ctx);
      nk_layout_row_begin(ctx, NK_DYNAMIC, 27, 3);
      nk_layout_row_push(ctx, 0.25f);
      bool jumpToAddress = false;
      if (nk_button_label(ctx, "Jump")) {
        jumpToAddress = true;
        Backend_SetControlsEnable(true);
      }
      nk_layout_row_push(ctx, 0.75f);
      nk_flags flags = nk_edit_string(ctx, NK_EDIT_SIMPLE, jumpAddressText, &jumpAddressTextLen, 7, nk_filter_hex);
      if (flags & NK_EDIT_ACTIVATED) {
        Backend_SetControlsEnable(false);
      }
      nk_layout_row_end(ctx);
      nk_menubar_end(ctx);

      // Generate Memory text boxes
      nk_layout_row_dynamic(ctx, 25, 9);
      if (jumpToAddress) {
        uint32_t jumpAddr = strtol(jumpAddressText, NULL, 16);
        if (jumpAddr >= 0x2800)
          jumpAddr = 0x2800;
        jumpAddr >>= 3;
        jumpAddr *= 29;
        nk_window_set_scroll(ctx, 0, jumpAddr);
        jumpAddressTextLen = 0;
        memset(&jumpAddressText, 0, 7);
      }
      char text[5];
      for (int32_t i = 0; i < 0x2800; i++) {
        if ((i & 7) == 0) {
          snprintf((char*)&text, 5, "%04x", i);
          nk_label(ctx, (const char*)&text, NK_TEXT_LEFT);
        }
        IOReg(NULL, i);
      }
    } else {
      showMemory = false;
      Backend_SetControlsEnable(true);
    }
    nk_end(ctx);
  }


  if (showMemWatch) {
    if (nk_begin(ctx, "Memory Watch", nk_rect(width/2 - 480/2, height/2 - 270/2, 480, 270), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_label(ctx, "Addresses", NK_LEFT);
    } else {
      showMemWatch = false;
      Backend_SetControlsEnable(true);
    }
  }


  if (showCPU) {
    if (nk_begin(ctx, "CPU", nk_rect(width/2.0f - 480.0f/2.0f, height/2.0f - 270.0f/2.0f, 480, 270), NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, "Coming soon!", NK_LEFT);
    } else {
      showCPU = false;
      Backend_SetControlsEnable(true);
    }
    nk_end(ctx);
  }

}


void UI_Render() {
  snk_render(sapp_width(), sapp_height());
}


void UI_Toggle() {
  showUI ^= true;
}


bool UI_IsCheatOverlayOpen() {
  return showCheats || showMemoryScan || showAddCheat || showEditCheat || showScanContext || showCheatContext;
}


bool UI_IsInputCaptureActive() {
  return hotkeyCaptureIndex >= 0 || mapCaptureCtrl >= 0;
}
