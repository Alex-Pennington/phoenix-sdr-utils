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

## Phase 4: DSP Library Extraction ✅

**Completed: December 23, 2025**

### Phoenix-DSP Repository Created

**Directory structure:**
```
phoenix-dsp/
├── include/
│   ├── pn_dsp.h           (master header)
│   ├── pn_filter.h        (lowpass, dc_block)
│   └── pn_agc.h           (audio AGC)
├── src/
│   ├── pn_filter.c        (filter implementations)
│   └── pn_agc.c           (AGC implementation)
├── lib/
│   ├── pn_filter.o
│   ├── pn_agc.o
│   └── libpn_dsp.a        (3,042 bytes)
└── README.md
```

### Algorithms Extracted

1. **pn_lowpass_t** - 2nd-order Butterworth lowpass
   - Extracted from simple_am_receiver.c lines 113-143
   - Configurable cutoff frequency and sample rate
   - Q = 0.7071 for maximally flat passband

2. **pn_dc_block_t** - Highpass IIR DC blocker
   - Extracted from simple_am_receiver.c lines 145-167
   - Configurable alpha coefficient (0.99 for voice)
   - Used for envelope demodulation

3. **pn_audio_agc_t** - Automatic Gain Control
   - Extracted from simple_am_receiver.c lines 169-220
   - Asymmetric attack/decay (0.01/0.0001)
   - Gain range: 0.1x to 100x

### Simple AM Receiver Updates

**Removed:**
- 96 lines of inline DSP code
- Duplicate algorithm implementations

**Added:**
- `#include "pn_dsp.h"`
- Updated types: `pn_lowpass_t`, `pn_dc_block_t`, `pn_audio_agc_t`
- Updated function calls: `pn_lowpass_init()`, `pn_dc_block_process()`, etc.

**Build command:**
```powershell
gcc -O2 -I include -I ../phoenix-dsp/include -I ../phoenix-discovery/include \
    src/simple_am_receiver.c \
    -L ../phoenix-dsp/lib -L ../phoenix-discovery/build \
    -lpn_dsp -lpn_discovery -liphlpapi -lws2_32 -lwinmm -lm \
    -o simple_am_receiver.exe
```

### Library Compilation

```powershell
cd ../phoenix-dsp
gcc -c -O2 -I include src/pn_filter.c -o lib/pn_filter.o
gcc -c -O2 -I include src/pn_agc.c -o lib/pn_agc.o
ar rcs lib/libpn_dsp.a lib/pn_filter.o lib/pn_agc.o
```

**Result:** libpn_dsp.a successfully created and linked

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
- Commit: `deb02f0`

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

### 2025-12-23 - Phase 4 Completed ✅
- Created phoenix-dsp repository with full structure
- Extracted all DSP algorithms to separate library
- Built libpn_dsp.a (3,042 bytes)
- Updated simple_am_receiver to use phoenix-dsp
- Verified compilation and linking successful
- Updated all documentation (README.md, copilot-instructions.md)
- Marked DSP_EXTRACTION_PLAN.md as complete
- Commit: `17e59b4`

---

## Final Summary

**All 4 phases complete.** Phoenix-sdr-utils is now fully modular:

### Architecture Achieved
```
Controller → sdr_server:4535 (control)
                ↓
         SDRplay RSP2 Pro
                ↓
         sdr_server:4536 (I/Q data)
                ↓
         simple_am_receiver
         ├── phoenix-discovery (service discovery)
         ├── phoenix-dsp (DSP algorithms)
         └── waveOut (audio output)
```

### Key Results

1. **Network Client:** simple_am_receiver is now hardware-independent
2. **Service Discovery:** Auto-finds sdr_server via UDP broadcast
3. **DSP Library:** Reusable algorithms for all Phoenix Nest tools
4. **Modular Build:** Clean separation of concerns

### Code Metrics

- **Lines removed:** 7,900+ (cleanup + refactor)
- **Lines added:** 1,219 (network client + documentation)
- **DSP library:** 3,042 bytes compiled
- **Net change:** -6,681 lines (leaner codebase)

### Commits

- `deb02f0` - Network client refactor
- `17e59b4` - DSP extraction to phoenix-dsp

### Dependencies

Phoenix-sdr-utils now requires:
- **phoenix-discovery** - Service discovery (UDP broadcast)
- **phoenix-dsp** - DSP algorithms (filters, AGC)
- **Windows:** ws2_32, winmm, iphlpapi
- **POSIX:** pthread, math library
