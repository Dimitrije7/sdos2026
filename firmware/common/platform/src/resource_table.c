/*
 * Resource table: tells Linux remoteproc to create one RPMsg device with two
 * 256-buffer vrings, and where the trace buffer is.
 *
 * TI's IPC library reads this table through its own Ipc_ResourceTable type,
 * so the layout is that type. The C71x build of the library declares a 64-bit
 * trace address and expects TI's version-1 trace entry; the C66x uses the
 * standard entry. ci/firmware_elf.py checks the result in every build.
 */
#include <stddef.h>

#include <ti/drv/ipc/ipc.h> /* brings the CSL types ipc_rsctypes.h relies on */

#include "sdos/platform.h"

#define VRING_BUFFERS 256u
#define FEATURES_NAME_SERVICE 1u /* VIRTIO_RPMSG_F_NS: announce endpoints */

#if defined(BUILD_C7X)
#define SDOS_TRACE_TYPE (TRACE_INTS_VER1 | TYPE_TRACE)
#else
#define SDOS_TRACE_TYPE (TRACE_INTS_VER0 | TYPE_TRACE)
#endif

const Ipc_ResourceTable sdos_resource_table __attribute__((section(".resource_table"), aligned(4096))) = {
    {1u, NUM_ENTRIES, {0u, 0u}},
    {offsetof(Ipc_ResourceTable, rpmsg_vdev), offsetof(Ipc_ResourceTable, trace)},
    {TYPE_VDEV, VIRTIO_ID_RPMSG, 0u, FEATURES_NAME_SERVICE, 0u, 0u, 0u, 2u, {0u, 0u}},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 1u, 0u},
    {(uint32_t)FW_RSC_ADDR_ANY, 4096u, VRING_BUFFERS, 2u, 0u},
    {SDOS_TRACE_TYPE, (uintptr_t)sdos_trace_buffer, SDOS_TRACE_SIZE, 0u, "trace:sdos"},
};

const void *const sdos_resource_table_address = &sdos_resource_table;
