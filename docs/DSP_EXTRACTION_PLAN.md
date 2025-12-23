# DSP Extraction Plan for phoenix-dsp

## ✅ COMPLETED - December 23, 2025

All DSP algorithms successfully extracted from simple_am_receiver to phoenix-dsp library.

### Completion Summary

- **phoenix-dsp created:** Directory structure, headers, source files
- **Library built:** libpn_dsp.a (3042 bytes)
- **simple_am_receiver updated:** Now links against phoenix-dsp
- **Documentation updated:** README.md, copilot-instructions.md

---

## Overview

Extract reusable DSP algorithms from simple_am_receiver.c to phoenix-dsp library. These algorithms are currently embedded but should be shared across all Phoenix Nest signal processing tools.

## Algorithms to Extract

### 1. Butterworth Lowpass Filter (lowpass_t)

**Current Location:** `src/simple_am_receiver.c` lines 113-143

**Implementation:**
- 2nd order Butterworth IIR filter
- Configurable cutoff frequency and sample rate
- Direct Form II implementation
- State: `x1, x2, y1, y2` (input/output history)
- Coefficients: `b0, b1, b2, a1, a2`

**API:**
```c
typedef struct {
    float x1, x2;   /* Input history */
    float y1, y2;   /* Output history */
    float b0, b1, b2, a1, a2;  /* Coefficients */
} lowpass_t;

void lowpass_init(lowpass_t *lp, float cutoff_hz, float sample_rate);
float lowpass_process(lowpass_t *lp, float x);
```

**Usage in simple_am_receiver:**
```c
lowpass_init(&i_lpf, IQ_FILTER_CUTOFF, SDR_SAMPLE_RATE);  // 3 kHz @ 2 MHz
lowpass_init(&q_lpf, IQ_FILTER_CUTOFF, SDR_SAMPLE_RATE);
float i_filt = lowpass_process(&i_lpf, (float)i_sample);
float q_filt = lowpass_process(&q_lpf, (float)q_sample);
```

**Dependencies:** `<math.h>` (sinf, cosf)

---

### 2. DC Block Filter (dc_block_t)

**Current Location:** `src/simple_am_receiver.c` lines 145-167

**Implementation:**
- Highpass IIR filter using pole-zero cancellation
- Transfer function: `y[n] = x[n] - x[n-1] + α*y[n-1]`
- Current α = 0.99 (optimized for voice, was 0.995 for pulse detection)
- Removes DC offset from envelope-detected signals

**API:**
```c
typedef struct {
    float x_prev;
    float y_prev;
} dc_block_t;

void dc_block_init(dc_block_t *dc);
float dc_block_process(dc_block_t *dc, float x);
```

**Usage in simple_am_receiver:**
```c
dc_block_init(&dc);
float ac_signal = dc_block_process(&dc, envelope);
```

**Dependencies:** None

**Configuration Note:** α coefficient should be configurable for different applications:
- 0.99 for voice (current)
- 0.995 for pulse/timing detection (higher preservation of low frequencies)

---

### 3. Audio AGC (audio_agc_t)

**Current Location:** `src/simple_am_receiver.c` lines 169-220

**Implementation:**
- Automatic Gain Control with asymmetric attack/decay
- Level tracking with exponential moving average
- Gain limiting (0.1x to 100x)
- Warmup period support

**Algorithm:**
```
If |sample| > level:
    level += attack * (|sample| - level)     # Fast attack (0.01)
else:
    level += decay * (|sample| - level)      # Slow decay (0.0001)

gain = target / level
gain = clamp(gain, 0.1, 100.0)
output = sample * gain
```

**API:**
```c
typedef struct {
    float level;        /* Running average of signal level */
    float target;       /* Target output level */
    float attack;       /* Attack time constant (fast) */
    float decay;        /* Decay time constant (slow) */
    int warmup;         /* Warmup counter */
} audio_agc_t;

void audio_agc_init(audio_agc_t *agc, float target);
float audio_agc_process(audio_agc_t *agc, float x);
```

**Usage in simple_am_receiver:**
```c
audio_agc_init(&agc, 0.3f);  // Target level 30%
float normalized = audio_agc_process(&agc, ac_signal);
```

**Dependencies:** `<math.h>` (fabsf)

**Tuning Parameters:**
- `attack = 0.01` (fast response to loud signals, prevents clipping)
- `decay = 0.0001` (slow recovery from quiet periods, prevents pumping)
- `gain_max = 100.0` (30 dB max boost)
- `gain_min = 0.1` (20 dB max cut)

---

## Migration Strategy

### Phase 1: Create phoenix-dsp Library

1. **Create repository structure:**
   ```
   phoenix-dsp/
   ├── include/
   │   ├── pn_dsp.h           (master header)
   │   ├── pn_filter.h        (lowpass, highpass, dc_block)
   │   └── pn_agc.h           (audio AGC)
   ├── src/
   │   ├── pn_filter.c
   │   └── pn_agc.c
   ├── lib/
   │   └── libpn_dsp.a
   └── README.md
   ```

