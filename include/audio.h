#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "minimp3/minimp3.h"

#include <stdbool.h>
#include <windows.h>
#include <mmsystem.h>
#include <stdatomic.h>
#include <stddef.h>

struct audio_desc {
  mp3d_sample_t *data;
  size_t samples;
  int nchannel;
  int hz;
  int avg_bitrate_kbps;
  HWAVEOUT hWaveOut;
  WAVEHDR waveHdr;
};

void audio_play(struct audio_desc *desc);
void audio_free(struct audio_desc *desc);
size_t audio_getpos(struct audio_desc *desc);
bool audio_end(struct audio_desc *desc);

#endif
