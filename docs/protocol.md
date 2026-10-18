# Host/DSP protocol 1.0

Status: **proposed**, implemented and tested on the PC, awaiting the maintainer
decision in [Task 027](../tasks/027-protocol-data-path-decision.md). The shared
data path on the board is the one open part; see
[Decision needed](#decision-needed).

Linux applications talk to DSP firmware through
[`libsdos_dsp`](../host/libsdos_dsp/include/sdos/dsp.h) and
[`dspctl`](dspctl.md). Both sides compile one implementation of this
protocol, in
[`firmware/common/protocol`](../firmware/common/protocol/include/sdos/protocol.h).
Control messages travel over RPMsg. Sample data stays in a shared memory
region and is referenced by descriptors, never copied into messages.

## Transport

Course firmware announces the RPMsg service `sdos-dsp` at endpoint 40. On
Linux, the library opens the core's `/dev/rpmsg_ctrlN` and creates an endpoint
to address 40 with the upstream `rpmsg_char` ioctl. It finds the control
device through sysfs by the core's device address, not by number.

One RPMsg buffer holds 512 bytes, of which 16 are the RPMsg header. An SDOS
message is therefore at most 496 bytes: a 16-byte header and up to 480 bytes
of payload.

## Message format

Every integer is little-endian. Encoders write each field byte by byte; no C
structure is ever copied to or from the wire.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 2 | magic | `S`, `D` |
| 2 | 1 | major version | 1; any other value is refused with `E_VERSION` |
| 3 | 1 | minor version | 0; a newer minor is accepted |
| 4 | 2 | type | see below |
| 6 | 2 | flags | bit 0 marks a response; other bits must be 0 |
| 8 | 4 | request id | chosen by the host; the response repeats it |
| 12 | 2 | payload length | must equal the message length minus 16 |
| 14 | 2 | status | 0 in requests; the result in responses |

| Type | Request payload | Response payload |
|---|---|---|
| 1 HELLO | none | identity, 48 bytes |
| 2 ECHO | 0 to 480 bytes | the same bytes |
| 3 STATUS | none | counters, 24 bytes |
| 4 RUN | algorithm, two descriptors, parameters | result, 20 bytes |

**Identity:**
- byte 0 is the core: 0 = C66x_0, 1 = C66x_1, 2 = C71x_0;
- byte 1 is the runtime: 1 = FreeRTOS, 2 = bare metal, 3 = mock;
- capabilities;
- maximum payload;
- buffer alignment, a power of two;
- runtime version;
- size of the shared region;
- the firmware name, at most 23 printable characters.

Reserved bytes must be zero.

**Counters:** uptime in ms, requests, errors, last error, last request id,
and completed runs.

**RUN request:**
- algorithm id (4 bytes) and reserved (4 bytes);
- input descriptor (24 bytes) and output descriptor (24 bytes);
- parameter length (2 bytes) and reserved (2 bytes);
- up to 420 bytes of parameters.

**Descriptor:**
- region, offset, length, sequence and CRC-32C, 4 bytes each;
- flags (bit 0: the checksum field is valid) and reserved, 2 bytes each.

**RUN result:**
- output length;
- output CRC-32C;
- DSP cycles, as a 64-bit count;
- the output sequence, which echoes the request.

## Status codes

| Code | Name | Meaning |
|---|---|---|
| 0 | ok | |
| 1 | bad magic | not an SDOS message |
| 2 | unsupported version | major version differs |
| 3 | unknown type | |
| 4 | bad length | length disagrees with the header or the type |
| 5 | bad field | reserved field or flag set, or a value out of range |
| 6 | bad descriptor | range outside the region, misaligned, or overlapping |
| 7 | checksum mismatch | input data differs from its CRC-32C |
| 8 | unknown algorithm | |
| 9 | busy | a RUN is still executing |
| 10 | bad buffer state | the ownership step is not allowed |
| 11 | internal error | the algorithm or runtime failed |
| 12 | timeout | host only: no response in time |
| 13 | transport error | host only: the endpoint or device failed |

The DSP answers every request that is at least 16 bytes long. Even a request
with bad magic gets a response carrying the raw type and request id, so the
host can match it. The fuzz test in
[`test_protocol.c`](../firmware/common/protocol/tests/test_protocol.c) sends
200,000 random requests and requires a valid response to each.

## Compatibility

- Major version: both sides must match. The host refuses firmware of another
  major version at `start`, before any RUN.
- Minor version: a newer minor may add message types and capabilities, never
  change existing fields. A receiver answers an unknown type with
  `unknown type`.
- Capability bits announce optional features. The host checks a bit before
  using the feature.
- Reserved fields must be zero in 1.0, so a later minor can give them a
  meaning without old firmware misreading them.

## Buffer ownership and caches

Every shared buffer has exactly one owner at a time:

| State | Owner | Entered by |
|---|---|---|
| FREE | nobody | release, or a DSP restart |
| HOST | host | acquire, or the end of a RUN |
| TO_DSP | in transit | host wrote back its cache and sent RUN |
| DSP | DSP | DSP invalidated its cache over the input |
| TO_HOST | in transit | DSP wrote back its cache over the output and answered |
| QUARANTINED | unknown | a timeout or transport error during RUN |

These transitions are the only ones allowed:

| From | Event | To |
|---|---|---|
| FREE | acquire | HOST |
| HOST | submit | TO_DSP |
| HOST | release | FREE |
| TO_DSP | DSP accepts | DSP |
| TO_DSP | DSP rejects the request | HOST |
| DSP | DSP completes | TO_HOST |
| TO_HOST | host invalidates | HOST |
| TO_DSP, DSP, or TO_HOST | timeout | QUARANTINED |
| any state except HOST | DSP restarted | FREE |
| HOST | DSP restarted | HOST |

Every other step is refused with `bad buffer state`. A quarantined buffer
stays unusable until the DSP restarts, because the firmware may still write
it. This is what keeps a timeout from leaving stale ownership silently.
[`test_protocol.c`](../firmware/common/protocol/tests/test_protocol.c) checks
the whole 6 × 9 table.

**Cache maintenance:**
- **Host side:** the library calls the backend's `sync_for_device` before RUN
  and `sync_for_cpu` after it.
- **DSP side:** the runtime invalidates the input range before reading it, and
  writes back the output range before answering.
- **Integrity:** the host attaches a CRC-32C to every input and asks for one
  over the output. A cache mistake on either side shows up as
  `checksum mismatch`, not as silently wrong data.

## Restart and stale messages

- **Restart:** stopping or recovering a DSP closes the endpoint. Each start
  increments a generation number, and buffers acquired under an older
  generation become FREE on their next use.
- **Stale responses:** after a timeout, a late response may still arrive.
  The host drops any response whose request id differs from the outstanding
  one, and counts it.
- **Malformed input:** a malformed response is reported as an error and is
  never interpreted.

## Decision needed

Task 027 is a maintainer decision. Nothing above depends on the open question
except the Linux side of the shared region.

**Recommended:** accept protocol 1.0 as specified, with the upstream
`rpmsg_char` interface used directly. It is in mainline Linux, and TI's own
`ti-rpmsg-char` library is a thin wrapper over it. That avoids one more
licensed input. Firmware keeps TI's IPC library on the DSP side.

**Open, after Task 012's memory audit:** how Linux maps a DSP's shared region.

| Option | How | For | Against |
|---|---|---|---|
| A. Dedicated carve-out with a small kernel driver or UIO node | A reserved-memory node per DSP, mapped through `/dev/sdos-<core>` | Exact physical addresses; one mapping; caches handled in the kernel | A course-owned kernel module or overlay to maintain |
| B. dma-buf heap (CMA) | Allocate with `/dev/dma_heap`; sync with `DMA_BUF_IOCTL_SYNC` | Upstream interfaces only | The DSP needs the physical address, which user space cannot read without a helper |
| C. `/dev/mem` on a no-map region | Map the carve-out directly | No kernel code | Needs root and a kernel that allows it; no cache maintenance |

Option A is the likely recommendation, once Task 012 shows which memory is
free. Until then, `libsdos_dsp`'s Linux backend runs every control message on
the board, and refuses buffer calls with an explicit message. The mock
backend runs the complete data path on the PC.
