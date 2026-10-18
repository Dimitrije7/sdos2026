/* SDOS protocol 1.0 encoding, validation, and buffer ownership. See protocol.h. */
#include "sdos/protocol.h"

#include <string.h>

#define MAGIC0 0x53u /* 'S' */
#define MAGIC1 0x44u /* 'D' */

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, (uint16_t)(v & 0xFFFFu));
    put16(p + 2, (uint16_t)(v >> 16));
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16);
}

size_t sdos_encode(const sdos_header *header, const uint8_t *payload, uint8_t *out, size_t cap)
{
    size_t total = SDOS_HEADER_SIZE + (size_t)header->payload_length;
    if (header->payload_length > SDOS_PAYLOAD_MAX || cap < total || (header->flags & (uint16_t)~SDOS_FLAGS_KNOWN) != 0u) {
        return 0u;
    }
    if (header->payload_length > 0u && payload == NULL) {
        return 0u;
    }
    out[0] = MAGIC0;
    out[1] = MAGIC1;
    out[2] = header->version_major;
    out[3] = header->version_minor;
    put16(out + 4, header->type);
    put16(out + 6, header->flags);
    put32(out + 8, header->request_id);
    put16(out + 12, header->payload_length);
    put16(out + 14, header->status);
    if (header->payload_length > 0u) {
        memmove(out + SDOS_HEADER_SIZE, payload, header->payload_length);
    }
    return total;
}

sdos_status sdos_decode_header(const uint8_t *in, size_t length, sdos_header *header)
{
    if (length < SDOS_HEADER_SIZE || length > SDOS_MSG_MAX) {
        return SDOS_E_LENGTH;
    }
    if (in[0] != MAGIC0 || in[1] != MAGIC1) {
        return SDOS_E_MAGIC;
    }
    header->version_major = in[2];
    header->version_minor = in[3];
    header->type = get16(in + 4);
    header->flags = get16(in + 6);
    header->request_id = get32(in + 8);
    header->payload_length = get16(in + 12);
    header->status = get16(in + 14);
    if (header->version_major != SDOS_PROTO_MAJOR) {
        return SDOS_E_VERSION;
    }
    if ((size_t)header->payload_length != length - SDOS_HEADER_SIZE) {
        return SDOS_E_LENGTH;
    }
    if ((header->flags & (uint16_t)~SDOS_FLAGS_KNOWN) != 0u) {
        return SDOS_E_FIELD;
    }
    return SDOS_OK;
}

size_t sdos_identity_encode(const sdos_identity *identity, uint8_t *out, size_t cap)
{
    size_t name_length = 0u;
    if (cap < SDOS_IDENTITY_SIZE) {
        return 0u;
    }
    while (name_length < SDOS_FIRMWARE_NAME_MAX && identity->firmware[name_length] != '\0') {
        name_length++;
    }
    if (name_length == SDOS_FIRMWARE_NAME_MAX) {
        return 0u; /* the name must be NUL-terminated inside its field */
    }
    memset(out, 0, SDOS_IDENTITY_SIZE);
    out[0] = identity->core;
    out[1] = identity->runtime;
    put32(out + 4, identity->capabilities);
    put16(out + 8, identity->max_payload);
    put16(out + 10, identity->buffer_alignment);
    put32(out + 12, identity->runtime_version);
    put32(out + 16, identity->region_size);
    memcpy(out + 24, identity->firmware, name_length);
    return SDOS_IDENTITY_SIZE;
}

