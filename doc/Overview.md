# Tinsel Overview

Efficient communication and low power consumption are two key goals in
the construction of large-scale distributed systems.  Potentially,
both of these requirements can be met using existing commodity
hardware components that are fairly straightforward to put together,
namely *FPGA development boards*.  These boards combine
state-of-the-art networking facilities with reconfigurable logic
which, when customised to a particular application or application
domain, can offer better performance-per-watt than other commodity
devices such as CPUs and GPUs.

However, FPGA-based systems face challenges of their own. Low-level
hardware description languages and long synthesis times are major
barriers to productivity for application developers.  An attractive
approach for the [POETS project](https://poets-project.org) is
therefore to provide a *soft-core overlay architecture* on top of the
FPGA logic that can be programmed quickly and easily using standard
software languages and tools.  While this overlay is not customised to
a particular POETS *application*, it is at least customised to the
POETS *application domain*.  This is a natural first step because
higher levels of customisation, using techniques such as high-level
synthesis, are somewhat more ambitous and are in any case likely to
reuse components and ideas from the overlay.

In the following sections we give an overview of our soft-core overlay
architecture for POETS (called *tinsel*) and describe our initial
POETS machine built using a network of commodity FPGA development
boards.

## Compute subsystem

We have developed our own 32-bit RISC-V core specially for POETS.
This core is heavily *multithreaded*, supporting up to 32 threads.
Crucially, this enables the core to tolerate the inherent latencies of
deeply-pipelined FPGA floating-point operations (execution of a
multi-cycle operation only blocks the issuing thread; other threads
remain runnable).  In the same way, multithreading also tolerates the
latency of arbitration logic, allowing aggressive sharing of large
components such as FPUs and caches between cores.  This kind of
sharing can reduce FPGA area significantly, allowing more cores per
FPGA, without compromising system throughput.

At most one instruction per thread is allowed in the core's pipeline
at any time, eliminating all control and data hazards.  This leads to
a small, simple, high-frequency design that is able to execute one
instruction per cycle provided there are sufficient parallel threads
(which we expect to be the case for POETS).

Custom instructions are provided for sending and receiving messages.
Threads are automatically suspended when they become blocked on an
event, e.g. waiting to receive a message, and are automatically
resumed when the event is triggered.  This results in a simple and
efficient programming model, avoiding the low-level interrupt handlers
that are required in similar machines such as SpiNNaker.

## Memory subsystem

One of our design requirements is that each thread has access to a
generously-sized private data segment, around 1MB in capacity.
Assuming a large number of threads per FPGA, this kind of memory
demand can only be met using off-chip DRAM.

FPGA boards typically provide a number of high-bandwidth DRAMs and it
is essential to exploit spatial locality for efficient access.  One
way to achieve this, employed by the SpiNNaker system, is to require
the programmer to use a DMA unit to explicitly transfer regions of
data between DRAM and a small, core-local SRAM.  In our view, this
complicates the programming model greatly, introducing an obstacle for
potential users.

Instead, we have developed our own data cache specifically to meet the
requirements of POETS.  This cache is partitioned by thread id (the
hash function combines the thread id with some number of address bits)
and permits at most one request per thread to be present in the cache
pipeline at any time.  Consequently, there is no aliasing between
threads and all data hazards are eliminated, yielding a simple,
non-blocking design that can consume a request on every cycle, even if
it is a miss.  This full-throughput cache can usefully be shared by up
to four cores, based on the observation that an average RISC workload
will access data memory once every four instructions.

## Communication subsystem

One of the challenges in supporting large 64-byte POETS messages is
the amount of serialisation/deserialisation required to get a message
into/out-of a 32-bit core.  Our solution is to use a dual-ported
memory-mapped *mailbox* with a 32-bit port connected to the core and a
much wider port connected to the on-chip network.  The mailbox stores
both incoming and outgoing messages.  A message can be forwarded
(received and sent) in a single instruction, which is useful to
implement efficient multcasting in software.  A single mailbox can be
shared between several cores, reducing the size of the on-chip network
needed connect the mailboxes together.

Fundamental to POETS is the ability to scale the hardware to an
arbitrary number of cores, and hence we must exploit multiple FPGAs.
The inter-board serial communication links available on modern FPGAs
are both numerous and fast but, like all serial links, some errors are
to be expected.  Even if there is only one bit error in every thousand
billion bits (10^12), that is still an error every ten seconds for a
10Gbps link.  Therefore, on top of a raw link we place a 10Gbps
Ethernet MAC, which automatically detects and drops packets containing
CRC errors.  On top of the MAC we place our own window-based
reliability layer that retransmits dropped packets.  The use of
Ethernet allows us to use mostly standard (and free) IP cores for
inter-board communication.  And since we are using the links
point-to-point, almost all of the Ethernet packet fields can be used
for our own purposes, resulting in very little overhead on the wire.

## Initial POETS machine

Our first POETS prototype, currently under construction, is based
around the DE5-Net FPGA board from circa 2012.  It will comprise of
around four multi-FPGA *boxes*, each containing ten FPGA boards and a
modern PC acting as a mothercore. The ten FPGAs in a box will be
connected together in a mesh arrangement and also to the mothercore
via a PCI Express link.  The four boxes when connected together will
provide a mesh of 40 FPGAs and 4 mothercores.  We expect that each
FPGA will host around a hundred RISC-V cores (thousands of RISC-V
threads). The prototype will therefore provide around 4000 cores in
total.
