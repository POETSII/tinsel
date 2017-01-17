# Tinsel Architecture Guide

Tinsel is not specific to any particular FPGA board, but the
description below uses Terasic's [DE5-NET](http://de5-net.terasic.com)
for illustration purposes.  This is a Stratix V board from circa 2012
that the [CL](http://www.cl.cam.ac.uk/) has in plentiful supply.

## Contents

Like any large system, Tinsel is comprised of several modules:

1. [Tinsel Core](#1-tinsel-core)
2. [Tinsel Cache](#2-tinsel-cache)
3. [Tinsel Mailbox](#3-tinsel-mailbox)
4. [Tinsel Mailbox Network](#4-tinsel-mailbox-network)
5. [Tinsel HostLink](#5-tinsel-hostlink)

For reference:

* [DE5-NET Synthesis Report](#de5-net-synthesis-report)
* [Tinsel Parameters](#tinsel-parameters)
* [Tinsel Memory Map](#tinsel-memory-map)
* [Tinsel CSRs](#tinsel-csrs)
* [Tinsel API](#tinsel-api)

## 1. Tinsel Core

Tinsel core is a **customised** 32-bit **multi-threaded** processor
implementing a subset of the RV32IM profile of the
[RISC-V](https://riscv.org/specifications/) ISA.  At present, this
excludes integer division and system instructions.  Custom features
are provided through a range of control/status registers
([CSRs](#tinsel-csrs)).

The number of hardware threads must be a power of two and is
controlled by a sythesis-time parameter `LogThreadsPerCore`.

Tinsel employs a generous **8-stage pipeline** to achieve an Fmax of
450MHz on the [DE5-NET](http://de5-net.terasic.com), while consuming
less than 450 ALMs (0.2%).  These figures are for a standalone
configuration without caches and custom features.

The pipeline is **hazard-free**: at most one instruction per thread is
present in the pipeline at any time.  To achieve **full throughput**
-- execution of an instruction on every clock cycle -- the number of
hardware threads must be greater than the pipeline depth.  The first
power of two that satisfies this requirement is 16.

In fact, the requirement is slightly stronger than this: for full
throughput, there must exist at least 8 **runnable** threads at any time.
When a thread executes a multi-cycle instruction (such as a DRAM
load/store or a blocking send/receive), it becomes **suspended** and is
only made runnable again when the instruction completes.  While
suspended, a thread is not present in the queue of runnable threads
from which the scheduler will select the next thread, so does
not burn CPU cycles.

The core fetches instructions from an **instruction memory**
implemented using on-chip block RAM.  The size of this memory is
controlled by the synthesis-time parameter `LogInstrsPerCore`.  All
threads in a core share the same instruction memory.  The initial
contents of the memory is specified in the FPGA bitstream and typically
contains a boot loader.  The instruction memory is not memory-mapped
(i.e. not accessible via load/store instructions) but two CSRs are
provided for writing instructions into the memory: `InstrAddr` and
`Instr`.

  CSR Name    | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `InstrAddr` | 0x800  | W   | Set address for instruction write
  `Instr`     | 0x801  | W   | Write to instruction memory

There is a read-only CSR for determining the globally unique id of the
currently running thread.

  CSR Name    | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `HartId`    | 0xf14  | R   | Globally unique hardware thread id

On power-up, only a single thread (with id 0) is present in the run
queue.  Further threads can be added to the run queue by writing to
the `NewThread` CSR (as typically done by the boot loader).

  CSR Name    | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `NewThread` | 0x80d  | W   | Create new thread with the given id

Access to all of these CSRs is wrapped up by the following C functions
in the [Tinsel API](#tinsel-api).

```c
// Write 32-bit word to instruction memory
inline void tinselWriteInstr(uint32_t addr, uint32_t word);

// Return a globally unique id for the calling thread
inline uint32_t tinselId();

// Insert new thread into run queue
inline void tinselCreateThread(uint32_t id);
```

A summary of synthesis-time parameters introduced in this section:

  Parameter           | Default | Description
  ------------------- | ------- | -----------
  `LogThreadsPerCore` |       4 | Number of hardware threads per core
  `LogInstrsPerCore`  |      11 | Size of each instruction memory

## 2. Tinsel Cache

The [DE5-NET](http://de5-net.terasic.com) contains two DDR3 DIMMs,
each capable of performing two 64-bit memory operations on every cycle
of an 800MHz clock (one operation on the rising edge and one on the
falling edge).  By serial-to-parallel conversion, a single 256-bit
memory operation can be performed by a single DIMM on every cycle of a
400MHz core clock.  This means that when a core performs a 32-bit
load, it potentially throws away 224 of the bits returned by DRAM.  To
avoid this, we use a **data cache** local to a group of cores, giving
the illusion of a 32-bit memory while behind-the-scenes transferring
256-bit **lines** (or larger, see below) between the cache and DRAM.

The cache line size must be larger than or equal to the DRAM data bus
width: lines are read and written by the cache in contiguous chunks
called **beats**.  The width of a beat is defined by
`DCacheLogWordsPerBeat` and the width of a line by
`DCacheLogBeatsPerLine`.  At present, the width of the DRAM data bus
must equal the width of a cache beat.

The number of cores sharing a cache is controlled by the
synthesis-time parameter `LogCoresPerDCache`.  A sensible value for
this parameter is two (giving four cores per cache), based on the
observation that a typical RISC workload will issue a memory
instruction once in every four instructions.

The number of caches sharing a DRAM is controlled by
`LogDCachesPerDRAM`.  A sensible value for this parameter on the
[DE5-NET](http://de5-net.terasic.com) with a 400MHz core clock might
be three, which combined with a `LogCoresPerDCache` of two, gives 32
cores per DRAM: assuming one cache miss in every eight accesses (ratio
between 32-bit word and 256-bit DRAM bus) and one memory instruction
in every four cycles per core, the full bandwidth will be saturated by
32 cores (1/8 \* 1/4 = 1/32).

For applications with lower memory-bandwidth requirements, the value
of `LogCoresPerDCache` might be increased to three, giving 64 cores
per DRAM.  (As a point of comparison,
[SpiNNaker](http://apt.cs.manchester.ac.uk/projects/SpiNNaker/) shares
a 1.6GB/s DRAM amongst 16 x 200MHz cores, giving 4 bits per
core-cycle.  For the same data width per core-cycle, each 12.8GB/s
DIMM on the [DE5-NET](http://de5-net.terasic.com) could serve 64 x
400MHz cores.)

The cache is an *N*-way **set-associative write-back** cache.
It is designed to serve one or more highly-threaded cores, where high
throughput and high Fmax are more important than low latency.  It
employs a hash function that appends the thread id and some number of
address bits.  This means that cache lines are **not shared** between
threads (and consequently, there is no aliasing between threads).

The cache pipeline is **hazard-free**: at most one request per thread
is present in the pipeline at any time which, combined with the
no-sharing property above, implies that in-flight requests always
operate on different lines, simplifying the implementation.  To allow
cores to meet this assumption, store responses are issued in addition
to load responses.

A **cache flush** operation is provided that evicts all cache lines
owned by the calling thread.  This operation is invoked through the
RISC-V `fence` opcode, or the tinsel API:

```c
// Cache flush
inline void tinselCacheFlush();
```

The following parameters control the number of caches and the
structure of each cache.

  Parameter                | Default | Description
  ------------------------ | ------- | -----------
  `LogCoresPerDCache`      |       2 | Cores per cache
  `LogDCachesPerDRAM`      |       3 | Caches per DRAM
  `DCacheLogWordsPerBeat`  |       3 | Number of 32-bit words per beat
  `DCacheLogBeatsPerLine`  |       0 | Beats per cache line
  `DCacheLogNumWays`       |       2 | Cache lines in each associative set
  `DCacheLogSetsPerThread` |       3 | Associative sets per thread
  `LogBeatsPerDRAM`        |      25 | Size of DRAM

## 3. Tinsel Mailbox

The **mailbox** is a component used by threads to send and receive
messages.  A single mailbox serves multiple threads, defined by
`LogCoresPerMailbox`.  Mailboxes are connected together to form a
network on which any thread can send a message to any other thread
(see section [Tinsel Mailbox Network](#4-tinsel-mailbox-network)), but
communication is more efficient between threads that share the same
mailbox.

A tinsel **message** is comprised of a bounded number of **flits**.  A
thread can send a message containing any number of flits (up to the
bound defined by `LogMaxFlitsPerMsg`), but conceptually the message is
treated as an **atomic unit**: at any moment, either the whole message
has reached the destination or none of it has.  As one would expect,
it is more efficient to send shorter messages than longer ones.  The
size of a flit is defined by `LogWordsPerFlit`.

At the heart of a mailbox is a memory-mapped **scratchpad** that
stores both incoming and outgoing messages.  Each thread has access to
space for several messages in the scratchpad, defined by
`LogMsgsPerThread`.  As well as storing messages, the scratchpad may
also be used as a small thread-local general-purpose memory.

Once a thread has written a message to the scratchpad, it can trigger
a *send* operation, provided that the `CanSend` CSR returns true.  It
does so by: (1) writing the number of flits in the message to the
`SendLen` CSR; (2) writing the address of the message in the
scratchpad to the `SendPtr` CSR; and (3) writing the destination
thread id to the `Send` CSR.

  CSR Name   | CSR    | R/W | Function
  ---------- | ------ | --- | --------
  `CanSend`  | 0x803  | R   | 1 if can send, 0 otherwise
  `SendLen`  | 0x806  | W   | Set message length for send
  `SendPtr`  | 0x807  | W   | Set message pointer for send
  `Send`     | 0x808  | W   | Send message to supplied destination

The [Tinsel API](#tinsel-api) provides wrapper functions for accessing
these CSRs.

```c
// Determine if calling thread can send a message
inline uint32_t tinselCanSend();

// Set message length for send operation
// (A message of length n is comprised of n+1 flits)
inline void tinselSetLen(uint32_t n);

// Send message at address to destination
// (Address must be aligned on message boundary)
inline void tinselSend(uint32_t dest, volatile void* addr);

// Get pointer to nth message-aligned slot in mailbox scratchpad
inline volatile void* tinselSlot(uint32_t n);
```

Several things to note:

* When sending a message, a thread must not modify the
contents of that message while `tinselCanSend()` returns false,
otherwise the in-flight message could be corrupted.

* The `SendLen` and `SendPtr` CSRs are persistent: if two consecutive
send operations wish to use the same length and address then the CSRs
need only be written once.

* The scratchpad pointer must be aligned on a max-message-size
boundary, which we refer to as a message **slot**. The `tinselSlot`
function yields a pointer to the nth slot in the calling thread's mailbox.

To *receive* a message, a thread must first *allocate* a slot in the
scratchpad for an incoming message to be stored.  Allocating a slot
can be viewed as transferring ownership of that slot from the software
to the hardware.  This is done by writing the address of the slot to
the `Alloc` CSR.  Multiple slots can be allocated, bounded by
`LogMsgsPerThread`, creating a receive buffer of the desired capacity.

The hardware may use any one of the allocated slots to store an
incoming message, but as soon as a slot is used it will be
automatically *deallocated*.  Now, provided the `CanRecv` CSR returns
true, the `Recv` CSR can be read, yielding a pointer to the slot
containing a received message.  Receiving a message can be viewed as
transferring ownership of a slot from the hardware to the software.
As soon as the thread is finished with the message, it can
*reallocate* it, restoring capacity to the receive buffer.  On
power-up, no slots are allocated for receiving messages, i.e. all
slots are owned by software and none by hardware.

 CSR Name   | CSR    | R/W | Function
 ---------- | ------ | --- | --------
 `Alloc`    | 0x802  | W   | Allocate slot in scratchpad for receiving a message
 `CanRecv`  | 0x805  | R   | 1 if can receive, 0 otherwise
 `Recv`     | 0x809  | R   | Return pointer to a received message

Again, the [Tinsel API](#tinsel-api) hides these low-level CSRs.

```c
// Give mailbox permission to use given slot to store an incoming message
inline void tinselAlloc(volatile void* addr);

// Determine if calling thread can receive a message
inline uint32_t tinselCanRecv();

// Receive message
inline volatile void* tinselRecv();
```

Sometimes, a thread may wish to wait until it can send or receive.  To
avoid busy waiting on the `tinselCanSend()` and `tinselCanRecv()`
functions, a thread can be suspended by writing to the `WaitUntil`
CSR.

  CSR Name    | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `WaitUntil` | 0x80a  | W   | Sleep until can send or receive

This CSR is treated as a bit string: bit 0 indicates whether the
thread would like to be woken when a send is possible, and bit 1
indicates whether the thread would like to be woken when a receive is
possible.  Both bits may be set, in which case the thread will be
woken when a send *or* a receive is possible. The [Tinsel
API](tinsel-api) abstracts this CSR as follows.

```c
// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} TinselWakeupCond;

// Suspend thread until wakeup condition satisfied
inline void tinselWaitUntil(TinselWakeupCond cond);
```

Finally, a quick note on the design.  One of the main goals of the
mailbox is to support efficient software multicasting: when a message
is received, it can be forwarded on to multiple destinations without
having to serialise the message contents into and out of the 32-bit
core.  The mailbox scratchpad has a flit-sized port on the network
side, providing much more efficient access to messages than is
possible from the core.

A summary of synthesis-time parameters introduced in this section:

  Parameter                | Default | Description
  ------------------------ | ------- | -----------
  `LogCoresPerMailbox`     |       2 | Number of cores sharing a mailbox
  `LogWordsPerFlit`        |       2 | Number of 32-bit words in a flit
  `LogMaxFlitsPerMsg`      |       2 | Max number of flits in a message
  `LogMsgsPerThread`       |       4 | Number of slots per thread in scratchpad


## 4. Tinsel Mailbox Network

The number of mailboxes on each FPGA board is goverened by the
parameter `LogMailboxesPerBoard`.

  Parameter                | Default | Description
  ------------------------ | ------- | -----------
  `LogMailboxesPerBoard`   |       4 | Number of mailboxes per FPGA board

The mailboxes are connected together by a **bidirectional serial bus**
carrying message flits (see section [Tinsel
Mailbox](#3-tinsel-mailbox)).  The network ensures that flits from
different messages are not interleaved or, equivalently, flits from
the same message appear contiguously on the bus.  This avoids complex
logic for reassembling messages.  It also avoids the deadlock case
whereby a receiver's buffer is exhausted with partial messages, yet is
unable to provide a single whole message for the receiver to consume
in order free space.

It is more efficient to send messages between threads that share a
mailbox than between threads on different mailboxes.  This is because,
in the former case, flits are simply copied from one part of a
scratchpad to another using a wide, flit-sized, read/write port.  Such
messages do not occupy any bandwidth on the bidirectional bus
connecting the mailboxes.

It is also more efficient to send messages between threads on
neighbouring mailboxes, w.r.t. the bidirectional bus, than between
threads on distant mailboxes.  This is because, in the former case,
the message spends less time on the bus, consuming less bandwidth.

In future, the mailbox network will be extended to multiple FPGA
boards.

## 5. Tinsel HostLink

HostLink is the mechanism by which tinsel cores running on FPGA
communicate with a host PC.  Using HostLink, programs compiled on the
PC can be written into the instruction memories of the tinsel cores,
memory can be initialised, and program outputs can be sent to the PC
for analysis.  HostLink may also be used for debug and diagnostic
purposes, such as implementing `printf` and monitoring performance
counters.

At present, HostLink uses a JTAG UART to transfer data between the PC
and FPGA.  This provides a transfer rate of about 4 MB/s. In future,
higher bandwidth solutions will be explored.  Therefore HostLink
should be viewed as an abstraction on top of the host-to-FPGA link.

HostLink commands can be sent from the host to the FPGA and viceversa.
All commands sent from the host consist of 5 bytes: a 1-byte command
tag and a 4-byte argument.  At present, there are two such commands:

  Command (1 byte) | Argument (4 bytes)
  ---------------- | ------------------
  `SetDest`        | Core Id           
  `StdIn`          | Payload           

The `SetDest` command sets the destination core id for all subsequent
commands (until the next `SetDest` command).  This is a meta-command
in that it is not actually sent to any core.  The MSB of core id is
the broadcast bit.  The `StdIn` command causes the payload to be
written to the `FromHost` CSR on the destination core(s):

  Name        | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `FromHost`  | 0x80b  | R   | Read word from HostLink

HostLink commands sent from FPGA to the host consist of 9 bytes: a
1-byte command tag, a 4-byte source core id, and a 4-byte argument.
For now, there is one such command:

  Command (1 byte) | Source (4 bytes) | Argument (4 bytes)
  ---------------- | ---------------- | ------------------
  `StdOut`         | Core Id          | Payload

The `StdOut` command contains a payload for the host, with the source
core id attached.  Cores can issue this command by writing to the
`ToHost` CSR.

  Name        | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `ToHost`    | 0x80c  | W   | Write word to HostLink

The above CSRs are abstracted in the tinsel API:

```c
// Send word to host (over HostLink)
inline void tinselHostPut(uint32_t x);

// Receive word from host (over HostLink)
inline uint32_t tinselHostGet();
```

## Reference

### DE5-NET Synthesis Report

The default tinsel configuration on a single DE5-NET board contains:

  * 64 cores
  * 16 threads per core
  * (1024 threads in total)
  * 16 mailboxes
  * 16 caches
  * two DDR3 DRAM controllers
  * a JTAG UART

The clock frequency is 275MHz and the resource utilisation is 94K
ALMs, 40% of the DE5-NET, leaving plenty of space for interboard comms
to be added in the near future.

### Tinsel Parameters

  Parameter                | Default | Description
  ------------------------ | ------- | -----------
  `LogThreadsPerCore`      |       4 | Number of hardware threads per core
  `LogInstrsPerCore`       |      11 | Size of each instruction memory
  `LogCoresPerDCache`      |       2 | Cores per cache
  `LogDCachesPerDRAM`      |       3 | Caches per DRAM
  `DCacheLogWordsPerBeat`  |       3 | Number of 32-bit words per beat
  `DCacheLogBeatsPerLine`  |       0 | Beats per cache line
  `DCacheLogNumWays`       |       2 | Cache lines in each associative set
  `DCacheLogSetsPerThread` |       3 | Associative sets per thread
  `LogBeatsPerDRAM`        |      25 | Size of DRAM
  `LogCoresPerMailbox`     |       2 | Number of cores sharing a mailbox
  `LogWordsPerFlit`        |       2 | Number of 32-bit words in a flit
  `LogMaxFlitsPerMsg`      |       2 | Max number of flits in a message
  `LogMsgsPerThread`       |       4 | Number of slots per thread in scratchpad
  `LogMailboxesPerBoard`   |       4 | Number of mailboxes per FPGA board

### Tinsel Memory Map

  Region                  | Description
  ----------------------- | -----------
  `0x00000000-0x000003ff` | Reserved
  `0x00000400-0x000007ff` | Thread-local mailbox scratchpad
  `0x00000800-0x000fffff` | Reserved
  `0x00100000-0x3fffffff` | Cached off-chip DRAM

### Tinsel CSRs

  Name        | CSR    | R/W | Function
  ----------- | ------ | --- | --------
  `InstrAddr` | 0x800  | W   | Set address for instruction write
  `Instr`     | 0x801  | W   | Write to instruction memory
  `Alloc`     | 0x802  | W   | Alloc space for new message in scratchpad
  `CanSend`   | 0x803  | R   | 1 if can send, 0 otherwise
  `HartId`    | 0xf14  | R   | Globally unique hardware thread id
  `CanRecv`   | 0x805  | R   | 1 if can receive, 0 otherwise
  `SendLen`   | 0x806  | W   | Set message length for send
  `SendPtr`   | 0x807  | W   | Set message pointer for send
  `Send`      | 0x808  | W   | Send message to supplied destination
  `Recv`      | 0x809  | R   | Return pointer to message received
  `WaitUntil` | 0x80a  | W   | Sleep until can-send or can-recv
  `FromHost`  | 0x80b  | R   | Read word from HostLink
  `ToHost`    | 0x80c  | W   | Write word to HostLink
  `NewThread` | 0x80d  | W   | Create new thread with the given id
  `Emit`      | 0x80f  | W   | Emit char to console (simulation only)

### Tinsel API

```c
// Return a globally unique id for the calling thread
inline uint32_t tinselId();

// Write 32-bit word to instruction memory
inline void tinselWriteInstr(uint32_t addr, uint32_t word);

// Cache flush
inline void tinselCacheFlush();

// Get pointer to nth message-aligned slot in mailbox scratchpad
inline volatile void* tinselSlot(uint32_t n);

// Determine if calling thread can send a message
inline uint32_t tinselCanSend();

// Set message length for send operation
// (A message of length n is comprised of n+1 flits)
inline void tinselSetLen(uint32_t n);

// Send message at address to destination
// (Address must be aligned on message boundary)
inline void tinselSend(uint32_t dest, volatile void* addr);

// Give mailbox permission to use given slot to store an incoming message
inline void tinselAlloc(volatile void* addr);

// Determine if calling thread can receive a message
inline uint32_t tinselCanRecv();

// Receive message
inline volatile void* tinselRecv();

// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} TinselWakeupCond;

// Suspend thread until wakeup condition satisfied
inline void tinselWaitUntil(TinselWakeupCond cond);

// Send word to host (over HostLink)
inline void tinselHostPut(uint32_t x);

// Receive word from host (over HostLink)
inline uint32_t tinselHostGet();

// Insert new thread into run queue
inline void tinselCreateThread(uint32_t id);

// Emit word to console (simulation only)
inline void tinselEmit(uint32_t x);
```
