/*
 * SDOS host/DSP protocol, version 1.0. The specification is docs/protocol.md.
 *
 * One source for both sides: Linux (libsdos_dsp) and every DSP firmware build
 * compile this file. It is C99 because the TI C6000 compiler builds in C99
 * mode. Every field is encoded explicitly little-endian; no structure is ever
 * copied to or from the wire, so layout and padding never matter.
 */
#ifndef SDOS_PROTOCOL_H
#define SDOS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDOS_PROTO_MAJOR 1u
#define SDOS_PROTO_MINOR 0u

/* Course firmware announces this service at this fixed RPMsg endpoint. */
#define SDOS_RPMSG_SERVICE "sdos-dsp"
#define SDOS_RPMSG_ENDPOINT 40u

/* An RPMsg buffer is 512 bytes, 16 of which are the RPMsg header. */
#define SDOS_MSG_MAX 496u
#define SDOS_HEADER_SIZE 16u
#define SDOS_PAYLOAD_MAX (SDOS_MSG_MAX - SDOS_HEADER_SIZE)

#define SDOS_FLAG_RESPONSE 0x0001u
#define SDOS_FLAGS_KNOWN SDOS_FLAG_RESPONSE

typedef enum sdos_msg_type {
    SDOS_MSG_HELLO = 1,  /* identity and version negotiation */
    SDOS_MSG_ECHO = 2,   /* payload returned unchanged */
    SDOS_MSG_STATUS = 3, /* counters since the firmware started */
    SDOS_MSG_RUN = 4     /* run one algorithm over shared buffers */
} sdos_msg_type;

typedef enum sdos_status {
    SDOS_OK = 0,
    SDOS_E_MAGIC = 1,      /* not an SDOS message */
    SDOS_E_VERSION = 2,    /* major version differs */
    SDOS_E_TYPE = 3,       /* unknown message type */
    SDOS_E_LENGTH = 4,     /* length does not match the type or the header */
    SDOS_E_FIELD = 5,      /* reserved field or flag set, or value out of range */
    SDOS_E_DESCRIPTOR = 6, /* buffer range outside the region or misaligned */
    SDOS_E_CHECKSUM = 7,   /* input data does not match its checksum */
    SDOS_E_ALGORITHM = 8,  /* algorithm id not present in this firmware */
    SDOS_E_BUSY = 9,       /* another request is still running */
    SDOS_E_STATE = 10,     /* buffer ownership does not allow this step */
    SDOS_E_INTERNAL = 11,  /* the algorithm or runtime failed */
    SDOS_E_TIMEOUT = 12,   /* host side only: no response in time */
    SDOS_E_TRANSPORT = 13, /* host side only: the transport failed */
    SDOS_STATUS_COUNT = 14
} sdos_status;

typedef struct sdos_header {
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t type;
    uint16_t flags;
    uint32_t request_id;
    uint16_t payload_length;
    uint16_t status;
} sdos_header;

/* HELLO response payload. */
#define SDOS_IDENTITY_SIZE 48u
#define SDOS_FIRMWARE_NAME_MAX 24u

typedef enum sdos_core { SDOS_CORE_C66X_0 = 0, SDOS_CORE_C66X_1 = 1, SDOS_CORE_C71X_0 = 2, SDOS_CORE_HOST_MOCK = 15 } sdos_core;
typedef enum sdos_runtime_kind { SDOS_RUNTIME_FREERTOS = 1, SDOS_RUNTIME_BAREMETAL = 2, SDOS_RUNTIME_MOCK = 3 } sdos_runtime_kind;

#define SDOS_CAP_ECHO 0x0001u
#define SDOS_CAP_STATUS 0x0002u
#define SDOS_CAP_RUN 0x0004u
#define SDOS_CAP_CHECKSUM 0x0008u
#define SDOS_CAP_CYCLES 0x0010u

typedef struct sdos_identity {
    uint8_t core;
    uint8_t runtime;
    uint32_t capabilities;
    uint16_t max_payload;
    uint16_t buffer_alignment;
    uint32_t runtime_version; /* major << 16 | minor << 8 | patch */
    uint32_t region_size;     /* bytes of the shared buffer region; 0 when none */
    char firmware[SDOS_FIRMWARE_NAME_MAX]; /* NUL-terminated within the field */
} sdos_identity;

/* STATUS response payload. */
#define SDOS_STATS_SIZE 24u

typedef struct sdos_stats {
    uint32_t uptime_ms;
    uint32_t requests;
    uint32_t errors;
    uint32_t last_error;
    uint32_t last_request_id;
    uint32_t runs;
} sdos_stats;

