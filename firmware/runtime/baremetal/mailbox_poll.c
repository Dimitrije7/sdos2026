/*
 * Mailbox polling for the bare-metal runtime.
 *
 * TI's IPC library normally learns about new messages from a mailbox
 * interrupt. Its bare-metal OSAL cannot register interrupts on the C7x (the
 * HwiP code is a TODO in PDK 11.02), so this runtime polls on every DSP, which
 * keeps C66x and C71x behavior identical. The loop below does what TI's own
 * Mailbox_Poll_Task does for RTOS builds.
 *
 * TI keeps its mailbox bookkeeping private to ipc_mailbox.c. The structures
 * mirror that file in PDK 11.02.01.08 and must be checked when the PDK pin
 * changes; sdos_mailbox_poll refuses counts outside TI's array bounds.
 */
#include <stdint.h>

#include "sdos/platform.h"

#define TI_MBOX_MAXFIFO_CNT 16u /* IPC_MBOX_MAXFIFO_CNT */
#define TI_MBOX_MAXDATA 8u      /* IPC_MBOX_MAXDATA */

typedef void (*ti_mailbox_callback)(uint32_t *message, uint32_t arg);

typedef struct ti_mailbox_fifo {
    int32_t cfgNdx;
    ti_mailbox_callback func;
    uint32_t arg;
    uint32_t queueId;
} ti_mailbox_fifo;

typedef struct ti_mailbox_data {
    uintptr_t baseAddr;
    uint32_t fifoCnt;
    ti_mailbox_fifo fifoTable[TI_MBOX_MAXFIFO_CNT];
    uint32_t noMsgCnt;
    uint32_t intCnt;
    uint32_t userId;
} ti_mailbox_data;

extern ti_mailbox_data g_ipc_mBoxData[TI_MBOX_MAXDATA];
extern uint32_t g_ipc_mBoxCnt;

uint32_t Mailbox_getMessageCount(uintptr_t baseAddr, uint32_t queueId);
uint32_t Mailbox_getMessage(uintptr_t baseAddr, uint32_t queueId, uint32_t *msgPtr);
void Mailbox_clrNewMsgStatus(uintptr_t baseAddr, uint32_t userId, uint32_t queueId);

void sdos_mailbox_poll(void);

void sdos_mailbox_poll(void)
{
    static int reported;
    uint32_t n;
    uint32_t q;
    if (g_ipc_mBoxCnt > TI_MBOX_MAXDATA) {
        if (!reported) {
            sdos_trace("mailbox poll: %u mailboxes exceed TI's %u; PDK layout changed?", (unsigned)g_ipc_mBoxCnt,
                       (unsigned)TI_MBOX_MAXDATA);
            reported = 1;
        }
        return;
    }
    for (n = 0u; n < g_ipc_mBoxCnt; n++) {
        ti_mailbox_data *mbox = &g_ipc_mBoxData[n];
        for (q = 0u; q < mbox->fifoCnt && q < TI_MBOX_MAXFIFO_CNT; q++) {
            ti_mailbox_fifo *fifo = &mbox->fifoTable[q];
            if (Mailbox_getMessageCount(mbox->baseAddr, fifo->queueId) > 0u) {
                uint32_t message[4];
                (void)Mailbox_getMessage(mbox->baseAddr, fifo->queueId, message);
                Mailbox_clrNewMsgStatus(mbox->baseAddr, mbox->userId, fifo->queueId);
                fifo->func(message, fifo->arg);
            }
        }
    }
}
