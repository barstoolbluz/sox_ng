# Brief: sox writes corrupt headers for digital-silence content in W64/MAT4

**Audience:** a reasoning model working in the `sox_ng` repository.
**Nature of this brief:** it states the **outcome we want** and how we will **judge success**. It does *not* prescribe an implementation. We include our own investigation at the end as *evidence to verify* — you own the diagnosis and the fix. If your analysis disagrees with ours, trust yours; the acceptance criteria are the contract.

---

## 1. What we want to happen

When sox writes **digital silence** — a fully all-zero stream, or a stream that *ends* in silence — to a seekable file in an affected format, the resulting file's header must declare the **true** amount of audio data, so that strict, standards-compliant readers accept it and decode the whole payload. Today it does not: the header under-declares the data (often to zero), the full payload is physically present but "orphaned" past the declared end, and strict readers (e.g. ffmpeg) reject the file.

Concretely, when this is fixed:
- A silent or silence-trailing file is **byte-for-byte a valid file** whose declared sizes match its real content.
- It is accepted and fully decoded by an independent reader that honours the declared sizes (e.g. ffmpeg for `w64`), not just by sox's own lenient reader. (Per-format oracles: §3.2/§5.)
- **Nothing else changes**: non-silent files, other formats, other encodings, read behaviour, and the existing disk-saving sparse-file optimization all remain exactly as they are.

That last clause is important: sox intentionally writes silence as **sparse files** (holes on disk) to save space. That behaviour is desirable and must survive. The bug is that the *header* is wrong, not that the file is sparse.

---

## 2. Reproduce it yourself first

Do this before reading our diagnosis, so you form your own view.

```bash
# Build (see §7). Then, all-zero Float64 W64 to a real file:
./src/sox_ng -D -r 88200 -n -e floating-point -b 64 -c 1 zeros.w64 synth 0.1 sine 1000 vol 0
./src/sox_ng -D -r 88200 -n -e floating-point -b 64 -c 1 tone.w64  synth 0.1 sine 1000 gain -6

ls -l tone.w64 zeros.w64        # same size on disk (~70,696 bytes) — full payload present in both
ffprobe zeros.w64               # rejects: "Invalid data" (declared data is ~empty)
ffprobe tone.w64                # opens fine

# Trailing silence is the more common real-world trigger — tone then silence:
./src/sox_ng -D -r 88200 -n -e floating-point -b 64 -c 1 tail.w64 \
   synth 0.05 sine 1000 gain -6 : synth 0.05 sine 1000 vol 0
# observe the declared data length covers only the tone half, dropping the trailing silence.
```

Inspect the W64 size fields directly (RIFF-GUID size is a little-endian u64 at byte offset 16; the `data` chunk size is a u64 following the `data` GUID). You'll see `zeros.w64` declares a header-only / empty size while the bytes are all on disk.

Silence produced any way reproduces it (synth `vol 0`, real audio `vol 0`, or raw all-zero input) — it is the **content**, not the effect chain.

---

## 3. Acceptance criteria (how we will judge the fix)

A fix is correct when **all** of these hold. Use the sparse-vs-nonsparse and independent-reader techniques in §5 to check them.