/* A range of the shared buffer region. */
#define SDOS_DESCRIPTOR_SIZE 24u
#define SDOS_DESC_CHECKSUM 0x0001u

typedef struct sdos_descriptor {
    uint32_t region; /* 0 is the core's shared region; no other region exists in 1.0 */
    uint32_t offset;
    uint32_t length;
    uint32_t sequence;
    uint32_t checksum; /* CRC-32C of the range when SDOS_DESC_CHECKSUM is set */
    uint16_t flags;
} sdos_descriptor;

/* RUN request and response payloads. */
#define SDOS_RUN_FIXED_SIZE 60u
#define SDOS_RUN_PARAMS_MAX (SDOS_PAYLOAD_MAX - SDOS_RUN_FIXED_SIZE)
#define SDOS_RUN_RESULT_SIZE 20u

typedef struct sdos_run_request {
    uint32_t algorithm;
    sdos_descriptor input;
    sdos_descriptor output;
    uint16_t params_length;
    uint8_t params[SDOS_RUN_PARAMS_MAX];
} sdos_run_request;

typedef struct sdos_run_result {
    uint32_t output_length;
    uint32_t output_checksum;
    uint64_t cycles;
    uint32_t output_sequence;
} sdos_run_result;

/* Ownership of one shared buffer; see docs/protocol.md for the diagram. */
typedef enum sdos_buf_state {
    SDOS_BUF_FREE = 0,
    SDOS_BUF_HOST = 1,        /* host may read and write it */
    SDOS_BUF_TO_DSP = 2,      /* host wrote back its cache and sent RUN */
    SDOS_BUF_DSP = 3,         /* DSP invalidated its cache and owns it */
    SDOS_BUF_TO_HOST = 4,     /* DSP wrote back its cache and answered */
    SDOS_BUF_QUARANTINED = 5  /* a timeout left ownership unknown */
} sdos_buf_state;

typedef enum sdos_buf_event {
    SDOS_BUF_ACQUIRE = 0,
    SDOS_BUF_SUBMIT = 1,
    SDOS_BUF_DSP_ACCEPT = 2,
    SDOS_BUF_DSP_COMPLETE = 3,
    SDOS_BUF_HOST_COMPLETE = 4,
    SDOS_BUF_RELEASE = 5,
    SDOS_BUF_TIMEOUT = 6,
    SDOS_BUF_DSP_RESTARTED = 7,
    SDOS_BUF_REJECTED = 8 /* the DSP refused RUN before taking the buffer */
} sdos_buf_event;

/* Message framing. Encoders return the bytes written, or 0 when `cap` is too
 * small or an input is out of range. Decoders validate every field. */
size_t sdos_encode(const sdos_header *header, const uint8_t *payload, uint8_t *out, size_t cap);
sdos_status sdos_decode_header(const uint8_t *in, size_t length, sdos_header *header);

size_t sdos_identity_encode(const sdos_identity *identity, uint8_t *out, size_t cap);
sdos_status sdos_identity_decode(const uint8_t *in, size_t length, sdos_identity *identity);
size_t sdos_stats_encode(const sdos_stats *stats, uint8_t *out, size_t cap);
sdos_status sdos_stats_decode(const uint8_t *in, size_t length, sdos_stats *stats);
size_t sdos_run_request_encode(const sdos_run_request *request, uint8_t *out, size_t cap);
sdos_status sdos_run_request_decode(const uint8_t *in, size_t length, sdos_run_request *request);
size_t sdos_run_result_encode(const sdos_run_result *result, uint8_t *out, size_t cap);
sdos_status sdos_run_result_decode(const uint8_t *in, size_t length, sdos_run_result *result);

/* SDOS_OK when the range lies inside a region of `region_size` bytes and its
 * offset is a multiple of `alignment` (a power of two). Overflow-safe. */
sdos_status sdos_descriptor_check(const sdos_descriptor *descriptor, uint32_t region_size, uint32_t alignment);

/* SDOS_OK and the next state, or SDOS_E_STATE for a step the state forbids. */
sdos_status sdos_buf_next(sdos_buf_state state, sdos_buf_event event, sdos_buf_state *next);

/* CRC-32C (Castagnoli). Pass 0 to start; feed the result back to continue. */
uint32_t sdos_crc32c(uint32_t crc, const uint8_t *data, size_t length);

const char *sdos_status_name(uint32_t status);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_PROTOCOL_H */
