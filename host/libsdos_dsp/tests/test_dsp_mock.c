/* libsdos_dsp against the mock backend: lifecycle, messages, buffers, faults. */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"
#include "sdos/dsp.h"
#include "sdos/runtime.h"
#include "sdos_elf_fixture.h"
#include "sdos_test.h"

static sdos_dsp *open_mock(const char *name, const char *fault)
{
    sdos_dsp_options options = {"mock", NULL, 200u};
    sdos_dsp *dsp = NULL;
    if (fault != NULL) {
        (void)setenv("SDOS_MOCK_FAULT", fault, 1);
    } else {
        (void)unsetenv("SDOS_MOCK_FAULT");
    }
    if (sdos_dsp_open(name, &options, &dsp) != SDOS_OK) {
        return NULL;
    }
    return dsp;
}

static void test_names_and_open(void)
{
    sdos_dsp *dsp = NULL;
    sdos_dsp_options options = {"mock", NULL, 0u};
    CHECK_EQ("three DSPs", sdos_dsp_count(), 3);
    CHECK("manifest order", strcmp(sdos_dsp_name(0u), "c66x_0") == 0 && strcmp(sdos_dsp_name(2u), "c71x_0") == 0 &&
                                sdos_dsp_name(3u) == NULL);
    CHECK_EQ("remoteproc numbers are not names", sdos_dsp_open("remoteproc0", &options, &dsp), SDOS_E_FIELD);
    options.backend = "serial";
    CHECK_EQ("unknown backend", sdos_dsp_open("c66x_0", &options, &dsp), SDOS_E_FIELD);
}

static void test_messages(void)
{
    sdos_dsp *dsp = open_mock("c71x_0", NULL);
    sdos_identity id;
    sdos_stats stats;
    sdos_dsp_info info;
    char reply[SDOS_PAYLOAD_MAX];
    size_t n = 0u;

    CHECK("mock opens", dsp != NULL);
    CHECK_EQ("info", sdos_dsp_info_get(dsp, &info), SDOS_OK);
    CHECK("a mock DSP starts running", info.state == SDOS_DSP_RUNNING && strcmp(info.device, "64800000.dsp") == 0);
    CHECK_EQ("hello", sdos_dsp_hello(dsp, &id), SDOS_OK);
    CHECK("identity names the core", id.core == 2u && strcmp(id.firmware, "mock c71x_0") == 0);
    CHECK_EQ("echo", sdos_dsp_echo(dsp, "abc", 3u, reply, sizeof reply, &n), SDOS_OK);
    CHECK("echo reply", n == 3u && memcmp(reply, "abc", 3u) == 0);
    CHECK_EQ("echo larger than one message", sdos_dsp_echo(dsp, reply, SDOS_PAYLOAD_MAX + 1u, reply, sizeof reply, &n),
             SDOS_E_LENGTH);
    CHECK_EQ("status", sdos_dsp_status(dsp, &stats), SDOS_OK);
    CHECK_EQ("status counts this session", stats.requests, 3);
    sdos_dsp_close(dsp);
}

static void test_buffers_and_run(void)
{
    sdos_dsp *dsp = open_mock("c66x_1", NULL);
    sdos_buffer in;
    sdos_buffer out;
    sdos_buffer overlap;
    sdos_run_result result;
    const uint8_t gain[2] = {0x00, 0x40};
    uint32_t i;

    CHECK_EQ("acquire input", sdos_dsp_buffer_acquire(dsp, 0u, 4096u, &in), SDOS_OK);
    CHECK_EQ("acquire output", sdos_dsp_buffer_acquire(dsp, 4096u, 4096u, &out), SDOS_OK);
    CHECK_EQ("misaligned range is refused", sdos_dsp_buffer_acquire(dsp, 100u, 16u, &overlap), SDOS_E_DESCRIPTOR);
    CHECK_EQ("range past the region is refused", sdos_dsp_buffer_acquire(dsp, 1024u * 1024u - 128u, 256u, &overlap),
             SDOS_E_DESCRIPTOR);
    for (i = 0u; i < 4096u; i += 2u) {
        in.data[i] = (uint8_t)(i & 0xFFu);
        in.data[i + 1u] = 0x10u;
    }
    CHECK_EQ("gain run", sdos_dsp_run(dsp, SDOS_ALG_GAIN_Q15, &in, &out, gain, 2u, &result), SDOS_OK);
    CHECK("half gain on the first sample", out.data[0] == 0x00u && out.data[1] == 0x08u);
    CHECK("both buffers are back with the host", in.state == SDOS_BUF_HOST && out.state == SDOS_BUF_HOST);
    CHECK_EQ("all output returned", result.output_length, 4096);

    CHECK_EQ("overlapping output", sdos_dsp_buffer_acquire(dsp, 2048u, 4096u, &overlap), SDOS_OK);
    CHECK_EQ("DSP refuses overlap", sdos_dsp_run(dsp, SDOS_ALG_COPY, &in, &overlap, NULL, 0u, &result),
             SDOS_E_DESCRIPTOR);
    CHECK("refused buffers return to the host", in.state == SDOS_BUF_HOST && overlap.state == SDOS_BUF_HOST);
    CHECK_EQ("unknown algorithm", sdos_dsp_run(dsp, 77u, &in, &out, NULL, 0u, &result), SDOS_E_ALGORITHM);
    CHECK_EQ("release", sdos_dsp_buffer_release(dsp, &in), SDOS_OK);
    CHECK_EQ("a released buffer cannot run", sdos_dsp_run(dsp, SDOS_ALG_COPY, &in, &out, NULL, 0u, &result),
             SDOS_E_STATE);
    sdos_dsp_close(dsp);
}

