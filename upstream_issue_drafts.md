# Upstream Issue Drafts — toolchain defects found by the DSD Reference qualification gate

Status: DRAFTS, not yet filed. Five real toolchain defects were isolated
with minimal reproductions during the DSD Reference P0 qualification
rounds (2026-07-19..21). File when convenient; none block tonepoet (all
are routed around or rejected fail-closed by policy), but upstream fixes
would simplify future policy re-attestations.

> **Verification update (2026-07-22).** Defects #1 and #2 were re-examined
> against the sox_ng source tree and a locally built binary. Corrections
> applied below:
> - **#1 is confirmed real** (reproduced against sox_ng 14.8.0.1) but is a
>   **long-standing upstream bug, not a sox_ng regression**: classic SoX
>   14.4.2 wraps identically on the same input. The correct sentinel is
>   SoX's own `MS_UNSPEC` (`0x7ffff000`), **not** `0xFFFFFFFF` as the draft
>   originally suggested. Root cause identified and a **fix is committed**
>   on branch `fix/wav-oversize-streaming-size` (see below). A related
>   sub-4 GiB `wRiffLength` overflow was also found (see #1a).
> - **#2 is misattributed.** sox_ng has **no native W64 writer** — W64 I/O
>   is delegated to **libsndfile** (`src/sndfile.c`, `SF_FORMAT_W64`). If
>   real, the defect belongs upstream in libsndfile and must be reproduced
>   there; it is not a sox_ng-source bug. Not independently reproduced yet.
> - #3–#5 (ffmpeg) were not re-examined here.

---

## 1. sox_ng (file at codeberg.org/sox_ng/sox_ng; also applies to barstoolbluz/sox_ng fork)

**Title:** WAV writer emits 32-bit-wrapped RIFF/data sizes instead of
streaming sentinels for >4 GiB output to unseekable streams

**Version:** sox_ng 14.8.0.1 (also present in 14.6.1); **also classic SoX
14.4.2 — this is long-standing upstream code, not a sox_ng regression.**

**Summary:** When SoX-ng writes WAV to an unseekable output
(stdout/pipe) and the payload exceeds 4 GiB, the initial header's RIFF
and `data` chunk sizes are the true 64-bit sizes truncated modulo 2^32,
rather than SoX's streaming sentinel `MS_UNSPEC` (`0x7ffff000`) used
when the final size cannot be seek-patched. (SoX's own reader honors
`MS_UNSPEC`, `0xFFFFFFFF`, and `0x7FFFFFFF` as read-to-EOF; `MS_UNSPEC`
is the self-consistent choice, not `0xFFFFFFFF`.) A downstream reader that
honors the declared sizes truncates the stream — for a payload of
4 GiB + 8 bytes, the header declares `data` = 8 bytes.

**Reproduction (Linux, sparse file — no real 4 GiB of I/O):**

```python
# make_big_w64.py — writes a valid W64, f64 mono 48 kHz, data = 4 GiB + 8 bytes (sparse)
import struct, uuid
def guid(s): return uuid.UUID(s).bytes_le
riff, wave = guid('66666972-912E-11CF-A5D6-28DB04C10000'), guid('65766177-ACF3-11D3-8CD1-00C04F8EDB8A')
fmt, data = guid('20746D66-ACF3-11D3-8CD1-00C04F8EDB8A'), guid('61746164-ACF3-11D3-8CD1-00C04F8EDB8A')
fmt_body = struct.pack('<HHIIHH', 3, 1, 48000, 48000*8, 8, 64)
data_size = 0x1_0000_0008
with open('big.w64', 'wb') as f:
    f.write(riff + struct.pack('<Q', 16+8+16+16+8+len(fmt_body)+16+8+data_size) + wave)
    f.write(fmt + struct.pack('<Q', 24+len(fmt_body)) + fmt_body)
    f.write(data + struct.pack('<Q', 24+data_size))
    f.seek(f.tell() + data_size - 1); f.write(b'\0')
```

```console
$ python3 make_big_w64.py
$ sox --info big.w64          # reader is CORRECT: 536,870,913 samples
$ sox -D big.w64 -t wav - | head -c 8 | od -A d -t x1
0000000 52 49 46 46 3a 00 00 00      # RIFF size = 0x3a — wrapped, not the MS_UNSPEC sentinel
```

**Cheaper reproduction (no W64/libsndfile; sparse raw input, ~0 disk).**
The trigger is a *length-supplying* input to a *non-seekable* output; a
sparse raw file supplies a known length without real I/O:

```console
$ truncate -s 4294967304 big.f64        # 4 GiB + 8 bytes, sparse
$ ./src/sox_ng -t f64 -r 48000 -c 1 big.f64 -t wav - | head -c 64 | od -A d -t x1
# RIFF size (bytes 4-7)  = 3a 00 00 00   (0x3a, wrapped)
# data size (bytes 54-57)= 08 00 00 00   ((4 GiB + 8) mod 2^32 = 8)
# After the fix: RIFF = 32 f0 ff 7f (MS_UNSPEC+overhead), data = 00 f0 ff 7f (MS_UNSPEC)
```

Note: `sox -n … synth N | head` does **not** reproduce this — `synth`
supplies no length to the first header, so the pipe shows `MS_UNSPEC`
whether or not the bug is present. A known-length input is required.

The `data` chunk size in the streamed header is likewise
`(4 GiB + 8) mod 2^32 = 8`. Expected behavior for unseekable output
where the size exceeds `u32`: write the streaming sentinels so
consumers read to EOF (matching common practice for piped WAV).

**Impact:** any pipeline of the form `sox big.w64 -t wav - | consumer`
silently truncates for >4 GiB audio if the consumer honors declared
sizes. The W64 *reader* is unaffected (verified above).

**Root cause (identified in `src/wav.c`, `wavwritehdr`):** three coupled
issues, not one. (1) The oversize→`MS_UNSPEC` clamp was gated on
`second_header`, so it ran only on the seek-back patch header; a
non-seekable output writes only the first header, which was never
clamped. (2) `wRiffLength` is computed and the RIFF field written
*before* that clamp, so even the seekable second header emitted a
wrapped RIFF size. (3) The trigger is byte-length > `u32`, which occurs
even when the sample count fits in `u32` (Float64/multichannel), so the
clamp must key off the byte length independently.

**Fix (committed):** move the clamp before `wRiffLength` is derived and
apply it to every header, keyed on both sample count and byte length;
emit `MS_UNSPEC` (not `0xFFFFFFFF`). Branch
`fix/wav-oversize-streaming-size`. Verified: streamed >4 GiB now emits
`MS_UNSPEC`; sub-4 GiB byte-exact; real 4 GiB+8 seekable writes (f64 and
u8) read back the exact sample count with full payload on disk;
`src/tests.sh` passes 172/0. There is **no RF64 write path** in sox_ng,
so `MS_UNSPEC` sentinels are the only correct large-file strategy.
After the fork fix lands: bump the tonepoet flake pin, re-attest the
toolchain closure, and lift any streamed-carrier capacity cap as a new
append-only policy ID.

---

## 1a. sox_ng — RIFF `wRiffLength` (u32) overflows for data length just below 4 GiB (sibling of #1)

**Version:** sox_ng 14.8.0.1 and classic SoX 14.4.2 (long-standing upstream).

**Summary:** Distinct from #1 and **not** covered by the #1 fix. The RIFF
`ChunkSize` (`wRiffLength`, a `uint32_t`) is `dwDataLength + header
overhead`. When the data length is ≤ `u32` (so #1's clamp does not fire)
but `dwDataLength + overhead` exceeds `u32`, the `data` field is written
correctly while the RIFF field wraps to a tiny value. The overflow
window is the last **36–72 bytes** below exactly 4 GiB — width equals the
header overhead, which is format-dependent: PCM ≤16-bit = `0x24` (36),
IEEE_FLOAT = `0x32` (50), WAVE_FORMAT_EXTENSIBLE = `0x48` (72) — all three
verified empirically.

**Reproduction:**
```console
$ truncate -s 4294967280 near.f64        # 0xFFFFFFF0 (in the f64 window)
$ ./src/sox_ng -t f64 -r 48000 -c 1 near.f64 -t wav - | head -c 64 | od -A d -t x1
# data size (54-57) = f0 ff ff ff  (0xFFFFFFF0, correct)
# RIFF size (4-7)   = 22 00 00 00  (0x22, wrapped — 0xFFFFFFF0 + 0x32 overflows u32)
```

**Latency:** sox_ng's own reader is unaffected — `findChunk` walks chunks
by each chunk's own length and never uses the RIFF `ChunkSize`; only
strict external validators that honor the RIFF `ChunkSize` break.

**Candidate fix:** compute `wRiffLength` in 64-bit and, if it exceeds
`u32`, fall back to `MS_UNSPEC` for the `data` chunk (keeps the header
internally consistent). This single `riff64 > u32` test subsumes #1's
data clamp. **Not yet fixed** — brief prepared (`BRIEF_wav_riff_overflow.md`).

---

## 2. sox_ng (file at codeberg.org/sox_ng/sox_ng; also applies to barstoolbluz/sox_ng fork)

**Title:** W64 writer finalizes header-only/empty size fields for
all-zero (digital-silence) content, while the full payload is present on
disk

> **CORRECTION (2026-07-22): misattributed.** sox_ng has **no native W64
> writer**. W64 read/write is delegated entirely to **libsndfile**
> (`src/sndfile.c:208`, `SF_FORMAT_W64`; the `w64` format handler at
> `src/sndfile.c:756`), or to ffmpeg. The `sox … tone.w64` command in the
> reproduction below exercises **libsndfile's** W64 writer, not sox_ng
> code. If the defect is real it must be reproduced against and filed with
> **libsndfile** — no change in the sox_ng tree can address it. This
> defect was **not** independently re-verified during the 2026-07-22 pass;
> the reproduction below is the original draft's and is retained for
> hand-off to a libsndfile bug report.

**Version:** sox_ng 14.8.0.1

**Summary:** When SoX-ng writes an **all-zero** payload to W64, both the
RIFF-GUID size field (finalized to the header length) and the `data`
chunk size field (finalized to empty) declare a header-only/empty file,
even though the complete zero-sample payload IS written to disk. A reader that honors the declared sizes (FFmpeg)
correctly refuses the file; SoX round-trips its own broken output because
its reader reads to EOF and ignores the size fields. This is a sibling of
defect #1 (the streamed-WAV >4 GiB size wrap): SoX-ng's WAV/W64 size
accounting has more than one finalization bug.

**Reproduction (Linux; two mono 88.2 kHz Float64 W64 files, 8,820
frames each, both 70,696 bytes on disk):**

```console
$ sox -D -r 88200 -n -e floating-point -b 64 -c 1 tone.w64  synth 0.1 sine 1000 gain -6
$ sox -D -r 88200 -n -e floating-point -b 64 -c 1 zeros.w64 synth 0.1 sine 1000 vol 0

$ sox --info tone.w64 && sox --info zeros.w64   # SoX reads BOTH: 8,820 samples

# RIFF-GUID size field (bytes 16..24, little-endian u64):
#   tone.w64  -> 0x00011428 = 70,696   (correct: whole file)
#   zeros.w64 -> 0x00000088 = 136       (HEADER-ONLY — bogus)
# data-chunk size field:
#   zeros.w64 -> 0x18 = 24              (declares EMPTY payload; correct
#                value = 0x113b8 = 70,584, i.e. 70,560 payload bytes +
#                24-byte W64 chunk header — the payload IS on disk)

$ ffprobe tone.w64    # opens fine
$ ffprobe zeros.w64   # "Invalid data" — correctly honoring the bogus size
$ ffprobe -f w64 zeros.w64   # forcing the demuxer does NOT bypass
```

**Impact:** any all-zero (or effects-quantized-to-zero) W64 SoX-ng
writes is unreadable by size-honoring consumers (FFmpeg, and any strict
W64 parser), while SoX itself masks the corruption by ignoring its own
size fields. Digital-silence carriers and silent lead-in/-out fixtures
are the natural triggers.

**Candidate fix:** one-line-class — in the W64 size finalization path,
compute the final RIFF and `data` sizes from the actual bytes written,
not from a running counter/flag that stays at its initial (empty) state
for all-zero content. Verify the byte-count update fires on all-zero
blocks identically to nonzero ones. Suggested to characterize the exact
trigger (all-zero-whole-file vs first-block-silence vs a threshold) with
leading-/trailing-silence controls while in the code. After the fix:
bump the tonepoet flake pin, re-attest, and lift the all-zero W64
refusal accommodation as a new append-only policy ID.

---

## 3. ffmpeg (trac.ffmpeg.org) — W64 demuxer mis-scales plain-IEEE_FLOAT f64 by 2^31

**Version:** ffmpeg 7.1

**Summary:** Decoding a W64 file whose fmt chunk uses the plain
`WAVE_FORMAT_IEEE_FLOAT (0x0003)` tag with 64-bit samples (as written
by SoX) yields samples scaled by exactly 2^31 (+186.64 dB). ffprobe
identifies the stream correctly as `pcm_f64le`; the corruption is in
decode scaling. ffmpeg's own W64 muxer writes `WAVE_FORMAT_EXTENSIBLE`
and reads its own files correctly. Repro: `sox -r 88200 -n -e
floating-point -b 64 t.w64 synth 1 sine 1000 gain -20` then
`ffmpeg -i t.w64 -af astats -f null -` → peak +166.64 dB (expect −20).
f32-in-W64 with the same plain tag decodes correctly; f64 WAV (RIFF)
decodes correctly.

## 4. ffmpeg (trac.ffmpeg.org) — W64 muxer folds alignment padding into the data chunk

**Version:** ffmpeg 7.1

**Summary:** `ffmpeg -i in.w64 -c:a copy -f w64 out.w64` on a stream
whose data byte length is not a multiple of W64's 8-byte alignment
(e.g. mono 24-bit, 8,820 samples = 26,460 bytes) produces a file that
decodes to one extra phantom sample (8,821): the muxer includes the
alignment padding in the declared data extent. Identical prefix,
zero-valued trailing sample. Aligned sizes round-trip cleanly.

## 5. ffmpeg (trac.ffmpeg.org) — f32 W64 mis-measured via streamed f64 WAV re-container

**Version:** ffmpeg 7.1 (interaction with SoX-ng 14.8.0.1 producer)

**Summary:** Streaming a SoX-written Float32 W64 through
`sox in.w64 -t wav -e floating-point -b 64 - | ffmpeg -f wav -i pipe:0`
measures near full scale (`input_tp ≈ -0.00` for a −20 dBFS fixture),
while direct ffmpeg decode of the same f32 W64 file is correct — the
inverse of defect #2 (f64: direct broken, streamed correct). Needs
joint isolation to attribute producer vs consumer before filing; the
empirical route matrix is recorded in tonepoet's
`docs/handoff_dsd_reference_p0_current.md` (v6 analyzer correction).

---

Provenance: all five were surfaced by tonepoet's DSD Reference
qualification gate (policy lineage `sox_ng_14_8_0_1_v4..v16`) and
isolated with the minimal fixtures described in
`docs/findings_dsd_reference_p0_admission_round.md` (F1/F5/F6 §373,
F10 §812) and the policy handoff docs. The sox_ng-source writer defects
(#1 and its sibling #1a) are the fork-fix track; see
`docs/handoff_sox_ng_fork_fixes.md`. Note (2026-07-22): #1 is fixed on
branch `fix/wav-oversize-streaming-size`, #1a has a hand-off brief
(`BRIEF_wav_riff_overflow.md`), and #2 has been reassigned to libsndfile
(sox_ng has no native W64 writer).
