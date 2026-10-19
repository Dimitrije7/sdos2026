/*
 * Fault injection, for the connected-board suite only (docs/safety.md).
 *
 * Built only with -DSDOS_FAULT_INJECTION=ON (the *-fi presets), never into
 * the course image. Each algorithm commits one fault that a safety mechanism
 * must catch; tests/target/sdos_board/fault_injection.py runs them and checks
 * the fault record and the recovery. Targets are chosen to be harmless if a
 * mechanism fails: a read of Linux memory, a write into the other C66x's
 * carve-out while that core is stopped, and writes that restore the value
 * they overwrite.
 */
#include <string.h>

#define SDOS_CORES_MACROS_ONLY
#include "sdos/cores.h"
#include "sdos/fault_injection.h"
#include "sdos/platform.h"

#include <ti/csl/csl_types.h>
#include <ti/drv/sciclient/sciclient.h>

#if !defined(BAREMETAL)
#include "FreeRTOS.h"
#include "portable.h"
#endif

#if defined(BUILD_C66X_1) || defined(BUILD_C7X)
#define FOREIGN_DDR SDOS_C66X_1_DDR_BASE
#else
#define FOREIGN_DDR SDOS_C66X_0_DDR_BASE
#endif
#define LINUX_DDR 0x80000000u /* start of DDR: Linux's memory on the BeagleBone AI-64 */
#define MSMC 0x70020000u      /* shared on-chip SRAM, past the part ATF and the DMSC hold */

extern char sdos_zone_code_start[];
extern char sdos_zone_l2code_start[];
extern char sdos_zone_l2code_end[];
static char *const zone_code_start = sdos_zone_code_start;
static char *const zone_l2code_start = sdos_zone_l2code_start;
static char *const zone_l2code_end = sdos_zone_l2code_end;

/* Read-only data in writable memory, covered by the integrity record: the
 * one place an image change can be injected past the write protection. */
static const uint8_t fi_target[64] __attribute__((section(".sdos.fi_target"))) = {
    0x53u, 0x44u, 0x4Fu, 0x53u, 0x2Du, 0x46u, 0x49u, 0x00u};
static uint32_t fi_code[16]; /* in the data zone, which is never executable */

static sdos_status no_output(uint32_t *output_length)
{
    *output_length = 0u;
    return SDOS_OK;
}

static sdos_status fi_write_foreign(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    *(volatile uint32_t *)(uintptr_t)FOREIGN_DDR = 0x5D05F1u;
    return no_output(output_length);
}

static sdos_status fi_read_foreign(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    (void)*(volatile uint32_t *)(uintptr_t)LINUX_DDR;
    return no_output(output_length);
}

/* Write `value` back over `address` so the write itself reaches the memory
 * controller. Reading the word pulls its line into the cache, and the C66x
 * checks the CPU's own access, not a later cache writeback: so the line leaves
 * the cache before the write (board, Task 042), as in the start-up self-test. */
static void write_through(volatile uint32_t *address)
{
    uint32_t value = *address;
    sdos_cache_writeback((void *)(uintptr_t)address, 64u);
    *address = value; /* same value: harmless if the protection failed */
    sdos_cache_writeback((void *)(uintptr_t)address, 64u);
}

static sdos_status fi_write_code(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    write_through((volatile uint32_t *)(void *)zone_code_start);
    return no_output(output_length);
}

static sdos_status fi_write_l2_code(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    if (zone_l2code_end == zone_l2code_start) {
        return SDOS_E_ALGORITHM; /* this layout runs nothing from L2 */
    }
    write_through((volatile uint32_t *)(void *)zone_l2code_start);
    return no_output(output_length);
}

static sdos_status fi_read_shared(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    (void)*(volatile uint32_t *)(uintptr_t)MSMC;
    return no_output(output_length);
}

static sdos_status fi_exec_data(const sdos_run_args *args, uint32_t *output_length)
{
    void (*jump)(void) = (void (*)(void))(uintptr_t)fi_code;
    (void)args;
    jump();
    return no_output(output_length);
}

/* Recurse until the frames reach 64 bytes above the stack's base: through the
 * C71x guard page (a fault), or into the C66x guard band (a monitor finding). */
