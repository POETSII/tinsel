# PIP-0024: Programmable routers and global multicast

Author: Matthew Naylor

This proposal replaces PIP 21.

## Proposal

We propose to generalise the destination component of a message so
that it can be (1) a thread id; or (2) a **routing key**.  A message,
sent by a thread, containing a routing key as a destination will go to
a **per-board router** on the same FPGA.  The router will use they key
as an index into a DRAM-based routing table and automatically
propagate the message towards all the destinations associated with
that key.

## Motivation/Rationale

PIP 22 resulted in a *mailbox-level* multicast feature, implemented in
Tinsel 0.7.  It enables each thread to send to a message
simultaneously to any subset of the 64 threads on a destination
mailbox.  It works well when graphs exhibit good locality, with
destination vertices often collocated on the same mailbox.

However, it has a few drawbacks:

  1. Costly graph partitioning algorithms are needed to identify
     locality. This is problematic for graphs with billions of edges
     and vertices, because mapping time may significantly outweigh
     execution time.  (Indeed, graph partitioning is itself an
     interesting application for the hardware.)

  2. In some graphs there are limits to how well destination vertices
     can be collocated after partitioning.  For example, *small-world
     graphs* contain some extremely large, highly-distributed fanouts.

A *global multicast* feature should reduce the need to find optimal
partitions for very large graphs, and support distributed fanouts.  It
should also move work away from the cores and into the hardware
routers: the softswitch no longer needs to iterate over the outgoing
edges of a pin.  While providing these improvements, it is also
important to maintain the advantages of the existing mailbox-level
multicast, for applications in which the mapping time is not a
concern.

## Functional overview

A **routing key** is a 32-bit value consisting of a *ram id*, an
*address*, and a *size*:

```sv
// 32-bit routing key (MSB to LSB)
typedef struct {
  // Which off-chip RAM on this board?
  Bit#(`LogDRAMsPerBoard) ram;
  // Pointer to array of routing beats containing routing records
  Bit#(`LogBeatsPerDRAM) ptr;
  // Number of beats in the array
  Bit#(`LogRoutingEntryLen) numBeats;
} RoutingKey;
```

When a message reaches the per-board router, the `ptr` field of the
routing key is used as an index into DRAM, where a sequence of 256-bit
**routing beats** are found.  The `numBeats` field of the routing key
indicates how many contiguous routing beats there are.  Knowing the
size before the lookup makes the hardware simpler and more efficient,
e.g. it can avoid blocking on responses and issue a burst of an
appropriate size.  The value of `numBeats` may be zero.

A routing beat consists of a *size* and a sequence of five 48-bit
*routing chunks*:

```sv
// 256-bit routing beat (aligned, MSB to LSB)
typedef struct {
  // Number of routing records present in this beat
  Bit#(16) size;
  // Five 48-bit record chunks
  Vector#(5, Bit#(48)) chunks;
} RoutingBeat;
```

The *size* must lie in the range 1 to 5 inclusive (0 is disallowed).
A **routing record** consists of one or two routing chunks, depending
on the **record type**.

All byte orderings are little endian.  For example, the order of bytes
in a routing beat is as follows.

```
Byte  Contents
----  --------
31:   Upper byte of length (i.e. number of records in beat)
30:   Lower byte of length
29:   Upper byte of first chunk
      ...
24:   Lower byte of first chunk
23:   Upper byte of second chunk
      ...
18:   Lower byte of second chunk
17:   Upper byte of third chunk
      ...
12:   Lower byte of third chunk
11:   Upper byte of fourth chunk
      ...
 6:   Lower byte of fourth chunk
 5:   Upper byte of fifth chunk
      ...
 0:   Lower byte of fifth chunk
```

Clearly, both routing keys and routing beats have a maximum size.
However, in principle there is no limit to the number of records
associated with a key, due to the possibility of *indirection records*
(see below).

There are five types of routing record, defined below.

**48-bit Unicast Router-to-Mailbox (URM1).**

```sv
typedef struct {
  // Record type (URM1 == 0)
  Bit#(3) tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Unused
  Bit#(3) unused;
  // Local key. The first word of the message
  // payload is overwritten with this.
  Bit#(32) localKey;
} URM1Record;
```

The `localKey` can be used for anything, but might encode the
destination thread-local device identifier, or edge identifier, or
both.  The `mbox` field is currently 4 bits (two Y bits followed by
two X bits), but there are spare bits available to increase the size
of this field in future if necessary.

**96-bit Unicast Router-to-Mailbox (URM2).**

```sv
typedef struct {
  // Record type (URM2 == 1)
  Bit#(3) tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Currently unused
  Bit#(19) unused;
  // Local key. The first two words of the message
  // payload is overwritten with this.
  Bit#(64) localKey;
} URM2Record;
```

This is the same as a URM1 record except the local key is 64-bits in
size.

**48-bit Router-to-Router (RR).**

```sv
typedef struct {
  // Record type (RR == 2)
  Bit#(3) tag;
  // Direction (N,S,E,W == 0,1,2,3)
  Bit#(2) dir;
  // Currently unused
  Bit#(11) unused;
  // New 32-bit routing key that will replace the one in the
  // current message for the next hop of the message's journey
  Bit#(32) newKey;
} RRRecord;
```

The `newKey` field will replace the key in the current message for the
next hop of the message's journey.  Introducing a new key at each hop
simplifies the mapping process (keeping it quick).

**96-bit Multicast Router-to-Mailbox (MRM).**

```sv
typedef struct {
  // Record type (MRM == 3)
  Bit#(3) tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Currently unused
  Bit#(9) unused;
  // Local key. The least-significant half-word
  // of the message is replaced with this
  Bit#(16) localKey;
  // Mailbox-local destination mask
  Bit#(64) destMask;
} MRMRecord;
```

**48-bit Indirection (IND).**

```sv
// 48-bit Indirection (IND) record
// Note the restrictions on IND records:
// 1. At most one IND record per key lookup
// 2. A max-sized key lookup must contain an IND record
typedef struct {
  // Record type (IND == 4)
  Bit#(3) tag;
  // Currently unused
  Bit#(13) unused;
  // New 32-bit routing key for new set of records on current router
  Bit#(32) newKey;
} INDRecord;
```

Indirection records can be used to handle large fanouts, which exceed
the number of bits available in the size portion of the routing key.

## Impact

Since use of routing keys is optional, existing applications will
continue to work unmodified.
