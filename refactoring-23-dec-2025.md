# Phoenix SDR Utils Refactoring - December 23, 2025

## Objective

Refactor `simple_am_receiver` from direct SDR hardware control to network client architecture, aligning with Phoenix Nest ecosystem design.

---

## Current Architecture

```
simple_am_receiver
    ↓
sdrplay_api (direct hardware)
    ↓
SDRplay RSP2 Pro
```

**Issues:**
- Tight coupling to SDR hardware
- Cannot share SDR with other programs
- Duplicates control logic across tools

---

## Target Architecture

```
Controller Program
    ↓
sdr_server:4535 (control) ← SET_FREQ, SET_GAIN, START, STOP
    ↓
SDRplay RSP2 Pro
    ↓
sdr_server:4536 (I/Q data) → PHXI/IQDQ protocol
    ↓
simple_am_receiver (DSP + audio)
```

**Benefits:**
- Multiple clients can consume same I/Q stream
- Separation of control and data paths
- Reusable across Phoenix Nest ecosystem

---

## Phases

### Phase 1: Network Client Architecture ✅

**Tasks:**
- [x] Add pn_discovery integration to find sdr_server
- [x] Implement I/Q network protocol (PHXI header + IQDQ frames)
- [x] Remove all SDR hardware control code
- [x] Keep DSP pipeline intact (lowpass, envelope, decimation, AGC)
- [x] Keep audio output system (waveOut)
- [x] Update command-line interface (remove -f, -g, -l flags)

**Files modified:**
- `src/simple_am_receiver.c` - Complete network client refactor

**Changes:**
- Replaced `#include "sdrplay_api.h"` with network includes (winsock2, pn_discovery)
- Added I/Q protocol structures (PHXI header, IQDQ data frames, META updates)
- Removed SDR globals (g_device, g_params)
- Added network globals (g_iq_socket, g_server_host, g_server_port)
- Created `process_iq_samples()` function from stream callback logic
- Added network client functions:
  - `connect_to_server()` - TCP connection to sdr_server
  - `read_stream_header()` - Parse PHXI header
  - `recv_full()` - Reliable TCP receive
  - `on_service_found()` - Discovery callback
- Rewrote main() for network client:
  - Uses pn_discovery to find sdr_server
  - Connects to I/Q data port (4536)
  - Reads and processes IQDQ frames
  - Handles META updates
- New CLI: `-s server`, `-p port`, `-v volume`, `-o`, `-a`
- Removed: `-f freq`, `-g gain`, `-l lna` (controller's responsibility)

### Phase 2: Documentation Updates ✅

**Tasks:**
- [x] Update README.md
  - Remove SDRplay hardware references
  - Add network architecture diagram
  - Update usage examples
  - Update build instructions
- [x] Update .github/copilot-instructions.md
  - Update tool descriptions
  - Add network client notes

### Phase 3: DSP Extraction Planning ✅

**Tasks:**
- [x] Create comprehensive DSP extraction plan
- [x] Document lowpass_t, dc_block_t, audio_agc_t algorithms
- [x] Define API specifications and migration strategy
- [x] Specify phoenix-dsp library structure

**Algorithms to extract to phoenix-dsp:**

1. **lowpass_t** - 2nd-order Butterworth lowpass filter
   - Coefficient calculation from cutoff/sample rate
   - Direct Form II implementation
   - State management (x1, x2, y1, y2)

2. **dc_block_t** - Highpass IIR for DC removal
   - Single-pole filter: `y[n] = x[n] - x[n-1] + α*y[n-1]`
   - Configurable α (currently 0.99)

3. **audio_agc_t** - Automatic Gain Control
   - Asymmetric time constants (fast attack 0.01, slow decay 0.0001)
   - Target level normalization
   - Gain limiting (0.1 to 100)

**Documentation created:**
- `docs/DSP_EXTRACTION_PLAN.md` - Complete migration guide

**Post-extraction:**
- simple_am_receiver links against libphoenix-dsp
- Shared DSP primitives across phoenix-wwv, phoenix-waterfall, etc.

---

## Progress Log

### 2025-12-23 - Initial Planning
- Created refactoring plan
- Documented current vs target architecture
- Identified DSP components for extraction

### 2025-12-23 - Phase 1 Completed ✅
- Replaced SDRplay API with network client architecture
- Implemented I/Q streaming protocol (PHXI/IQDQ)
- Integrated pn_discovery for auto-discovery of sdr_server
- Preserved DSP pipeline (lowpass, envelope, DC block, AGC)
- Updated CLI to network client model
- Removed all hardware control code

### 2025-12-23 - Phase 2 Completed ✅
- Updated README.md with network architecture diagram
- Added PHXI/IQDQ protocol description
- Updated usage examples for network client
- Modified build instructions to include phoenix-discovery

### 2025-12-23 - Phase 3 Completed ✅
- Updated .github/copilot-instructions.md with network architecture section
- Added detailed protocol specifications
- Created docs/DSP_EXTRACTION_PLAN.md
- Documented lowpass_t, dc_block_t, audio_agc_t algorithms
- Defined phoenix-dsp migration strategy

---

## Summary

**All phases complete.** simple_am_receiver is now a pure network client that:
- Auto-discovers sdr_server via UDP broadcast (phoenix-discovery)
- Receives I/Q data over TCP socket (PHXI/IQDQ protocol)
- Preserves DSP pipeline for future extraction to phoenix-dsp
- No longer requires SDR hardware access

**Next steps** (future work):
- Create phoenix-dsp repository
- Extract DSP algorithms (lowpass, dc_block, audio_agc)
- Update simple_am_receiver to link libpn_dsp