2. **Namespace convention:**
   - Prefix all types: `pn_lowpass_t`, `pn_dc_block_t`, `pn_audio_agc_t`
   - Prefix all functions: `pn_lowpass_init()`, `pn_dc_block_process()`, etc.

3. **Header guards:**
   ```c
   #ifndef PN_DSP_H
   #define PN_DSP_H
   // ...
   #endif
   ```

### Phase 2: Port Algorithms

1. **Extract each algorithm** into separate source files
2. **Preserve exact implementations** - no optimization yet
3. **Add unit tests** for each algorithm
4. **Document parameter tuning** in comments

### Phase 3: Update simple_am_receiver

1. **Remove inline DSP code**
2. **Link against libpn_dsp:**
   ```powershell
   gcc -O2 -I include -I ../phoenix-dsp/include -I ../phoenix-discovery/include \
       src/simple_am_receiver.c \
       -L ../phoenix-dsp/lib -L ../phoenix-discovery/lib \
       -lpn_dsp -lpn_discovery -lws2_32 -lwinmm -lm \
       -o simple_am_receiver.exe
   ```

3. **Update includes:**
   ```c
   #include "pn_filter.h"
   #include "pn_agc.h"
   ```

4. **Update type names:**
   ```c
   // Old
   lowpass_t i_lpf, q_lpf;
   dc_block_t dc;
   audio_agc_t agc;

   // New
   pn_lowpass_t i_lpf, q_lpf;
   pn_dc_block_t dc;
   pn_audio_agc_t agc;
   ```

### Phase 4: Documentation

1. **Create API documentation** in phoenix-dsp/README.md
2. **Add design notes** explaining filter choices
3. **Provide usage examples** for each algorithm
4. **Document performance characteristics** (cycles per sample, memory usage)

---

## Implementation Notes

### Floating-Point Precision

All algorithms use `float` (32-bit) for efficiency:
- Adequate precision for audio applications (96 dB SNR)
- SIMD-friendly (SSE/AVX can process 4-8 floats in parallel)
- Matches typical audio hardware (24-bit ADC/DAC)

### Platform Considerations

**Dependencies:**
- Standard C99 math functions: `sinf()`, `cosf()`, `fabsf()`
- Link with `-lm` on POSIX systems
- No platform-specific code (Windows/Linux portable)

**Compiler flags:**
```powershell
# Windows (MinGW)
gcc -O2 -ffast-math -msse2

# Linux
gcc -O2 -ffast-math -msse2 -lm
```

### Future Enhancements

1. **SIMD vectorization** for batch processing
2. **Higher-order Butterworth filters** (4th, 6th order)
3. **Other filter types** (Chebyshev, Elliptic, Bessel)
4. **Configurable AGC parameters** at runtime
5. **Fixed-point implementations** for embedded targets

---

## Testing Requirements

### Unit Tests

1. **Lowpass Filter:**
   - Verify cutoff frequency (-3 dB point)
   - Check stopband attenuation
   - Test impulse response
   - Validate phase response

2. **DC Block:**
   - Confirm DC rejection (input DC → output 0)
   - Test transient response
   - Verify time constant

3. **Audio AGC:**
   - Test gain calculation
   - Verify attack/decay time constants
   - Check gain limiting
   - Test target level tracking

### Integration Tests

1. **End-to-end signal path** with synthetic test signals
2. **Performance benchmarks** (samples/second throughput)
3. **Memory leak detection** (valgrind/DrMemory)

---

## Timeline Estimate

- **Phase 1 (Setup):** 1 hour (repository structure, makefiles)
- **Phase 2 (Port):** 2 hours (extract code, add tests)
- **Phase 3 (Integration):** 1 hour (update simple_am_receiver, verify)
- **Phase 4 (Documentation):** 1 hour (API docs, examples)

**Total:** ~5 hours

---

## Dependencies After Migration

**simple_am_receiver.c will require:**
- `libpn_dsp.a` (DSP algorithms)
- `libpn_discovery.a` (service discovery)
- `ws2_32.lib` (Windows sockets)
- `winmm.lib` (Windows audio)

**Build command:**
```powershell
gcc -O2 -I include -I ../phoenix-dsp/include -I ../phoenix-discovery/include \
    src/simple_am_receiver.c \
    -L ../phoenix-dsp/lib -L ../phoenix-discovery/lib \
    -lpn_dsp -lpn_discovery -lws2_32 -lwinmm -lm \
    -o simple_am_receiver.exe
```

---

## References

- **Butterworth filter design:** *Digital Signal Processing, Oppenheim & Schafer*
- **DC blocking filter:** Classic pole-zero highpass (standard in audio DSP)
- **AGC design:** *Audio Engineering Society papers on broadcast AGC*
