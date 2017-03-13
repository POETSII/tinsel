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

## Design Decisions

The motivation behind the POETS project is that many scientific
algorithms that do not run efficiently on commodity PC-based clusters
can be described in a massively-parallel event-triggered form.

**Continuous refinement**.  Tinsel is a first-cut design.  In practise
it is difficult to predict the requirements of algorithms so we aim to
be benchmark driven - as we better understand the algorithms we will
refine the architecture.

**FPGAs**.  On the BIMPA project we found that in the limit (that
was often achievable), performance was bounded by external memory
bandwidth and chip-to-chip comms, both of which FPGAs do very well.
FPGAs are also well-suited to continuous refinement: we can develop
highly-parameterised hardware from an early stage and instantiate it
in the right way once we understand the applications better.

**Multi-threading**. In the massively-parallel event-triggered
model it is perhaps inevitable that processors will spend a lot of
time waiting for events, e.g. for a message to arrive from another
processor.  This observation led to us to use multi-threaded
processors in tinsel: when a thread is blocked on an event, the
processor can get on with executing another thread, maintaining full
throughput.  Of course, this non-blocking behaviour can also be
achieved in a single-threaded processor using a software event loop,
but at the cost of interpretive overhead and context-switching.  In
any case, multi-threaded processors open up many oppertunities for
efficient hardware design on FPGA.

**Off-chip memory**. As in Spinnaker, POETS applications may require
access to large amounts of memory.  For example, if we are to deal
with large irregular graphs, we'll need somewhere to store the graph
structure.  Consequently, we decided that every tinsel thread should
have access to off-chip DRAM.  And to provide efficient access to such
DRAM, we need data caches.  This starts to become expensive in terms
of FPGA area but multi-threading is very helpful here: by hiding
latency, it allows us to share a single L1 cache between many cores --
something rarely seen in conventional architectures.

**Software multicasting**. While Spinnaker provides dedicated hardware
routers for efficient multicasting, we decided that the CAMs
neccessary to implement something similar on FPGA would be too
expensive.  Our aim, therefore, is to support efficient multicasting
in software where possible.

**Medium-sized messages**.  Our partners on the POETS project
requested a larger message size (a 512-bit payload at the least) than
Spinnaker (32-bit payload).  This brings a challenge for software
multicasting: it is inefficient to serialise a 512-bit message through
a 32-bit core and then deserialise it again before sending it off to
other cores.  Our solution is a scratchpad-based buffer (called a
mailbox) with a 32-bit port on the core side and a much wider port on
the network side.  Messages can therefore be efficiently forwarded
(received and sent) by a core without the core having to do much work.

**FPGA clusters**. A key feature of the tinsel architecture is that
it will spread transparently across a large cluster of FPGAs connected
by high-speed links.

**No hardware division (to begin)**. We've deliberately kept tinsel
fast (high clock frequency) and simple.  At present there is no
integer division in hardware but this is common for a RISC design
(e.g. all of the earlier 32-bit ARM cores, the DEC Alpha cores, etc.)
since it messes up the pipeline.

**No floating-point (to begin)**. There is no floating-point yet but
we intend to add this particularly when we get to new FPGAs like the
Stratix 10 that have much better floating-point support.
Multi-threading is again a great help here: not only can it hide the
long latency of floating-point operations, but it also permits large
floating-point units to be efficiently shared by multiple cores.
