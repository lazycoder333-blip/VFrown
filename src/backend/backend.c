#include "backend.h"
#include "input.h"
#include "font.xpm" // TODO: remove
#include "lib/tinyfiledialogs.h"
#include "userSettings.h"
#include "../core/cheats.h"
#include "../core/tas.h"

#include <errno.h>
#include <math.h>

static Backend_t this;
static TAS_t tas;
static bool tasRecording = false;
static bool tasPlaying = false;
static uint32_t tasPlayIndex = 0;
static uint32_t tasMarkerIndex = 0;
static bool tasStartFromBoot = false;
static const char* hotkeySettingNames[HOTKEY_MAX] = {
  "hotkeyToggleUI",
  "hotkeyToggleFullscreen",
  "hotkeyReset",
  "hotkeyPause",
  "hotkeyStep",
  "hotkeySaveState",
  "hotkeyLoadState",
  "hotkeyFastForward"
};

static const int32_t hotkeyDefaults[HOTKEY_MAX] = {
  SAPP_KEYCODE_U,
  SAPP_KEYCODE_Y,
  SAPP_KEYCODE_R,
  SAPP_KEYCODE_P,
  SAPP_KEYCODE_O,
  SAPP_KEYCODE_J,
  SAPP_KEYCODE_K,
  SAPP_KEYCODE_TAB
};

bool Backend_Init() {
  memset(&this, 0, sizeof(Backend_t));

  this.sampleCount = 0;
  this.sampleReadPos = 0.0f;

  // Sokol GFX
  sg_desc sgDesc = {
    .context = sapp_sgcontext()
  };
  sg_setup(&sgDesc);
  if (!sg_isvalid()) {
    VSmile_Error("Failed to create Sokol GFX context\n");
    return false;
  }

  // Sokol GL
  sgl_setup(&(sgl_desc_t){
    .face_winding = SG_FACEWINDING_CW,
    .max_vertices = 4*64*1024,
  });

  // Create GPU-side textures for rendering emulated frame
  sg_image_desc imgDesc = {
    .width        = 320,
    .height       = 240,
    // .min_filter   = SG_FILTER_LINEAR,
    // .mag_filter   = SG_FILTER_LINEAR,
    .usage        = SG_USAGE_DYNAMIC,
    .pixel_format = SG_PIXELFORMAT_RGBA8
  };
  this.screenTexture = sg_make_image(&imgDesc);

  this.samplers[SCREENFILTER_NEAREST] = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_NEAREST,
    .mag_filter = SG_FILTER_NEAREST,
  });

  this.samplers[SCREENFILTER_LINEAR] = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_LINEAR,
    .mag_filter = SG_FILTER_LINEAR,
  });

  // Create and load pipeline
  this.pipeline = sgl_make_pipeline(&(sg_pipeline_desc){
    .colors[0].blend = {
      .enabled          = true,
      .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
      .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .op_rgb           = SG_BLENDOP_ADD,
      .src_factor_alpha = SG_BLENDFACTOR_ONE,
      .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .op_alpha         = SG_BLENDOP_ADD
    }
  });

  // Sokol audio
  saudio_setup(&(saudio_desc){
    .sample_rate  = OUTPUT_FREQUENCY,
    .num_channels = 2,
    .buffer_frames = 4096,
    .stream_cb = Backend_AudioCallback,
  });
  saudio_sample_rate();
  saudio_channels();

  this.saveFile = NULL;
  this.saveBuffer = NULL;
  this.saveBufferSize = 0;
  this.saveBufferCap = 0;
  this.saveBufferPos = 0;
  this.loadBuffer = NULL;
  this.loadBufferSize = 0;
  this.loadBufferPos = 0;
  this.saveBufferActive = false;
  this.loadBufferActive = false;

  this.emulationSpeed = 1.0f;
  this.controlsEnabled = true;
  this.currButtons = 0;
  this.prevButtons = 0;
  this.quickSaveSlot = 0;
  this.quickLoadSlot = 0;
  this.fastForwardEnabled = false;
  this.fastForwardPrevSpeed = 1.0f;
  for (int32_t i = 0; i < HOTKEY_MAX; i++) {
    this.hotkeys[i] = hotkeyDefaults[i];
  }

  if (!UserSettings_Init()) return false;

  TAS_Init(&tas);

  bool warningsEnabled = true;
  if (UserSettings_ReadBool("warningsEnabled", &warningsEnabled)) {
    VSmile_SetWarningsEnabled(warningsEnabled);
  }

  bool cheatsEnabled = true;
  if (UserSettings_ReadBool("cheatsEnabled", &cheatsEnabled)) {
    Cheats_SetGlobalEnabled(cheatsEnabled);
  }

  for (int32_t i = 0; i < HOTKEY_MAX; i++) {
    int storedKey = 0;
    if (UserSettings_ReadInt(hotkeySettingNames[i], &storedKey)) {
      this.hotkeys[i] = storedKey;
    }
  }

  return true;
}


void Backend_Cleanup() {
  UserSettings_Cleanup();

  free(this.saveBuffer);
  this.saveBuffer = NULL;
  this.saveBufferSize = 0;
  this.saveBufferCap = 0;

  TAS_Free(&tas);

  saudio_shutdown();
}


