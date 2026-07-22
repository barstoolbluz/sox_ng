# Brief: fix the WAV `wRiffLength` 32-bit overflow, and validate the prior >4 GiB size-wrap fix

**Audience:** a reasoning model working directly in the `sox_ng` repository.
**Two deliverables:**
1. **FIX** the "sibling" bug — the RIFF `ChunkSize` field (`wRiffLength`) overflows `uint32_t` for a data length in the last ~36–72 bytes below 4 GiB, emitting a wrapped (tiny) RIFF size while the `data` chunk size stays correct.
2. **VALIDATE** (independently, adversarially) the already-committed fix for the primary bug — >4 GiB streamed WAV writing wrapped RIFF/`data` sizes instead of the `MS_UNSPEC` sentinel. Confirm it is correct and complete, or report where it is not.

Treat every empirical claim below as *reproducible, not authoritative* — re-run and confirm. One claim in the investigation that produced this brief was initially wrong due to a contaminated test (see §8); do not inherit that failure mode.

---

## 0. Repository / build / environment

- Repo: `/home/daedalus/dev/sox_ng`, branch **`fix/wav-oversize-streaming-size`**. The prior fix under validation is commit **`058d1c90`** (`src/wav.c`); a docs commit sits on top of it as the branch tip, so building the current tree (`HEAD`) yields the **patched** binary, and the **unpatched** baseline is **`058d1c90^`** (see §5).
- There is a **`.flox`** dev environment. Build **inside it** (autotools + gcc + all codec libs live there). The binary is **`src/sox_ng`** (not `src/sox`).
  ```bash
  # first time only:
  autoreconf -i && ./configure --disable-shared
  # each rebuild:
  make -j"$(nproc)"          # produces ./src/sox_ng
  ```
  If invoking through the flox MCP/`flox activate`, run commands with the environment active; a bare shell will miss libs and headers.
- Classic SoX 14.4.2 is available at `/usr/bin/sox` as an upstream reference. **All the bugs here exist in classic SoX too** — this is upstream lineage, not a sox_ng regression.
- **Do not** write multi-GB files to `/tmp` — it is `tmpfs` (RAM-backed, ~6.5 GB free). Use `/home/daedalus/dev` (NVMe, ~700 GB free) for any real large writes.

---

## 1. The code under discussion

All of this is in `src/wav.c`, function `wavwritehdr(sox_format_t *ft, int second_header)`.

Key facts:
- `MS_UNSPEC` is SoX's "unspecified length" sentinel: `#define MS_UNSPEC 0x7ffff000` (`wav.c:51`).
- `dwDataLength` and `dwSamplesWritten` are **`uint64_t`** (`wav.c:1700`, `wav.c:1697`).
- `wRiffLength` is **`uint32_t`** (`wav.c:1681`) — this is the field that overflows in the sibling bug.
- `wavwritehdr` is called **twice for seekable output** (first header in `startwrite_wav`, then a seek-back "second header" in `stopwrite_wav` at `wav.c:2058` via `wavwritehdr(ft,1)`), and **once for non-seekable output** (pipe: `stopwrite_wav` returns `SOX_EOF` before any rewrite, `wav.c:2042`).

Current (post-prior-fix) shape of the relevant region:

```c
    else if (wFormatTag != WAVE_FORMAT_PCM)
        wFmtSize += 2+wExtSize; /* plus ExtData */

    /* [prior fix] clamp oversized sizes to MS_UNSPEC BEFORE wRiffLength is
       derived, for every header (not just second_header). */
    if (dwSamplesWritten > 0xffffffffu) {
        if (second_header || !ft->seekable)
            lsx_warn("length is 4G or more samples: writing unspecified length");
        dwSamplesWritten = MS_UNSPEC;
    }
    if (dwDataLength > 0xffffffffu) {
        if (second_header || !ft->seekable)
            lsx_warn("length is 4GB or more of data: writing unspecified length");
        dwDataLength = MS_UNSPEC;
    }

    wRiffLength = 4 + (8+wFmtSize) + (8+dwDataLength+dwDataLength%2);   /* <-- uint32_t overflow point */
    if (isExtensible || wFormatTag != WAVE_FORMAT_PCM) /* PCM omits the "fact" chunk */
        wRiffLength += (8+dwFactSize);
```

The size fields are written later: RIFF size at `wav.c:1861` (`lsx_writedw(ft, wRiffLength)`), `fact` sample count at `wav.c:~1936`, `data` size at `wav.c:1941` (`lsx_writedw(ft, (uint32_t)dwDataLength)`).