sdos_status sdos_identity_decode(const uint8_t *in, size_t length, sdos_identity *identity)
{
    size_t i;
    if (length != SDOS_IDENTITY_SIZE) {
        return SDOS_E_LENGTH;
    }
    if (get16(in + 2) != 0u || get32(in + 20) != 0u) {
        return SDOS_E_FIELD;
    }
    identity->core = in[0];
    identity->runtime = in[1];
    identity->capabilities = get32(in + 4);
    identity->max_payload = get16(in + 8);
    identity->buffer_alignment = get16(in + 10);
    identity->runtime_version = get32(in + 12);
    identity->region_size = get32(in + 16);
    for (i = 0u; i < SDOS_FIRMWARE_NAME_MAX; i++) {
        uint8_t c = in[24u + i];
        if (c != 0u && (c < 0x20u || c > 0x7Eu)) {
            return SDOS_E_FIELD;
        }
        identity->firmware[i] = (char)c;
    }
    if (identity->firmware[SDOS_FIRMWARE_NAME_MAX - 1u] != '\0') {
        return SDOS_E_FIELD;
    }
    if (identity->buffer_alignment == 0u || (identity->buffer_alignment & (identity->buffer_alignment - 1u)) != 0u) {
        return SDOS_E_FIELD;
    }
    return SDOS_OK;
}

size_t sdos_stats_encode(const sdos_stats *stats, uint8_t *out, size_t cap)
{
    if (cap < SDOS_STATS_SIZE) {
        return 0u;
    }
    put32(out, stats->uptime_ms);
    put32(out + 4, stats->requests);
    put32(out + 8, stats->errors);
    put32(out + 12, stats->last_error);
    put32(out + 16, stats->last_request_id);
    put32(out + 20, stats->runs);
    return SDOS_STATS_SIZE;
}

sdos_status sdos_stats_decode(const uint8_t *in, size_t length, sdos_stats *stats)
{
    if (length != SDOS_STATS_SIZE) {
        return SDOS_E_LENGTH;
    }
    stats->uptime_ms = get32(in);
    stats->requests = get32(in + 4);
    stats->errors = get32(in + 8);
    stats->last_error = get32(in + 12);
    stats->last_request_id = get32(in + 16);
    stats->runs = get32(in + 20);
    return SDOS_OK;
}

static void descriptor_encode(const sdos_descriptor *d, uint8_t *out)
{
    put32(out, d->region);
    put32(out + 4, d->offset);
    put32(out + 8, d->length);
    put32(out + 12, d->sequence);
    put32(out + 16, d->checksum);
    put16(out + 20, d->flags);
    put16(out + 22, 0u);
}

static sdos_status descriptor_decode(const uint8_t *in, sdos_descriptor *d)
{
    d->region = get32(in);
    d->offset = get32(in + 4);
    d->length = get32(in + 8);
    d->sequence = get32(in + 12);
    d->checksum = get32(in + 16);
    d->flags = get16(in + 20);
    if (get16(in + 22) != 0u || (d->flags & (uint16_t)~SDOS_DESC_CHECKSUM) != 0u) {
        return SDOS_E_FIELD;
    }
    return SDOS_OK;
}

size_t sdos_run_request_encode(const sdos_run_request *request, uint8_t *out, size_t cap)
{
    size_t total = SDOS_RUN_FIXED_SIZE + (size_t)request->params_length;
    if (request->params_length > SDOS_RUN_PARAMS_MAX || cap < total) {
        return 0u;
    }
    put32(out, request->algorithm);
    put32(out + 4, 0u);
    descriptor_encode(&request->input, out + 8);
    descriptor_encode(&request->output, out + 32);
    put16(out + 56, request->params_length);
    put16(out + 58, 0u);
    if (request->params_length > 0u) {
        memcpy(out + SDOS_RUN_FIXED_SIZE, request->params, request->params_length);
    }
    return total;
}

sdos_status sdos_run_request_decode(const uint8_t *in, size_t length, sdos_run_request *request)
{
    sdos_status status;
    if (length < SDOS_RUN_FIXED_SIZE) {
        return SDOS_E_LENGTH;
    }
    request->algorithm = get32(in);
    request->params_length = get16(in + 56);
    if (get32(in + 4) != 0u || get16(in + 58) != 0u) {
        return SDOS_E_FIELD;
    }
    if ((size_t)request->params_length != length - SDOS_RUN_FIXED_SIZE) {
        return SDOS_E_LENGTH;
    }
    status = descriptor_decode(in + 8, &request->input);
    if (status == SDOS_OK) {
        status = descriptor_decode(in + 32, &request->output);
    }
    if (status == SDOS_OK && request->params_length > 0u) {
        memcpy(request->params, in + SDOS_RUN_FIXED_SIZE, request->params_length);
    }
    return status;
}

