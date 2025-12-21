# phoenix-sdr-utils

**Version:** v0.1.0  
**Part of:** Phoenix Nest MARS Communications Suite  
**Developer:** Alex Pennington (KY4OLB)

---

## Overview

Utility programs and analysis tools for the Phoenix Nest SDR system. Includes I/Q file playback, signal analysis, GPS timing, telemetry logging, and debugging utilities.

---

## Tools

### I/Q Recording & Playback

| Tool | Description |
|------|-------------|
| `iqr_play` | Play back recorded I/Q files (.iqr format) |
| `iq_recorder` | Record I/Q samples to file with metadata |
| `iqr_meta` | I/Q recording metadata handling |

### Signal Analysis

| Tool | Description |
|------|-------------|
| `simple_am_receiver` | Basic AM demodulator for testing |
| `wwv_analyze` | Analyze WWV signal recordings |
| `wwv_scan` | Scan for WWV signals across frequencies |
| `wwv_listen` | Real-time WWV listening tool |
| `env_dump` | Dump signal envelope for analysis |
| `wwv_envelope_dump` | WWV-specific envelope dumping |
| `subcarrier_detector` | 100 Hz subcarrier detection |

### Debugging & Testing

| Tool | Description |
|------|-------------|
| `wwv_debug` | WWV detection debugging |
| `wwv_tick_detect` | Standalone tick detection testing |
| `wwv_tick_detect2` | Tick detection v2 algorithms |
| `wwv_sync` | Synchronization testing |
| `wwv_gps_verify` | GPS time verification |

### GPS & Timing

| Tool | Description |
|------|-------------|
| `gps_time` | GPS time extraction utility |
| `gps_serial` | GPS serial port interface |

### Telemetry & Logging

| Tool | Description |
|------|-------------|
| `telem_logger` | Log UDP telemetry to file |
| `serial_dump` | Dump serial port data for debugging |

### Python Tools

| Tool | Description |
|------|-------------|
| `wwv_analyze.py` | Python WWV analysis script |
| `wwv_plot.py` | Signal plotting and visualization |

---

## I/Q Recording Format (.iqr)

Phoenix SDR uses a custom I/Q recording format with embedded metadata.

### File Structure

```
┌────────────────────────────────────────┐
│           IQR Header (64 bytes)        │
├────────────────────────────────────────┤
│         Sample Data (variable)         │
│         int16 I, int16 Q pairs         │
└────────────────────────────────────────┘
```

### Header Fields

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | 0x49515252 ("IQRR") |
| 4 | 4 | version | Format version |
| 8 | 4 | sample_rate | Samples per second |
| 12 | 4 | sample_format | 1=S16, 2=F32 |
| 16 | 8 | center_freq | Center frequency Hz |
| 24 | 8 | timestamp | Unix timestamp (μs) |
| 32 | 4 | gps_valid | GPS lock status |
| 36 | 4 | gain_reduction | IF gain dB |
| 40 | 4 | lna_state | LNA setting |
| 44 | 20 | reserved | Future use |

---

## Usage Examples

### Play I/Q Recording

```bash
# Play to audio output
iqr_play recording.iqr

# Play with frequency offset
iqr_play -f 1000 recording.iqr

# Output to stdout for piping
iqr_play -o - recording.iqr | waterfall.exe
```

### Simple AM Receiver

```bash
# Demodulate AM at 15 MHz with 1 kHz audio offset
simple_am_receiver -f 1000 -i -o | aplay -f S16_LE -r 48000
```

### WWV Analysis

```bash
# Analyze recording for WWV signals
wwv_analyze recording.iqr

# Python analysis with plotting
python wwv_analyze.py recording.iqr --plot
```

### Telemetry Logging

```bash
# Log all telemetry to file
telem_logger -p 3005 -o telemetry.csv

# Log specific channels
telem_logger -p 3005 -c TICK,SYNC -o sync_events.csv
```

### GPS Verification

```bash
# Compare GPS time with WWV decoded time
wwv_gps_verify -g COM3 -w localhost:4536
```

---

## Building

### Windows

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

### Linux

```bash
gcc -O2 -I include src/iqr_play.c src/iqr_meta.c -o iqr_play
gcc -O2 -I include src/telem_logger.c -o telem_logger
```

---

## Dependencies

- Standard C library
- Windows: ws2_32 (Winsock)
- Python tools: numpy, matplotlib

---

## Related Repositories

| Repository | Description |
|------------|-------------|
| [mars-suite](https://github.com/Alex-Pennington/mars-suite) | Phoenix Nest MARS Suite index |
| [phoenix-sdr-core](https://github.com/Alex-Pennington/phoenix-sdr-core) | SDR hardware interface |
| [phoenix-waterfall](https://github.com/Alex-Pennington/phoenix-waterfall) | Waterfall display |
| [phoenix-wwv](https://github.com/Alex-Pennington/phoenix-wwv) | WWV detection library |
| [phoenix-sdr-net](https://github.com/Alex-Pennington/phoenix-sdr-net) | Network streaming |

---

## License

AGPL-3.0 — See [LICENSE](LICENSE)

---

*Phoenix Nest MARS Communications Suite*  
*KY4OLB*
