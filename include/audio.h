#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "minimp3/minimp3.h"

#include <stdbool.h>
#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <alsa/asoundlib.h>
#endif
#include <stdatomic.h>
#include <stddef.h>

struct audio_desc {
#ifdef WIN32
  HWAVEOUT hWaveOut;
  WAVEHDR waveHdr;
#else
  snd_pcm_t *pcm_handle;
  size_t currpos;
  snd_pcm_uframes_t period_size;
#endif
  mp3d_sample_t *data;
  size_t samples;
  int nchannel;
  unsigned int rate;
  int avg_bitrate_kbps;
};

void audio_play(struct audio_desc *desc, const char *params);
void audio_free(struct audio_desc *desc);
size_t audio_getpos(struct audio_desc *desc);
bool audio_end(struct audio_desc *desc);
void audio_continue(struct audio_desc *desc);

#endif
