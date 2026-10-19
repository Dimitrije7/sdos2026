;
; vectors.asm: the C71x event service table the firmware installs at start
; (ESTP_S, set by sdos_protection_init in c71x.c).
;
; It is TI's table (Hwi_disp_always.asm, PDK 11.02.01.08, BSD-3-Clause; notice
; in LICENSES/TI-PDK-examples-BSD-3-Clause.txt) with one change: the two
; exception entries move to the firmware's exception stack before they store
; anything. TI's entries push onto the stack that faulted, and its dispatcher
; then runs the handler there. A stack overflow faults on the read-only guard
; page below the stack, so with TI's entries the first store faulted again and
; the core hung without a fault record (Task 042, criterion 2).
;
; Interrupts and everything after the switch are TI's code: the entries branch
; to TI's Exception_dispatch exactly as TI's do, with a0 pointing at the saved
; a0 and a1 and the vector type in a1.
;

SP      .set    d15

        .ref    Exception_dispatch
        .ref    Hwi_dispatchAlways
        .ref    sdos_exception_stack_top
        .ref    sdos_exception_sp

;
; ENTRY type: leave the faulting stack, then enter TI's dispatcher.
; a0 and a1 are parked in this event's context save area (ECSP; TI's
; interrupt dispatcher uses words 0x300 and 0x301 of it the same way) so
; nothing is written through SP until SP is the exception stack. The stack
; pointer at the exception goes to sdos_exception_sp for the fault record.
;
ENTRY   .macro  type
        std.d1    a0, *ECSP[0x302]
        std.d1    a1, *ECSP[0x303]
        mv.d1     SP, a1
        addkpc.d1 $PCR_OFFSET(sdos_exception_sp), a0
        std.d1    a1, *a0
        addkpc.d1 $PCR_OFFSET(sdos_exception_stack_top), a0
        ldd.d1    *a0, a0
        mv.d1     a0, SP
        ldd.d1    *ECSP[0x302], a0
        ldd.d1    *ECSP[0x303], a1
        std.d2x   a0, *SP++[-1]
||      mv.d1     SP, a0
        std.d1    a1, *SP++[-1]
        mvk64.l1  :type:, a1
        b.b1      $PCR_OFFSET(Exception_dispatch)
        .endm

        .sect   ".sdos.vecs"
        .global sdos_vectors

sdos_vectors:
        b.b1      $PCR_OFFSET(sdos_vectors)

        .align 0x200
sdos_int_exc:
        ENTRY     0x0

        .align 0x200
sdos_page_fault:
        ENTRY     0x1

        .align 0x200
sdos_nme:
        b.b1      $PCR_OFFSET(sdos_nme)

        .align 0x200
sdos_evt:
        b.b1      $PCR_OFFSET(Hwi_dispatchAlways)
