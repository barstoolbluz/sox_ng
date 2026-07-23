/* Regression test for sparse libsndfile-backed output finalization.
 *
 * Copyright (c) 2026 sox_ng contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "soxconfig.h"
#include "sox_ng.h"

#include <sndfile.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static char paths[9][1024];
static size_t path_count;

static void cleanup(void)
{
  size_t i;
  for (i = 0; i < path_count; ++i)
    if (paths[i][0])
      unlink(paths[i]);
}

static void fail(char const *message, char const *path)
{
  if (path)
    fprintf(stderr, "sparse_file_test: %s: %s\n", message, path);
  else
    fprintf(stderr, "sparse_file_test: %s\n", message);
  cleanup();
  sox_quit();
  exit(1);
}

static char const *new_path(char const *base, char const *suffix)
{
  int n;

  if (path_count == ARRAY_SIZE(paths))
    fail("internal path capacity exceeded", NULL);
  n = snprintf(paths[path_count], sizeof(paths[path_count]), "%s.%s", base, suffix);
  if (n < 0 || (size_t)n >= sizeof(paths[path_count]))
    fail("temporary path is too long", NULL);
  return paths[path_count++];
}

static uint64_t read_le64(unsigned char const *p)
{
  return (uint64_t)p[0] |
      (uint64_t)p[1] << 8 |
      (uint64_t)p[2] << 16 |
      (uint64_t)p[3] << 24 |
      (uint64_t)p[4] << 32 |
      (uint64_t)p[5] << 40 |
      (uint64_t)p[6] << 48 |
      (uint64_t)p[7] << 56;
}

static void write_audio(char const *path, char const *filetype,
    unsigned channels, size_t frames, sox_sample_t const *samples,
    size_t const *chunks, size_t chunk_count)
{
  sox_signalinfo_t signal = {
    44100,
    channels,
    53,
    (sox_uint64_t)frames * channels,
    NULL
  };
  sox_encodinginfo_t encoding = {
    SOX_ENCODING_FLOAT,
    64,
    0,
    sox_option_default,
    sox_option_default,
    sox_option_default,
    sox_false
  };
  sox_format_t *out = sox_open_write(path, &signal, &encoding, filetype,
      NULL, NULL);
  size_t total = frames * channels;
  size_t offset = 0;
  size_t i;

  if (!out)
    fail("cannot open output", path);
  if (!chunks) {
    if (sox_write(out, samples, total) != total) {
      sox_close(out);
      fail("short write", path);
    }
  } else {
    for (i = 0; i < chunk_count; ++i) {
      size_t count = chunks[i];
      if (count > total - offset) {
        sox_close(out);
        fail("invalid test chunking", path);
      }
      if (sox_write(out, samples + offset, count) != count) {
        sox_close(out);
        fail("short chunked write", path);
      }
      offset += count;
    }
    if (offset != total) {
      sox_close(out);
      fail("test chunks do not cover all samples", path);
    }
  }
  if (sox_close(out) != SOX_SUCCESS)
    fail("close failed", path);
}

static void write_zero_audio(char const *path, char const *filetype,
    unsigned channels, size_t frames)
{
  enum { chunk_samples = 6144 };
  sox_sample_t zeros[chunk_samples];
  sox_signalinfo_t signal = {
    44100,
    channels,
    53,
    (sox_uint64_t)frames * channels,
    NULL
  };
  sox_encodinginfo_t encoding = {
    SOX_ENCODING_FLOAT,
    64,
    0,
    sox_option_default,
    sox_option_default,
    sox_option_default,
    sox_false
  };
  sox_format_t *out;
  size_t remaining = frames * channels;

  memset(zeros, 0, sizeof(zeros));
  out = sox_open_write(path, &signal, &encoding, filetype, NULL, NULL);
  if (!out)
    fail("cannot open sparse output", path);
  while (remaining) {
    size_t count = remaining < chunk_samples ? remaining : chunk_samples;
    if (sox_write(out, zeros, count) != count) {
      sox_close(out);
      fail("short sparse write", path);
    }
    remaining -= count;
  }
  if (sox_close(out) != SOX_SUCCESS)
    fail("sparse close failed", path);
}

static void check_w64(char const *path, size_t frames, unsigned channels)
{
  static unsigned char const data_guid[16] = {
    0x64, 0x61, 0x74, 0x61, 0xf3, 0xac, 0xd3, 0x11,
    0x8c, 0xd1, 0x00, 0xc0, 0x4f, 0x8e, 0xdb, 0x8a
  };
  unsigned char header[4096];
  struct stat st;
  FILE *file = fopen(path, "rb");
  size_t got;
  size_t i;
  uint64_t riff_size;
  uint64_t data_size = 0;
  uint64_t expected_payload = (uint64_t)frames * channels * 8;
  int found = 0;

  if (!file)
    fail("cannot read W64 output", path);
  got = fread(header, 1, sizeof(header), file);
  if (ferror(file)) {
    fclose(file);
    fail("cannot read W64 header", path);
  }
  fclose(file);
  if (got < 24 || stat(path, &st) != 0)
    fail("cannot inspect W64 output", path);

  riff_size = read_le64(header + 16);
  for (i = 24; i + 24 <= got; ++i) {
    if (memcmp(header + i, data_guid, sizeof(data_guid)) == 0) {
      data_size = read_le64(header + i + 16);
      found = 1;
      break;
    }
  }
  if (!found)
    fail("W64 data chunk not found", path);
  if (riff_size != (uint64_t)st.st_size)
    fail("W64 RIFF size does not match file length", path);
  if (data_size != expected_payload + 24)
    fail("W64 data size does not match sample payload", path);
  if ((uint64_t)i + data_size != (uint64_t)st.st_size)
    fail("W64 data chunk does not end at EOF", path);
}

static void check_mat4(char const *path, size_t frames, unsigned channels)
{
  SF_INFO info;
  SNDFILE *file;

  memset(&info, 0, sizeof(info));
  file = sf_open(path, SFM_READ, &info);
  if (!file)
    fail(sf_strerror(NULL), path);
  if (info.frames != (sf_count_t)frames || info.channels != (int)channels) {
    sf_close(file);
    fail("MAT4 frame count or channel count is wrong", path);
  }
  if (sf_close(file) != 0)
    fail("cannot close MAT4 input", path);
}

static int files_equal(char const *left, char const *right)
{
  unsigned char a[4096];
  unsigned char b[4096];
  FILE *fa = fopen(left, "rb");
  FILE *fb = fopen(right, "rb");
  int equal = 1;

  if (!fa || !fb) {
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    fail("cannot open byte-equivalence control", NULL);
  }
  for (;;) {
    size_t na = fread(a, 1, sizeof(a), fa);
    size_t nb = fread(b, 1, sizeof(b), fb);
    if (na != nb || memcmp(a, b, na) != 0) {
      equal = 0;
      break;
    }
    if (na < sizeof(a)) {
      if (ferror(fa) || ferror(fb))
        equal = 0;
      break;
    }
  }
  fclose(fa);
  fclose(fb);
  return equal;
}

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
static int filesystem_supports_sparse(char const *path, off_t size)
{
  struct stat st;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  unsigned char zero = 0;

  if (fd < 0)
    fail("cannot create sparse capability probe", path);
  if (lseek(fd, size - 1, SEEK_SET) < 0 || write(fd, &zero, 1) != 1) {
    close(fd);
    fail("cannot create sparse capability probe", path);
  }
  if (close(fd) != 0 || stat(path, &st) != 0)
    fail("cannot inspect sparse capability probe", path);
  return (uint64_t)st.st_blocks * 512 < (uint64_t)st.st_size / 4;
}

static void check_sparse(char const *path, int sparse_supported)
{
  struct stat st;

  if (stat(path, &st) != 0)
    fail("cannot stat sparse output", path);
  if (sparse_supported &&
      (uint64_t)st.st_blocks * 512 >= (uint64_t)st.st_size / 4)
    fail("silent output is not sparse", path);
}
#endif

int main(void)
{
  char template[] = "sox-ng-sparse-file-test-XXXXXX";
  int fd = mkstemp(template);
  char const *zero_w64;
  char const *tail_w64;
  char const *zero_mat4;
  char const *tail_mat4;
  char const *sparse_w64;
  char const *sparse_mat4;
  char const *tone_one_write;
  char const *tone_chunked;
  char const *sparse_probe;
  enum { frames = 480, channels = 1 };
  sox_sample_t zero[frames * channels];
  sox_sample_t tail[frames * channels];
  sox_sample_t tone[frames * channels];
  size_t chunks[] = {17, 103, 1, frames - 121};
  size_t i;

  if (fd < 0) {
    fprintf(stderr, "sparse_file_test: mkstemp failed: %s\n", strerror(errno));
    return 1;
  }
  close(fd);
  unlink(template);
  if (atexit(cleanup) != 0)
    return 1;

  zero_w64 = new_path(template, "zero.w64");
  tail_w64 = new_path(template, "tail.w64");
  zero_mat4 = new_path(template, "zero.mat4");
  tail_mat4 = new_path(template, "tail.mat4");
  sparse_w64 = new_path(template, "sparse.w64");
  sparse_mat4 = new_path(template, "sparse.mat4");
  tone_one_write = new_path(template, "tone-one.w64");
  tone_chunked = new_path(template, "tone-chunked.w64");
  sparse_probe = new_path(template, "sparse-probe");

  memset(zero, 0, sizeof(zero));
  for (i = 0; i < ARRAY_SIZE(tone); ++i)
    tone[i] = (i & 1) ? -(SOX_SAMPLE_MAX / 4) : SOX_SAMPLE_MAX / 4;
  memcpy(tail, tone, sizeof(tail));
  memset(tail + ARRAY_SIZE(tail) / 2, 0, sizeof(tail) / 2);

  if (sox_init() != SOX_SUCCESS || sox_format_init() != SOX_SUCCESS) {
    fprintf(stderr, "sparse_file_test: cannot initialize libSoX\n");
    return 1;
  }

  write_audio(zero_w64, "w64", channels, frames, zero, NULL, 0);
  write_audio(tail_w64, "w64", channels, frames, tail, NULL, 0);
  write_audio(zero_mat4, "mat4", channels, frames, zero, NULL, 0);
  write_audio(tail_mat4, "mat4", channels, frames, tail, NULL, 0);
  check_w64(zero_w64, frames, channels);
  check_w64(tail_w64, frames, channels);
  check_mat4(zero_mat4, frames, channels);
  check_mat4(tail_mat4, frames, channels);

  write_zero_audio(sparse_w64, "w64", 6, 44100 * 3);
  write_zero_audio(sparse_mat4, "mat4", 6, 44100 * 3);
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
  {
    struct stat st;
    int sparse_supported;
    if (stat(sparse_w64, &st) != 0)
      fail("cannot stat sparse W64 output", sparse_w64);
    sparse_supported = filesystem_supports_sparse(sparse_probe, st.st_size);
    check_sparse(sparse_w64, sparse_supported);
    check_sparse(sparse_mat4, sparse_supported);
  }
#else
  (void)sparse_probe;
#endif

  write_audio(tone_one_write, "w64", channels, frames, tone, NULL, 0);
  write_audio(tone_chunked, "w64", channels, frames, tone,
      chunks, ARRAY_SIZE(chunks));
  if (!files_equal(tone_one_write, tone_chunked))
    fail("non-silent output changed with write chunking", NULL);

  if (sox_quit() != SOX_SUCCESS)
    fail("cannot shut down libSoX", NULL);
  cleanup();
  return 0;
}
