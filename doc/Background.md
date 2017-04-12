# Background and Design Motivations for Tinsel

Tinsel is a first-cut soft-core design for the POETS project that
embraces massively parallel event-triggered programming.  The design
stems from earlier work in Cambridge on the BIMPA project, and lessons
learnt from the [SpiNNaker](spinnaker.cs.manchester.ac.uk) design in
Manchester.  BIMPA work at Cambridge included Bluehive (16 FPGA
cluster with high-speed interconnect), BlueVec (vector processor
optimised for neural network simulation on FPGA), and BlueLink
(efficient, reliable components for FPGA interconnect).

* [Bluehive - A Field-Programmable Custom Computing Machine for
Extreme-Scale Real-Time Neural Network Simulation](http://www.cl.cam.ac.uk/~swm11/research/papers/FCCM2012-Bluehive-preprint.pdf)

* [Managing the FPGA Memory Wall: custom computing or vector processing?](http://www.cl.cam.ac.uk/~swm11/research/papers/FPL2013-BlueVec.pdf)

* [Rapid codesign of a soft vector processor and its compiler](http://www.cl.cam.ac.uk/~swm11/research/papers/FPL2014-Vector.pdf)

* [Interconnect for commodity FPGA clusters: standardized or customized?](http://www.cl.cam.ac.uk/~swm11/research/papers/FPL2014-Network.pdf)

The motivation behind the POETS project is that many scientific
algorithms that do not run efficiently on commodity PC-based clusters
can be described in a massively-parallel event-triggered form.

## High-level Overview of Tinsel

**Highly parameterised**. In practise, it is difficult to predict the
optimal allocation of FPGA resources across the compute, memory, and
communication subsystems.  This has led us to a highly  parameterised
design, where the number of resources used by each subsystem can be
easily modified.

**High clock frequency**.  To make good use of the FPGA fabric we wish
to clock it at a high frequency. On the Stratix V FPGA, we can run
Tinsel at a higher frequency than commercial soft cores. This has been
made possible by focusing design decisions on throughput rather than
single-threaded performance.

**Multi-threading**.  In the massively-parallel event-triggered model
it is perhaps inevitable that processors will spend a lot of time
waiting for events, typically for a message to arrive from another
processor. This observation led to us to a multi-threaded design: when
a thread is blocked on an event, the processor can get on with
executing another thread, maintaining full throughput.
Multi-threading reduces the cost of latent operations (such as memory
and floating point instructions) and, by hiding the latency of
arbitration logic,  enables aggressive sharing of resources (such as
caches and FPUs) between cores.

**Off-chip memory**.  As in SpiNNaker, POETS applications may require
access to large amounts of memory. For example, if we are to deal with
large irregular graphs, somewhere is needed to store the graph
structure, even though it will be distributed over the entire engine.
Consequently, we decided that every Tinsel thread should have access
to off-chip DRAM.  To achieve this efficiently, we must exploit
spatial locality, which we do using data caches.  Caches are expensive
in terms of FPGA area (a cache is about the same size as a core) but
multi-threading is very helpful here: by hiding the added latency of
arbitration logic, it allows us to share a single L1 cache between
several cores -- something rarely seen in conventional architectures.

**Software multicasting**.  While SpiNNaker provides dedicated
hardware routers for efficient multicasting, we decided that the CAMs
necessary to implement something similar on FPGA would be too
expensive. Our aim, therefore, is to support efficient multicasting in
software where possible.

**Message size**.  Our partners on the POETS project requested a
larger message size (a 512-bit payload at the least) than SpiNNaker
(32-bit payload). This brings a challenge for software multicasting:
it is inefficient to serialise a 512-bit message through a 32-bit
core. Our solution is a scratchpad-based buffer (the mailbox) that
stores both incoming and outgoing messages. This enables efficient
forwarding of messages using just a few instructions.  The mailbox is
dual-ported, with a 32-bit port on the core side and a much wider port
on the network-side for high-bandwidth delivery.

**FPGA clusters**.  A key feature of the Tinsel architecture is that
it will spread transparently across a large cluster of FPGAs connected
by high-speed links. To enable this, we have developed a custom
reliability and control-flow layer on top of the raw serial links.

**Hardware arithmetic**.  In the present prototype there is no integer
division or floating point operations in hardware. However, the domain
portfolio envisaged for POETS means that these capabilities will be
added in the near future. 
