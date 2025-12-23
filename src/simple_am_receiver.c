/**
 * @file simple_am_receiver.c
 * @brief Simple AM Receiver - Network I/Q Client
 *
 * Architecture:
 * - Connects to sdr_server via Phoenix Nest discovery
 * - Receives I/Q stream on port 4536 (PHXI/IQDQ protocol)
 * - Frequency/gain control handled by separate controller program
 *
 * DSP Pipeline:
 * 1. Receive IQ samples from network (int16_t I/Q pairs)
 * 2. Lowpass filter I and Q separately (isolate signal at DC, reject off-center stations)
 * 3. Envelope detection: magnitude = sqrt(I² + Q²)
 * 4. Decimation: 2 MHz → 48 kHz (factor 42)
 * 5. DC removal: highpass IIR y[n] = x[n] - x[n-1] + 0.99*y[n-1]
 * 6. Output to speakers
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#include "pn_discovery.h"
#include "version.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <mmsystem.h>
#include <io.h>
#include <fcntl.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#define sleep_ms(ms) Sleep(ms)
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#define SDR_SAMPLE_RATE     2000000.0   /* 2 MHz from sdr_server */
#define AUDIO_SAMPLE_RATE   48000.0     /* 48 kHz audio output */
#define DECIMATION_FACTOR   42          /* 2M / 48k ≈ 42 */
#define IQ_FILTER_CUTOFF    3000.0      /* 3 kHz lowpass on I/Q before magnitude */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Audio output */
#define AUDIO_BUFFERS       4
#define AUDIO_BUFFER_SIZE   4096

/* I/Q Network Protocol */
#define IQ_DEFAULT_PORT     4536
#define IQ_MAGIC_HEADER     0x50485849  /* "PHXI" */
#define IQ_MAGIC_DATA       0x49514451  /* "IQDQ" */
#define IQ_MAGIC_META       0x4D455441  /* "META" */
#define IQ_FORMAT_S16       1

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           /* 0x50485849 = "PHXI" */
    uint32_t version;
    uint32_t sample_rate;
    uint32_t sample_format;   /* 1=S16 */
    uint32_t center_freq_lo;
    uint32_t center_freq_hi;
    uint32_t gain_reduction;
    uint32_t lna_state;
} iq_stream_header_t;

typedef struct {
    uint32_t magic;           /* 0x49514451 = "IQDQ" */
    uint32_t sequence;
    uint32_t num_samples;
    uint32_t flags;
} iq_data_frame_t;

typedef struct {
    uint32_t magic;           /* 0x4D455441 = "META" */
    uint32_t sample_rate;
    uint32_t sample_format;
    uint32_t center_freq_lo;
    uint32_t center_freq_hi;
    uint32_t gain_reduction;
    uint32_t lna_state;
    uint32_t reserved;
} iq_metadata_update_t;
#pragma pack(pop)

/*============================================================================
 * Lowpass Filter (simple 2nd order Butterworth, 2.5 kHz @ 2 MHz)
 *============================================================================*/

typedef struct {
    float x1, x2;   /* Input history */
    float y1, y2;   /* Output history */
    float b0, b1, b2, a1, a2;  /* Coefficients */
} lowpass_t;

static void lowpass_init(lowpass_t *lp, float cutoff_hz, float sample_rate) {
    /* 2nd order Butterworth lowpass */
    float w0 = 2.0f * 3.14159265f * cutoff_hz / sample_rate;
    float alpha = sinf(w0) / (2.0f * 0.7071f);  /* Q = 0.7071 for Butterworth */
    float cos_w0 = cosf(w0);

    float a0 = 1.0f + alpha;
    lp->b0 = (1.0f - cos_w0) / 2.0f / a0;
    lp->b1 = (1.0f - cos_w0) / a0;
    lp->b2 = (1.0f - cos_w0) / 2.0f / a0;
    lp->a1 = -2.0f * cos_w0 / a0;
    lp->a2 = (1.0f - alpha) / a0;

    lp->x1 = lp->x2 = 0.0f;
    lp->y1 = lp->y2 = 0.0f;
}

static float lowpass_process(lowpass_t *lp, float x) {
    float y = lp->b0 * x + lp->b1 * lp->x1 + lp->b2 * lp->x2
            - lp->a1 * lp->y1 - lp->a2 * lp->y2;
    lp->x2 = lp->x1;
    lp->x1 = x;
    lp->y2 = lp->y1;
    lp->y1 = y;
    return y;
}