**Correctness**
1. All-zero and trailing-silence output in every **affected format × encoding** (see §4) has header size fields equal to the true content size.
2. Such files are accepted by an independent reader that **honours the declared header sizes** (not sox's own lenient read-to-EOF), and decode to the exact expected sample-byte count. Use the right oracle per format: `ffprobe`/ffmpeg for `w64` (it *rejects* the corrupt file today); `sndfile-info`'s reported frame count for `mat4` (it reports `0` today — ffmpeg has no mat4 support). Note `sndfile-info` is lenient for `w64` and will *not* reveal the w64 bug — see §5.
3. The fix holds however the silence arises (synth `vol 0`, real audio attenuated to zero, or an all-zero raw input), for mono **and** multichannel, and across durations from ~1 frame to several seconds.

**No collateral change**
4. **The sparse optimization is preserved** — a silent file is still sparse on disk (allocated blocks ≪ apparent size).
5. Non-silent files are **byte-for-byte unchanged** from current behaviour.
6. Formats/encodings that are *not* affected today (see §4) are **byte-for-byte unchanged**.
7. Read behaviour is unchanged — reading any file (including a silent/sparse one) yields identical results before and after.
8. The full conversion test suite still passes with the **same** count as an unmodified build (see §5).

**Discipline**
9. The change is as small and local as the correctness criteria allow, and does not disable or bypass the sparse optimization to achieve the fix.

---

## 4. Scope we observed (verify; don't assume complete)

Determined empirically by comparing a stock build against a build with the sparse optimization disabled (identical output ⇒ unaffected; differing output ⇒ affected). **Reproduce this yourself** — your fix must match "affected" and leave "unaffected" untouched.

| Dimension | Affected | Unaffected |
|---|---|---|
| Format | **`w64`, `mat4`** | `caf`, `paf`, `pvf`, `mat5`, `sds`, `xi`, `sd2`, `fap`; all native formats (`wav`, `aiff`, `au`, `sph`, …) |
| Encoding | signed PCM (`s16/s24/s32`), float (`f32/f64`) | `u8`, `a-law`, `µ-law` |
| Content | fully all-zero **or** trailing silence | any nonzero tail |
| Channels | mono and multichannel | — |
| Output | seekable file | non-seekable (pipe) |

The encoding split matters and is a good sanity check on any diagnosis: only encodings whose *silence is zero-bytes* are affected (signed/float); `u8` silence is `0x80`, a-law/µ-law silence is non-zero, so they never trigger it. Confirm this holds after your fix.

---

## 5. How to validate (techniques, not a script)

- **Affected-vs-unaffected by construction:** build twice — stock, and with the sparse optimization neutralised — and byte-compare all-zero output per format/encoding. The neutralised build is your *correct reference* for silent content; your fixed build must match it for affected cases and match *stock* for everything else. (`cmp` treats sparse holes as zeros, so this compares logical content.)
- **Independent reader:** `ffprobe`/ffmpeg for `w64` and `caf`; `sndfile-info` reports frames but is **lenient for `w64`** (it read-to-EOFs and masks the bug) — do not rely on it as the sole oracle for w64. It does catch `mat4`.
- **Sparse preservation:** compare `du` (allocated) against `stat -c%s` (apparent); a fixed silent file must still be sparse.
- **Regression suite:** `cd src && make sox_sample_test && builddir=. bash ./tests.sh` — record the pass count on an unmodified build first, then require the same after your change.
- **Boundaries:** tiny (≈1 frame) through large (seconds) silence; mono and multichannel; and at least one non-affected encoding (`u8`) to confirm it stays untouched.
- **Round-trip:** decode a fixed silent file and confirm the exact expected sample-byte count.

---

## 6. Our investigation — evidence to verify, not a spec

We reproduced, isolated, and prototyped a fix. Treat this as a head-start you should independently confirm (or refute). **You are free to diagnose differently and fix differently**, provided §3 holds.

**What we believe is happening (verify):**
- sox has a sparse-file optimization in its I/O layer that, for a seekable output, **seeks over all-zero buffers instead of writing them**, and defers materialising the file's final byte until the next seek.
- The affected formats (`w64`, `mat4`) are written via **libsndfile**, whose header-finalisation for these formats derives the data size from the **file length** at close (an `fstat`-based query through sox's I/O callbacks). Because the sparse tail hasn't been materialised at that moment, the queried length is short, so the declared size is wrong.
- Unaffected libsndfile formats (`caf` etc.) size their headers a different way (or happen to materialise the file before querying), and native sox writers size headers from their own sample counters — which is why they're immune.
- We confirmed this by building an instrumented libsndfile and watching the length query return the short value, and by neutralising the sparse optimization and observing the bug vanish.

**A candidate fix we prototyped (passed our tests; you may adopt, adapt, or replace):**
- Make sox's file-length query report the true logical end when a sparse extension is pending, rather than the not-yet-materialised physical size. In our prototype this was a few lines in the length function and it: fixed all affected format/encoding/channel/trailing-silence cases, preserved sparseness, left non-silent and unaffected cases byte-identical, kept reads unchanged, and passed the full suite.
- **Caveats we hit that you should weigh:**
  - The logical-length signal we used is only valid in the exact pending-sparse state; make sure whatever you use cannot *over*-declare, and cannot perturb the read path (reads must be unaffected — criterion §3.7).
  - Our prototype was gated to real on-disk files. It therefore does **not** address the memory-stream case in §8. If you want one change to cover both, a length signal that doesn't depend on `fstat` (e.g. a tracked high-water mark) is worth considering — but only if it cleanly satisfies §3.7 for reads and pipes.
- Alternative direction we considered and rejected as heavier: disabling the sparse optimization for libsndfile-backed outputs. It works but sacrifices the disk-saving benefit (violates the spirit of §3.4/§3.9). Mentioned only so you don't have to rediscover it.

Please **reproduce our isolation independently** before trusting the above — verify the affected set, verify the mechanism, and satisfy yourself the fix location is right.

---

## 7. Environment / tooling

- Build inside the `.flox` env: `autoreconf -i && ./configure --disable-shared && make -j"$(nproc)"` → `./src/sox_ng`.
- Do **not** write multi-GB files to `/tmp` if it is tmpfs; but note these repros are tiny (~70 KB), so it's a non-issue here.
- `ffprobe`/ffmpeg, `sndfile-info`, and `sndfile-convert` are available in the env for cross-checking.
- To build the sparse-neutralised reference build for §5, locate sox's zero-buffer sparse branch in its I/O layer and disable it; rebuild to a separate binary. (Finding it is part of forming your own diagnosis.)

---

## 8. Separate finding — out of scope, documented for context

While chasing the above we found a **different, pre-existing bug**: writing to an in-memory stream (the libsox library API `sox_open_memstream_write` / `sox_open_mem_write`) produces a **zero-sized header for the same libsndfile formats regardless of content** (nonzero and all-zero alike) — because a memory stream has no file descriptor, so the `fstat`-based length query returns 0. Native formats to memory are fine.

This is **not** the silence bug and is **library-API only** (no command-line path reaches it). We are **not** asking you to fix it here — keep this PR scoped to §1–§5. Just:
- be aware your fix must not *worsen* it (our prototype left it byte-identical), and
- if you happen to choose a length mechanism that naturally covers memory streams too, that's a bonus, not a requirement — do not expand scope or risk the §3 criteria to chase it.

---

## 9. Deliverables

1. A minimal patch to sox that satisfies every criterion in §3.
2. A short written account of: your independent diagnosis (with the reproduction/isolation you ran), the fix you chose and why, and the trade-off you made if any.
3. A validation report: PASS/FAIL against each §3 criterion, the affected-vs-unaffected byte-compare matrix, independent-reader results, sparse-preservation evidence, and the regression-suite count vs an unmodified build.
4. A commit message describing the observed behaviour, the fix, and the scope (which formats/encodings), and noting the memory-stream issue in §8 as a known, separate, unaddressed item.