void Backend_Update() {
  // if (this.controlsEnabled) {
  //   if (Backend_GetChangedButtons())
  //     Controller_UpdateButtons(0, this.currButtons);
  //   this.prevButtons = this.currButtons;
  // }

  // Update screen texture
  sg_image_data imageData;
  imageData.subimage[0][0].ptr  = PPU_GetPixelBuffer();
  imageData.subimage[0][0].size = 320*240*sizeof(uint32_t);
  sg_update_image(this.screenTexture, &imageData);

  const float width  = sapp_widthf();
  const float height = sapp_heightf();

  // Begin render pass
  const sg_pass_action defaultPass = {
    .colors[0] = {
      .load_action  = SG_LOADACTION_CLEAR,
      .store_action = SG_STOREACTION_DONTCARE,
      .clear_value  = { 0.0f, 0.0f, 0.0f, 1.0f }
    }
  };

  UI_StartFrame();

  sg_begin_default_pass(&defaultPass, width, height);

  // Draw emulated frame to window
  sgl_defaults();
  sgl_load_pipeline(this.pipeline);
  sgl_enable_texture();
  sgl_texture(this.screenTexture, this.samplers[this.currScreenFilter]);
  sgl_matrix_mode_projection();
  sgl_push_matrix();
  sgl_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
  sgl_begin_quads();

  sgl_c1i(0xffffffff);

  if (this.pixelPerfectScale) {
    const int fbWidth = sapp_width();
    const int fbHeight = sapp_height();
    int scaleX = fbWidth / 320;
    int scaleY = fbHeight / 240;
    int scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 1) scale = 1;
    const int outWidth = 320 * scale;
    const int outHeight = 240 * scale;
    const int x0 = (fbWidth - outWidth) / 2;
    const int y0 = (fbHeight - outHeight) / 2;

    const float left = (float)x0;
    const float right = (float)(x0 + outWidth);
    const float top = (float)y0;
    const float bottom = (float)(y0 + outHeight);

    sgl_v2f_t2f(left,  top,    0.0f, 0.0f);
    sgl_v2f_t2f(right, top,    1.0f, 0.0f);
    sgl_v2f_t2f(right, bottom, 1.0f, 1.0f);
    sgl_v2f_t2f(left,  bottom, 0.0f, 1.0f);
  } else if (this.keepAspectRatio) {

    float scaleX = width / 320.0f;
    float scaleY = height / 240.0f;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale <= 0.0f) scale = 1.0f;
    float outWidth = 320.0f * scale;
    float outHeight = 240.0f * scale;

    sgl_v2f_t2f(width*0.5f-(outWidth*0.5f), height*0.5f-(outHeight*0.5f), 0.0f, 0.0f);
    sgl_v2f_t2f(width*0.5f+(outWidth*0.5f), height*0.5f-(outHeight*0.5f), 1.0f, 0.0f);
    sgl_v2f_t2f(width*0.5f+(outWidth*0.5f), height*0.5f+(outHeight*0.5f), 1.0f, 1.0f);
    sgl_v2f_t2f(width*0.5f-(outWidth*0.5f), height*0.5f+(outHeight*0.5f), 0.0f, 1.0f);

  } else {

    sgl_v2f_t2f(0,      0,      0.0f, 0.0f);
    sgl_v2f_t2f(width,  0,      1.0f, 0.0f);
    sgl_v2f_t2f(width,  height, 1.0f, 1.0f);
    sgl_v2f_t2f(0,      height, 0.0f, 1.0f);

  }

  sgl_end();
  sgl_draw();
  sgl_pop_matrix();

  UI_RunFrame();

  // End rendering
  UI_Render();
  sg_end_pass();
  sg_commit();

  // if (this.oscilloscopeEnabled) {
  //   memset(this.pixelBuffer, 0, 320*240*sizeof(uint32_t));
  //   for (int32_t i = 0; i < 16; i++)
  //     this.currSampleX[i] = 0;
  // }
}


// Find and store the title of the current ROM
void Backend_GetFileName(const char* path) {
  if (!path)
    return;

  // Get the end of the file name before the '.' of the file extension
  char* end = (char*)(path + strlen(path));
  while (end != path && (*end) != PATH_CHAR && (*end) != '.') {
    end--;
  }
  // If the found character is not '.', then there's probably no file extension
  if (end == path || (*end) != '.')
    end = (char*)(path + strlen(path));

  // Start searching for the start of the title without the directory
  char* start = end;
  while (start != path && (*start) != PATH_CHAR) {
    start--;
  }
  start++; // Moved one too far

  int32_t size = (int32_t)(end - start);
  int32_t maxSize = (int32_t)(sizeof(this.title) - 1);
  if (size > maxSize)
    size = maxSize;

  memset(this.title, 0, sizeof(this.title));
  if (size > 0)
    memcpy(this.title, start, (size_t)size);
}


const char* Backend_GetRomTitle() {
  return (const char*)this.title;
}


const char* Backend_OpenFileDialog(const char* title) {
  return tinyfd_openFileDialog(
  	title,
  	NULL, 0, 0, NULL, 0
  );
}