/*============================================================================
 * DC Removal (highpass IIR: y[n] = x[n] - x[n-1] + 0.995*y[n-1])
 *============================================================================*/

typedef struct {
    float x_prev;
    float y_prev;
} dc_block_t;

static void dc_block_init(dc_block_t *dc) {
    dc->x_prev = 0.0f;
    dc->y_prev = 0.0f;
}

static float dc_block_process(dc_block_t *dc, float x) {
    float y = x - dc->x_prev + 0.99f * dc->y_prev;  /* 0.99 for voice (was 0.995 for pulse detection) */
    dc->x_prev = x;
    dc->y_prev = y;
    return y;
}

/*============================================================================
 * Audio AGC (Automatic Gain Control)
 *============================================================================*/

typedef struct {
    float level;        /* Running average of signal level */
    float target;       /* Target output level */
    float attack;       /* Attack time constant (fast) */
    float decay;        /* Decay time constant (slow) */
    int warmup;         /* Warmup counter */
} audio_agc_t;

static void audio_agc_init(audio_agc_t *agc, float target) {
    agc->level = 0.0001f;
    agc->target = target;
    agc->attack = 0.01f;   /* Fast attack for loud signals */
    agc->decay = 0.0001f;  /* Slow decay for quiet signals */
    agc->warmup = 0;
}

static float audio_agc_process(audio_agc_t *agc, float x) {
    float mag = fabsf(x);

    /* Track signal level with asymmetric time constants */
    if (mag > agc->level) {
        agc->level += agc->attack * (mag - agc->level);  /* Fast attack */
    } else {
        agc->level += agc->decay * (mag - agc->level);   /* Slow decay */
    }

    /* Prevent division by zero */
    if (agc->level < 0.0001f) agc->level = 0.0001f;

    /* Calculate gain to reach target level */
    float gain = agc->target / agc->level;

    /* Limit gain to prevent over-amplification during silence */
    if (gain > 100.0f) gain = 100.0f;
    if (gain < 0.1f) gain = 0.1f;

    /* Apply gain */
    return x * gain;
}

/*============================================================================
 * Audio Output (Windows waveOut)
 *============================================================================*/

#ifdef _WIN32
static HWAVEOUT g_waveOut;
static WAVEHDR g_headers[AUDIO_BUFFERS];
static int16_t *g_audio_buffers[AUDIO_BUFFERS];
static int g_current_buffer = 0;
static CRITICAL_SECTION g_audio_cs;
static bool g_audio_running = false;

static bool audio_init(void) {
    InitializeCriticalSection(&g_audio_cs);

    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        g_audio_buffers[i] = (int16_t *)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
        if (!g_audio_buffers[i]) return false;
        memset(g_audio_buffers[i], 0, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    }

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = (DWORD)AUDIO_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * 2;

    if (waveOutOpen(&g_waveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return false;
    }

    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        g_headers[i].lpData = (LPSTR)g_audio_buffers[i];
        g_headers[i].dwBufferLength = AUDIO_BUFFER_SIZE * sizeof(int16_t);
        waveOutPrepareHeader(g_waveOut, &g_headers[i], sizeof(WAVEHDR));
    }

    g_audio_running = true;
    return true;
}

static void audio_write(const int16_t *samples, uint32_t count) {
    if (!g_audio_running || count == 0) return;

    EnterCriticalSection(&g_audio_cs);

    WAVEHDR *hdr = &g_headers[g_current_buffer];

    /* Wait if buffer busy */
    while (hdr->dwFlags & WHDR_INQUEUE) {
        LeaveCriticalSection(&g_audio_cs);
        Sleep(1);
        EnterCriticalSection(&g_audio_cs);
    }

    uint32_t to_copy = (count > AUDIO_BUFFER_SIZE) ? AUDIO_BUFFER_SIZE : count;
    memcpy(g_audio_buffers[g_current_buffer], samples, to_copy * sizeof(int16_t));
    hdr->dwBufferLength = to_copy * sizeof(int16_t);

    waveOutWrite(g_waveOut, hdr, sizeof(WAVEHDR));
    g_current_buffer = (g_current_buffer + 1) % AUDIO_BUFFERS;

    LeaveCriticalSection(&g_audio_cs);
}

static void audio_close(void) {
    if (!g_audio_running) return;
    g_audio_running = false;

    waveOutReset(g_waveOut);
    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        waveOutUnprepareHeader(g_waveOut, &g_headers[i], sizeof(WAVEHDR));
        free(g_audio_buffers[i]);
    }
    waveOutClose(g_waveOut);
    DeleteCriticalSection(&g_audio_cs);
}
#endif

