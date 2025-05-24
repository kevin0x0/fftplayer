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

void audio_play(struct audio_desc *desc, const char *params) {
  (void)params;
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
void audio_play(struct audio_desc *desc, const char *params) {
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *hw_params;

  const char *device = params ? params : "default";
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
  snd_pcm_uframes_t period = 4096;
  snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period, 0);
  snd_pcm_hw_params(pcm_handle, hw_params);

  snd_pcm_prepare(pcm_handle);

  desc->pcm_handle = pcm_handle;
  desc->currpos = 0;
  desc->period_size = period;
}

void audio_continue(struct audio_desc *desc) {
  snd_pcm_uframes_t nframe = desc->samples / desc->nchannel;
  if (desc->currpos >= nframe)
    return;

  snd_pcm_t *pcm_handle = desc->pcm_handle;
  while (true) {
    snd_pcm_state_t state = snd_pcm_state(pcm_handle);
    if (state == SND_PCM_STATE_XRUN) {
      if (snd_pcm_recover(pcm_handle, -EPIPE, 0) < 0) {
        fprintf(stderr, "failed to recover from underrun\n");
        exit(EXIT_FAILURE);
      }
      if (snd_pcm_prepare(pcm_handle) < 0) {
        fprintf(stderr, "failed to recover from underrun\n");
        exit(EXIT_FAILURE);
      }
    }
    size_t remaining_frames = nframe - desc->currpos;
    size_t writeframes = desc->period_size > remaining_frames ? remaining_frames : desc->period_size;
    snd_pcm_sframes_t sframes = snd_pcm_writei(pcm_handle, desc->data + desc->nchannel * desc->currpos, writeframes);
    if (sframes == -EPIPE) {
      continue;
    } else if (sframes == -EAGAIN) {
      return;
    } else if (sframes < 0) {
      fprintf(stderr, "error: %s\n", snd_strerror(sframes));
      exit(EXIT_FAILURE);
    }
    desc->currpos += sframes;
    return;
  }
}

void audio_free(struct audio_desc *desc) {
  snd_pcm_drain(desc->pcm_handle);
  snd_pcm_close(desc->pcm_handle);
}

size_t audio_getpos(struct audio_desc *desc) {
  snd_pcm_sframes_t delay;
  if (snd_pcm_delay(desc->pcm_handle, &delay) < 0)
    return desc->currpos;
  return desc->currpos - delay;
}

bool audio_end(struct audio_desc *desc) {
  return desc->currpos * desc->nchannel >= desc->samples;
}

#endif