const char* Backend_SaveFileDialog(const char* title, const char* defaultPath) {
  return tinyfd_saveFileDialog(title, defaultPath, 0, NULL, NULL);
}


void Backend_OpenMessageBox(const char* title, const char* message) {
  tinyfd_messageBox(title, message, NULL, NULL, 0);
}


void Backend_WriteSave(void* data, uint32_t size) {
  if (this.saveBufferActive) {
    if (!data || size == 0) return;
    uint32_t alignedSize = (size + 0xf) & ~0xf;
    uint32_t needed = this.saveBufferPos + alignedSize;
    if (needed > this.saveBufferCap) {
      uint32_t newCap = this.saveBufferCap ? this.saveBufferCap : 1024;
      while (newCap < needed) newCap *= 2;
      uint8_t* newBuffer = realloc(this.saveBuffer, newCap);
      if (!newBuffer) return;
      this.saveBuffer = newBuffer;
      this.saveBufferCap = newCap;
    }

    memcpy(this.saveBuffer + this.saveBufferPos, data, size);
    if (alignedSize > size) {
      memset(this.saveBuffer + this.saveBufferPos + size, 0, alignedSize - size);
    }
    this.saveBufferPos += alignedSize;
    if (this.saveBufferPos > this.saveBufferSize) {
      this.saveBufferSize = this.saveBufferPos;
    }
    return;
  }

  if (!this.saveFile)
    return;

  uint8_t* bytes = (uint8_t*)data;
  for (int32_t i = 0; i < size; i++)
    fputc(bytes[i], this.saveFile);

  uint32_t alignmentSize = (size + 0xf) & ~0xf;
  for (int32_t i = 0; i < (alignmentSize - size); i++)
    fputc(0x00, this.saveFile);

}


void Backend_ReadSave(void* data, uint32_t size) {
  if (this.loadBufferActive) {
    if (!data || size == 0) return;
    uint8_t* bytes = (uint8_t*)data;
    for (uint32_t i = 0; i < size; i++) {
      if (this.loadBufferPos < this.loadBufferSize) {
        bytes[i] = this.loadBuffer[this.loadBufferPos++];
      } else {
        bytes[i] = 0;
      }
    }
    uint32_t alignmentSize = (size + 0xf) & ~0xf;
    uint32_t padding = alignmentSize - size;
    if (this.loadBufferPos + padding <= this.loadBufferSize) {
      this.loadBufferPos += padding;
    } else {
      this.loadBufferPos = this.loadBufferSize;
    }
    return;
  }

  if (!this.saveFile)
    return;

  uint8_t* bytes = (uint8_t*)data;
  for (int32_t i = 0; i < size; i++)
    bytes[i] = fgetc(this.saveFile);

  uint32_t alignmentSize = (size + 0xf) & ~0xf;
  for (int32_t i = 0; i < (alignmentSize - size); i++)
    fgetc(this.saveFile);
}


static bool Backend_BuildStatePath(char* outPath, size_t outSize, int32_t slot) {
  if (!outPath || outSize == 0)
    return false;

  if (!this.title[0])
    return false;

  if (slot < 0 || slot > 99) {
    VSmile_Warning("Savestate slot out of range: %d", slot);
    return false;
  }

  snprintf(outPath, outSize, "savestates/%s-%02d.vfss", (const char*)&this.title, slot);
  return true;
}


bool Backend_GetStatePath(int32_t slot, char* outPath, size_t outSize) {
  return Backend_BuildStatePath(outPath, outSize, slot);
}


void Backend_SaveState() {
  Backend_SaveStateSlot(this.quickSaveSlot);
}


void Backend_SaveStateSlot(int32_t slot) {
  char path[256];
  if (!Backend_BuildStatePath(path, sizeof(path), slot))
    return;

  VSmile_Log("Saving state to '%s'...", (const char*)&path);
  this.saveFile = fopen(path, "wb+");
  if (!this.saveFile) {
    VSmile_Warning("Save state failed!");
    return;
  }

  Bus_SaveState();
  CPU_SaveState();
  PPU_SaveState();
  SPU_SaveState();
  VSmile_SaveState();
  Controller_SaveState();
  DMA_SaveState();
  GPIO_SaveState();
  Misc_SaveState();
  Timers_SaveState();
  UART_SaveState();

  Backend_WriteSave(&this.currButtons, sizeof(uint32_t));
  Backend_WriteSave(&this.prevButtons, sizeof(uint32_t));

  fclose(this.saveFile);

  VSmile_Log("State saved!");
}


bool Backend_SaveStateToBuffer(uint8_t** outData, uint32_t* outSize) {
  if (!outData || !outSize)
    return false;

  this.saveBufferActive = true;
  this.saveBufferPos = 0;
  this.saveBufferSize = 0;

  Bus_SaveState();
  CPU_SaveState();
  PPU_SaveState();
  SPU_SaveState();
  VSmile_SaveState();
  Controller_SaveState();
  DMA_SaveState();
  GPIO_SaveState();
  Misc_SaveState();
  Timers_SaveState();
  UART_SaveState();

  Backend_WriteSave(&this.currButtons, sizeof(uint32_t));
  Backend_WriteSave(&this.prevButtons, sizeof(uint32_t));

  this.saveBufferActive = false;

  if (!this.saveBuffer || this.saveBufferSize == 0)
    return false;

  uint8_t* copy = malloc(this.saveBufferSize);
  if (!copy) return false;
  memcpy(copy, this.saveBuffer, this.saveBufferSize);
  *outData = copy;
  *outSize = this.saveBufferSize;
  return true;
}


