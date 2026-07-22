#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wav-size.h"

#define CHECK(expr) do { \
  if (!(expr)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
    return EXIT_FAILURE; \
  } \
} while (0)

static int test_seekable_fast_path_warning(void)
{
  lsx_wav_warning_state_t state;
  lsx_wav_header_sizes_t sizes;
  unsigned actions;

  lsx_wav_warning_state_init(&state);
  sizes = lsx_wav_header_sizes(UINT64_C(0xfffffff0),
      UINT64_C(0x1ffffffe), 18, 4, 1);

  CHECK(sizes.riff_length == UINT32_C(0x7ffff032));
  CHECK(sizes.data_length == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.sample_count == UINT32_C(0x1ffffffe));
  CHECK(!sizes.sample_count_unspecified);
  CHECK(sizes.data_length_unspecified);

  actions = lsx_wav_header_warning_actions(&state, &sizes, 0, 1);
  CHECK(actions == LSX_WAV_WARN_NONE);
  CHECK(state.first_header_data_unspecified);
  CHECK(!state.warned_data_length);

  actions = lsx_wav_retained_header_warning_actions(&state);
  CHECK(actions == LSX_WAV_WARN_DATA_LENGTH);
  CHECK(!state.first_header_data_unspecified);
  CHECK(state.warned_data_length);

  /* Re-entering the terminal path must not duplicate the advisory. */
  CHECK(lsx_wav_retained_header_warning_actions(&state) ==
      LSX_WAV_WARN_NONE);
  return EXIT_SUCCESS;
}

static int test_seekable_second_header_warning(void)
{
  lsx_wav_warning_state_t state;
  lsx_wav_header_sizes_t sizes;
  unsigned actions;

  lsx_wav_warning_state_init(&state);
  sizes = lsx_wav_header_sizes(UINT64_C(34359738368),
      UINT64_C(4294967296), 18, 4, 1);

  CHECK(sizes.riff_length == UINT32_C(0x7ffff032));
  CHECK(sizes.data_length == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.sample_count == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.sample_count_unspecified);
  CHECK(sizes.data_length_unspecified);

  /* The provisional seekable header defers both advisories. */
  actions = lsx_wav_header_warning_actions(&state, &sizes, 0, 1);
  CHECK(actions == LSX_WAV_WARN_NONE);
  CHECK(state.first_header_data_unspecified);
  CHECK(!state.warned_sample_count);
  CHECK(!state.warned_data_length);

  /* The seek-back header emits each applicable advisory exactly once. */
  actions = lsx_wav_header_warning_actions(&state, &sizes, 1, 1);
  CHECK(actions ==
      (LSX_WAV_WARN_SAMPLE_COUNT | LSX_WAV_WARN_DATA_LENGTH));
  CHECK(!state.first_header_data_unspecified);
  CHECK(state.warned_sample_count);
  CHECK(state.warned_data_length);

  CHECK(lsx_wav_header_warning_actions(&state, &sizes, 1, 1) ==
      LSX_WAV_WARN_NONE);
  CHECK(lsx_wav_retained_header_warning_actions(&state) ==
      LSX_WAV_WARN_NONE);
  return EXIT_SUCCESS;
}


static int test_rewritten_exact_header_clears_pending_warning(void)
{
  lsx_wav_warning_state_t state;
  lsx_wav_header_sizes_t provisional;
  lsx_wav_header_sizes_t final;

  lsx_wav_warning_state_init(&state);
  provisional = lsx_wav_header_sizes(UINT64_C(0xfffffff0),
      UINT64_C(0x1ffffffe), 18, 4, 1);
  CHECK(lsx_wav_header_warning_actions(&state, &provisional, 0, 1) ==
      LSX_WAV_WARN_NONE);
  CHECK(state.first_header_data_unspecified);

  final = lsx_wav_header_sizes(UINT64_C(8000), UINT64_C(1000), 18, 4, 1);
  CHECK(lsx_wav_header_warning_actions(&state, &final, 1, 1) ==
      LSX_WAV_WARN_NONE);
  CHECK(!state.first_header_data_unspecified);
  CHECK(lsx_wav_retained_header_warning_actions(&state) ==
      LSX_WAV_WARN_NONE);
  return EXIT_SUCCESS;
}

static int test_boundary_and_extreme_inputs(void)
{
  lsx_wav_header_sizes_t sizes;

  sizes = lsx_wav_header_sizes(UINT64_C(0xffffffda),
      UINT64_C(0xffffffda), 16, 4, 0);
  CHECK(sizes.riff_length == UINT32_C(0xfffffffe));
  CHECK(sizes.data_length == UINT32_C(0xffffffda));
  CHECK(!sizes.data_length_unspecified);

  sizes = lsx_wav_header_sizes(UINT64_C(0xffffffdb),
      UINT64_C(0xffffffdb), 16, 4, 0);
  CHECK(sizes.riff_length == UINT32_C(0x7ffff024));
  CHECK(sizes.data_length == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.data_length_unspecified);

  /* The comparison must remain safe even for a maximal uint64_t input. */
  sizes = lsx_wav_header_sizes(UINT64_MAX, UINT64_MAX, 40, 4, 1);
  CHECK(sizes.riff_length == UINT32_C(0x7ffff048));
  CHECK(sizes.data_length == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.sample_count == LSX_WAV_MS_UNSPEC);
  CHECK(sizes.data_length_unspecified);
  CHECK(sizes.sample_count_unspecified);
  return EXIT_SUCCESS;
}

int main(void)
{
  if (test_seekable_fast_path_warning() != EXIT_SUCCESS ||
      test_seekable_second_header_warning() != EXIT_SUCCESS ||
      test_rewritten_exact_header_clears_pending_warning() != EXIT_SUCCESS ||
      test_boundary_and_extreme_inputs() != EXIT_SUCCESS)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
