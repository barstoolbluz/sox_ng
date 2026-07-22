# WAV RIFF/data 32-bit overflow validation

Date: 2026-07-22

## Result

- Sibling RIFF `ChunkSize` overflow: **FIXED**.
- Prior commit `058d1c90` (>4 GiB streamed-size fix): **PASS for its stated >4 GiB defect**, with the already-documented near-4-GiB sibling gap reproduced adversarially and closed by this change.
- Warning gap: **FIXED**. A successful seekable write that retains the first sentinel header now emits one advisory. PCM no longer emits a sample-count warning because PCM writes no `fact` sample-count field.

## Design decision

`wavwritehdr()` now derives the complete RIFF size in `uint64_t`. If the true data length *or* data-plus-container-overhead does not fit the 32-bit RIFF/data fields, the writer serializes `MS_UNSPEC` (`0x7ffff000`) in the `data` field and recomputes a consistent RIFF size from that sentinel.

This deliberately chooses an internally consistent read-to-EOF header over preserving an exact data size in the final 36-72-byte window below 4 GiB. Keeping the exact `data` size while wrapping or otherwise understating RIFF would leave a contradictory container that strict RIFF readers may reject or truncate.

The independent `fact` sample-count clamp remains separate and is applied only to layouts that actually write a `fact` chunk.

## Archive and provenance

### Input handoff archive

The archive used as the source input for this work matched the digest specified in the brief:

```
4cec1e85a7f6255e2fa7b6ac18686dc7fd21d202a3298132d3adde97eb860f7e  sox_ng_wav_fix_handoff.tar.gz
```

This hash identifies the **input handoff archive only**. It does not identify a corrected delivery bundle. A previous revision used the ambiguous phrase "supplied archive" here even though the first corrected delivery archive was a different artifact with SHA-256 `6cc64f6b1ba725a5ff8b0299f4eabd26fe5a518e36884a974c2c71b9926f0f78`. That provenance wording was incorrect and is corrected in this revision.

### Corrected delivery archive

The corrected delivery archive is identified by its detached `.sha256` sidecar, generated only after the repository has been committed, cleaned, and packaged. Its checksum is intentionally not embedded in this tracked report: changing this report would change the archive and invalidate any hash written inside it. The detached sidecar is therefore the authoritative binding between the delivered filename and bytes.

The unpatched baseline was built from `058d1c90^` (`a615a4f16e739d33bfa541abb11234300148eac4`), not `HEAD~1`. Classic `/usr/bin/sox` reports SoX 14.4.2 and reproduced both wraps, confirming long-standing upstream lineage rather than a sox_ng regression.

## Prior-fix validation (`058d1c90` before this patch)

Sparse raw inputs supplied a known length to the first header; each streamed probe used a unique path and no background process.

| Case | Patched `058d1c90` RIFF / data | Parent `058d1c90^` RIFF / data | Result |
|---|---|---|---|
| f64, 4 GiB + 8 | `0x7ffff032` / `0x7ffff000` | `0x0000003a` / `0x00000008` | PASS |
| u8, 4 GiB + 8 | `0x7ffff024` / `0x7ffff000` | `0x0000002c` / `0x00000008` | PASS |
| f64, 8000 bytes | `0x00001f72` / `0x00001f40` | identical | PASS, not over-clamped |

Real seekable writes at 4 GiB + 8 also passed before the sibling patch:

| Case | Final header | Physical size | Decode-and-count |
|---|---|---:|---:|
| f64 mono | RIFF `0x7ffff032`, data `0x7ffff000` | 4,294,967,362 | 4,294,967,304 bytes |
| u8 mono | RIFF `0x7ffff024`, data `0x7ffff000` | 4,294,967,348 | 4,294,967,304 bytes |

The prior commit's warning behavior was also reproduced: the f64 seekable fast path emitted no warning, while u8 emitted both a sample-count and data warning. Those diagnostics are corrected by the new patch.

## Adversarial findings

### Exact 32-bit boundary

Before the sibling patch:

- u8 `dwDataLength == 0xffffffff`: RIFF wrapped to `0x00000024`, data remained `0xffffffff`.
- u8 `dwDataLength == 0x100000000`: commit `058d1c90` correctly selected `MS_UNSPEC`.

Exact reproduction against the build of `058d1c90` before this patch:

```sh
truncate -s 4294967295 in.raw
./src/sox_ng -t u8 -r 48000 -c 1 in.raw -t wav - 2>/dev/null \
  | head -c 64 | od -A d -t x1
```

Observed little-endian fields were RIFF bytes `24 00 00 00` at offsets 4-7 and data bytes `ff ff ff ff` at offsets 40-43. That is not a failure of `058d1c90`'s narrower `dwDataLength > UINT32_MAX` repair, but it proves the commit was not a complete defense against all 32-bit WAV size wraps until the sibling condition was added.