bool Backend_LoadStateFromBuffer(const uint8_t* data, uint32_t size) {
  if (!data || size == 0)
    return false;

  this.loadBufferActive = true;
  this.loadBuffer = data;
  this.loadBufferSize = size;
  this.loadBufferPos = 0;

  Bus_LoadState();
  CPU_LoadState();
  PPU_LoadState();
  SPU_LoadState();
  VSmile_LoadState();
  Controller_LoadState();
  DMA_LoadState();
  GPIO_LoadState();
  Misc_LoadState();
  Timers_LoadState();
  UART_LoadState();

  Backend_ReadSave(&this.currButtons, sizeof(uint32_t));
  Backend_ReadSave(&this.prevButtons, sizeof(uint32_t));

  this.loadBufferActive = false;
  this.loadBuffer = NULL;
  this.loadBufferSize = 0;
  this.loadBufferPos = 0;

  return true;
}


void Backend_LoadState() {
  Backend_LoadStateSlot(this.quickLoadSlot);
}


void Backend_LoadStateSlot(int32_t slot) {
  char path[256];
  if (!Backend_BuildStatePath(path, sizeof(path), slot))
    return;

  VSmile_Log("Loading state from '%s'...", (const char*)&path);
  this.saveFile = fopen(path, "rb");
  if (!this.saveFile) {
    VSmile_Warning("Load state failed!");
    return;
  }

  Bus_LoadState();
  CPU_LoadState();
  PPU_LoadState();
  SPU_LoadState();
  VSmile_LoadState();
  Controller_LoadState();
  DMA_LoadState();
  GPIO_LoadState();
  Misc_LoadState();
  Timers_LoadState();
  UART_LoadState();

  Backend_ReadSave(&this.currButtons, sizeof(uint32_t));
  Backend_ReadSave(&this.prevButtons, sizeof(uint32_t));

  fclose(this.saveFile);

  VSmile_Log("State loaded!");
}


bool Backend_DeleteStateSlot(int32_t slot) {
  char path[256];
  if (!Backend_BuildStatePath(path, sizeof(path), slot))
    return false;

  if (remove(path) != 0) {
    VSmile_Warning("Failed to delete savestate '%s' (%s)", path, strerror(errno));
    return false;
  }

  return true;
}


bool Backend_StateSlotExists(int32_t slot) {
  char path[256];
  if (!Backend_BuildStatePath(path, sizeof(path), slot))
    return false;

  FILE* file = fopen(path, "rb");
  if (!file)
    return false;

  fclose(file);
  return true;
}


int32_t Backend_GetQuickSaveSlot() {
  return this.quickSaveSlot;
}


int32_t Backend_GetQuickLoadSlot() {
  return this.quickLoadSlot;
}


void Backend_SetQuickSaveSlot(int32_t slot) {
  if (slot < 0 || slot > 99) {
    VSmile_Warning("Quick save slot out of range: %d", slot);
    return;
  }
  this.quickSaveSlot = slot;
}


void Backend_SetQuickLoadSlot(int32_t slot) {
  if (slot < 0 || slot > 99) {
    VSmile_Warning("Quick load slot out of range: %d", slot);
    return;
  }
  this.quickLoadSlot = slot;
}


float Backend_GetSpeed() {
  return this.emulationSpeed;
}


void Backend_SetSpeed(float newSpeed) {
  // Prevent speed changes during TAS playback to avoid frame desyncs
  if (tasPlaying) return;
  this.emulationSpeed = newSpeed;
}


void Backend_SetControlsEnable(bool isEnabled) {
  this.controlsEnabled = isEnabled;
}


uint32_t* Backend_GetScanlinePointer(uint16_t scanlineNum) {
  return PPU_GetPixelBuffer() + (320*scanlineNum);
}


bool Backend_RenderScanline() {
  return !this.oscilloscopeEnabled;
}


bool Backend_GetInput() {
  return true;
}


uint32_t Backend_GetChangedButtons() {
  return this.currButtons ^ this.prevButtons;
}


uint8_t Backend_SetLedStates(uint8_t state) {
  this.currLed = state;
  return this.currLed;
}

