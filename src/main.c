#ifdef WIN32
#include "GLFW/glfw3.h"
#else
#include <GLFW/glfw3.h>
#endif

#include "glad/glad.h"
#include "minimp3/minimp3.h"
#include "minimp3/minimp3_ex.h"
#include "audio.h"
#include "fft.h"

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(arr)    (sizeof (arr) / sizeof ((arr)[0]))
#endif

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 640
#define WINDOW_TITLE  "opengl learning"

#define FFT_LOGSIZE   10
#define FFT_SIZE      ((size_t)1 << FFT_LOGSIZE)
#define FFT_NFREQ     (FFT_SIZE / 2 + 1)

#define POSITION_LOCATION 0
#define COLOR_LOCATION    1

#define GL_CALL(call) do {                                              \
  call;                                                                 \
  int error = glGetError();                                             \
  if (error) {                                                          \
    fprintf(stderr, "error occurred at %s:%d:%s, error code: %d\n",     \
            __FILE__, __LINE__, __func__, error);                       \
    exit(0);                                                            \
  }                                                                     \
} while (0)

struct point {
  float x;
  float y;
};

struct context {
  GLuint VAO;
  GLuint VBO;
  GLuint program;
  mp3dec_t mp3dec;
  mp3dec_file_info_t mp3fileinfo;
  struct audio_desc audio;
  fft_complex_t (*fftbuffers)[FFT_SIZE];
  struct {
    struct point a1;
    struct point a2;
    struct point a3;
    struct point b1;
    struct point b2;
    struct point b3;
  } *blocks;
};

static void context_init(struct context *context, const char *music, const char *vspath, const char *fspath);
static void context_deinit(struct context *context);
static void prepare_data(struct context *context, const char *music);
static void prepare_program(struct context *context, const char *vspath, const char *fspath);
static void render(struct context *context, size_t currpos);
static void play_audio(struct context *context, const char *params);
static void window_resize_callback(GLFWwindow* window, int width, int height);

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "you must provide mp3 file path\n");
    return EXIT_FAILURE;
  }

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
  if (!window) {
    fprintf(stderr, "failed to create window\n");
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSetWindowSizeCallback(window, window_resize_callback);

  GL_CALL(gladLoadGL());

  struct context context;
  context_init(&context, argv[1], "resources/vs.glsl", "resources/fs.glsl");

  glClearColor(0.0, 0.0, 0.0, 1.0);
  play_audio(&context, argc > 2 ? argv[2] : NULL);

  size_t audiopos = 0;
  audiopos = audio_getpos(&context.audio);
  while (!glfwWindowShouldClose(window) && !audio_end(&context.audio)) {
    render(&context, audiopos);
    glfwSwapBuffers(window);
    glfwPollEvents();
    audio_continue(&context.audio);
    audiopos = audio_getpos(&context.audio);
  }

  context_deinit(&context);
  glfwDestroyWindow(window);

  glfwTerminate();
  return EXIT_SUCCESS;
}

static void do_fft(struct context *context, size_t currpos) {
  /* prepare data */
  size_t nchannel = context->audio.nchannel;
  const mp3d_sample_t *buffer = context->audio.data + currpos * nchannel;

  if ((currpos + FFT_SIZE) * nchannel > context->audio.samples)
    return;

  for (size_t channel = 0; channel < nchannel; ++channel) {
    for (size_t i = 0; i < FFT_SIZE; ++i) {
      context->fftbuffers[channel][i].real = buffer[i * nchannel + channel];
      context->fftbuffers[channel][i].imag = 0.0;
    }
  }
  for (size_t channel = 0; channel < nchannel; ++channel)
    fft_inplace(context->fftbuffers[channel], FFT_LOGSIZE);
}

static inline float complex_mod(fft_complex_t complex) {
  return sqrtf(complex.real * complex.real + complex.imag * complex.imag);
}

static void render_allchannels(struct context *context) {
  for (size_t i = 0; i < (size_t)context->audio.nchannel; ++i) {
    GL_CALL(glUseProgram(context->program));
    GL_CALL(glBindVertexArray(context->VAO));

    GLint total_channel_uniform = glGetUniformLocation(context->program, "total_channel");
    GL_CALL(glUniform1f(total_channel_uniform, (float)context->audio.nchannel));
    GLint channel_uniform = glGetUniformLocation(context->program, "channel");
    GL_CALL(glUniform1f(channel_uniform, (float)i));

    fft_complex_t *fft_result = context->fftbuffers[i];
    const float divisor = (0.7) * ((size_t)1 << sizeof (mp3d_sample_t) * CHAR_BIT) / 2;
    for (size_t j = 0; j < FFT_NFREQ; ++j) {
      float amplitude = complex_mod(fft_result[j]) * 2 / FFT_SIZE / divisor;
      context->blocks[j].a1.x = (float)j / FFT_NFREQ;
      context->blocks[j].a1.y = 0;
      context->blocks[j].a2.x = (float)(j + 1) / FFT_NFREQ;
      context->blocks[j].a2.y = 0;
      context->blocks[j].a3.x = (float)j / FFT_NFREQ;
      context->blocks[j].a3.y = amplitude;
      context->blocks[j].b1 = context->blocks[j].a2;
      context->blocks[j].b2 = context->blocks[j].a3;
      context->blocks[j].b3.x = (float)(j + 1) / FFT_NFREQ;
      context->blocks[j].b3.y = amplitude;
    }
    /* fix 0 and FFT_SIZE / 2 */
    context->blocks[0].a3.y /= 2;
    context->blocks[0].b3.y /= 2;
    context->blocks[0].b2.y /= 2;
    if (FFT_SIZE / 2 < FFT_NFREQ) {
      context->blocks[FFT_SIZE / 2].a3.y /= 2;
      context->blocks[FFT_SIZE / 2].b3.y /= 2;
      context->blocks[FFT_SIZE / 2].b2.y /= 2;
    }
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, context->VBO));
    GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof (context->blocks[0]) * FFT_NFREQ, context->blocks));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, FFT_NFREQ * 6));
    GL_CALL(glBindVertexArray(0));
  }
}

