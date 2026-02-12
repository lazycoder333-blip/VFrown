#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include "../common.h"

typedef struct WavWriter_t WavWriter_t;

WavWriter_t* Wav_Open(const char* path, int sampleRate, int numChannels);
bool Wav_WriteSamples(WavWriter_t* wav, const float* samples, uint32_t count);
bool Wav_Close(WavWriter_t* wav);

#endif // WAV_WRITER_H
