#ifndef SOX_WAV_SIZE_H
#define SOX_WAV_SIZE_H

#include <stdint.h>

/* SoX's established WAV read-to-EOF sentinel. */
#define LSX_WAV_MS_UNSPEC UINT32_C(0x7ffff000)

enum {
  LSX_WAV_WARN_NONE = 0,
  LSX_WAV_WARN_SAMPLE_COUNT = 1u << 0,
  LSX_WAV_WARN_DATA_LENGTH = 1u << 1
};

typedef struct {
  uint32_t riff_length;
  uint32_t data_length;
  uint32_t sample_count;
  unsigned sample_count_unspecified;
  unsigned data_length_unspecified;
} lsx_wav_header_sizes_t;

typedef struct {
  unsigned warned_sample_count;
  unsigned warned_data_length;
  unsigned first_header_data_unspecified;
} lsx_wav_warning_state_t;

static inline void lsx_wav_warning_state_init(lsx_wav_warning_state_t * state)
{
  state->warned_sample_count = 0;
  state->warned_data_length = 0;
  state->first_header_data_unspecified = 0;
}

/*
 * Calculate the three 32-bit WAV size fields without allowing either the
 * payload or payload-plus-container-overhead to wrap.  The subtraction form
 * deliberately avoids adding an arbitrary uint64_t payload to the overhead
 * before checking the 32-bit limit.
 */
static inline lsx_wav_header_sizes_t lsx_wav_header_sizes(
    uint64_t data_length, uint64_t sample_count, uint16_t fmt_size,
    uint32_t fact_size, unsigned writes_fact)
{
  lsx_wav_header_sizes_t result;
  uint64_t overhead = UINT64_C(4) + (UINT64_C(8) + fmt_size) + UINT64_C(8) +
      (data_length & UINT64_C(1));

  if (writes_fact)
    overhead += UINT64_C(8) + fact_size;

  result.sample_count_unspecified =
      writes_fact && sample_count > UINT32_MAX;
  result.sample_count = result.sample_count_unspecified ?
      LSX_WAV_MS_UNSPEC : (uint32_t)sample_count;

  result.data_length_unspecified =
      overhead > UINT32_MAX || data_length > UINT32_MAX - overhead;
  if (result.data_length_unspecified) {
    uint64_t sentinel_overhead = UINT64_C(4) +
        (UINT64_C(8) + fmt_size) + UINT64_C(8) +
        (LSX_WAV_MS_UNSPEC & UINT32_C(1));

    if (writes_fact)
      sentinel_overhead += UINT64_C(8) + fact_size;

    result.data_length = LSX_WAV_MS_UNSPEC;
    result.riff_length = (uint32_t)(LSX_WAV_MS_UNSPEC + sentinel_overhead);
  } else {
    result.data_length = (uint32_t)data_length;
    result.riff_length = (uint32_t)(data_length + overhead);
  }

  return result;
}

/*
 * Decide which advisories a header write must emit and update per-output
 * state.  Seekable first headers defer the data-length advisory because a
 * later seek-back header may replace the provisional sentinel.
 */
static inline unsigned lsx_wav_header_warning_actions(
    lsx_wav_warning_state_t * state,
    lsx_wav_header_sizes_t const * sizes,
    unsigned second_header, unsigned seekable)
{
  unsigned actions = LSX_WAV_WARN_NONE;

  /* A seek-back header replaces any provisional first-header decision. */
  if (second_header)
    state->first_header_data_unspecified = 0;

  if (sizes->sample_count_unspecified && (second_header || !seekable) &&
      !state->warned_sample_count) {
    state->warned_sample_count = 1;
    actions |= LSX_WAV_WARN_SAMPLE_COUNT;
  }

  if (sizes->data_length_unspecified) {
    if (second_header || !seekable) {
      if (!state->warned_data_length) {
        state->warned_data_length = 1;
        actions |= LSX_WAV_WARN_DATA_LENGTH;
      }
      state->first_header_data_unspecified = 0;
    } else {
      state->first_header_data_unspecified = 1;
    }
  }

  return actions;
}

/* Emit the deferred advisory when stopwrite retains the first header. */
static inline unsigned lsx_wav_retained_header_warning_actions(
    lsx_wav_warning_state_t * state)
{
  if (state->first_header_data_unspecified &&
      !state->warned_data_length) {
    state->first_header_data_unspecified = 0;
    state->warned_data_length = 1;
    return LSX_WAV_WARN_DATA_LENGTH;
  }

  state->first_header_data_unspecified = 0;
  return LSX_WAV_WARN_NONE;
}

#endif
