# phoenix-sdr-utils

**Version:** v0.1.0  
**Part of:** Phoenix Nest MARS Communications Suite  
**Developer:** Alex Pennington (KY4OLB)

---

## Overview

Core SDR utility programs for the Phoenix Nest SDR system. Includes I/Q file recording and playback, network-based AM signal analysis, and GPS timing tools.

**Architecture:** Tools connect to [phoenix-sdr-net](https://github.com/Alex-Pennington/phoenix-sdr-net) sdr_server for I/Q data streaming. Frequency/gain control handled by separate controller programs.

**Note:** WWV-specific analysis tools have been moved to [phoenix-wwv](https://github.com/Alex-Pennington/phoenix-wwv).

### Network Architecture

```
┌────────────────┐         UDP:5400         ┌─────────────────┐
│  Controller    │◄────────discovery────────│ simple_am_      │
│  Program       │                          │ receiver        │
└────────┬───────┘                          └────────┬────────┘
         │                                           │
         │ TCP:4535                                  │
         │ (control)                                 │ TCP:4536
         ↓                                           │ (I/Q data)
    ┌────────────────┐                               │
    │  sdr_server    │───────────────────────────────┘
    │  (phoenix-sdr- │
    │   net)         │
    └────────┬───────┘
             │ USB
             ↓
    ┌────────────────┐
    │  SDRplay       │
    │  Hardware      │
    └────────────────┘
```

- **Controller:** Sets frequency, gain, starts/stops streaming
- **sdr_server:** Interfaces with SDR hardware, streams I/Q data
- **simple_am_receiver:** Processes I/Q stream → audio output
- **Discovery:** Auto-finds sdr_server on LAN via UDP broadcast

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
| `simple_am_receiver` | Network I/Q client with AM demodulation |

### GPS & Timing

| Tool | Description |
|------|-------------|
| `gps_time` | GPS time extraction utility |
| `gps_serial` | GPS serial port interface |
| `wwv_gps_verify` | Verify WWV time signals against GPS |

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
# Auto-discover sdr_server and connect
simple_am_receiver

# Connect to specific server
simple_am_receiver -s 192.168.1.100 -p 4536

# Output to stdout for waterfall piping
simple_am_receiver -o | waterfall.exe

# Adjust volume
simple_am_receiver -v 100
```

**Note:** Frequency and gain are controlled via sdr_server:4535 control port by a separate controller program. simple_am_receiver only processes the I/Q data stream.

### GPS Timing

```bash
# Extract GPS time from serial port
gps_time COM3

# Verify WWV time signals against GPS
wwv_gps_verify -g COM3 -w localhost:4536
```

---

## Building

### Windows

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

# GPS verification tool
gcc -O2 -I include src/wwv_gps_verify.c src/gps_serial.c -lws2_32 -o wwv_gps_verify.exe
```

### Linux

```bash
gcc -O2 -I include src/iqr_play.c src/iqr_meta.c -o iqr_play
gcc -O2 -I include -I ../phoenix-dsp/include -I ../phoenix-discovery/include \
    src/simple_am_receiver.c \
    -L ../phoenix-dsp/lib -L ../phoenix-discovery/build \
    -lpn_dsp -lpn_discovery -lpthread -lm \
    -o simple_am_receiver
```

---

## Dependencies

- Standard C library
- Windows: ws2_32 (Winsock), winmm (audio output), iphlpapi (network adapters)
- [phoenix-dsp](../phoenix-dsp) - DSP algorithms library
- [phoenix-discovery](https://github.com/Alex-Pennington/phoenix-discovery) - Service discovery library

---

## Related Repositories

| Repository | Description |
|------------|-------------|
| [mars-suite](https://github.com/Alex-Pennington/mars-suite) | Phoenix Nest MARS Suite index |
| [phoenix-sdr-core](https://github.com/Alex-Pennington/phoenix-sdr-core) | SDR hardware interface |
| [phoenix-sdr-net](https://github.com/Alex-Pennington/phoenix-sdr-net) | I/Q streaming server |
| [phoenix-dsp](../phoenix-dsp) | DSP algorithms library |
| [phoenix-discovery](https://github.com/Alex-Pennington/phoenix-discovery) | Service discovery |
| [phoenix-waterfall](https://github.com/Alex-Pennington/phoenix-waterfall) | Waterfall display |
| [phoenix-wwv](https://github.com/Alex-Pennington/phoenix-wwv) | WWV detection library |

---

## License

AGPL-3.0 — See [LICENSE](LICENSE)

---

*Phoenix Nest MARS Communications Suite*  
*KY4OLB*