void Backend_RenderLeds() {
  if (!this.showLeds)
    return;

  uint8_t alpha = 128;

  if (this.currLed & (1 << LED_RED))
    Backend_SetDrawColor(255, 0, 0, alpha);
  else
    Backend_SetDrawColor(0, 0, 0, alpha);

  Backend_DrawCircle(16, 224, 10);

  if (this.currLed & (1 << LED_YELLOW))
    Backend_SetDrawColor(255, 255, 0, alpha);
  else
    Backend_SetDrawColor(0, 0, 0, alpha);

  Backend_DrawCircle(46, 224, 10);

  if (this.currLed & (1 << LED_BLUE))
    Backend_SetDrawColor(0, 0, 255, alpha);
  else
    Backend_SetDrawColor(0, 0, 0, alpha);

  Backend_DrawCircle(76, 224, 10);

  if (this.currLed & (1 << LED_GREEN))
    Backend_SetDrawColor(0, 255, 0, alpha);
  else
    Backend_SetDrawColor(0, 0, 0, alpha);

  Backend_DrawCircle(106, 224, 10);
}


void Backend_ShowLeds(bool shouldShowLeds) {
  this.showLeds = shouldShowLeds;
}


void Backend_PushAudioSample(float leftSample, float rightSample) {
  if (this.sampleCount >= MAX_AUDIO_FRAMES) {
    this.sampleTail = (this.sampleTail + 1) % MAX_AUDIO_FRAMES;
    if (this.sampleReadPos > 0.0f) {
      this.sampleReadPos -= 1.0f;
      if (this.sampleReadPos < 0.0f) this.sampleReadPos = 0.0f;
    }
    this.sampleCount--;
  }

  const int32_t sampleIndex = this.sampleHead * 2;
  this.sampleBuffer[sampleIndex] = leftSample;
  this.sampleBuffer[sampleIndex + 1] = rightSample;

  this.sampleHead = (this.sampleHead + 1) % MAX_AUDIO_FRAMES;
  this.sampleCount++;

  UI_AudioDumper_WriteSample();
}

void Backend_AudioCallback(float* buffer, int numFrames, int numChannels) {
  if (numChannels != 2) {
    VSmile_Error("Audio callback expects 2 channels, %d recieved", numChannels);
    return;
  }

  if (VSmile_GetPaused() || this.fastForwardEnabled) {
    for (int i = 0; i < numFrames; i++) {
      buffer[(i<<1)  ] = 0.0f;
      buffer[(i<<1)+1] = 0.0f;
    }
  } else {
    const float targetFrames = (float)(MAX_AUDIO_FRAMES / 2);
    float readStep = 1.0f;
    if (this.sampleCount > 0) {
      const float error = ((float)this.sampleCount - targetFrames) / targetFrames;
      readStep = 1.0f + (error * 0.02f);
      if (readStep < 0.95f) readStep = 0.95f;
      if (readStep > 1.05f) readStep = 1.05f;
    }

    for (int i = 0; i < numFrames; i++) {
      if (this.sampleCount >= 2) {
        const int32_t idxA = this.sampleTail;
        const int32_t idxB = (idxA + 1) % MAX_AUDIO_FRAMES;
        const int32_t baseA = idxA * 2;
        const int32_t baseB = idxB * 2;
        const float frac = this.sampleReadPos;

        const float sampleL = this.sampleBuffer[baseA] + (this.sampleBuffer[baseB] - this.sampleBuffer[baseA]) * frac;
        const float sampleR = this.sampleBuffer[baseA + 1] + (this.sampleBuffer[baseB + 1] - this.sampleBuffer[baseA + 1]) * frac;

        this.filterL = sampleL;
        this.filterR = sampleR;

        this.sampleReadPos += readStep;
        int32_t advance = (int32_t)this.sampleReadPos;
        if (advance > 0) {
          if (advance > this.sampleCount) advance = this.sampleCount;
          this.sampleTail = (this.sampleTail + advance) % MAX_AUDIO_FRAMES;
          this.sampleCount -= advance;
          this.sampleReadPos -= (float)advance;
        }
      } else {
        this.filterL *= 0.5f;
        this.filterR *= 0.5f;
      }

      buffer[(i<<1)  ] = this.filterL * 2.0f;
      buffer[(i<<1)+1] = this.filterR * 2.0f;
    }
  }
}

static const uint16_t channelColors[] = {
  0x7c00, 0x83e0, 0x001f, 0xffe0,
  0x7c1f, 0x83ff, 0xfe24, 0xffff,
  0x7c00, 0x83e0, 0x001f, 0xffe0,
  0x7c1f, 0x83ff, 0xfe24, 0xffff
};


void Backend_PushOscilloscopeSample(uint8_t ch, int16_t sample) {
  if (!this.oscilloscopeEnabled)
    return;

  sample = sample / 1200;

  if (this.currSampleX[ch] % 10 == 0) {

    int32_t x = (ch & 0x3) * 80;
    int32_t y = ((ch >> 2) * 60) + 30;

    x += (this.currSampleX[ch] / 10);

    int32_t start, end;
    if (sample > this.prevSample[ch]) {
      start = this.prevSample[ch];
      end = sample;
    } else if (sample < this.prevSample[ch]){
      start = sample;
      end = this.prevSample[ch];
    } else {
      start = sample;
      end = sample+1;
    }

    Backend_SetDrawColor32(RGB5A1_TO_RGBA8(channelColors[ch]));

    for (int32_t i = start; i < end; i++) {
      Backend_SetPixel(x, y+i);
    }

    this.prevSample[ch] = sample;
  }

  this.currSampleX[ch]++;
}


