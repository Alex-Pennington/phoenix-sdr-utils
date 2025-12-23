# Phoenix SDR Utils - Copilot Instructions

## P0 - CRITICAL RULES

1. **NO unauthorized additions.** Do not add features, flags, modes, or files without explicit instruction. ASK FIRST.
2. **Minimal scope.** Fix only what's requested. One change at a time.
3. **Suggest before acting.** Explain your plan, wait for confirmation.
4. **Verify before modifying.** Run `git status`, read files before editing.

---

## Overview

**Phoenix SDR Utils** contains standalone utility programs for I/Q playback, network-based signal analysis, and GPS timing.

**Architecture:** Tools connect to sdr_server (phoenix-sdr-net) for I/Q data streaming. Service discovery via phoenix-discovery.

**Note:** WWV-specific analysis tools have been moved to [phoenix-wwv](https://github.com/Alex-Pennington/phoenix-wwv).

### Tool Categories

| Category | Tools |
|----------|-------|
| I/Q Recording | `iqr_play`, `iq_recorder`, `iqr_meta` |
| Signal Analysis | `simple_am_receiver` (network I/Q client) |
| GPS/Timing | `gps_time`, `gps_serial`, `wwv_gps_verify` |

---

## Network Architecture

### I/Q Streaming (phoenix-sdr-net)

```
Controller → sdr_server:4535 (control: SET_FREQ, SET_GAIN, START, STOP)
                    ↓
            sdr_server:4536 (I/Q data: PHXI header + IQDQ frames)
                    ↓
          simple_am_receiver (DSP + audio output)
```

**Protocol:** PHXI/IQDQ binary streaming
- **PHXI header** (32 bytes): sample rate, format, frequency, gain
- **IQDQ frames** (16 byte header + samples): sequence, sample count, flags + S16 I/Q pairs
- **META updates**: Parameter changes during streaming

**Discovery:** phoenix-discovery UDP broadcast (port 5400)
- `pn_discovery_init()` + `pn_listen()` to find sdr_server
- Auto-discovers IP and ports

### simple_am_receiver Architecture

**Input:** Network I/Q stream from sdr_server:4536
**Output:** Audio (speakers) or PCM (stdout)

**DSP Pipeline:**
1. Lowpass filter I and Q (3 kHz cutoff, Butterworth 2nd order)
2. Envelope detection (magnitude = sqrt(I² + Q²))
3. DC removal (highpass IIR)
4. Audio AGC (asymmetric attack/decay)
5. Decimation (2 MHz → 48 kHz)
6. Audio output (Windows waveOut)

**No hardware control** - frequency/gain managed by controller via port 4535

---

## Build

```powershell
# I/Q playback
gcc -O2 -I include src/iqr_play.c src/iqr_meta.c -o iqr_play.exe

# AM receiver (network client - requires phoenix-dsp and phoenix-discovery)
gcc -O2 -I include -I ../phoenix-dsp/include -I ../phoenix-discovery/include \
    src/simple_am_receiver.c \
    -L ../phoenix-dsp/lib -L ../phoenix-discovery/build \
    -lpn_dsp -lpn_discovery -liphlpapi -lws2_32 -lwinmm -lm \
    -o simple_am_receiver.exe

# GPS tools
gcc -O2 -I include src/gps_time.c src/gps_serial.c -o gps_time.exe

# GPS verification
gcc -O2 -I include src/wwv_gps_verify.c src/gps_serial.c -lws2_32 -o wwv_gps_verify.exe
```

---

## IQR Recording Format

Phoenix I/Q recording files use a 64-byte header followed by sample data.

### Header Structure
```c
typedef struct {
    uint32_t magic;           // 0x49515252 "IQRR"
    uint32_t version;         // Format version (1)
    uint32_t sample_rate;     // Samples per second
    uint32_t sample_format;   // 1=S16, 2=F32
    uint64_t center_freq;     // Center frequency Hz
    uint64_t timestamp_us;    // Unix timestamp (microseconds)
    uint32_t gps_valid;       // GPS lock status
    uint32_t gain_reduction;  // IF gain dB
    uint32_t lna_state;       // LNA setting
    uint8_t  reserved[20];    // Future use
} iqr_header_t;
```

### Sample Data
Interleaved I/Q pairs immediately after header:
- S16: `[I0:int16][Q0:int16][I1:int16][Q1:int16]...`
- F32: `[I0:float][Q0:float][I1:float][Q1:float]...`

---

## GPS Serial Interface

### NMEA Parsing
```c
// Standard NMEA sentences
$GPGGA,hhmmss.ss,lat,N,lon,W,quality,sats,hdop,alt,M,...*checksum
$GPRMC,hhmmss.ss,status,lat,N,lon,W,speed,course,ddmmyy,...*checksum
```

### GPS Time Extraction
```c
gps_serial_t *gps = gps_serial_open("COM3", 9600);
gps_time_t time;
if (gps_serial_read_time(gps, &time)) {
    printf("%02d:%02d:%02d.%03d UTC\n", 
           time.hour, time.minute, time.second, time.millisecond);
}
gps_serial_close(gps);
```

---

## Command Line Patterns

### Standard Options
```
-h, --help      Show usage
-v, --verbose   Verbose output
-q, --quiet     Suppress non-error output
-o FILE         Output file
-i FILE         Input file
```

### Frequency Specification
```
-f 15.000       Frequency in MHz
-f 15000000     Frequency in Hz
```

### Gain Settings
```
-g 59           IF gain reduction (dB)
-l 4            LNA state (0-8)
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| phoenix-dsp | DSP algorithms (filters, AGC) |
| phoenix-discovery | Service discovery |
| Winsock2 | Windows networking |
| Windows serial API | COM port access |
| iphlpapi | Network adapter enumeration |