---

## 2. The sibling bug (deliverable #1)

`wRiffLength` is `4 + (8+wFmtSize) + (8+dwDataLength+pad) [+ (8+dwFactSize)]`, i.e. **`dwDataLength + overhead`**, truncated into a `uint32_t`. When:

- `dwDataLength ≤ 0xFFFFFFFF` → the prior fix's data clamp does **not** fire; the `data` field is written correctly, **but**
- `dwDataLength + overhead > 0xFFFFFFFF` → `wRiffLength` wraps to a tiny value.

**Overhead is format-dependent** (this sets the window width):

| format | `wFmtSize` | `fact` chunk? | overhead (`wRiffLength − dwDataLength`) |
|---|---|---|---|
| PCM ≤16-bit, ≤2 ch | 16 | no | `0x24` (36) |
| IEEE_FLOAT (f64/f32), non-extensible | 18 | yes | `0x32` (50) |
| Extensible (PCM >16-bit or >2 ch; `wFmtSize += 2+22`) | 40 | yes | `0x48` (72) |

So the trigger is a `data` length within the last **36–72 bytes** below exactly 4 GiB (varies by encoding). Plus the `dwDataLength%2` pad term (±1). Verified points (both `src/sox_ng` and `/usr/bin/sox`, identical):

| input encoding | data length | `data` field | `RIFF` field |
|---|---|---|---|
| f64 mono | `0xFFFF0000` (below window) | `0xFFFF0000` ✓ | `0xFFFF0032` ✓ |
| f64 mono | `0xFFFFFFF0` (in window) | `0xFFFFFFF0` ✓ | **`0x22`** ✗ |
| f64 mono | `0xFFFFFFF8` (in window) | `0xFFFFFFF8` ✓ | **`0x2A`** ✗ |
| s16 mono (PCM) | `0xFFFFFFF0` (in window) | `0xFFFFFFF0` ✓ | **`0x14`** ✗ |

**Why it is latent in-tree:** `findChunk` (`wav.c:408`) walks chunks by each chunk's *own* length field and **never uses the RIFF `ChunkSize`** to bound the scan; `qwRiffLength` is read but only used in the RF64 branch (`wav.c:854-859`). So sox_ng's own reader is unaffected. The bug breaks **strict external readers** that validate the RIFF `ChunkSize` (i.e. treat the data chunk as running past the declared RIFF end).

---

## 3. Recommended fix shape (evaluate; you own the final design)

Compute `wRiffLength` in **64-bit**, and if it exceeds `0xFFFFFFFF`, fall back to the `MS_UNSPEC` sentinel for the `data` chunk (which makes the file read-to-EOF and keeps the header internally consistent). Observation that simplifies the code: **`riff64 > u32` subsumes the prior fix's `dwDataLength > u32` data clamp** — if `dwDataLength > u32` then `riff64 > u32` necessarily. So the two data-side conditions collapse into one. Keep the **separate** `dwSamplesWritten > u32` clamp for the `fact`/sample-count field (that field is independent and can be fine while data overflows, or vice-versa).