bool Backend_GetOscilloscopeEnabled() {
  return this.oscilloscopeEnabled;
}


void Backend_SetOscilloscopeEnabled(bool shouldShow) {
  this.oscilloscopeEnabled = shouldShow;
}


void Backend_SetScreenFilter(uint8_t filterMode) {
  this.currScreenFilter = filterMode;
}

// Old SDL keycodes
// case SDLK_BACKQUOTE: SDLBackend_ToggleFullscreen(); break;
// case SDLK_1: PPU_ToggleLayer(0); break; // Layer 0
// case SDLK_2: PPU_ToggleLayer(1); break; // Layer 1
// case SDLK_3: PPU_ToggleLayer(2); break; // Sprites
// case SDLK_4: VSmile_TogglePause(); break;
// case SDLK_5: VSmile_Step(); break;
// case SDLK_6: PPU_ToggleSpriteOutlines(); break;
// case SDLK_7: PPU_ToggleFlipVisual(); break;
// case SDLK_8: this.isOscilloscopeView = !this.isOscilloscopeView; break;
// case SDLK_0: VSmile_Reset(); break;
// case SDLK_F10: running = false; break; // Exit
// case SDLK_F1: this.showLed ^=1; break;
// case SDLK_h: this.showHelp ^=1; break;
// case SDLK_g: this.showRegisters ^=1; break;
//
// case SDLK_p: SDLBackend_SetVSync(!this.isVsyncEnabled); break;
//
// case SDLK_UP:    this.currButtons |= (1 << INPUT_UP);     break;
// case SDLK_DOWN:  this.currButtons |= (1 << INPUT_DOWN);   break;
// case SDLK_LEFT:  this.currButtons |= (1 << INPUT_LEFT);   break;
// case SDLK_RIGHT: this.currButtons |= (1 << INPUT_RIGHT);  break;
// case SDLK_SPACE: this.currButtons |= (1 << INPUT_ENTER);  break;
// case SDLK_z:     this.currButtons |= (1 << INPUT_RED);    break;
// case SDLK_x:     this.currButtons |= (1 << INPUT_YELLOW); break;
// case SDLK_v:     this.currButtons |= (1 << INPUT_BLUE);   break;
// case SDLK_c:     this.currButtons |= (1 << INPUT_GREEN);  break;
// case SDLK_a:     this.currButtons |= (1 << INPUT_HELP);   break;
// case SDLK_s:     this.currButtons |= (1 << INPUT_EXIT);   break;
// case SDLK_d:     this.currButtons |= (1 << INPUT_ABC);    break;


void Backend_HandleInput(int32_t keycode, int32_t eventType) {
  if (UI_IsCheatOverlayOpen() || UI_IsInputCaptureActive()) {
    return;
  }

  if (eventType == SAPP_EVENTTYPE_KEY_DOWN) {
    if (keycode == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
      return;
    }

    if (this.hotkeys[HOTKEY_TOGGLE_FULLSCREEN] && keycode == this.hotkeys[HOTKEY_TOGGLE_FULLSCREEN]) {
      sapp_toggle_fullscreen();
      return;
    }
    if (this.hotkeys[HOTKEY_RESET] && keycode == this.hotkeys[HOTKEY_RESET]) {
      VSmile_Reset();
      return;
    }
    if (this.hotkeys[HOTKEY_TOGGLE_UI] && keycode == this.hotkeys[HOTKEY_TOGGLE_UI]) {
      UI_Toggle();
      return;
    }
    if (this.hotkeys[HOTKEY_STEP] && keycode == this.hotkeys[HOTKEY_STEP]) {
      VSmile_Step();
      return;
    }
    if (this.hotkeys[HOTKEY_PAUSE] && keycode == this.hotkeys[HOTKEY_PAUSE]) {
      VSmile_SetPause(!VSmile_GetPaused());
      return;
    }
    if (this.hotkeys[HOTKEY_SAVE_STATE] && keycode == this.hotkeys[HOTKEY_SAVE_STATE]) {
      if (this.title[0])
        Backend_SaveState();
      return;
    }
    if (this.hotkeys[HOTKEY_LOAD_STATE] && keycode == this.hotkeys[HOTKEY_LOAD_STATE]) {
      if (this.title[0])
        Backend_LoadState();
      return;
    }
    if (this.hotkeys[HOTKEY_FAST_FORWARD] && keycode == this.hotkeys[HOTKEY_FAST_FORWARD]) {
      Backend_SetFastForward(true);
      return;
    }
  } else if (eventType == SAPP_EVENTTYPE_KEY_UP){
    if (this.hotkeys[HOTKEY_FAST_FORWARD] && keycode == this.hotkeys[HOTKEY_FAST_FORWARD]) {
      Backend_SetFastForward(false);
      return;
    }
    // switch (keycode) {
    // case SAPP_KEYCODE_UP:    this.currButtons &= ~(1 << INPUT_UP);     break;
    // case SAPP_KEYCODE_DOWN:  this.currButtons &= ~(1 << INPUT_DOWN);   break;
    // case SAPP_KEYCODE_LEFT:  this.currButtons &= ~(1 << INPUT_LEFT);   break;
    // case SAPP_KEYCODE_RIGHT: this.currButtons &= ~(1 << INPUT_RIGHT);  break;
    // case SAPP_KEYCODE_SPACE: this.currButtons &= ~(1 << INPUT_ENTER);  break;
    // case SAPP_KEYCODE_Z:     this.currButtons &= ~(1 << INPUT_RED);    break;
    // case SAPP_KEYCODE_X:     this.currButtons &= ~(1 << INPUT_YELLOW); break;
    // case SAPP_KEYCODE_V:     this.currButtons &= ~(1 << INPUT_BLUE);   break;
    // case SAPP_KEYCODE_C:     this.currButtons &= ~(1 << INPUT_GREEN);  break;
    // case SAPP_KEYCODE_A:     this.currButtons &= ~(1 << INPUT_HELP);   break;
    // case SAPP_KEYCODE_S:     this.currButtons &= ~(1 << INPUT_EXIT);   break;
    // case SAPP_KEYCODE_D:     this.currButtons &= ~(1 << INPUT_ABC);    break;
    // }
  }
}