static uint32_t dive(uintptr_t floor, uint32_t depth)
{
    volatile uint8_t frame[256];
    frame[0] = (uint8_t)depth;
    if ((uintptr_t)frame <= floor) {
        return depth;
    }
    return dive(floor, depth + 1u) + frame[0];
}

static sdos_status fi_stack(const sdos_run_args *args, uint32_t *output_length)
{
    uint8_t here = 0u;
    const sdos_stack *stack = sdos_monitor_stack_containing(&here);
    (void)args;
    if (stack == NULL) {
        return SDOS_E_INTERNAL;
    }
    (void)dive((uintptr_t)stack->base + 64u, 0u);
    return no_output(output_length);
}

static sdos_status fi_hang(const sdos_run_args *args, uint32_t *output_length)
{
    volatile int forever = 1;
    (void)args;
    while (forever != 0) {
    }
    return no_output(output_length);
}

/* params: uint32 microseconds to keep busy. */
static sdos_status fi_overrun(const sdos_run_args *args, uint32_t *output_length)
{
    uint32_t us;
    uint64_t end;
    if (args->params_length != 4u) {
        return SDOS_E_FIELD;
    }
    us = (uint32_t)args->params[0] | ((uint32_t)args->params[1] << 8) | ((uint32_t)args->params[2] << 16) |
         ((uint32_t)args->params[3] << 24);
    end = sdos_cycles() + (uint64_t)us * sdos_cycles_per_us();
    while (sdos_cycles() < end) {
    }
    return no_output(output_length);
}

static sdos_status fi_corrupt_image(const sdos_run_args *args, uint32_t *output_length)
{
    volatile uint8_t *target = (volatile uint8_t *)(uintptr_t)fi_target;
    (void)args;
    target[3] ^= 0x01u;
    sdos_cache_writeback((void *)(uintptr_t)fi_target, sizeof fi_target);
    return no_output(output_length);
}

/* Prevented: OK. Taken effect: an error, and the monitor's readback check
 * must record the change at its next period. */
static sdos_status fi_rewrite_protection(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
    if (sdos_protection_tamper() != 0) {
        return SDOS_E_INTERNAL;
    }
    return no_output(output_length);
}

/* Diagnostic, injects nothing: read through TI-SCI the regions of every
 * firewall on a path into the DSP DDR carve-outs and DSP L2, and trace the
 * enabled ones (or the refusal: this host may not read that firewall).
 * Read-only. J721E ids, region and privilege-id counts from TI's
 * csl_soc_firewalls.h. */
