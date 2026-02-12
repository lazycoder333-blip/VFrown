#include "wav_writer.h"

struct WavWriter_t {
  FILE* file;
  uint32_t dataSize;
  int sampleRate;
  int numChannels;
};

static void Wav_WriteU16(FILE* f, uint16_t value) {
  unsigned char bytes[2];
  bytes[0] = (unsigned char)(value & 0xff);
  bytes[1] = (unsigned char)((value >> 8) & 0xff);
  fwrite(bytes, 1, 2, f);
}

static void Wav_WriteU32(FILE* f, uint32_t value) {
  unsigned char bytes[4];
  bytes[0] = (unsigned char)(value & 0xff);
  bytes[1] = (unsigned char)((value >> 8) & 0xff);
  bytes[2] = (unsigned char)((value >> 16) & 0xff);
  bytes[3] = (unsigned char)((value >> 24) & 0xff);
  fwrite(bytes, 1, 4, f);
}

WavWriter_t* Wav_Open(const char* path, int sampleRate, int numChannels) {
  if (!path || sampleRate <= 0 || numChannels <= 0 || numChannels > 2) {
    return NULL;
  }

  FILE* f = fopen(path, "wb");
  if (!f) {
    return NULL;
  }

  WavWriter_t* wav = (WavWriter_t*)malloc(sizeof(WavWriter_t));
  if (!wav) {
    fclose(f);
    return NULL;
  }

  wav->file = f;
  wav->dataSize = 0;
  wav->sampleRate = sampleRate;
  wav->numChannels = numChannels;

  // Write WAV header (will be updated on close)
  fwrite("RIFF", 1, 4, f);
  Wav_WriteU32(f, 0); // File size - 8 (placeholder)
  fwrite("WAVE", 1, 4, f);

  // fmt chunk
  fwrite("fmt ", 1, 4, f);
  Wav_WriteU32(f, 16); // fmt chunk size
  Wav_WriteU16(f, 1);  // PCM format
  Wav_WriteU16(f, (uint16_t)numChannels);
  Wav_WriteU32(f, (uint32_t)sampleRate);
  Wav_WriteU32(f, (uint32_t)(sampleRate * numChannels * 2)); // byte rate
  Wav_WriteU16(f, (uint16_t)(numChannels * 2)); // block align
  Wav_WriteU16(f, 16); // bits per sample

  // data chunk header
  fwrite("data", 1, 4, f);
  Wav_WriteU32(f, 0); // data size (placeholder)

  return wav;
}

bool Wav_WriteSamples(WavWriter_t* wav, const float* samples, uint32_t count) {
  if (!wav || !wav->file || !samples || count == 0) {
    return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    float sample = samples[i];
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int16_t pcm = (int16_t)(sample * 32767.0f);
    
    unsigned char bytes[2];
    bytes[0] = (unsigned char)(pcm & 0xff);
    bytes[1] = (unsigned char)((pcm >> 8) & 0xff);
    fwrite(bytes, 1, 2, wav->file);
  }

  wav->dataSize += count * 2;
  return true;
}

bool Wav_Close(WavWriter_t* wav) {
  if (!wav || !wav->file) {
    return false;
  }

  // Update file size in header
  fseek(wav->file, 4, SEEK_SET);
  Wav_WriteU32(wav->file, wav->dataSize + 36);

  // Update data chunk size
  fseek(wav->file, 40, SEEK_SET);
  Wav_WriteU32(wav->file, wav->dataSize);

  fclose(wav->file);
  free(wav);
  return true;
}
