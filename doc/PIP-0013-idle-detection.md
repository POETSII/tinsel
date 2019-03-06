# PIP-0013: Idle detection

Author: Matthew Naylor

This proposal was implemented in Tinsel 0.5.

## Proposal

Introduce a new Tinsel API function

```c++
  bool tinselIdle();
```

which blocks until either

  1. a message is available to receive, or

  2. all threads are blocked on a call to `tinselIdle()` and there are
     no undelivered messages in the system.

The function returns **false** in the former case and **true** in the
latter.

## Motivation/Rationale

Detecting termination is one of the big challenges when writing POETS
applications.  For example, in the weighted single-source
shortest-path (SSSP) problem, the main application logic is incredibly
simple:

  1. Each device *d* stores the shortest known path from the source to *d*.

  2. When the shortest known path to a device *d* changes, *d* sends a
     message to each neighbour *n* containing the shortest known path
     to *n* that goes via *d*.

  3. When a device *d* receives a message, it updates the shortest path to
     *d* if the new path is indeed shorter that the currently
     known shortest.

This is probably expressible in ~10 lines of POETS code.

However, determining when the algorithm converges, i.e. when all the
shortest paths have been found, is a far more challenging problem.
And it's especially difficult to do efficiently in software, i.e.
without introducing traffic that could slow the application down.

Here, we consider adding a hardware feature to make termination
detection simple-to-express and efficient-to-use.  When a device in
our SSSP example is waiting for a message, it behaves as follows.

```c++
if (tinselIdle()) {
  // Converged.  Send result to host.

else {
  // A new message has arrived.  Deal with it.
}
```

This feature not only provides a way to detect termination, but also a
way to implement syncrhonous execution semantics, which can support an
easier (and sometimes more efficient) programming model.  For example,
consider the classic heat transfer application which is most easily
solved in POETS using the GALS technique.  The two problems with this
technique are (1) it is easy to get wrong and result in deadlock; and
(2) a device sends a message to all its neighours on every time step,
even if the heat at that device is unchanged.  If instead we use the
`tinselIdle()` call to advance time then the application becomes
trivial to code, and devices only send messages when they need to
(which seems more in the spirit of event-driven computing!).

Another motivation for this proposal is to implement a backend to
Google's [Pregel
framework](https://github.com/POETSII/reference-papers/blob/master/frameworks/malewicz-2010-pregel-distributed-graph-processing.pdf)
for solving large graph problems.  This framework is very similar to
POETS, but uses a synchronous execution semantics.

POETS applications may now decide whether to use a synchronous or
asychronous execution semantics.  This offers an interesting advantage
over the Pregel approach, which mandates synchronisation.  For
example, in the weighted SSSP problem, an asynchronous solution (with
efficient termination detection) is surely better than Pregel's
synchronous version.

In summary, we have considered two examples: weighted SSSP and heat
transfer.  There are advantages in implementing the former
asynchronously and the latter synchronously.  The `tinselIdle()`
feature greatly benefits both: detecting termination in the former and
implementing synchronisation in the latter.

## Host semantics

There is a question about whether or not the host needs to call a
corresponding `hostIdle()` function for the system to be considered
idle.  Arguments can be made for and against:

  * **For**.  When messages are injected into the system from the
    host, the system shouldn't be considered idle until all
    those messages have been received.

  * **Against**.  The overhead of agreeing termination with the host
    makes the feature less useful as a synchronisation mechanism.
    Host messages can simply be considered asynchronous with respect
    to the rest of the system.

For simplicity, we propose **not** to provide a `hostIdle()` function
(at least to begin with).

## Design sketch

We propose to implement Safra's distributed termination detection
algorithm
([EWD998](https://www.cs.utexas.edu/users/EWD/ewd09xx/EWD998.PDF)) in
hardware.

In a distributed system consisting of *N* "machines", we can treat
Safra's algorithm as black box running on each machine with the
following interface:

  * **Active In**. A single bit that's high when the machine is active,
    e.g. not in a call to `tinselIdle()`.

  * **Count In**.  A 62-bit signed integer denoting the number of
    messages sent by the machine minus the number messages received by
    the machine.

  * **Token In** and **Token Out**. Each a 64-bit token, with control-flow,
    containing a 1-bit colour (0=white, 1=black), a 1-bit termination
    flag, and a 62-bit count.

The 1-bit termination flag in a token has been added by us -- it's
ignored by Safra's algorithm.  The idea is that once termination is
detected, a token is sent **twice** round all the machines with the
termination bit set.  In the first round, each machine observes that
termination has been detected and disables message sending -- at this
point, all calls to `tinselIdle()` return true.  In the second round,
sending is enabled again.  The two rounds are required to avoid a
machine that has returned from `tinselIdle()` from sending a message
to a machine that has not yet returned from `tinselIdle()`.

In the above description we assume "machines" correspond to
Tinsel threads.  For efficiency, we actually want machines to
correspond to FPGAs.  This can be achieved using on-chip reduction
networks that determine the **Active In** and **Count In** inputs for
a whole FPGA as follows.

  * **Active In**.  Every core produces a 1-bit signal that is low
    when all its threads are in a call to `tinselIdle()`.  All these
    signals are then ORed together and fed into Safra's algorithm.

  * **Count In**.  Every core pulses a 1-bit "sent" signal and a
    1-bit "received" signal whenever a message is sent or received.
    These signals are use to maintain the 62-bit count on each FPGA.

Note that these signals being produced by each core must be sampled
atomically.  It is fine for them to be delayed arbitrarily (e.g.
registered multiple times), as along as they are sampled at a
consistent point in time.

Given that messages can move to and from the host, the bridge FPGA
must be considered as one of the "machines" in the termination
detection algorithm.  It makes sense for the bridge to take the role
of "machine 0", i.e. the machine that initiates the termination probe.
This way, the bridge will be the first board to observe termination,
at which point it can disable messages arriving from the host before
signalling termination to all the worker FPGAs.

## Impact

Existing software is unaffected.  The only costs are hardware
complexity and area, and the latter is expected to be small.

## Comments from David Thomas

David writes:

> I like the idea - it's quite a simple API from the software
> point of view, and lightweight in hardware.
> 
> To the list of applications you could add:
> - Spiking neural networks
> - Artificial neural network training

Yes, I plan to implement an Izhikevich simulator using this feature.

> There is the challenge of how it gets mapped into the abstract
> graph representation, as it would interfere with the pure
> atomic state machine model. The easiest would be do have a
> dunder-form like "__network_idle__" or something, which is an
> input pin with no message payload, a bit like "__init__".

I've extended POLite with an `idle()` handler that seems to work very
nicely for a handful for graph algorithms.  This was very
straighforward (~5 new lines of code in the softswitch).

> Some potential problems:
> 
> - Multi-user systems: if multiple applications and graphs are
>   running in the system then only one of them could use this
>   feature at once.
> 
> - Debugging: the termination detection would have persistent
>   state, so there needs to be a way of resetting it without
>   restarting all the boards (assuming we are moving towards
>   a more persistent model).
> 
> - Mixed-mode applications: an application might be running two
>   types of computation, with some threads performing interaction
>   with the host (for example), while others do the compute. In
>   the proposed model the entire machine has to be idle before
>   the next tick happens, so you can't really have these extra
>   threads doing their own thing.

I agree with these concerns, but as software can simply ignore the
feature, I decided to go ahead and implement it.  At least we can
evaluate its usefulness and in future address the above concerns
if/when they become a reality.

> But overall it looks like quite a simple extension that has
> some big potential wins, especially in bare-metal apps.