After the sibling patch, both cases select RIFF `0x7ffff024` and data `0x7ffff000`; neither wraps.

### Format-dependent last-valid boundary

| Layout | Last exact payload | Exact RIFF | Next aligned payload | Fixed RIFF / data |
|---|---:|---:|---:|---|
| PCM u8, overhead `0x24` | `0xffffffda` | `0xfffffffe` | `0xffffffdb` | `0x7ffff024` / `0x7ffff000` |
| IEEE f64, overhead `0x32` | `0xffffffc8` | `0xfffffffa` | `0xffffffd0` | `0x7ffff032` / `0x7ffff000` |
| Extensible 4ch s24, overhead `0x48` | `0xffffffb4` | `0xfffffffc` | `0xffffffc0` | `0x7ffff048` / `0x7ffff000` |

Headers for representative sub-threshold f64 small, f64 `0xffff0000`, and extensible `0xffffffb4` cases were byte-for-byte identical before and after the sibling patch.

### Seekable sibling end-to-end

A real f64 mono write with `0xfffffff0` data bytes produced:

- RIFF `0x7ffff032`
- `fact` sample count `0x1ffffffe` (still exact)
- data `0x7ffff000`
- physical size 4,294,967,338 bytes
- exactly one warning
- decode-and-count 4,294,967,280 bytes

`--info` printed no `Duration` line and the decoder derived the unspecified data length from the physical file. This is the pre-existing reader behavior described in the brief, not a new regression.

### Seekable second-header rewrite with both fact and data overflow

A real f64 mono write of 34,359,738,368 data bytes (2^32 samples) forced the seek-back header rewrite and produced:

- RIFF `0x7ffff032`
- `fact` sample count `0x7ffff000`
- data `0x7ffff000`
- physical size 34,359,738,426 bytes
- one sample-count warning and one RIFF/data warning

This confirms that the independent `fact` clamp and the unified RIFF/data clamp coexist correctly on the second-header path.

## Regression tests

The permanent regression now has two complementary layers:

1. `test/wav-riff-size-overflow/run` exercises the real streamed writer at the PCM, IEEE float, and WAVE_FORMAT_EXTENSIBLE last-exact/first-overflow boundaries and at the prior >4 GiB cases.
2. `test/wav-size-state.c` compiles against the same internal size and warning-state planner used by `src/wav.c`. It permanently asserts:
   - the seekable fast path that retains the first sentinel header;
   - exactly one deferred data-length advisory on that path;
   - the seekable second-header rewrite when both the `fact` sample count and data/RIFF sizes overflow;
   - exactly one sample-count advisory and one data-length advisory on that path;
   - no duplicate advisory on repeated terminal calls;
   - clearing of a provisional warning when an exact seek-back header replaces the first header; and
   - overflow safety for a maximal `uint64_t` input.

The shared planner uses a subtraction-based fit check rather than adding arbitrary `uint64_t` payload lengths before comparison, so even pathological internal values cannot wrap the intermediate calculation.

After adding this coverage, the real seekable paths were rerun against the rebuilt binary:

- f64 payload `0xfffffff0`: RIFF `0x7ffff032`, exact `fact` count `0x1ffffffe`, data `0x7ffff000`, exactly one data-length warning, physical size 4,294,967,338 bytes, full decode count 4,294,967,280 bytes.
- f64 payload 34,359,738,368 bytes (2^32 samples): RIFF `0x7ffff032`, `fact` `0x7ffff000`, data `0x7ffff000`, exactly one sample-count warning and one data-length warning, physical size 34,359,738,426 bytes.

Results in the available native build environment:

- `make check`: exit 0; both `wav-length-over-4GB` and the expanded `wav-riff-size-overflow` report `OK`.
- `cd src && make sox_sample_test && builddir=. bash ./tests.sh`: exit 0; 148 `ok`, zero failure/error/not-ok lines. The build lacked optional Flox-provided codec libraries, so `wv w64 paf mat5 mat4 flac caf` were skipped and the brief's 172-case count was not available.
- Rebuild produced no compiler warnings attributable to the patch.
- `make distcheck` was attempted but could not start the distribution build because this container lacks the documentation tools `tbl`, `nroff`, and `pdfroff`. No source or test failure occurred before that toolchain error.

## Environment limitation

The supplied `.flox` definition was inspected, but this execution environment had no `flox`, no `nix`, no prebuilt `.flox/run`, and no container runtime. The official Flox package could not be installed because binary materialization and outbound DNS are blocked in the code-execution container. The code was therefore built with the available native GCC/autotools environment using the required `autoreconf -i`, `./configure --disable-shared`, and `make` sequence. This report does not claim a Flox-proven build.
