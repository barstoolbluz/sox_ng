# DSD Encoding Primer

## Introduction

Direct Stream Digital (DSD) is a digital audio encoding method utilizing 1-bit sigma-delta modulation. DSD64, the original standard used in SACD, samples at 2.8224 MHz. Higher sample rates like DSD128, DSD256, and DSD512 provide significant advantages by reducing ultrasonic noise and enabling gentler noise shaping.

## Noise Shaping and Modulators

### Key Concepts:

* **Sigma-Delta Modulation (SDM)**: Converts PCM audio data into 1-bit data with high sampling rates.
* **Noise Shaping**: Moves quantization noise to ultrasonic frequencies, reducing audible noise.

### Modulator Options:

Multiple methods available in SoX-DSD:

* **CLANS (Closed Loop Analysis of Noise Shapers)**: Optimizes noise shaping for lower distortion and stable operation.
* **SDM (Standard Delta-Sigma Modulation)**: Conventional approach leveraging Delta-Sigma Toolbox functions.
* **CRFB (Cascade of Resonators with distributed FeedBack)**: Alternative modulator design with different noise-shaping characteristics.

Higher-order modulators (e.g., 7th or 8th order) significantly reduce audible noise (\~6dB/octave increase), but risk instability and overload, particularly at higher amplitudes and lower DSD rates.

### Recommended Settings:

* **DSD64**: CLANS-8 (recommended for stability) or SDM-8 (lower noise but less stable)
* **DSD128**: CLANS-7
* **DSD256**: CLANS-6
* **DSD512**: CLANS-5

Higher DSD rates allow using lower-order modulators due to inherent noise reduction at elevated sampling frequencies.

### Stability Observations:

* High-order modulators, especially SDM-8, can become unstable with continuous, high-amplitude test signals.
* DSD128 is notably more challenging to encode compared to other DSD rates.
* Testing stability across different modulator settings and signals is strongly advised.

## Encoding Commands

### Basic Encoding Command (DSD64 example):

```bash
sox RightMark32-96.wav RightMark32-96-DSD64.dsf rate -v 2822400 sdm -f sdm-8
```

* `rate -v` ensures very high-quality sample rate conversion.
* `sdm -f sdm-8` specifies the 8th-order standard noise shaper.
* Alternative filters: `clans-8`, `crfb-8`, `clans-4`, `sdm-4`, etc.

### Advanced Encoding Command with Trellis Optimization:

Trellis optimization in SoX-DSD improves encoding accuracy and minimizes distortion by exploring alternative bitstream paths. It is computationally heavy and significantly increases encoding time, often taking hours for short audio segments, especially at higher rates such as DSD512:

```bash
sox "+3.1dBDSD 1kHz Sine (32-192kHz).wav" "+3.1dBDSD 1kHz Sine (DSD64, clans-8).dsf" rate -v 2822400 sdm -f clans-8 -t 32 -n 32
```

#### Trellis Parameter Explanation:

| Parameter | Meaning                                                                                                                                                                                                           |
| --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-t 32`   | **Trellis lookahead depth**: Determines how many samples ahead the encoder considers for optimal quantization. Higher values (up to 64) significantly improve optimization but at substantial computational cost. |
| `-n 32`   | **Number of trellis nodes**: Controls the complexity by evaluating more quantization paths. Higher values (16–32+) greatly enhance quality but drastically increase computational requirements.                   |
| `-l 64`   | **Trellis latency**: Overrides internal latency (delay in samples). Usually set automatically, but can be manually increased for slightly better optimization results at the cost of additional processing delay. |

**Practical Interpretation:**

* Settings `-t 32 -n 32` represent extremely high-quality trellis optimization, ideal for mastering or critical archival purposes, with substantial CPU and memory use.
* Moderate settings like `-t 8 -n 8` may offer acceptable quality with significantly less computational overhead.

**Typical Usage Examples:**

```bash
# Moderate-quality (default-like) trellis settings:
sox input.wav output.dsf rate -v 2822400 sdm -f clans-8 -t 8 -n 8

# Higher-quality mastering-level settings:
sox input.wav output.dsf rate -v 2822400 sdm -f clans-8 -t 32 -n 32

# Very high quality with explicit latency control:
sox input.wav output.dsf rate -v 2822400 sdm -f clans-8 -t 32 -n 32 -l 64
```
*Note*: Trellis is always optional.

## SACD and DSD Signal Level Standards

* **0dBDSD**: Defined as 50% theoretical maximum amplitude, equivalent to -6dBFS PCM.
* **Peak Modulation**: Official SACD limit is 71.43% amplitude (+3.1dBDSD or -2.9dBFS PCM).
* Encoding beyond this modulation level risks modulator overload and distortion.

### Practical PCM Conversion Recommendation:

* Base encoding level should target around 0dBDSD (-6dBFS PCM).
* Occasional peaks up to +3.1dBDSD (-2.9dBFS PCM) are acceptable briefly.
* Apply approximately +2.5dB gain when converting DSD to PCM to avoid clipping.

## Noise and Dynamic Range Analysis

* SACD (DSD64) typically achieves a dynamic range around 120dB (5th-order noise shaping).
* Higher-order (8th-order) shaping achieves around 140dB dynamic range, comparable to 24-bit PCM.
* High-speed DSD (128, 256, 512) reduces the need for aggressive noise shaping.

## Comparative Performance Insights

Comparative measurements indicate:

* SDM-8 (SoX-DSD) closely matches PCM playback performance in THD and noise floor.
* CLANS provides greater stability at slightly elevated audible noise levels.
* JRiver’s DSD conversion closely resembles SoX-DSD’s SDM-8 in performance, though slightly higher noise just below 20kHz.

## Practical Guidelines for Audiophiles

* **DSD128 and above**: Recommended for audiophiles to shift noise further into ultrasonic frequencies.
* At DSD64, a low-pass filter around 50kHz is advisable to manage aggressive ultrasonic noise shaping.

## Modulator Selection and Subjective Audio Quality

* Higher-order modulators require significant negative feedback, potentially impacting subjective audio perception.
* Lower-order modulators (CLANS-4 or SDM-4) can offer a more "analog-like" subjective experience, though with increased audible noise.

## SACD Production Reference Guidelines (Scarlet Book)

* Official SACD production guidelines specify a 50kHz Butterworth 30 dB/oct low-pass filter for audio measurement.
* Recommended maximum modulation level is +3.1dBDSD, clearly cautioning against exceeding this threshold due to distortion risks.

## Final Observations and Recommendations

DSD encoding involves trade-offs between stability, computational resources, noise-shaping intensity, and subjective audio quality. Meticulous experimentation and selection of modulator settings, particularly at lower DSD rates, are crucial for achieving optimal fidelity. Understanding Trellis optimization, SACD production guidelines, and the nuances of subjective listening preferences further refines practical DSD encoding practices.
