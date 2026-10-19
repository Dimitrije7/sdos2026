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

static sdos_status fi_write_code(const sdos_run_args *args, uint32_t *output_length)
{
    volatile uint32_t *code = (volatile uint32_t *)(void *)zone_code_start;
    (void)args;
    *code = *code; /* same value: harmless if the protection failed */
    return no_output(output_length);
}

static sdos_status fi_write_l2_code(const sdos_run_args *args, uint32_t *output_length)
{
    volatile uint32_t *code = (volatile uint32_t *)(void *)zone_l2code_start;
    (void)args;
    if (zone_l2code_end == zone_l2code_start) {
        return SDOS_E_ALGORITHM; /* this layout runs nothing from L2 */
    }
    sdos_cache_writeback((void *)zone_l2code_start, 64u); /* the write must reach L2 */
    *code = *code;
    sdos_cache_writeback((void *)zone_l2code_start, 64u);
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
static sdos_algorithm algorithms[BUILTINS + 11u] = {
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
