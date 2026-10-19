/*
 * IPC service: bring up RPMsg with Linux, announce the course endpoint, and
 * answer protocol requests with sdos_runtime_handle.
 *
 * The bring-up follows TI's PDK echo examples (ipc_testsetup.c, BSD-3-Clause),
 * with one deliberate difference: this firmware talks only to Linux on the
 * A72 (IPC_MPU1_0). It never waits for R5F cores, which may run no firmware
 * on a BeagleBone AI-64, and it drops messages from any other processor
 * (docs/safety.md, communication interference). Both runtimes link TI's
 * bare-metal IPC library and poll the mailbox (mailbox_poll.c): no interrupt
 * route, no IPC control task.
 */
#include <string.h>

#include <ti/drv/ipc/ipc.h>
#include <ti/osal/HwiP.h>
#include <ti/osal/TaskP.h>

#include "sdos/platform.h"
#include "sdos/protocol.h"

/* RPMessage buffers: 256 messages of 496 + 32 bytes plus the object. */
#define RPMSG_BUFFER_SIZE (256u * (496u + 32u) + 256u)
#define VQ_BUFFER_SIZE 2048u
#define VRING_BASE 0xAA000000u /* unused with Linux: vrings come from the resource table */

static uint8_t control_buffer[RPMSG_BUFFER_SIZE] __attribute__((section("ipc_data_buffer"), aligned(8)));
static uint8_t endpoint_buffer[RPMSG_BUFFER_SIZE] __attribute__((section("ipc_data_buffer"), aligned(8)));
static uint8_t vq_buffer[VQ_BUFFER_SIZE] __attribute__((section("ipc_data_buffer"), aligned(8)));

static uint32_t remote_procs[1] = {IPC_MPU1_0};
static RPMessage_Handle endpoint;
static uint32_t local_endpoint;
static volatile uint32_t shutdown_requested;
static volatile uint32_t messages_arrived;

static void ipc_print(const char *text)
{
    sdos_trace("ipc: %s", text);
}

/* Mailbox messages from Linux outside RPMsg, such as shutdown before stop. */
static void mailbox_message(uint32_t remote, uint32_t value)
{
    if (remote == IPC_MPU1_0 && value == IPC_RP_MBOX_SHUTDOWN) {
        shutdown_requested = 1u;
        if (endpoint != NULL) {
            RPMessage_unblock(endpoint);
        }
    }
}

void sdos_mailbox_poll(void); /* mailbox_poll.c */
/* TI's ipc_mailbox.h, private to the library: send one mailbox word. */
int32_t Ipc_mailboxSend(uint32_t selfId, uint32_t remoteProcId, uint32_t val, uint32_t timeoutCnt);

/* Called by the IPC library when a message is queued for an endpoint. */
static void message_arrived(uint32_t source_endpoint, uint32_t source_proc)
{
    (void)source_endpoint;
    (void)source_proc;
    messages_arrived++;
}

/* Between two polls: bare metal runs the monitor, FreeRTOS gives the CPU away
 * for one tick (at most 1 ms of added latency; the monitor is a task). */
static void idle(void)
{
#if defined(BAREMETAL)
    sdos_monitor_poll();
    sdos_wait_for_event();
#else
    TaskP_sleep(1u);
#endif
}