static void render(struct context *context, size_t currpos) {
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
  do_fft(context, currpos);
  render_allchannels(context);
}

static void context_init(struct context *context, const char *music, const char *vspath, const char *fspath) {
  prepare_data(context, music);
  prepare_program(context, vspath, fspath);
}

static void context_deinit(struct context *context) {
  GL_CALL(glDeleteProgram(context->program));
  GL_CALL(glDeleteBuffers(1, &context->VBO));
  GL_CALL(glDeleteVertexArrays(1, &context->VAO));
  audio_free(&context->audio);

  free(context->mp3fileinfo.buffer);
  free(context->fftbuffers);
  free(context->blocks);
}

static void prepare_data(struct context *context, const char *music) {
  mp3dec_init(&context->mp3dec);
  if (mp3dec_load(&context->mp3dec, music, &context->mp3fileinfo, NULL, NULL)) {
    fprintf(stderr, "failed to load file: %s\n", music);
    exit(EXIT_FAILURE);
  }

  context->fftbuffers = malloc(sizeof (context->fftbuffers[0]) * context->mp3fileinfo.channels);
  if (!context->fftbuffers) {
    fprintf(stderr, "failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }

  context->blocks = malloc(FFT_NFREQ * sizeof (context->blocks[0]));
  if (!context->blocks) {
    fprintf(stderr, "failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }
  memset(context->blocks, 0, FFT_NFREQ * sizeof (context->blocks[0])); 

  GL_CALL(glGenVertexArrays(1, &context->VAO));
  GL_CALL(glBindVertexArray(context->VAO));

  GL_CALL(glGenBuffers(1, &context->VBO));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, context->VBO));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, FFT_NFREQ * sizeof (context->blocks[0]), context->blocks, GL_DYNAMIC_DRAW));

  GL_CALL(glEnableVertexAttribArray(POSITION_LOCATION));
  GL_CALL(glVertexAttribPointer(POSITION_LOCATION, 2, GL_FLOAT, GL_FALSE, 2 * sizeof (float), (void*)0));
  // GL_CALL(glEnableVertexAttribArray(COLOR_LOCATION));
  // GL_CALL(glVertexAttribPointer(COLOR_LOCATION, 3, GL_FLOAT, GL_FALSE, 6 * sizeof (float), (void*)(sizeof (float) * 3)));

  GL_CALL(glBindVertexArray(0));
}

static char *read_full_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "can not open file: %s\n", path);
    exit(EXIT_FAILURE);
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = malloc((size + 1) * sizeof (char));
  if (!buf) {
    fprintf(stderr, "faied to allocate memory\n");
    exit(EXIT_FAILURE);
  }
  fread(buf, sizeof (char), size, f);
  buf[size] = '\0';

  fclose(f);
  return buf;
}

static void compile_shader(GLuint shader, const char *srcpath) {
  char *src = read_full_file(srcpath);
  const char *tmp = src;
  GL_CALL(glShaderSource(shader, 1, &tmp, NULL));
  GL_CALL(glCompileShader(shader));
  free(src);

  GLint status;
  GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
  if (status == GL_FALSE) {
    fprintf(stderr, "compile error in shader: %s\n", srcpath);
    GLchar log[512];
    GL_CALL(glGetShaderInfoLog(shader, ARRAYSIZE(log), NULL, log));
    fprintf(stderr, "%s\n", log);
    exit(EXIT_FAILURE);
  }
}

static void link_shader(GLuint program, GLuint vs, GLuint fs) {
  GL_CALL(glAttachShader(program, vs));
  GL_CALL(glAttachShader(program, fs));
  GL_CALL(glLinkProgram(program));

  GLint status;
  GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &status));
  if (status == GL_FALSE) {
    fprintf(stderr, "link error in program:\n");
    GLchar log[512];
    GL_CALL(glGetProgramInfoLog(program, ARRAYSIZE(log), NULL, log));
    fprintf(stderr, "%s\n", log);
    exit(EXIT_FAILURE);
  }
}

static void prepare_program(struct context *context, const char *vspath, const char *fspath) {
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

  compile_shader(vs, vspath);
  compile_shader(fs, fspath);

  context->program = glCreateProgram();
  link_shader(context->program, vs, fs);

  glDeleteShader(vs);
  glDeleteShader(fs);
}

static void play_audio(struct context *context, const char *params) {
  context->audio = (struct audio_desc) {
    .nchannel = context->mp3fileinfo.channels,
    .samples = context->mp3fileinfo.samples,
    .rate = context->mp3fileinfo.hz,
    .data = context->mp3fileinfo.buffer,
    .avg_bitrate_kbps = context->mp3fileinfo.avg_bitrate_kbps,
  };

  audio_play(&context->audio, params);
}

static void window_resize_callback(GLFWwindow* window, int width, int height) {
  (void)window;
  glViewport(0, 0, width, height);
}
