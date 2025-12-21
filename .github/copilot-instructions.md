# Phoenix SDR Utils - Copilot Instructions

## P0 - CRITICAL RULES

1. **NO unauthorized additions.** Do not add features, flags, modes, or files without explicit instruction. ASK FIRST.
2. **Minimal scope.** Fix only what's requested. One change at a time.
3. **Suggest before acting.** Explain your plan, wait for confirmation.
4. **Verify before modifying.** Run `git status`, read files before editing.

---

## Overview

**Phoenix SDR Utils** contains standalone utility programs for I/Q playback, signal analysis, GPS timing, and debugging.

### Tool Categories

| Category | Tools |
|----------|-------|
| I/Q Recording | `iqr_play`, `iq_recorder`, `iqr_meta` |
| Signal Analysis | `simple_am_receiver`, `wwv_analyze`, `env_dump` |
| WWV Debugging | `wwv_debug`, `wwv_tick_detect`, `wwv_scan`, `wwv_sync` |
| GPS/Timing | `gps_time`, `gps_serial`, `wwv_gps_verify` |
| Telemetry | `telem_logger`, `serial_dump` |
| Python Tools | `wwv_analyze.py`, `wwv_plot.py` |

---

## Build

```powershell
# I/Q playback
gcc -O2 -I include src/iqr_play.c src/iqr_meta.c -o iqr_play.exe

# AM receiver
gcc -O2 -I include src/simple_am_receiver.c -lws2_32 -o simple_am_receiver.exe

# Telemetry logger
gcc -O2 -I include src/telem_logger.c -lws2_32 -o telem_logger.exe

# GPS tools
gcc -O2 -I include src/gps_time.c src/gps_serial.c -o gps_time.exe
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

## Telemetry Logger

Receives UDP broadcast telemetry and logs to CSV.

### Usage
```bash
telem_logger -p 3005 -o telemetry.csv
telem_logger -p 3005 -c TICK,SYNC -o events.csv
```

### Channel Prefixes
| Prefix | Source |
|--------|--------|
| `TICK` | Tick detector |
| `MARK` | Marker detector |
| `SYNC` | Sync state machine |
| `BCDS` | BCD decoder |
| `CHAN` | Channel quality |
| `T500` | 500 Hz tone tracker |
| `T600` | 600 Hz tone tracker |
| `CONS` | Console messages |

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

## Python Tools

### Requirements
```
numpy
matplotlib
scipy (optional)
```

### wwv_analyze.py
```bash
python wwv_analyze.py recording.iqr --plot
python wwv_analyze.py recording.iqr --output analysis.csv
```

### wwv_plot.py
```bash
python wwv_plot.py telemetry.csv --channel TICK
python wwv_plot.py telemetry.csv --waterfall
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| Winsock2 | Windows networking |
| Windows serial API | COM port access |
| Python 3.x | Python tools |
| numpy/matplotlib | Python analysis |