static int bring_up(void)
{
    Ipc_InitPrms init;
    Ipc_VirtIoParams vq;
    RPMessage_Params params;

    if (Ipc_mpSetConfig(SDOS_IPC_SELF, 1u, remote_procs) != IPC_SOK) {
        sdos_trace("Ipc_mpSetConfig failed");
        return -1;
    }
    IpcInitPrms_init(0u, &init);
    init.printFxn = ipc_print;
    init.rpMboxMsgFxn = mailbox_message;
    init.newMsgFxn = message_arrived;
    if (Ipc_init(&init) != IPC_SOK) {
        sdos_trace("Ipc_init failed");
        return -1;
    }
    (void)Ipc_loadResourceTable((void *)sdos_resource_table_address);
    sdos_trace("waiting for Linux to set up the vrings");
    while (Ipc_isRemoteReady(IPC_MPU1_0) == 0u) {
        sdos_mailbox_poll();
        idle();
    }
    vq.vqObjBaseAddr = vq_buffer;
    vq.vqBufSize = Ipc_getVqObjMemoryRequiredPerCore();
    vq.vringBaseAddr = (void *)VRING_BASE;
    vq.vringBufSize = IPC_VRING_BUFFER_SIZE;
    vq.timeoutCnt = 100u;
    if (Ipc_initVirtIO(&vq) != IPC_SOK) {
        sdos_trace("Ipc_initVirtIO failed");
        return -1;
    }
    (void)RPMessageParams_init(&params);
    params.buf = control_buffer;
    params.bufSize = sizeof control_buffer;
    params.stackBuffer = NULL; /* the bare-metal library runs no control task */
    params.stackSize = 0u;
    if (RPMessage_init(&params) != IPC_SOK) {
        sdos_trace("RPMessage_init failed");
        return -1;
    }
#if defined(BAREMETAL) && !defined(BUILD_C7X)
    /* Polling alone serves the mailbox; an interrupt the OSAL may have
     * enabled must not race it. Not on the C71x: its only interrupt is the
     * wake tick (sdos_wait_for_event), and switching it off here, after the
     * vring wait had started it, left the core idling for good (Task 042). */
    (void)HwiP_disable();
#endif
    (void)RPMessageParams_init(&params);
    params.requestedEndpt = SDOS_RPMSG_ENDPOINT;
    params.buf = endpoint_buffer;
    params.bufSize = sizeof endpoint_buffer;
    endpoint = RPMessage_create(&params, &local_endpoint);
    if (endpoint == NULL) {
        sdos_trace("RPMessage_create(%u) failed", (unsigned)SDOS_RPMSG_ENDPOINT);
        return -1;
    }
    if (RPMessage_announce(IPC_MPU1_0, local_endpoint, SDOS_RPMSG_SERVICE) != IPC_SOK) {
        sdos_trace("RPMessage_announce failed");
        return -1;
    }
    sdos_trace("serving protocol %u.%u on endpoint %u as %s", (unsigned)SDOS_PROTO_MAJOR,
               (unsigned)SDOS_PROTO_MINOR, (unsigned)local_endpoint, SDOS_RPMSG_SERVICE);
    return 0;
}

void sdos_service_run(sdos_runtime *runtime)
{
    static uint8_t request[SDOS_MSG_MAX];
    static uint8_t response[SDOS_MSG_MAX];

    if (bring_up() != 0) {
        sdos_runtime_fault(runtime, SDOS_FAULT_IPC, 0u, 2u);
        return;
    }
    /* Start-up is over: protection is complete and RUN is accepted. */
    sdos_monitor_started();
    while (shutdown_requested == 0u) {
        uint16_t length = (uint16_t)sizeof request;
        uint32_t remote_endpoint = 0u;
        uint32_t remote_proc = 0u;
        int32_t status;
        size_t n;
        /* Drain every queued message before polling the mailbox again. */
        status = RPMessage_recvNb(endpoint, request, &length, &remote_endpoint, &remote_proc);
        if (status != IPC_SOK) {
            uint32_t before = messages_arrived;
            sdos_mailbox_poll();
            if (messages_arrived == before) {
                idle();
            }
            continue;
        }
        if (remote_proc != IPC_MPU1_0) {
            runtime->stats.foreign_messages++; /* only Linux on the A72 may send requests */
            continue;
        }
        n = sdos_runtime_handle(runtime, request, length, response, sizeof response);
        if (n > 0u && RPMessage_send(endpoint, remote_proc, remote_endpoint, local_endpoint, response,
                                     (uint16_t)n) != IPC_SOK) {
            sdos_trace("reply to request failed");
        }
    }
    /* TI's graceful shutdown (ipc_testsetup.c): stop taking interrupts, leave
     * no tick running behind (the timer outlives the core), acknowledge, and
     * idle so the device manager can power the core off. */
    sdos_trace("shutdown requested by Linux; acknowledging and going idle");
    (void)HwiP_disable();
    sdos_tick_timer_reset();
    (void)Ipc_mailboxSend(SDOS_IPC_SELF, IPC_MPU1_0, IPC_RP_MBOX_SHUTDOWN_ACK, 1u);
    sdos_idle_forever();
}

void sdos_identity_init(sdos_identity *identity, uint8_t runtime_kind)
{
    memset(identity, 0, sizeof *identity);
    identity->core = SDOS_CORE_ID;
    identity->runtime = runtime_kind;
    identity->capabilities = SDOS_CAP_ECHO | SDOS_CAP_STATUS | SDOS_CAP_RUN | SDOS_CAP_CHECKSUM | SDOS_CAP_CYCLES;
    identity->max_payload = SDOS_PAYLOAD_MAX;
    identity->buffer_alignment = 128u;
    identity->runtime_version = SDOS_VERSION_CODE;
    identity->region_size = 0u; /* shared buffers follow the Task 027 decision */
    (void)strncpy(identity->firmware, SDOS_FIRMWARE_NAME, SDOS_FIRMWARE_NAME_MAX - 1u);
}