size_t sdos_run_result_encode(const sdos_run_result *result, uint8_t *out, size_t cap)
{
    if (cap < SDOS_RUN_RESULT_SIZE) {
        return 0u;
    }
    put32(out, result->output_length);
    put32(out + 4, result->output_checksum);
    put32(out + 8, (uint32_t)(result->cycles & 0xFFFFFFFFu));
    put32(out + 12, (uint32_t)(result->cycles >> 32));
    put32(out + 16, result->output_sequence);
    return SDOS_RUN_RESULT_SIZE;
}

sdos_status sdos_run_result_decode(const uint8_t *in, size_t length, sdos_run_result *result)
{
    if (length != SDOS_RUN_RESULT_SIZE) {
        return SDOS_E_LENGTH;
    }
    result->output_length = get32(in);
    result->output_checksum = get32(in + 4);
    result->cycles = (uint64_t)get32(in + 8) | ((uint64_t)get32(in + 12) << 32);
    result->output_sequence = get32(in + 16);
    return SDOS_OK;
}

sdos_status sdos_descriptor_check(const sdos_descriptor *descriptor, uint32_t region_size, uint32_t alignment)
{
    if (descriptor->region != 0u || (descriptor->flags & (uint16_t)~SDOS_DESC_CHECKSUM) != 0u) {
        return SDOS_E_FIELD;
    }
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u || (descriptor->offset & (alignment - 1u)) != 0u) {
        return SDOS_E_DESCRIPTOR;
    }
    /* offset + length <= region_size, written so it cannot wrap */
    if (descriptor->length > region_size || descriptor->offset > region_size - descriptor->length) {
        return SDOS_E_DESCRIPTOR;
    }
    return SDOS_OK;
}