Sketch (adapt, don't paste blindly):
```c
    /* fact/sample-count field is independent */
    if (dwSamplesWritten > 0xffffffffu) { warn_once(); dwSamplesWritten = MS_UNSPEC; }

    uint64_t riff64 = 4 + (8+wFmtSize) + (8 + dwDataLength + dwDataLength%2);
    if (isExtensible || wFormatTag != WAVE_FORMAT_PCM) riff64 += (8+dwFactSize);
    if (riff64 > 0xffffffffu) {              /* subsumes dwDataLength > u32 */
        warn_once();
        dwDataLength = MS_UNSPEC;
        riff64 = 4 + (8+wFmtSize) + (8 + dwDataLength + dwDataLength%2);
        if (isExtensible || wFormatTag != WAVE_FORMAT_PCM) riff64 += (8+dwFactSize);
    }
    wRiffLength = (uint32_t)riff64;
```

**Trade-off to decide explicitly:** clamping `dwDataLength → MS_UNSPEC` when the data length itself fit in 32 bits means a file ~30–70 B under 4 GiB loses its *exact* `data` size (becomes read-to-EOF). The alternative — wrap only RIFF but keep `data` exact — produces an *internally inconsistent* header (data chunk extends past declared RIFF end), which is worse for the strict readers this is meant to satisfy. Recommendation: prefer the sentinel (consistent) over the exact-but-inconsistent option, but state your choice.

**Also decide:** whether to fold this into the existing prior-fix commit region (one unified `riff64 > u32` clamp) or add it as a distinct clamp. Folding is cleaner and is recommended.

### Secondary issue to consider (not strictly the sibling bug): warning gap
The current warn gate `if (second_header || !ft->seekable)` is **silent in exactly one path**: seekable + `samples ≤ u32` + `bytes > u32`. There, the first header clamps (but isn't `second_header`, and is seekable → no warn) and `stopwrite` early-returns (`wav.c:2039-2041`) so no second header is ever written → the user gets a correct-but-unspecified-length file with **no advisory at all**. Confirmed with the f64 end-to-end test in §5. A `priv_t` "already-warned" flag (or warning on the surviving header) would let it warn exactly once across all paths. Optional; call it out even if you don't fix it.

---

## 4. Constraints & invariants (do not violate)

- **No RF64 *write* path exists.** `isRF64` is read-only (`wav.c:90` decl; only set in the reader). So for >4 GiB there is no "promote to RF64" option — `MS_UNSPEC` sentinels are the only correct strategy. Do not invent an RF64 writer as part of this.
- The reader honors these `data`-size values as read-to-EOF sentinels: `MS_UNSPEC` (0x7ffff000), `0xFFFFFFFF`, `0x7FFFFFFF` (`wav.c:1207-1209`). Prefer **`MS_UNSPEC`** for self-consistency (that is what SoX itself writes and reads). Do **not** switch to `0xFFFFFFFF`.
- Files **≤ ~4 GiB − overhead must remain byte-exact** — the clamp must be a strict no-op below the overflow point. This is the #1 regression risk of the sibling fix.
- Keep the separate sample/`fact` clamp — do not merge it into the data/RIFF condition.

---

## 5. Validation protocol for the PRIOR fix (deliverable #2)

Reproduce independently. Build `src/sox_ng` at HEAD, then confirm each row. Known-good baseline (what a correct build must produce) is given.

**Repro primitives** (sparse input → ~0 disk; `head` closes the pipe before bulk I/O):
```bash
truncate -s <BYTES> in.raw           # sparse, real file length = known input length
./src/sox_ng -t <enc> -r 48000 -c 1 in.raw -t wav - 2>/dev/null | head -c 64 | od -A d -t x1
# size-field byte offsets:  RIFF size @ 4-7 (always)
#                           data size @ 54-57 for f64/f32 (has fact chunk)
#                           data size @ 40-43 for PCM u8/s16 (no fact chunk)
# enc ∈ {f64,f32,u8,s16}; MS_UNSPEC little-endian = 00 f0 ff 7f
```

**Streamed (pipe / non-seekable) — the core defect #1 matrix:**

| BYTES | enc | expect `data` | expect `RIFF` |
|---|---|---|---|
| `4294967304` (4 GiB+8) | f64 | `00 f0 ff 7f` (MS_UNSPEC) | `32 f0 ff 7f` (MS_UNSPEC+0x32) |
| `4294967304` | u8 | `00 f0 ff 7f` | `24 f0 ff 7f` (MS_UNSPEC+0x24) |
| `8000` (sub-4 GiB) | f64 | `40 1f 00 00` (=8000, **not** clamped) | correct small value |

Against the **unpatched** tree (or `/usr/bin/sox`) the 4 GiB rows instead give `data=08 00 00 00` and a wrapped `RIFF = 8 + overhead` (f64 → `3a 00 00 00`; u8 → `2c 00 00 00`) — that is the bug the fix removes. The code fix is commit **`058d1c90`**, so the unpatched baseline is its parent, **`058d1c90^`** — **not** `HEAD~1` (a docs commit sits on top of the fix, so `HEAD~1` *is* the fix). Build it without disturbing the working tree: `git worktree add ../sox_unpatched 058d1c90^ && (cd ../sox_unpatched && autoreconf -i && ./configure --disable-shared && make -j"$(nproc)")`.

**Real end-to-end (seekable, full 4 GiB written to `/home`, not `/tmp`)** — this is the strongest check; both paths validated as correct in the investigation:

```bash
W=/home/daedalus/dev/sox_ng/_val; mkdir -p $W
truncate -s 4294967304 $W/in_f64.raw   # samples 536,870,913 (≤u32) -> stopwrite early-return path
truncate -s 4294967304 $W/in_u8.raw    # samples 4,294,967,304 (>u32) -> second-header rewrite path
./src/sox_ng -t f64 -r 48000 -c 1 $W/in_f64.raw $W/out_f64.wav
./src/sox_ng -t u8  -r 48000 -c 1 $W/in_u8.raw  $W/out_u8.wav
# decode-and-count proves full, exact payload read back to EOF:
./src/sox_ng $W/out_f64.wav -t f64 - 2>/dev/null | wc -c   # expect 4294967304  (=/8 -> 536,870,913 samples)
./src/sox_ng $W/out_u8.wav  -t u8  - 2>/dev/null | wc -c   # expect 4294967304  (= 4,294,967,304 samples)
rm -rf $W   # ~8 GB; clean up
```
Expected final headers: f64 → RIFF `0x7ffff032`, data `MS_UNSPEC`; u8 → RIFF `0x7ffff024`, data `MS_UNSPEC`. File sizes: `header + 4294967304` exactly (58-byte header f64, 44-byte u8) ⇒ no truncation. Warnings: **f64 emits none** (the §3 gap); **u8 emits two** ("4G or more samples" + "4GB or more of data" — note the sample-count warning fires even though u8 is PCM and writes no `fact`/sample-count field, so it is harmless but imprecise; a refinement could gate that warning on the `fact` chunk actually being written).

**Regression suite:**
```bash
cd src && make sox_sample_test && builddir=. bash ./tests.sh
# baseline: 172 "ok", 0 failures; grep -c '^ok' and grep -ciE 'fail|error|not ok'
```

**Adversarial angles to actively probe (do not just re-run the happy path):**
- Does the fix break the **seekable second-header rewrite** when both sample count and data length exceed u32? (u8 test covers it — confirm the `fact` sample field is also `MS_UNSPEC`, not wrapped.)
- Off-by-one at the exact boundary `dwDataLength == 0xFFFFFFFF` and `== 0x100000000`.
- Non-mono / extensible layouts (e.g. `-c 4 -b 24` → extensible, overhead 0x48) — confirm both the clamp and the sub-threshold exactness.
- `--info` on a written `MS_UNSPEC` file prints **no Duration line** — this is *pre-existing reader behavior* (it doesn't force a scan for unspecified length), **not** a defect introduced by the fix. Do not "fix" it by mistake. Confirm the count via decode-and-count instead.

---

## 6. After the sibling fix — additional validation

- Re-run the §2 verified points: all four rows must now show a **correct** RIFF field (either the true `data+overhead` when it fits, or `MS_UNSPEC+overhead` when the sentinel kicks in), never a tiny wrapped value.
- Confirm the sub-threshold row (`0xFFFF0000`) is **unchanged / byte-exact** (no over-eager clamping).
- Re-run `tests.sh` → still 172 ok / 0 fail.
- Confirm the prior-fix matrix (§5) still passes (the collapsed condition must not regress the >4 GiB cases).

---

## 7. Deliverables

1. A patch to `src/wav.c` implementing the sibling fix (recommended: unified `riff64 > u32` clamp; state the exact/consistent trade-off you chose).
2. A written validation report on the prior fix: PASS/FAIL per row of §5, plus the adversarial findings. If you find the prior fix incomplete or wrong, say so with the reproducing command and observed bytes.
3. A note on the §3 warning gap: fixed, or explicitly deferred with rationale.
4. Commit message(s) that (a) describe the bug as **long-standing / present in classic SoX too** (not a sox_ng regression) and (b) record the verification performed.

---

## 8. Pitfalls that already bit the prior investigator (avoid these)

- **Test contamination:** an earlier run left a backgrounded `sox … &` writing to the *same* scratch file a foreground `| head > file` was writing — the race produced a bogus "classic SoX writes MS_UNSPEC" reading that falsely implied a regression. **Use unique temp paths, no stray `&`, and repeat each measurement.**
- **Invalid repro via `synth`:** `sox -n … synth N | head` does **not** exercise defect #1 — `synth` supplies no length to the first header, so the pipe always shows `MS_UNSPEC` regardless of the bug. You need a **length-supplying input** (sparse raw of a known size). The seekable file's "correct size" comes from the stopwrite seek-patch, not the first header.
- **Wrong byte offset:** the f64 `data`-size field is at bytes **54–57**, not within the first 44. PCM (no `fact`) is at **40–43**. Read the right offset for the encoding.
- **tmpfs:** writing 4 GiB to `/tmp` consumes RAM; use `/home/daedalus/dev`.
- The provenance is **upstream** — verify against `/usr/bin/sox` before calling anything a sox_ng regression.