/*============================================================================
 * Global State
 *============================================================================*/

static volatile bool g_running = true;

/* Network connection */
static SOCKET g_iq_socket = INVALID_SOCKET;
static char g_server_host[256] = "localhost";
static int g_server_port = IQ_DEFAULT_PORT;

/* DSP state */
static lowpass_t g_lowpass_i;   /* Lowpass for I channel */
static lowpass_t g_lowpass_q;   /* Lowpass for Q channel */
static dc_block_t g_dc_block;
static audio_agc_t g_audio_agc;
static int g_decim_counter = 0;

/* Audio output buffer */
static int16_t g_audio_out[8192];
static int g_audio_out_count = 0;

/* Volume */
static float g_volume = 50.0f;

/* Output modes - can both be enabled */
static bool g_stdout_mode = false;  /* true = also output PCM to stdout (for waterfall) */
static bool g_audio_enabled = true; /* true = output to speakers */

/* Diagnostic output - goes to stderr in stdout mode */
#define LOG(...) fprintf(g_stdout_mode ? stderr : stdout, __VA_ARGS__)

/*============================================================================
 * I/Q Sample Processing
 *============================================================================*/

static void process_iq_samples(const int16_t *samples, unsigned int num_samples) {
    for (unsigned int i = 0; i < num_samples; i++) {
        /* Step 1: Get IQ sample (interleaved I/Q pairs) */
        float I = (float)samples[i * 2];
        float Q = (float)samples[i * 2 + 1];

        /* Step 2: Lowpass filter I and Q separately
         * This isolates the signal at DC (our tuned frequency)
         * and rejects off-center stations within the bandwidth */
        float I_filt = lowpass_process(&g_lowpass_i, I);
        float Q_filt = lowpass_process(&g_lowpass_q, Q);

        /* Step 3: Envelope detection on filtered signal */
        float magnitude = sqrtf(I_filt * I_filt + Q_filt * Q_filt);

        /* Step 4: DC removal (BEFORE decimation - keeps modulation clean) */
        float audio = dc_block_process(&g_dc_block, magnitude);

        /* Step 5: Audio AGC (automatic gain control for consistent volume) */
        audio = audio_agc_process(&g_audio_agc, audio);

        /* Step 6: Decimation (keep every 42nd sample) */
        g_decim_counter++;
        if (g_decim_counter >= DECIMATION_FACTOR) {
            g_decim_counter = 0;

            /* Scale to audio level */
            audio = audio * g_volume;

            /* Clip */
            if (audio > 32767.0f) audio = 32767.0f;
            if (audio < -32768.0f) audio = -32768.0f;

            /* Store in output buffer */
            g_audio_out[g_audio_out_count++] = (int16_t)audio;

            /* Step 7: Output when buffer full */
            if (g_audio_out_count >= AUDIO_BUFFER_SIZE) {
                if (g_stdout_mode) {
                    fwrite(g_audio_out, sizeof(int16_t), g_audio_out_count, stdout);
                    fflush(stdout);
                }
                if (g_audio_enabled) {
                    audio_write(g_audio_out, g_audio_out_count);
                }
                g_audio_out_count = 0;
            }
        }
    }
}

/*============================================================================
 * Network I/Q Client
 *============================================================================*/

static int socket_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
#else
    return 0;
#endif
}

static void socket_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static int recv_full(SOCKET sock, void *buf, int len) {
    int total = 0;
    char *ptr = (char *)buf;
    while (total < len && g_running) {
        int received = recv(sock, ptr + total, len - total, 0);
        if (received <= 0) return -1;
        total += received;
    }
    return total;
}

static bool connect_to_server(const char *host, int port) {
    struct sockaddr_in addr;
    
    g_iq_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_iq_socket == INVALID_SOCKET) {
        LOG("Failed to create socket\n");
        return false;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        LOG("Invalid server address: %s\n", host);
        closesocket(g_iq_socket);
        g_iq_socket = INVALID_SOCKET;
        return false;
    }
    
    if (connect(g_iq_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG("Failed to connect to %s:%d\n", host, port);
        closesocket(g_iq_socket);
        g_iq_socket = INVALID_SOCKET;
        return false;
    }
    
    LOG("Connected to sdr_server at %s:%d\n", host, port);
    return true;
}

