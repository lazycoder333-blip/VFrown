#ifndef BACKEND_H
#define BACKEND_H

#include "../common.h"
#include "../core/vsmile.h"
#include "libs.h"
#include "lib/sokol_gfx.h"
#include "lib/sokol_gl.h"

#include "ui.h"
#include "userSettings.h"

#define MAX_AUDIO_FRAMES 32768
#define MAX_AUDIO_SAMPLES (MAX_AUDIO_FRAMES * 2)

enum {
  SCREENFILTER_NEAREST,
  SCREENFILTER_LINEAR,
  NUM_SCREENFILTERS,
};

typedef enum {
  HOTKEY_TOGGLE_UI,
  HOTKEY_TOGGLE_FULLSCREEN,
  HOTKEY_RESET,
  HOTKEY_PAUSE,
  HOTKEY_STEP,
  HOTKEY_SAVE_STATE,
  HOTKEY_LOAD_STATE,
  HOTKEY_FAST_FORWARD,
  HOTKEY_MAX
} HotkeyAction_t;

typedef struct {
  sgl_context  context;
  sgl_pipeline pipeline;
  sg_image     screenTexture;
  sg_sampler   samplers[NUM_SCREENFILTERS];

  FILE* saveFile;
  uint8_t* saveBuffer;
  uint32_t saveBufferSize;
  uint32_t saveBufferCap;
  uint32_t saveBufferPos;
  const uint8_t* loadBuffer;
  uint32_t loadBufferSize;
  uint32_t loadBufferPos;
  bool saveBufferActive;
  bool loadBufferActive;

  float sampleBuffer[MAX_AUDIO_SAMPLES];
  float filterL, filterR;
  float sampleReadPos;
  int32_t sampleHead, sampleTail;
  int32_t sampleCount;

  float emulationSpeed;
  int32_t currSampleX[16];
  int16_t prevSample[16];
  uint32_t drawColor;
  uint32_t currButtons;
  uint32_t prevButtons;
  uint32_t currLed;
  char title[256];
  bool showLeds;
  bool oscilloscopeEnabled;
  bool controlsEnabled;
  bool keepAspectRatio;
  bool pixelPerfectScale;
  uint8_t currScreenFilter;
  int32_t quickSaveSlot;
  int32_t quickLoadSlot;
  int32_t hotkeys[HOTKEY_MAX];
  bool fastForwardEnabled;
  float fastForwardPrevSpeed;
} Backend_t;

bool Backend_Init();
void Backend_Cleanup();
void Backend_Update();

void Backend_AudioCallback(float* buffer, int numFrames, int numChannels);

// Save states
void Backend_GetFileName(const char* path);
const char* Backend_GetRomTitle();
const char* Backend_OpenFileDialog(const char* title);
const char* Backend_SaveFileDialog(const char* title, const char* defaultPath);
void Backend_OpenMessageBox(const char* title, const char* message);
void Backend_WriteSave(void* data, uint32_t size);
void Backend_ReadSave(void* data, uint32_t size);
void Backend_SaveState();
void Backend_LoadState();
bool Backend_SaveStateToBuffer(uint8_t** outData, uint32_t* outSize);
bool Backend_LoadStateFromBuffer(const uint8_t* data, uint32_t size);
void Backend_SaveStateSlot(int32_t slot);
void Backend_LoadStateSlot(int32_t slot);
bool Backend_DeleteStateSlot(int32_t slot);
bool Backend_StateSlotExists(int32_t slot);
bool Backend_GetStatePath(int32_t slot, char* outPath, size_t outSize);
int32_t Backend_GetQuickSaveSlot();
int32_t Backend_GetQuickLoadSlot();
void Backend_SetQuickSaveSlot(int32_t slot);
void Backend_SetQuickLoadSlot(int32_t slot);
int32_t Backend_GetHotkey(HotkeyAction_t action);
void Backend_SetHotkey(HotkeyAction_t action, int32_t keycode);
bool Backend_GetFastForward();
void Backend_SetFastForward(bool enabled);

// TAS recording
bool Backend_TAS_IsRecording();
uint32_t Backend_TAS_GetFrameCount();
void Backend_TAS_Start();
void Backend_TAS_Stop();
void Backend_TAS_Clear();
bool Backend_TAS_Save(const char* path);
void Backend_TAS_RecordFrame(uint32_t buttons0, uint32_t buttons1);
bool Backend_TAS_Load(const char* path);
bool Backend_TAS_IsPlaying();
uint32_t Backend_TAS_GetPlaybackFrame();
uint32_t Backend_TAS_GetPlaybackLength();
void Backend_TAS_StartPlayback();
void Backend_TAS_StopPlayback();
bool Backend_TAS_NextFrame(uint32_t* outButtons0, uint32_t* outButtons1);
bool Backend_TAS_GetStartFromBoot();
void Backend_TAS_SetStartFromBoot(bool enabled);
bool Backend_TAS_GetFrame(uint32_t index, uint32_t* outButtons0, uint32_t* outButtons1);
bool Backend_TAS_SetFrame(uint32_t index, uint32_t buttons0, uint32_t buttons1, bool grow);

float Backend_GetSpeed();
void Backend_SetSpeed(float newSpeed);
void Backend_SetControlsEnable(bool isEnabled);

// Window
uint32_t* Backend_GetScanlinePointer(uint16_t scanlineNum);
bool Backend_RenderScanline();

// Input
bool Backend_GetInput();
uint32_t Backend_GetButtonStates();
uint32_t Backend_GetChangedButtons();
void Backend_HandleInput(int32_t keycode, int32_t eventType);
void Backend_UpdateButtonMapping(const char* buttonName, char* mappingText, uint32_t mappingLen);

// Leds
uint8_t Backend_SetLedStates(uint8_t state);
void Backend_RenderLeds();
void Backend_ShowLeds(bool shouldShowLeds);

// Audio
void Backend_PushAudioSample(float leftSample, float rightSample);
void Backend_InitAudioDevice(float* buffer, int32_t* count);
void Backend_PushBuffer();
void Backend_PushOscilloscopeSample(uint8_t ch, int16_t sample);
bool Backend_GetOscilloscopeEnabled();
void Backend_SetOscilloscopeEnabled(bool shouldShow);

void Backend_SetScreenFilter(uint8_t filterMode);

// Drawing
void Backend_SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void Backend_SetDrawColor32(uint32_t color);
void Backend_SetPixel(int32_t x, int32_t y);
void Backend_DrawCircle(int32_t x, int32_t y, uint32_t radius);
void Backend_DrawText(int32_t x, int32_t y, const char* text, ...);
void Backend_DrawChar(int32_t x, int32_t y, char c);

void Backend_SetKeepAspectRatio(bool keepAR);
void Backend_SetPixelPerfectScale(bool enabled);

#endif // BACKEND_H