int32_t Backend_GetHotkey(HotkeyAction_t action) {
  if (action < 0 || action >= HOTKEY_MAX) return 0;
  return this.hotkeys[action];
}


void Backend_SetHotkey(HotkeyAction_t action, int32_t keycode) {
  if (action < 0 || action >= HOTKEY_MAX) return;
  this.hotkeys[action] = keycode;
  if (hotkeySettingNames[action]) {
    UserSettings_WriteInt(hotkeySettingNames[action], keycode);
  }
}


bool Backend_GetFastForward() {
  return this.fastForwardEnabled;
}


void Backend_SetFastForward(bool enabled) {
  if (this.fastForwardEnabled == enabled)
    return;

  this.fastForwardEnabled = enabled;
  if (enabled) {
    this.fastForwardPrevSpeed = this.emulationSpeed;
    this.emulationSpeed = this.fastForwardPrevSpeed * 4.0f;
  } else {
    this.emulationSpeed = this.fastForwardPrevSpeed;
  }

  this.sampleHead = 0;
  this.sampleTail = 0;
  this.sampleCount = 0;
  this.sampleReadPos = 0.0f;
  this.filterL = 0.0f;
  this.filterR = 0.0f;
}


bool Backend_TAS_IsRecording() {
  return tasRecording;
}


uint32_t Backend_TAS_GetFrameCount() {
  return tas.frameCount;
}


void Backend_TAS_Start() {
  TAS_Clear(&tas);
  tasRecording = true;
  tasPlaying = false;
  tasPlayIndex = 0;
  tasMarkerIndex = 0;
  Input_SetOverrideEnabled(false);
  if (tasStartFromBoot) {
    VSmile_Reset();
    VSmile_SetPause(false);
  }
  uint8_t* stateData = NULL;
  uint32_t stateSize = 0;
  if (Backend_SaveStateToBuffer(&stateData, &stateSize)) {
    if (TAS_AddSavestateData(&tas, "initial", stateData, stateSize)) {
      TAS_AddMarker(&tas, 0, 0);
    }
    free(stateData);
  }
  VSmile_Log("TAS recording started");
}


void Backend_TAS_Stop() {
  tasRecording = false;
  VSmile_Log("TAS recording stopped");
}


void Backend_TAS_Clear() {
  TAS_Clear(&tas);
  tasPlayIndex = 0;
  tasMarkerIndex = 0;
}


bool Backend_TAS_Save(const char* path) {
  if (!path || path[0] == '\0') return false;
  return TAS_SaveToFile(&tas, path);
}


void Backend_TAS_RecordFrame(uint32_t buttons0, uint32_t buttons1) {
  if (!tasRecording || VSmile_GetPaused()) return;
  TAS_AddFrame(&tas, buttons0, buttons1);
}


bool Backend_TAS_Load(const char* path) {
  if (!path || path[0] == '\0') return false;
  tasRecording = false;
  tasPlaying = false;
  tasPlayIndex = 0;
  tasMarkerIndex = 0;
  return TAS_LoadFromFile(&tas, path);
}


bool Backend_TAS_IsPlaying() {
  return tasPlaying;
}


uint32_t Backend_TAS_GetPlaybackFrame() {
  return tasPlayIndex;
}


uint32_t Backend_TAS_GetPlaybackLength() {
  return tas.frameCount;
}


void Backend_TAS_StartPlayback() {
  if (tas.frameCount == 0) return;
  tasRecording = false;
  tasPlaying = true;
  tasPlayIndex = 0;
  tasMarkerIndex = 0;
  Input_ResetButtonState();
  Input_SetOverrideEnabled(true);
  if (tasStartFromBoot) {
    VSmile_Reset();
    VSmile_SetPause(false);
    while (tasMarkerIndex < tas.markerCount && tas.markers[tasMarkerIndex].frameIndex == 0) {
      tasMarkerIndex++;
    }
  }
  VSmile_Log("TAS playback started - %u frames", tas.frameCount);
}