static bool read_stream_header(void) {
    iq_stream_header_t header;
    
    if (recv_full(g_iq_socket, &header, sizeof(header)) != sizeof(header)) {
        LOG("Failed to read stream header\n");
        return false;
    }
    
    if (header.magic != IQ_MAGIC_HEADER) {
        LOG("Invalid stream header magic: 0x%08X\n", header.magic);
        return false;
    }
    
    uint64_t freq = ((uint64_t)header.center_freq_hi << 32) | header.center_freq_lo;
    
    LOG("Stream header received:\n");
    LOG("  Version: %u\n", header.version);
    LOG("  Sample Rate: %u Hz\n", header.sample_rate);
    LOG("  Format: %s\n", header.sample_format == IQ_FORMAT_S16 ? "S16" : "Unknown");
    LOG("  Center Freq: %.3f MHz\n", freq / 1e6);
    LOG("  Gain Reduction: %u dB\n", header.gain_reduction);
    LOG("  LNA State: %u\n", header.lna_state);
    LOG("\n");
    
    if (header.sample_format != IQ_FORMAT_S16) {
        LOG("Unsupported sample format: %u\n", header.sample_format);
        return false;
    }
    
    return true;
}

/*============================================================================
 * Signal Handler
 *============================================================================*/

static void signal_handler(int sig) {
    (void)sig;
    LOG("\nStopping...\n");
    g_running = false;
}

/*===========================================================================
 * Service Discovery Callback
 *============================================================================*/

static void on_service_found(const char *id, const char *service,
                              const char *ip, int ctrl_port, int data_port,
                              const char *caps, bool is_bye, void *userdata) {
    (void)id; (void)ctrl_port; (void)caps; (void)userdata;
    
    if (is_bye) return;
    
    if (strcmp(service, "sdr_server") == 0 && data_port > 0) {
        strncpy(g_server_host, ip, sizeof(g_server_host) - 1);
        g_server_port = data_port;
        LOG("Found sdr_server at %s:%d\n", g_server_host, g_server_port);
    }
}

/*============================================================================
 * Main
 *============================================================================*/

