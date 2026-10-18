# Gate 0 record

Status: accepted 2026-10-18, conditional go  
Prepared: 2026-10-18 by claude for [Task 006](../../../tasks/006-gate-0-decision.md)  
Baseline commit: `802293b`

Gate 0 asks whether scope, hardware, host OS, and redistribution approach are
approved for physical bring-up. This record maps each Phase 0 deliverable to
its evidence and names every open condition with an owner.

## Recommendation: conditional go

Physical bring-up, starting with Task 008, can begin on the current bench.
Licensing, pins, and CI feasibility are decided and evidenced. The remaining
gaps are bench hardware for later phases and one license blocker whose gate
should move. None of them makes the Task 008 target ambiguous.

## Evidence index

| Phase 0 deliverable | Evidence | State |
|---|---|---|
| Requirements and course policy | [`docs/course-policy.md`](../../../docs/course-policy.md), accepted 2026-10-18 | Done |
| Hardware BOM and inventory contract | [`docs/hardware-bom.md`](../../../docs/hardware-bom.md), [`manifest/hardware-bom.json`](../../../manifest/hardware-bom.json) | Done |
| License and redistribution matrix | [`docs/licensing-policy.md`](../../../docs/licensing-policy.md), [`manifest/license-matrix.json`](../../../manifest/license-matrix.json) revision 4, accepted with the no-cost rule | Done |
| Candidate version manifest | [`manifest/versions.json`](../../../manifest/versions.json) revision 2, 50 pinned inputs | Done |
| GitHub-hosted feasibility | [`docs/research/task-005-feasibility.md`](../../../docs/research/task-005-feasibility.md), two passing runs | Done |
| Safe board access | [`docs/board-access.md`](../../../docs/board-access.md), read-only discovery of the connected board | Serial capture pending |

## Observed bench

- One BeagleBone AI-64, booted from its factory eMMC image dated 2022. It is
  identified by its SSH host keys; the EEPROM identity is read in Task 009.
- One 32 GB microSD card holding an older image, in the PC's built-in SD slot,
  with by-id `mmc-SD32G_<serial>`.
- Board USB-C to the PC's USB-A port for power and USB networking. There is no
  Ethernet cable, which is not needed, and no barrel-jack supply.

## Conditions

The maintainer accepted these conditions as revised on 2026-10-18 after
reporting the bench changes quoted under Decision.

| ID | Condition | Owner | Blocks | State at acceptance |
|---|---|---|---|---|
| G0-1 | Board power. BeagleBoard asks for 5 V/3 A or more; the baseline supply is a 5 V, 4 A center-positive barrel jack. | Maintainer | Task 010 stability and all DSP work | Accepted for now: a Xiaomi 5 V/3 A USB-C charger, or the PC's USB-A port while the board needs USB networking. Unexpected resets are first attributed to power. |
| G0-2 | Serial access for the workstation account. | Maintainer | Serial capture in Tasks 007 and 008 | The account is in `dialout`; the running session predates it. Until a reboot, the maintainer grants a per-device ACL with `setfacl`. |
| G0-3 | A boot-loader log needs the 3-pin `UART0` debug header J3. | Maintainer purchase | Complete serial boot log in Task 008 | Open. The maintainer's adapter is a CH340 USB-to-ESP-01 board with Dupont wires, which do not fit J3. Captures on P9.11/P9.13 received no console text, because the stock image routes UART0 to J3. A 3-pin cable matching J3 is needed; the Linux console over USB works meanwhile. |
| G0-4 | Move `LIC-001`'s gate from Gate 0 to "before any TI-derived artifact is published". | Agent | Nothing before Task 019 | Done in license matrix revision 5. |
| G0-5 | Repository `Dimitrije7/sdos2026`, public, with the maintainer as the sole staff owner until a course organization is chosen before student onboarding. | Maintainer | Student onboarding | Accepted. |

Deferred manifest items keep their owners: TI Linux firmware in Task 013,
ti-rpmsg-char in Task 027, and minimum Docker host versions in Task 017.

## Alternatives

- **Go:** treat G0-1 through G0-3 as non-blocking for the whole of Phase 1. This
  risks an unstable Gate 1 run on USB-A power.
- **No-go:** wait for the barrel supply and hub before flashing anything. Nothing
  in Task 008 needs them except the U-Boot log.

## Frozen baseline

These SHA-256 values identify the documents the decision approves, at commit
`802293b`.

Applying G0-4 afterwards changed `docs/licensing-policy.md` and
`manifest/license-matrix.json` only in `LIC-001`'s owner, condition, and gate,
the matrix revision, and a closing note in the policy.

| File | SHA-256 |
|---|---|
| `docs/course-policy.md` | `9857ae8d629b122a157dacad36eee12cc295aa0ec601758acd282860764a74c5` |
| `docs/hardware-bom.md` | `bf8edc5d7a3eb679bfbbdd0b915044dca245978c882c7fc1277610b8dc2764d3` |
| `manifest/hardware-bom.json` | `b5d481f9c850bb58fa496de00de55afb9e0f9f4738bcea0f3d0a3578dc4eeb9a` |
| `docs/licensing-policy.md` | `13413a1aedf39bd0bd86050591d77d5154dd08773d524514ac081a0d0718ada8` |
| `manifest/license-matrix.json` | `35573a156de6c0f9cb3ef8966aeeae94318fe2f5bbfa70658b08b909bfcc2145` |
| `manifest/versions.json` | `cc1474c07e1a44711b8a256d969879bc6f2140bd1b7c1db4a882eca3d5164441` |

## Decision

Accepted on 2026-10-18. The agent asked: "Do you accept Gate 0 with these
conditions, plus the two-track plan?" The table it showed listed G0-1 as the
Xiaomi 5 V/3 A adapter for now, G0-2 as `setfacl` or a reboot, G0-3 as solved by
the UART adapter on USB-A, and G0-4 and G0-5 unchanged. The two-track plan runs
the board track from Task 008 alongside the build track of Tasks 016, 017, and
019.

The maintainer answered:

> 1. I accept
> 2. both options

The second answer replied to "Should the image keep BeagleBoard's kernel
package, or must the kernel also be built from source?" It is recorded in
[Task 035](../../../tasks/035-reproducible-image-builder.md): the image builder
supports both.

The maintainer had earlier written "I do not have barrel-jack, I will power up
the board via my Xiaomi adapter that delivers 5V and 3A, 15W it should be enough
for now, and I will connect UART to USB A port via converter." After the UART
attempts failed, the maintainer reconnected the board's USB-C to the PC and
wrote "connected via usb c continue". G0-1 and G0-3 above record that state.
