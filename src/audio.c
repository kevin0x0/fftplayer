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

  const char *device = params ? params: "default";
  int err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "error: can not open default device\n");
    exit(EXIT_FAILURE);
  }

  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(pcm_handle, hw_params);
  snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, hw_params, desc->nchannel);
  snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &desc->rate, NULL);
  snd_pcm_uframes_t period = 4096;
  snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period, 0);
  snd_pcm_hw_params(pcm_handle, hw_params);

  snd_pcm_prepare(pcm_handle);
  snd_pcm_start(pcm_handle);

  desc->pcm_handle = pcm_handle;
  desc->currpos = 0;
  desc->period_size = period;
}

void audio_continue(struct audio_desc *desc) {
  snd_pcm_t *pcm_handle = desc->pcm_handle;
  while (true) {
    snd_pcm_state_t state = snd_pcm_state(pcm_handle);
    if (state == SND_PCM_STATE_XRUN) {
      if (snd_pcm_recover(pcm_handle, -EPIPE, 1) < 0) {
        fprintf(stderr, "can not recover from underrun state\n");
        exit(EXIT_FAILURE);
      }
      continue;
    } else if (state == SND_PCM_STATE_PREPARED) {
      snd_pcm_start(pcm_handle);
    } else if (state != SND_PCM_STATE_RUNNING) {
      fprintf(stderr, "bad state: %s\n", snd_pcm_state_name(state));
      exit(EXIT_FAILURE);
    }
    snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm_handle);
    if (avail < 0)
      continue;

    if ((snd_pcm_uframes_t)avail < desc->period_size)
      return;

    snd_pcm_uframes_t nframe = desc->samples / desc->nchannel;
    if (desc->currpos >= nframe)
      return;

    snd_pcm_uframes_t frames = nframe - desc->currpos > desc->period_size ? desc->period_size : nframe - desc->currpos;
    snd_pcm_uframes_t offset;
    const snd_pcm_channel_area_t *areas;
    int err = snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
    if (err < 0)
      continue;
    size_t framesize = desc->nchannel * sizeof(mp3d_sample_t);
    void *mmapped_buffer = (char *)areas[0].addr + offset * framesize;
    memcpy(mmapped_buffer, desc->data + desc->nchannel * desc->currpos, framesize * frames);
    snd_pcm_sframes_t commit_frames = snd_pcm_mmap_commit(desc->pcm_handle, offset, frames);
    if (commit_frames < 0 || (snd_pcm_uframes_t)commit_frames < frames) {
      fprintf(stderr, "commit failure");
      if (snd_pcm_recover(pcm_handle, commit_frames < 0 ? commit_frames : -EPIPE, 1) < 0) {
        fprintf(stderr, "can not recover\n");
        exit(EXIT_FAILURE);
      }
    }

    if (commit_frames > 0)
      desc->currpos += commit_frames;
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
