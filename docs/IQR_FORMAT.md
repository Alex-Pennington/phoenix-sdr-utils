# IQR Recording Format

Phoenix SDR I/Q Recording format specification.

---

## File Extension

`.iqr` — Phoenix I/Q Recording

---

## Header Structure

64 bytes, little-endian.

```c
typedef struct {
    uint32_t magic;           // 0x49515252 "IQRR"
    uint32_t version;         // Format version (1)
    uint32_t sample_rate;     // Samples per second
    uint32_t sample_format;   // 1=S16, 2=F32
    uint64_t center_freq;     // Center frequency in Hz
    uint64_t timestamp_us;    // Unix timestamp (microseconds)
    uint32_t gps_valid;       // GPS lock status (0/1)
    uint32_t gain_reduction;  // IF gain reduction (dB)
    uint32_t lna_state;       // LNA attenuation state
    uint8_t  reserved[20];    // Future use (zero-filled)
} iqr_header_t;
```

---

## Sample Data

Immediately follows header. Interleaved I/Q pairs.

### S16 Format (sample_format = 1)

```
[I0:int16][Q0:int16][I1:int16][Q1:int16]...
```

4 bytes per sample pair. Values -32768 to +32767.

### F32 Format (sample_format = 2)

```
[I0:float32][Q0:float32][I1:float32][Q1:float32]...
```

8 bytes per sample pair. IEEE 754 single precision.

---

## File Size Calculation

```
file_size = 64 + (num_samples * bytes_per_pair)

S16: bytes_per_pair = 4
F32: bytes_per_pair = 8
```

---

## Example: Reading Header (C)

```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t sample_rate;
    uint32_t sample_format;
    uint64_t center_freq;
    uint64_t timestamp_us;
    uint32_t gps_valid;
    uint32_t gain_reduction;
    uint32_t lna_state;
    uint8_t  reserved[20];
} iqr_header_t;

int read_iqr_header(const char *filename, iqr_header_t *hdr) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    if (fread(hdr, sizeof(iqr_header_t), 1, f) != 1) {
        fclose(f);
        return -2;
    }
    fclose(f);
    
    if (hdr->magic != 0x49515252) {
        return -3;  // Invalid magic
    }
    
    return 0;
}
```

---

## Example: Reading Header (Python)

```python
import struct

def read_iqr_header(filename):
    with open(filename, 'rb') as f:
        data = f.read(64)
    
    magic, version, rate, fmt = struct.unpack('<IIII', data[0:16])
    center_freq, timestamp = struct.unpack('<QQ', data[16:32])
    gps, gain, lna = struct.unpack('<III', data[32:44])
    
    if magic != 0x49515252:
        raise ValueError("Invalid IQR file")
    
    return {
        'sample_rate': rate,
        'format': 'S16' if fmt == 1 else 'F32',
        'center_freq': center_freq,
        'timestamp_us': timestamp,
        'gps_valid': bool(gps),
        'gain_reduction': gain,
        'lna_state': lna
    }
```

---

## Metadata Sidecar

Optional `.iqr.meta` JSON file with extended metadata:

```json
{
    "recording_id": "uuid",
    "location": {
        "lat": 38.5,
        "lon": -82.6,
        "alt": 200
    },
    "antenna": "HI-Z",
    "notes": "WWV 15 MHz recording",
    "duration_sec": 60.0
}
```

---

## Version History

| Version | Changes |
|---------|---------|
| 1 | Initial format |