static void test_timeout_quarantines_until_restart(void)
{
    sdos_dsp *dsp = open_mock("c66x_0", "drop");
    sdos_buffer in;
    sdos_buffer out;
    sdos_run_result result;
    sdos_identity id;

    (void)sdos_dsp_buffer_acquire(dsp, 0u, 256u, &in);
    (void)sdos_dsp_buffer_acquire(dsp, 256u, 256u, &out);
    CHECK_EQ("a lost response times out", sdos_dsp_run(dsp, SDOS_ALG_COPY, &in, &out, NULL, 0u, &result),
             SDOS_E_TIMEOUT);
    CHECK("timed-out buffers are quarantined", in.state == SDOS_BUF_QUARANTINED && out.state == SDOS_BUF_QUARANTINED);
    CHECK_EQ("quarantined buffers cannot run", sdos_dsp_run(dsp, SDOS_ALG_COPY, &in, &out, NULL, 0u, &result),
             SDOS_E_STATE);
    CHECK_EQ("recover restarts the DSP", sdos_dsp_recover(dsp, &id), SDOS_OK);
    CHECK_EQ("after a restart the buffer is free and can be released", sdos_dsp_buffer_release(dsp, &in), SDOS_E_STATE);
    CHECK("restart freed both buffers", in.state == SDOS_BUF_FREE);
    sdos_dsp_close(dsp);
}

static void test_corruption_and_stale_replies(void)
{
    sdos_dsp *dsp = open_mock("c66x_0", "corrupt");
    sdos_buffer in;
    sdos_buffer out;
    sdos_run_result result;
    sdos_stats stats;
    char reply[16];
    size_t n = 0u;

    (void)sdos_dsp_buffer_acquire(dsp, 0u, 256u, &in);
    (void)sdos_dsp_buffer_acquire(dsp, 256u, 256u, &out);
    CHECK_EQ("output changed after the DSP checksummed it",
             sdos_dsp_run(dsp, SDOS_ALG_COPY, &in, &out, NULL, 0u, &result), SDOS_E_CHECKSUM);
    sdos_dsp_close(dsp);

    dsp = open_mock("c66x_0", "stale");
    CHECK_EQ("first echo", sdos_dsp_echo(dsp, "a", 1u, reply, sizeof reply, &n), SDOS_OK);
    CHECK_EQ("a duplicate reply is skipped", sdos_dsp_status(dsp, &stats), SDOS_OK);
    CHECK_EQ("the stale reply was counted", dsp->stale_responses, 1);
    sdos_dsp_close(dsp);
}

static void test_load_checks_the_elf(void)
{
    char good[] = "/tmp/sdos-good-XXXXXX";
    char bad[] = "/tmp/sdos-bad-XXXXXX";
    sdos_dsp *dsp = open_mock("c66x_0", NULL);
    sdos_dsp_info info;
    int fd_good = mkstemp(good);
    int fd_bad = mkstemp(bad);
    (void)close(fd_good);
    (void)close(fd_bad);
    (void)sdos_write_elf(good, 140u, 0xA6100000u, 0x1000u);
    (void)sdos_write_elf(bad, 140u, 0xA7100000u, 0x1000u);

    CHECK_EQ("load needs a stopped DSP", sdos_dsp_load(dsp, good), SDOS_E_STATE);
    CHECK_EQ("stop", sdos_dsp_stop(dsp), SDOS_OK);
    CHECK_EQ("firmware for another core is refused", sdos_dsp_load(dsp, bad), SDOS_E_DESCRIPTOR);
    CHECK("the refusal says why", strstr(sdos_dsp_error(dsp), "outside the c66x_0 carve-out") != NULL);
    CHECK_EQ("fitting firmware loads", sdos_dsp_load(dsp, good), SDOS_OK);
    (void)sdos_dsp_info_get(dsp, &info);
    CHECK("firmware name is set", strcmp(info.firmware, "sdos/c66x_0.xe66") == 0);
    (void)sdos_write_elf(bad, 145u, 0xA6100000u, 0x1000u);
    CHECK_EQ("a C7x file on a C66x core is refused", sdos_dsp_load(dsp, bad), SDOS_E_FIELD);
    CHECK_EQ("missing file", sdos_dsp_load(dsp, "/nonexistent.xe66"), SDOS_E_TRANSPORT);
    (void)unlink(good);
    (void)unlink(bad);
    sdos_dsp_close(dsp);
}

int main(void)
{
    test_names_and_open();
    test_messages();
    test_buffers_and_run();
    test_timeout_quarantines_until_restart();
    test_corruption_and_stale_replies();
    test_load_checks_the_elf();
    return TEST_EXIT();
}