void Backend_TAS_StopPlayback() {
  tasPlaying = false;
  tasPlayIndex = 0;
  tasMarkerIndex = 0;
  Input_SetOverrideEnabled(false);
  VSmile_Log("TAS playback stopped");
}


bool Backend_TAS_NextFrame(uint32_t* outButtons0, uint32_t* outButtons1) {
  if (!tasPlaying || tasPlayIndex >= tas.frameCount) {
    tasPlaying = false;
    VSmile_SetPause(true);
    return false;
  }

  while (tasMarkerIndex < tas.markerCount && tas.markers[tasMarkerIndex].frameIndex == tasPlayIndex) {
    uint32_t stateIndex = tas.markers[tasMarkerIndex].stateIndex;
    if (stateIndex < tas.stateCount) {
      Backend_LoadStateFromBuffer(tas.states[stateIndex].data, tas.states[stateIndex].size);
    }
    tasMarkerIndex++;
  }

  TAS_Frame_t frame;
  if (!TAS_GetFrame(&tas, tasPlayIndex, &frame)) {
    tasPlaying = false;
    return false;
  }
  if (outButtons0) *outButtons0 = frame.buttons0;
  if (outButtons1) *outButtons1 = frame.buttons1;
  tasPlayIndex++;
  if (tasPlayIndex >= tas.frameCount) {
    tasPlaying = false;
    VSmile_SetPause(true);
  }
  return true;
}


bool Backend_TAS_GetStartFromBoot() {
  return tasStartFromBoot;
}


void Backend_TAS_SetStartFromBoot(bool enabled) {
  tasStartFromBoot = enabled;
}


bool Backend_TAS_GetFrame(uint32_t index, uint32_t* outButtons0, uint32_t* outButtons1) {
  TAS_Frame_t frame;
  if (!TAS_GetFrame(&tas, index, &frame)) return false;
  if (outButtons0) *outButtons0 = frame.buttons0;
  if (outButtons1) *outButtons1 = frame.buttons1;
  return true;
}


bool Backend_TAS_SetFrame(uint32_t index, uint32_t buttons0, uint32_t buttons1, bool grow) {
  if (grow && index >= tas.frameCount) {
    while (tas.frameCount <= index) {
      if (!TAS_AddFrame(&tas, 0, 0)) return false;
    }
  }
  return TAS_SetFrame(&tas, index, buttons0, buttons1);
}


void Backend_UpdateButtonMapping(const char* buttonName, char* mappingText, uint32_t mappingLen) {

}


void Backend_SetDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  this.drawColor = (a << 24) | (b << 16) | (g << 8) | r;
}


void Backend_SetDrawColor32(uint32_t color) {
  this.drawColor = color;
}


void Backend_SetPixel(int32_t x, int32_t y) {
  // if (x >= 0 && x < 320 && y >= 0 && y < 240)
  //   this.pixelBuffer[y][x] = this.drawColor;
}


void Backend_DrawCircle(int32_t x, int32_t y, uint32_t radius) {
  int32_t offsetx, offsety, d;

  offsetx = 0;
  offsety = radius;
  d = radius - 1;

  while (offsety >= offsetx) {
    Backend_SetPixel(x + offsetx, y + offsety);
    Backend_SetPixel(x + offsety, y + offsetx);
    Backend_SetPixel(x - offsetx, y + offsety);
    Backend_SetPixel(x - offsety, y + offsetx);

    Backend_SetPixel(x + offsetx, y - offsety);
    Backend_SetPixel(x + offsety, y - offsetx);
    Backend_SetPixel(x - offsetx, y - offsety);
    Backend_SetPixel(x - offsety, y - offsetx);

    if (d >= 2*offsetx) {
      d -= 2*offsetx + 1;
      offsetx += 1;
    } else if (d < 2 * (radius - offsety)) {
      d += 2 * offsety - 1;
      offsety -= 1;
    } else {
      d += 2 * (offsety - offsetx -1);
      offsety -= 1;
      offsetx += 1;
    }
  }
}


void Backend_DrawText(int32_t x, int32_t y, const char* text, ...) {
  if (!text)
    return;

  char buffer[256];
  va_list args;
  va_start(args, text);
  vsnprintf(buffer, sizeof(buffer), text, args);
  va_end(args);

  buffer[255] = '\0';

  size_t len = strlen(buffer);
  for(int32_t i = 0 ; i < len; i++) {
    Backend_DrawChar(x, y, buffer[i]);
    x += 11;
  }
}


void Backend_DrawChar(int32_t x, int32_t y, char c) {
  char p;
  for(int32_t i = 0; i < 11; i++)
    for(int32_t j = 0; j < 17; j++) {
      p = font[j+3][((c-32)*11)+i];
      if (p == ' ')
        Backend_SetDrawColor(0xff, 0xff, 0xff, 0xff);
      else
        Backend_SetDrawColor(0x00, 0x00, 0x00, 0x80);

      Backend_SetPixel(x+i, y+j);
    }
}


void Backend_SetKeepAspectRatio(bool keepAR) {
  this.keepAspectRatio = keepAR;
}

void Backend_SetPixelPerfectScale(bool enabled) {
  this.pixelPerfectScale = enabled;
}
