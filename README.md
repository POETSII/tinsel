# Tinsel

**Tinsel** is a **many-core message-passing** machine designed for
massively parallel computing on an **FPGA cluster**.  It is not
specific to any particular FPGA board, but the description below
uses Terasic's [DE5-NET](http://de5-net.terasic.com) for illustration
purposes.  (This is a fairly high-end board that the
[CL](http://www.cl.cam.ac.uk/) has in plentiful supply.)

(This respository is work-in-progress.)

## Contents

Like any large system, Tinsel is comprised of several modules:

1. [Tinsel Core](#tinsel-core)
2. [Tinsel Cache](#tinsel-cache)
3. [Tinsel Mailbox](#tinsel-mailbox)

## Tinsel Core

**Tinsel Core** is a 32-bit **multi-threaded** processor implementing the
[RISC-V](https://riscv.org/specifications/) ISA (RV32I).

The number of hardware threads must be a power of two and is
controlled by a sythesis-time parameter `LogThreadsPerCore`.

Tinsel employs a generous **9-stage pipeline** to achieve an Fmax of
450MHz on the [DE5-NET](http://de5-net.terasic.com), while consuming
less than 450 ALMs (0.2%).

The pipeline is **hazard-free**: at most one instruction per thread
can be present in the pipeline at any time.  To achieve **full
throughput** -- execution of an instruction on every clock cycle -- the
number of hardware threads must be greater than the pipeline depth.
The first power of two that satisfies this requirement is 16.

In fact, the requirement is slightly stronger than this: for full
throughput, there must exist at least 9 **runnable** threads at any time.
When a thread executes a multi-cycle instruction (such as a DRAM
load/store or a blocking send/receive), it becomes **suspended** and is
only made runnable again when the instruction completes.  While
suspended, a thread is not present in the queue of runnable threads
from which the scheduler will select the next thread, so does
not burn CPU cycles.

A 9-stage pipeline is perhaps excessive but incurs little cost on
FPGA, and there will be no shortage of program threads in the
massively-parallel applications for which the machine is intended.

The core fetches instructions from an **instruction memory**
implemented using on-chip block RAM.  The size of this memory is
controlled by the synthesis-time parameter `LogInstrsPerCore`.  All
threads in a core share the same instruction memory.  The initial
contents of the memory is specified in the FPGA bitstream.

A globally unique identifier for the currently running thread can be
obtained from a RISC-V [control/status
register](https://riscv.org/specifications/privileged-isa/) (CSR
`0xF14`).  The `Tinsel.h` header file provides a function to read this
register:

```
// Return a globally unique id for the calling hardware thread
inline uint32_t me()
{
  uint32_t id;
  asm("csrr %0, 0xF14" : "=r"(id));
  return id;
}

```

A summary of parameters introduced in this section:

  Parameter                 | Description
  ------------------------- | -----------
  `LogThreadsPerCore`       | Number of hardware threads per core
  `LogInstrsPerCore`        | Size of each instruction memory

## Tinsel Cache

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
`DCacheLogBeatsPerLine`.  The width of the DRAM data bus must equal
the width of a cache beat.

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

The cache implements a low-cost **coherence mechanism**.  When a
thread issues a `fence` instruction, all cache lines for that thread are
invalidated and all dirty lines are written out to DRAM.  If a thread
wishes to make its writes visible to other threads then it issues a
`fence`.  Similarly, if a thread wishes to make sure it can read the
latest values written by other threads then it also issues a `fence`.
This scheme meets the [WMO without local dependencies]
(https://github.com/CTSRD-CHERI/axe/raw/master/doc/manual.pdf)
consistency model, where a thread may observe operations by
another thread out-of-order but reorderings over a `fence` are
forbidden.

`Tinsel.h` defines a `fence()` function as follows.

```
// Memory fence: memory operations can't appear to be reordered over this
inline void fence()
{
  asm volatile("fence");
}

```

There is no support for **atomic** memory operations.  Tinsel is a
message-passing machine and it is not clear that atomics are needed.
For example, instead of gaining exclusive write access to a block of
shared memory in order to perform an atomic update, a message can be
sent to the owner of the block (by software) telling it to perform the
update.

The following parameters control the number of caches and the
structure of each cache.

  Parameter                 | Description
  ------------------------- | -----------
  `LogCoresPerDCache`       | Cores per cache
  `LogDCachesPerDRAM`       | Caches per DRAM
  `DCacheLogWordsPerBeat`   | Number of 32-bit words per beat
  `DCacheLogBeatsPerLine`   | Beats per cache line
  `DCacheLogNumWays`        | Cache lines in each associative set
  `DCacheLogSetsPerThread`  | Associative sets per thread

## Tinsel Mailbox

**Under construction.**