int main(int argc, char *argv[]) {
    print_version("Phoenix SDR - AM Receiver (Network Client)");

    bool use_discovery = true;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            /* Manual server specification */
            strncpy(g_server_host, argv[++i], sizeof(g_server_host) - 1);
            use_discovery = false;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            g_server_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            g_volume = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0) {
            g_stdout_mode = true;
        } else if (strcmp(argv[i], "-a") == 0) {
            g_audio_enabled = false;
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Simple AM Receiver - Network I/Q Client\n");
            printf("Usage: %s [-s server] [-p port] [-v volume] [-o] [-a]\n", argv[0]);
            printf("\nConnection:\n");
            printf("  -s HOST  Server hostname/IP (default: auto-discover)\n");
            printf("  -p PORT  I/Q data port (default: %d)\n", IQ_DEFAULT_PORT);
            printf("\nAudio:\n");
            printf("  -v NUM   Volume multiplier (default: %.1f)\n", g_volume);
            printf("  -o       Output raw PCM to stdout (for waterfall)\n");
            printf("  -a       Mute audio (disable speakers)\n");
            printf("\nNote: Frequency/gain controlled by separate program via sdr_server:4535\n");
            printf("      This program only processes I/Q data stream.\n");
            return 0;
        }
    }

    signal(SIGINT, signal_handler);

    /* Initialize sockets */
    if (socket_init() != 0) {
        fprintf(stderr, "Failed to initialize sockets\n");
        return 1;
    }

    /* Initialize discovery if needed */
    if (use_discovery) {
        LOG("Initializing Phoenix Nest discovery...\n");
        if (pn_discovery_init(0) < 0) {
            fprintf(stderr, "Failed to initialize discovery\n");
            socket_cleanup();
            return 1;
        }

        if (pn_listen(on_service_found, NULL) < 0) {
            fprintf(stderr, "Failed to start discovery listener\n");
            pn_discovery_shutdown();
            socket_cleanup();
            return 1;
        }

        LOG("Searching for sdr_server...\n");
        
        /* Wait for discovery */
        int timeout = 50;  /* 5 seconds */
        while (timeout-- > 0 && strcmp(g_server_host, "localhost") == 0) {
            sleep_ms(100);
        }

        if (strcmp(g_server_host, "localhost") == 0) {
            LOG("No sdr_server found via discovery, trying localhost:%d\n", IQ_DEFAULT_PORT);
        }
    }

    LOG("Network AM Receiver\n");
    LOG("Server: %s:%d\n", g_server_host, g_server_port);
    LOG("Audio: %s\n", g_audio_enabled ? "speakers" : "muted");
    LOG("Waterfall: %s\n", g_stdout_mode ? "stdout (raw PCM)" : "off");
    LOG("Volume: %.1f\n\n", g_volume);

    /* Initialize DSP - lowpass I and Q at 3 kHz (gives 6 kHz RF bandwidth) */
    lowpass_init(&g_lowpass_i, IQ_FILTER_CUTOFF, SDR_SAMPLE_RATE);
    lowpass_init(&g_lowpass_q, IQ_FILTER_CUTOFF, SDR_SAMPLE_RATE);
    dc_block_init(&g_dc_block);
    audio_agc_init(&g_audio_agc, 5000.0f);

    /* Initialize audio if enabled */
    if (g_audio_enabled) {
        if (!audio_init()) {
            fprintf(stderr, "Failed to initialize audio\n");
            if (use_discovery) pn_discovery_shutdown();
            socket_cleanup();
            return 1;
        }
        LOG("Audio initialized (%.0f Hz)\n", AUDIO_SAMPLE_RATE);
    }

    /* Set up stdout for PCM if waterfall mode */
    if (g_stdout_mode) {
        LOG("PCM output: 48000 Hz, 16-bit signed, mono\n");
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    }

    /* Connect to server */
    if (!connect_to_server(g_server_host, g_server_port)) {
        if (g_audio_enabled) audio_close();
        if (use_discovery) pn_discovery_shutdown();
        socket_cleanup();
        return 1;
    }

    /* Read stream header */
    if (!read_stream_header()) {
        closesocket(g_iq_socket);
        if (g_audio_enabled) audio_close();
        if (use_discovery) pn_discovery_shutdown();
        socket_cleanup();
        return 1;
    }

    LOG("Listening to I/Q stream... (Ctrl+C to stop)\n\n");

    /* Main I/Q processing loop */
    int16_t *frame_buffer = (int16_t *)malloc(16384 * 2 * sizeof(int16_t));
    if (!frame_buffer) {
        fprintf(stderr, "Failed to allocate frame buffer\n");
        closesocket(g_iq_socket);
        if (g_audio_enabled) audio_close();
        if (use_discovery) pn_discovery_shutdown();
        socket_cleanup();
        return 1;
    }

    while (g_running) {
        /* Read frame header */
        iq_data_frame_t frame_hdr;
        if (recv_full(g_iq_socket, &frame_hdr, sizeof(frame_hdr)) != sizeof(frame_hdr)) {
            if (g_running) LOG("Connection lost\n");
            break;
        }

        /* Check for data frame */
        if (frame_hdr.magic == IQ_MAGIC_DATA) {
            /* Read sample data */
            size_t data_size = frame_hdr.num_samples * 2 * sizeof(int16_t);
            if (recv_full(g_iq_socket, frame_buffer, (int)data_size) != (int)data_size) {
                if (g_running) LOG("Data read failed\n");
                break;
            }

            /* Process I/Q samples through DSP pipeline */
            process_iq_samples(frame_buffer, frame_hdr.num_samples);

        } else if (frame_hdr.magic == IQ_MAGIC_META) {
            /* Metadata update - read remaining bytes */
            iq_metadata_update_t meta;
            memcpy(&meta, &frame_hdr, sizeof(frame_hdr));
            size_t remaining = sizeof(meta) - sizeof(frame_hdr);
            if (recv_full(g_iq_socket, ((char*)&meta) + sizeof(frame_hdr), (int)remaining) != (int)remaining) {
                if (g_running) LOG("Metadata read failed\n");
                break;
            }

            uint64_t freq = ((uint64_t)meta.center_freq_hi << 32) | meta.center_freq_lo;
            LOG("[META] Freq: %.3f MHz, Sample Rate: %u Hz, Gain: %u dB\n",
                freq / 1e6, meta.sample_rate, meta.gain_reduction);
        } else {
            LOG("Unknown frame magic: 0x%08X\n", frame_hdr.magic);
            break;
        }
    }

    /* Cleanup */
    free(frame_buffer);
    closesocket(g_iq_socket);
    if (g_audio_enabled) audio_close();
    if (use_discovery) pn_discovery_shutdown();
    socket_cleanup();

    LOG("Done.\n");
    return 0;
}