sdos_status sdos_buf_next(sdos_buf_state state, sdos_buf_event event, sdos_buf_state *next)
{
    sdos_buf_state result = state;
    sdos_status status = SDOS_OK;
    switch (event) {
    case SDOS_BUF_ACQUIRE:
        if (state == SDOS_BUF_FREE) { result = SDOS_BUF_HOST; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_SUBMIT:
        if (state == SDOS_BUF_HOST) { result = SDOS_BUF_TO_DSP; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_DSP_ACCEPT:
        if (state == SDOS_BUF_TO_DSP) { result = SDOS_BUF_DSP; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_DSP_COMPLETE:
        if (state == SDOS_BUF_DSP) { result = SDOS_BUF_TO_HOST; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_HOST_COMPLETE:
        if (state == SDOS_BUF_TO_HOST) { result = SDOS_BUF_HOST; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_RELEASE:
        if (state == SDOS_BUF_HOST) { result = SDOS_BUF_FREE; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_TIMEOUT:
        /* The DSP may still write the buffer: nobody may reuse it until a restart. */
        if (state == SDOS_BUF_TO_DSP || state == SDOS_BUF_DSP || state == SDOS_BUF_TO_HOST) {
            result = SDOS_BUF_QUARANTINED;
        } else {
            status = SDOS_E_STATE;
        }
        break;
    case SDOS_BUF_REJECTED:
        if (state == SDOS_BUF_TO_DSP) { result = SDOS_BUF_HOST; } else { status = SDOS_E_STATE; }
        break;
    case SDOS_BUF_DSP_RESTARTED:
        /* A stopped DSP holds nothing; host-owned buffers stay with the host. */
        if (state != SDOS_BUF_HOST) { result = SDOS_BUF_FREE; }
        break;
    default:
        status = SDOS_E_FIELD;
        break;
    }
    if (status == SDOS_OK) {
        *next = result;
    }
    return status;
}

static const uint32_t crc32c_table[256] = {
    0x00000000u, 0xF26B8303u, 0xE13B70F7u, 0x1350F3F4u, 0xC79A971Fu, 0x35F1141Cu,
    0x26A1E7E8u, 0xD4CA64EBu, 0x8AD958CFu, 0x78B2DBCCu, 0x6BE22838u, 0x9989AB3Bu,
    0x4D43CFD0u, 0xBF284CD3u, 0xAC78BF27u, 0x5E133C24u, 0x105EC76Fu, 0xE235446Cu,
    0xF165B798u, 0x030E349Bu, 0xD7C45070u, 0x25AFD373u, 0x36FF2087u, 0xC494A384u,
    0x9A879FA0u, 0x68EC1CA3u, 0x7BBCEF57u, 0x89D76C54u, 0x5D1D08BFu, 0xAF768BBCu,
    0xBC267848u, 0x4E4DFB4Bu, 0x20BD8EDEu, 0xD2D60DDDu, 0xC186FE29u, 0x33ED7D2Au,
    0xE72719C1u, 0x154C9AC2u, 0x061C6936u, 0xF477EA35u, 0xAA64D611u, 0x580F5512u,
    0x4B5FA6E6u, 0xB93425E5u, 0x6DFE410Eu, 0x9F95C20Du, 0x8CC531F9u, 0x7EAEB2FAu,
    0x30E349B1u, 0xC288CAB2u, 0xD1D83946u, 0x23B3BA45u, 0xF779DEAEu, 0x05125DADu,
    0x1642AE59u, 0xE4292D5Au, 0xBA3A117Eu, 0x4851927Du, 0x5B016189u, 0xA96AE28Au,
    0x7DA08661u, 0x8FCB0562u, 0x9C9BF696u, 0x6EF07595u, 0x417B1DBCu, 0xB3109EBFu,
    0xA0406D4Bu, 0x522BEE48u, 0x86E18AA3u, 0x748A09A0u, 0x67DAFA54u, 0x95B17957u,
    0xCBA24573u, 0x39C9C670u, 0x2A993584u, 0xD8F2B687u, 0x0C38D26Cu, 0xFE53516Fu,
    0xED03A29Bu, 0x1F682198u, 0x5125DAD3u, 0xA34E59D0u, 0xB01EAA24u, 0x42752927u,
    0x96BF4DCCu, 0x64D4CECFu, 0x77843D3Bu, 0x85EFBE38u, 0xDBFC821Cu, 0x2997011Fu,
    0x3AC7F2EBu, 0xC8AC71E8u, 0x1C661503u, 0xEE0D9600u, 0xFD5D65F4u, 0x0F36E6F7u,
    0x61C69362u, 0x93AD1061u, 0x80FDE395u, 0x72966096u, 0xA65C047Du, 0x5437877Eu,
    0x4767748Au, 0xB50CF789u, 0xEB1FCBADu, 0x197448AEu, 0x0A24BB5Au, 0xF84F3859u,
    0x2C855CB2u, 0xDEEEDFB1u, 0xCDBE2C45u, 0x3FD5AF46u, 0x7198540Du, 0x83F3D70Eu,
    0x90A324FAu, 0x62C8A7F9u, 0xB602C312u, 0x44694011u, 0x5739B3E5u, 0xA55230E6u,
    0xFB410CC2u, 0x092A8FC1u, 0x1A7A7C35u, 0xE811FF36u, 0x3CDB9BDDu, 0xCEB018DEu,
    0xDDE0EB2Au, 0x2F8B6829u, 0x82F63B78u, 0x709DB87Bu, 0x63CD4B8Fu, 0x91A6C88Cu,
    0x456CAC67u, 0xB7072F64u, 0xA457DC90u, 0x563C5F93u, 0x082F63B7u, 0xFA44E0B4u,
    0xE9141340u, 0x1B7F9043u, 0xCFB5F4A8u, 0x3DDE77ABu, 0x2E8E845Fu, 0xDCE5075Cu,
    0x92A8FC17u, 0x60C37F14u, 0x73938CE0u, 0x81F80FE3u, 0x55326B08u, 0xA759E80Bu,
    0xB4091BFFu, 0x466298FCu, 0x1871A4D8u, 0xEA1A27DBu, 0xF94AD42Fu, 0x0B21572Cu,
    0xDFEB33C7u, 0x2D80B0C4u, 0x3ED04330u, 0xCCBBC033u, 0xA24BB5A6u, 0x502036A5u,
    0x4370C551u, 0xB11B4652u, 0x65D122B9u, 0x97BAA1BAu, 0x84EA524Eu, 0x7681D14Du,
    0x2892ED69u, 0xDAF96E6Au, 0xC9A99D9Eu, 0x3BC21E9Du, 0xEF087A76u, 0x1D63F975u,
    0x0E330A81u, 0xFC588982u, 0xB21572C9u, 0x407EF1CAu, 0x532E023Eu, 0xA145813Du,
    0x758FE5D6u, 0x87E466D5u, 0x94B49521u, 0x66DF1622u, 0x38CC2A06u, 0xCAA7A905u,
    0xD9F75AF1u, 0x2B9CD9F2u, 0xFF56BD19u, 0x0D3D3E1Au, 0x1E6DCDEEu, 0xEC064EEDu,
    0xC38D26C4u, 0x31E6A5C7u, 0x22B65633u, 0xD0DDD530u, 0x0417B1DBu, 0xF67C32D8u,
    0xE52CC12Cu, 0x1747422Fu, 0x49547E0Bu, 0xBB3FFD08u, 0xA86F0EFCu, 0x5A048DFFu,
    0x8ECEE914u, 0x7CA56A17u, 0x6FF599E3u, 0x9D9E1AE0u, 0xD3D3E1ABu, 0x21B862A8u,
    0x32E8915Cu, 0xC083125Fu, 0x144976B4u, 0xE622F5B7u, 0xF5720643u, 0x07198540u,
    0x590AB964u, 0xAB613A67u, 0xB831C993u, 0x4A5A4A90u, 0x9E902E7Bu, 0x6CFBAD78u,
    0x7FAB5E8Cu, 0x8DC0DD8Fu, 0xE330A81Au, 0x115B2B19u, 0x020BD8EDu, 0xF0605BEEu,
    0x24AA3F05u, 0xD6C1BC06u, 0xC5914FF2u, 0x37FACCF1u, 0x69E9F0D5u, 0x9B8273D6u,
    0x88D28022u, 0x7AB90321u, 0xAE7367CAu, 0x5C18E4C9u, 0x4F48173Du, 0xBD23943Eu,
    0xF36E6F75u, 0x0105EC76u, 0x12551F82u, 0xE03E9C81u, 0x34F4F86Au, 0xC69F7B69u,
    0xD5CF889Du, 0x27A40B9Eu, 0x79B737BAu, 0x8BDCB4B9u, 0x988C474Du, 0x6AE7C44Eu,
    0xBE2DA0A5u, 0x4C4623A6u, 0x5F16D052u, 0xAD7D5351u,};

uint32_t sdos_crc32c(uint32_t crc, const uint8_t *data, size_t length)
{
    size_t i;
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (i = 0u; i < length; i++) {
        c = crc32c_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

const char *sdos_status_name(uint32_t status)
{
    static const char *const names[SDOS_STATUS_COUNT] = {
        "ok", "bad magic", "unsupported version", "unknown type", "bad length", "bad field",
        "bad descriptor", "checksum mismatch", "unknown algorithm", "busy", "bad buffer state",
        "internal error", "timeout", "transport error",
    };
    return status < (uint32_t)SDOS_STATUS_COUNT ? names[status] : "unknown status";
}
