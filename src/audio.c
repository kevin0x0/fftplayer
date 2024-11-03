#include "audio.h"

#include <pthread.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool audio_end(struct audio_desc *desc) {
  return desc->waveHdr.dwFlags & WHDR_DONE;
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