static sdos_status fi_firewall_survey(const sdos_run_args *args, uint32_t *output_length)
{
    static const struct {
        uint16_t id;
        uint8_t regions;
        uint8_t priv_ids;
        const char *path;
    } firewalls[] = {
        {257u, 16u, 1u, "A72 to MSMC and DDR (master)"},
        {265u, 8u, 1u, "C71x to MSMC and DDR (master)"},
        {284u, 16u, 3u, "C71x DRU to MSMC and DDR (master)"},
        {4762u, 24u, 3u, "NAVSS to DDR0 (main-domain masters)"},
        {4763u, 24u, 3u, "NAVSS to DDR1 (main-domain masters)"},
        {1280u, 8u, 3u, "MCU domain to main and DDR"},
        {4760u, 24u, 3u, "NAVSS to MSMC SRAM"},
        {264u, 2u, 1u, "C71x L2 SRAM"},
        {2304u, 8u, 3u, "C66x_0 L2 and L1D SRAM"},
        {2305u, 8u, 3u, "C66x_1 L2 and L1D SRAM"},
    };
    size_t f;
    (void)args;
    for (f = 0u; f < sizeof firewalls / sizeof firewalls[0]; f++) {
        uint16_t region;
        unsigned enabled = 0u;
        int32_t status = CSL_PASS;
        for (region = 0u; region < firewalls[f].regions; region++) {
            struct tisci_msg_fwl_get_firewall_region_req req;
            struct tisci_msg_fwl_get_firewall_region_resp resp;
            (void)memset(&req, 0, sizeof req);
            (void)memset(&resp, 0, sizeof resp);
            req.fwl_id = firewalls[f].id;
            req.region = region;
            req.n_permission_regs = firewalls[f].priv_ids;
            status = Sciclient_firewallGetRegion(&req, &resp, SCICLIENT_SERVICE_WAIT_FOREVER);
            if (status != CSL_PASS) {
                break;
            }
            if ((resp.control & 0xFu) == 0u) {
                continue; /* region disabled */
            }
            enabled++;
            sdos_trace("firewall %u region %u: control %08lx perm %08lx %08lx %08lx, %08lx%08lx..%08lx%08lx",
                       (unsigned)req.fwl_id, (unsigned)region, (unsigned long)resp.control,
                       (unsigned long)resp.permissions[0], (unsigned long)resp.permissions[1],
                       (unsigned long)resp.permissions[2], (unsigned long)(uint32_t)(resp.start_address >> 32),
                       (unsigned long)(uint32_t)resp.start_address, (unsigned long)(uint32_t)(resp.end_address >> 32),
                       (unsigned long)(uint32_t)resp.end_address);
        }
        if (status != CSL_PASS) {
            sdos_trace("firewall %u (%s): region %u refused (%ld), %u enabled before it", (unsigned)firewalls[f].id,
                       firewalls[f].path, (unsigned)region, (long)status, enabled);
        } else {
            sdos_trace("firewall %u (%s): %u of %u regions enabled", (unsigned)firewalls[f].id, firewalls[f].path,
                       enabled, (unsigned)firewalls[f].regions);
        }
    }
    return no_output(output_length);
}

static sdos_status fi_heap(const sdos_run_args *args, uint32_t *output_length)
{
    (void)args;
#if defined(BAREMETAL)
    (void)output_length;
    return SDOS_E_ALGORITHM; /* no kernel heap in use on bare metal */
#else
    return pvPortMalloc(64u) != NULL ? no_output(output_length) : SDOS_E_INTERNAL;
#endif
}

#define BUILTINS 3u
static sdos_algorithm algorithms[BUILTINS + 13u] = {
    {0u, "", NULL}, {0u, "", NULL}, {0u, "", NULL},
    {SDOS_ALG_FI_WRITE_FOREIGN, "fi_write_foreign", fi_write_foreign},
    {SDOS_ALG_FI_READ_FOREIGN, "fi_read_foreign", fi_read_foreign},
    {SDOS_ALG_FI_WRITE_CODE, "fi_write_code", fi_write_code},
    {SDOS_ALG_FI_EXEC_DATA, "fi_exec_data", fi_exec_data},
    {SDOS_ALG_FI_STACK, "fi_stack", fi_stack},
    {SDOS_ALG_FI_HANG, "fi_hang", fi_hang},
    {SDOS_ALG_FI_OVERRUN, "fi_overrun", fi_overrun},
    {SDOS_ALG_FI_CORRUPT_IMAGE, "fi_corrupt_image", fi_corrupt_image},
    {SDOS_ALG_FI_HEAP, "fi_heap", fi_heap},
    {SDOS_ALG_FI_WRITE_L2_CODE, "fi_write_l2_code", fi_write_l2_code},
    {SDOS_ALG_FI_READ_SHARED, "fi_read_shared", fi_read_shared},
    {SDOS_ALG_FI_REWRITE_PROTECTION, "fi_rewrite_protection", fi_rewrite_protection},
    {SDOS_ALG_FI_FIREWALL_SURVEY, "fi_firewall_survey", fi_firewall_survey},
};

void sdos_fault_injection_install(sdos_runtime *runtime)
{
    size_t i;
    for (i = 0u; i < BUILTINS && i < sdos_builtin_algorithm_count; i++) {
        algorithms[i] = sdos_builtin_algorithms[i];
    }
    runtime->algorithms = algorithms;
    runtime->algorithm_count = sizeof algorithms / sizeof algorithms[0];
    runtime->identity.capabilities |= SDOS_CAP_FAULT_INJECTION;
    sdos_trace("FAULT INJECTION BUILD: for the connected-board suite only, never for course use");
}
