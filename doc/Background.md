# Background and Design Motivations for Tinsel

Tinsel is a first-cut soft-core design for the POETS project that
embraces massively parallel event-triggered programming.  The design
stems from earlier work on in Cambridge on the BIMPA project, and
lessons learnt from the [SpiNNaker](spinnaker.cs.manchester.ac.uk)
design in Manchester.  We developed BlueVec, a vector processor that
was optimised to run neural network simulations quickly on FPGA.

There are two papers about BlueVec:
* [Managing the FPGA Memory Wall: custom computing or vector processing?](http://www.cl.cam.ac.uk/~swm11/research/papers/FPL2013-BlueVec.pdf)
* [Rapid codesign of a soft vector processor and its compiler](http://www.cl.cam.ac.uk/~swm11/research/papers/FPL2014-Vector.pdf)

## Design Principles

The motivation behind the POETS project is that many scientific
algorithms that do not run efficiently on commodity PC-based clusters
can be described in a massively-parallel event-triggered form.

Tinsel is a first-cut design.  In practise it is difficult to predict
the requirements of algorithms so we aim to be benchmark drive - as we
better understand the algorithms we will refine the architecture.

We are using - on the BIMPA project we found that in the limit (that
was often achievable) the performance was limited by external memory
bandwidth and chip-to-chip comms, both of which FPGAs do very well.

We've deliberately kept Tinsel fast (high clock frequency) and
simple.  At present there is no integer division in hardware but this
is common for a RISC design (e.g. all of the earlier 32-bit ARM cores,
the DEC Alpha cores, etc.) since it messes up the pipeline.

There is no floating-point yet but we intent to add this particularly
when we get to new FPGAs like the Stratix 10 that have much better
floating-point support.

Instead we've focused more on efficient messaging and event handling.



