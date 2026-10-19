/*
 * Resource table: tells Linux remoteproc to create one RPMsg device with two
 * 256-buffer vrings, and where the trace buffer is.
 *
 * TI's IPC library reads this table through its own Ipc_ResourceTable type:
 * it uses the header and the vdev, which come first. On the C66x the table is
 * that type. TI's C71x build of the type ends with a version-1 trace entry
 * with a 64-bit address, which the course kernel ignores ("unsupported
 * resource 65538", Task 021), leaving the C71x without trace or health line;
 * the C71x table therefore ends with the standard entry, whose 32-bit address
 * holds the trace buffer in the carve-out. ci/firmware_elf.py checks the result
 * in every build.
 */
#include <stddef.h>

#include <ti/drv/ipc/ipc.h> /* brings the CSL types ipc_rsctypes.h relies on */

#include "sdos/platform.h"

#define VRING_BUFFERS 256u
#define FEATURES_NAME_SERVICE 1u /* VIRTIO_RPMSG_F_NS: announce endpoints */

#if defined(BUILD_C7X)
/* Linux include/linux/remoteproc.h: struct fw_rsc_trace after its type word. */
typedef struct sdos_trace_entry {
    uint32_t type;
    uint32_t da;
    uint32_t len;
    uint32_t reserved;
    uint8_t name[32];
} sdos_trace_entry;

typedef struct sdos_resource_table_t {
    Ipc_Hdr base;
    uint32_t offset[NUM_ENTRIES];
    Ipc_VDev rpmsg_vdev;
    Ipc_VDevVRing rpmsg_vring0;
    Ipc_VDevVRing rpmsg_vring1;
    sdos_trace_entry trace;
} sdos_resource_table_t;

/* The library reads the vdev at its own type's offset. */
_Static_assert(offsetof(sdos_resource_table_t, rpmsg_vdev) == offsetof(Ipc_ResourceTable, rpmsg_vdev),
               "the vdev must sit where TI's Ipc_ResourceTable has it");

/* The trace buffer's 64-bit address fits the entry's 32 bits: it lies in the
 * carve-out below 4 GiB, which ci/firmware_elf.py checks. */
#pragma diag_push
#pragma diag_suppress 70
const sdos_resource_table_t sdos_resource_table __attribute__((section(".resource_table"), aligned(4096))) = {
    {1u, NUM_ENTRIES, {0u, 0u}},
    {offsetof(sdos_resource_table_t, rpmsg_vdev), offsetof(sdos_resource_table_t, trace)},
    {TYPE_VDEV, VIRTIO_ID_RPMSG, 0u, FEATURES_NAME_SERVICE, 0u, 0u, 0u, 2u, {0u, 0u}},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 1u, 0u},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 2u, 0u},
    {TYPE_TRACE, (uint32_t)(uintptr_t)sdos_trace_buffer, SDOS_TRACE_SIZE, 0u, "trace:sdos"},
};
#pragma diag_pop
#else
const Ipc_ResourceTable sdos_resource_table __attribute__((section(".resource_table"), aligned(4096))) = {
    {1u, NUM_ENTRIES, {0u, 0u}},
    {offsetof(Ipc_ResourceTable, rpmsg_vdev), offsetof(Ipc_ResourceTable, trace)},
    {TYPE_VDEV, VIRTIO_ID_RPMSG, 0u, FEATURES_NAME_SERVICE, 0u, 0u, 0u, 2u, {0u, 0u}},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 1u, 0u},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 2u, 0u},
    {TRACE_INTS_VER0 | TYPE_TRACE, (uintptr_t)sdos_trace_buffer, SDOS_TRACE_SIZE, 0u, "trace:sdos"},
};
#endif

const void *const sdos_resource_table_address = &sdos_resource_table;
