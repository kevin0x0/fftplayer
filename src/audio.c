#include "audio.h"

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
bool audio_end(struct audio_desc *desc) {
  return desc->waveHdr.dwFlags & WHDR_DONE;
}

void audio_continue(struct audio_desc *desc) {
  (void)desc;
}

size_t audio_getpos(struct audio_desc *desc) {
  MMTIME mmtime;
  mmtime.wType = TIME_SAMPLES;

  waveOutGetPosition(desc->hWaveOut, &mmtime, sizeof(MMTIME));
  return mmtime.u.sample;
}

void audio_play(struct audio_desc *desc) {
  // 配置音频格式
  WAVEFORMATEX wf;
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = desc->nchannel;
  wf.nSamplesPerSec = desc->hz;
  wf.wBitsPerSample = sizeof (desc->data[0]) * CHAR_BIT;
  wf.nBlockAlign = wf.nChannels * (wf.wBitsPerSample / 8);
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
  wf.cbSize = 0;

  // 加载 PCM 数据
  DWORD dataSize = sizeof (desc->data[0]) * desc->samples;
  // 打开音频输出设备
  if (waveOutOpen(&desc->hWaveOut, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
    fprintf(stderr, "Failed to open wave output\n");
    exit(EXIT_FAILURE);
  }

  // 配置和加载音频缓冲区
  desc->waveHdr.lpData = (LPSTR)desc->data;
  desc->waveHdr.dwBufferLength = dataSize;
  desc->waveHdr.dwFlags = 0;
  desc->waveHdr.dwLoops = 0;

  if (waveOutPrepareHeader(desc->hWaveOut, &desc->waveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
    fprintf(stderr, "Failed to prepare wave header\n");
    waveOutClose(desc->hWaveOut);
    exit(EXIT_FAILURE);
  }

  // 播放音频
  if (waveOutWrite(desc->hWaveOut, &desc->waveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
    fprintf(stderr, "Failed to write to wave output\n");
    waveOutUnprepareHeader(desc->hWaveOut, &desc->waveHdr, sizeof(WAVEHDR));
    waveOutClose(desc->hWaveOut);
    exit(EXIT_FAILURE);
  }
}

void audio_free(struct audio_desc *desc) {
  waveOutUnprepareHeader(desc->hWaveOut, &desc->waveHdr, sizeof(WAVEHDR));
  waveOutClose(desc->hWaveOut);
}
#else
void audio_play(struct audio_desc *desc) {
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *hw_params;

  const char *device = "hw";
  int err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
  if (err < 0) {
    fprintf(stderr, "error: can not open default device\n");
    exit(EXIT_FAILURE);
  }

  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(pcm_handle, hw_params);
  snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, hw_params, desc->nchannel);
  snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &desc->rate, NULL);
  snd_pcm_uframes_t period = 16384;
  snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period, 0);
  snd_pcm_hw_params(pcm_handle, hw_params);

  desc->pcm_handle = pcm_handle;
  desc->currpos = 0;
  desc->period_size = period;
}

static void ensure_good_state(snd_pcm_t *pcm_handle) {
  snd_pcm_state_t state = snd_pcm_state(pcm_handle);

  switch (state) {
    case SND_PCM_STATE_PREPARED:
    case SND_PCM_STATE_RUNNING: {
      break;
    }
    case SND_PCM_STATE_XRUN: {
      if (snd_pcm_prepare(pcm_handle) < 0) {
        fprintf(stderr, "can not recover from underrun state");
        exit(EXIT_FAILURE);
      }
      break;
    }
    case SND_PCM_STATE_SETUP: {
      if (snd_pcm_prepare(pcm_handle) < 0)
        exit(EXIT_FAILURE);

      break;
    }
    default: {
      exit(EXIT_FAILURE);
    }
  }
}

void audio_continue(struct audio_desc *desc) {
  snd_pcm_uframes_t nframe = desc->samples / desc->nchannel;
  if (desc->currpos >= nframe)
    return;
  ensure_good_state(desc->pcm_handle);

  size_t residual_frames = nframe - desc->currpos;
  snd_pcm_uframes_t writeframes = desc->period_size > residual_frames ? residual_frames : desc->period_size;
  snd_pcm_sframes_t actual_sframes = snd_pcm_writei(desc->pcm_handle, desc->data + desc->nchannel * desc->currpos, writeframes);
  if (actual_sframes == -EAGAIN) {
    return;
  } else if (actual_sframes == -EPIPE) {
    snd_pcm_prepare(desc->pcm_handle);
    actual_sframes = snd_pcm_writei(desc->pcm_handle, desc->data, desc->period_size);
    if (actual_sframes < 0) {
      fprintf(stderr, "writei again error: %s\n", snd_strerror(actual_sframes));
      exit(EXIT_FAILURE);
    }
  } else if (actual_sframes < 0) {
    fprintf(stderr, "writei error(%d): %s\n", (int)actual_sframes, snd_strerror(actual_sframes));
    exit(EXIT_FAILURE);
  }
  desc->currpos += actual_sframes;
}

void audio_free(struct audio_desc *desc) {
  snd_pcm_drain(desc->pcm_handle);
  snd_pcm_close(desc->pcm_handle);
}

size_t audio_getpos(struct audio_desc *desc) {
  return desc->currpos;
}

bool audio_end(struct audio_desc *desc) {
  return desc->currpos * desc->nchannel >= desc->samples;
}

#endif
