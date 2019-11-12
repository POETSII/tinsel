# PIP-0022: Mailbox-local multicast

Author: Matthew Naylor

Revision: 2

## Proposal

We propose a new Tinsel feature to support multicasting (pins in the
POETS model) where a single message may be sent to multiple destinations.

The main idea is to allow a thread to multicast a message to any set
of threads on a given mailbox.  The Tinsel API will be extended with
the following new function:

```c++
// Send message to multiple threads on the given mailbox.
void tinselMulticast(
  uint32_t mboxDest,      // Destination mailbox
  uint32_t destMaskHigh,  // Destination bit mask (high bits)
  uint32_t destMaskLow,   // Destination bit mask (low bits)
  volatile void* addr);   // Message pointer
```

A maximum number of 64 threads per mailbox will be allowed, so that
all destinations can be specified in two 32-bit words.  The existing
`tinselSend()` function will still be supported, but will be
implemented in terms of `tinselMulticast()`.

The `tinselSlot()` and `tinselAlloc()` API calls will be dropped.
Instead, a single message slot will be reserved per thread for sending
messages, a pointer to which can be obtained by calling:

```c++
// Get pointer to thread's message slot reserved for sending.
volatile void* tinselSendSlot();
```

All other message slots will be implicitly made available for
receiving messages.  This means that hardware now owns these slots by
default, rather than software.  After reciving a message via
`tinselRecv()`, and processing it, a thread can indicate it has
finished with the message via a new function:

```c++
// Indicate that we've finished with the given message.
void tinselFree(void* addr);
```

## Motivation/Rationale

Threads will spend less time sending the same message to each
destination, one at a time.  This will give threads more time for
*consuming* messages, increasing throughput.

The mailbox will be capable of sending a message to multiple threads
independently in parallel, reducing NoC congestion, and only a single
copy of a multicast message will be kept in a mailbox, freeing
valuable buffer space.

## Design sketch

A mailbox serving N threads will now contain N queues.  Each queue
holds message-pointers into the scratchpad.  The set of destinations
for a multicast will be defined by a N-element bit vector.  On a
multicast operation, the message will be written to the next free
message slot in the scratchpad, and a pointer to that slot will be
inserted into each queue for which the bit-vector holds a '1'.

In addition, a reference count will be maintained for each message
slot indicating the total number of pointers to that slot in all the
queues.  When the reference count becomes 0, the hardware can reclaim
that message slot for a future incoming message.  A linked list of
unused message slots can be maintained in the reference count memory:
once a count reaches zero, that memory location can hold a pointer
(rather than a count) to the next free message slot.

Maintaing a separate message-pointer queue for every thread is
expensive in hardware: 1024 queues consumes around 20K ALMs (10% of
DE5-Net).  We propose to allow sharing of queues between threads using
a new parameter: `LogThreadsPerMulticastQueue`.  Queue sharing means
that a queue element cannot be consumed until all threads sharing the
queue have freed it, so the saving in ALMs is offset by reduced
parallelism.

## Impact

The changes to the Tinsel API are fairly minor, and it should be
trivial to adapt existing codebases (provided they don't use the
scratchpad as a general-purposes memory; I don't believe any existing
applications actually do this).

## Related ideas

This PIP has a similar goal to PIP 21, but reaches it in a completely
different way.  The two PIPs are orthogonal and could work well
together.  On the other hand, each PIP on its own may be sufficient to
advance application performance significantly.

## Comments

### _dt10_, 2019/09/25

This looks like a good idea to me, particularly for applications with
relatively local communication, and it general it would make better
use of slot resources due to the dynamic allocation. e.g. for DPD it
would work really well, and for anything on a mesh you'd get nice
improvements.

The hardware cost sounds like it might be a little high, but I guess
critical path can be handled through pipelining - a slack cycle here
or there will be dwarfed by the savings from all the software fanout
we get rid of.

This approach requires the compilation tools to be more
complex/sophisticated - not massively so, but it does require a bit
more data-structure work during placement/routing/generation. However,
that's what the orchestrator is there for, so that's not an issue.


I wasn't quite clear how this interacts with non-local sends - does
the original call still work for sending?  Does tinselCanSend determine
whether you can call tinselMulticast?


~~A downside of this method may also be partial synchronisation and
blocking within the mailbox. If a thread only has one unique send
slot, then it cannot prepare a new message until every other thread
has completed processing the previous one. So one really slow thread
could stop every other thread in the mailbox from making progress,
while in the current approach they would be able to keep moving forwards.~~
This was clarified, and is the wrong mental model.

Might it be better to allow each thread to only own one sending slot,
but that slot could be allocated dynamically?

## _mn416_, 2019/09/25

Thank you very much David for these helpful comments.

> If a thread only has one unique send slot, then it cannot prepare a
> new message until every other thread has completed processing the
> previous one.

> Might it be better to allow each thread to only own one sending slot,
> but that slot could be allocated dynamically?

So these questions make me realise we are both thinking about the
design slightly differently.

You are thinking that `tinselMulticast()` will simply cause a pointer
to thread's send slot to be inserted into each destination's queue.
Then we get the blocking problem, because that slot is stuck until all
receivers have consumed it.  But I was thinking that
`tinselMulticast()` will cause a *copy* of the message to be inserted
into the next free slot in the scratchpad, and a pointer to that slot
would then be inserted into each destination's queue.  With this
approach, there is no blocking problem as the send slot is available
to use before the message has reached all receivers.

But as you say, we could just allocate the send slot dynamically, i.e.
`tinselSendSlot()` would not return a reserved slot, but any free
slot.  I like this idea, but have a few slight reservations:

  * `tinselSendSlot()` can now fail, though we could arrange for
    it to always succeed when a thread doesn't currently own a send
    slot.  Nevertheless, there are cases when it can fail.

  * To avoid the blocking problem, threads need to call
    `tinselSendSlot()` for every `tinselSend()`, which is not needed
    in the current API.

> I wasn't quite clear how this interacts with non-local sends - does
> the original call still work for sending?  Does tinselCanSend determine
> whether you can call tinselMulticast?

Yes and yes.  The semantics of `tinselCanSend()` remains the same: a
thread can only have one message-pointer in the mailbox's transmit
buffer at a time, and `tinselCanSend()` returns false on a thread that
has a message-pointer sitting in the transmit buffer.

Thinking more about this proposal, I'd like to make a refinement.
Originally, I had thought that a multicast from one thread to other
threads on the same mailbox would be sufficient. To send a message to
multiple threads on another mailbox, you would first do a unicast send
to one of those threads, and then have that thread do a forwarding
multicast to the other threads.  But message forwarding in this way
bothers me because it seems to require unbounded buffering: to forward
a message you would like to receive it without consuming it, but this
can lead to deadlock because threads always need to be willing to
receive.  So the alternative is to buffer the messages to be forwarded
in memory, but this is unpleasant and raises the question of how big
the buffer needs to be.

To avoid this issue, I think I'll generalise the proposal so that
`tinselMulticast()` can target not only threads on the current
mailbox, but threads on any mailbox.

### dt10, 2019/09/26

Yup, I hadn't appreciated that the messages would still be copied,
which makes sense. It also hadn't occured to me that my mental
model of sharing buffers would require a lot of arbitration to
allow multiple threads to access each slot in the mailbox at the
some time, so it isn't practical for that reason either.

#### Issues around identical messages

Another concern that came up is that this model forces us to
really send exactly the same message to each receiver. At the
moment this holds at the application level, but at the lower
level the message is often customised. For example, adding information
about which edge it is coming on, embedding the destination
thread, or directly embedding edge properties.

Using this multi-cast model we'd have to send exactly the same
message to each thread, which causes a problem if there are
multiple receivers. For example, assume we have d0 and d1 on thread
A, and d2 and d3 on thread B, with edges d0.in <- d2.out and
d1.in <- d2.out. When thread B sends a message from d2.out, then
thread A will receive just one message coming from d2.out, even
though it goes to two places. So thread A will have to manually
fan it out to both d0.in and d1.in.

I suppose this is not really a problem, and is actually more
optimal in terms of messaging, as you move the minimum messages
over the network. In practise you'd need to build a data structure
mapping input addresses to all local fanout, so the receive
path looks something like:
```
msg *m=tinselRecv();
fanout f = lookup_fanout(m->source);
for(auto dest : f)
{
   dest.handler( dest.edge_properties, m->payload ); 
}
```
The function `lookup_fanout` would need to be efficient, though under
this new model the number of inter-thread edges would go down. Some
kind of hash-table would be pretty fast, and the orchestrator could probably
build a perfect hash function for each device when doing placement.

So I guess this is not really a problem, it's more an opportunity,
as it would reduce the number of messages moving around.


#### Inter- plus intra-thread multicast

> To avoid this issue, I think I'll generalise the proposal so that
> `tinselMulticast()` can target not only threads on the current
> mailbox, but threads on any mailbox.

If you're able to have the mailbox issue general reads to RAM, a
nice API would look like:
```c++
void tinselSend(uint32_t* localDestsBitMask, unsigned numNonLocalDest, const uint32_t *nonLocalDests, volatile void* addr);
```
So the mailbox would deliver the message to all the local threads in the
bit-mask, and also all the non-local addresses in the list. If the mailbox
was able to fetch and walk down the non-local lists independently of the
thread, then you'd effectively get some soft-switch compute time back in
order to run handlers. The cache of the thread is also not polluted by the
destination list, while the mailbox could optimise for the known access patterns
over the list (pre-fetching and so on).

It does require more logic in the mailbox though - it starts to look
more like a specialised processor.


#### Location of the bit-mask

Another minor point that came up is the location of the bit mask. I think
you've made it a pointer to a uint32_t as there could be more than 32 threads
in a mailbox - I think at the moment it is 64?

However, if it's a pointer then there is an issue about where that pointer
lives, and whether the mailbox will see the same value at that address. If
it's in cache RAM, then does the thread need to flush that value so the router
can see it? I guess not such a big problem if the tables are constant, but
it does mean there is some extra memory traffic.

Or possibly the pointer is written into registers of some sort within
the wrapper function, so the bit-mask is capture directly?

### mn416, 2019/09/27

I did a quick experiment, and 1024 of these per-thread message-pointer
queues costs about 20,000 ALMs (10% of the DE5).  As expected, that's
pretty expensive, but possibly in budget. One alternative is to have
every pair of threads share a queue, halving the requirement.  A
message in a shared queue cannot be freed until all threads sharing
the queue have freed it.

David, I agree entirely with all of your suggestions.

> I suppose this is not really a problem, and is actually more
> optimal in terms of messaging, as you move the minimum messages
> over the network. 

Yep, and I agree these receiver lookups could point to fairly small
arrays, and maybe that is something that placement can optimise, by
trying to group receivers together on the same thread.  After each
stage of partititioning in the hiearhical partitioning, we could add
edges between devices that connect to a common neighbouring partition,
to encourage METIS to keep them together.  Even if we don't do this, I
suspect the desired grouping will arise anyway for graphs that exhibit
locality, because the receviers are likely to be connected together
too (thinking DPD).

> If you're able to have the mailbox issue general reads to RAM, a
> nice API would look like ...  It does require more logic in the
> mailbox though - it starts to look more like a specialised
> processor.

It's a nice idea, and I think it's worth considering in the context of
PIP 21, which is a similar idea.

My gut feeling is that PIP 22 (this proposal) should already reduce
the cost of sending significantly (at east for medium to large
fanouts).  So perhaps the bottleneck of applications will shift to the
point where faster sending doesn't really help overall.

> Or possibly the pointer is written into registers of some sort
> within the wrapper function, so the bit-mask is capture directly?

Yes, I was thinking we'd use a two-operand custom instruction, so the
64-bit mask could be grabbed from two 32-bit registers.  For
simplicity, I will probably introduce the constraint that the number
of threads per mailbox must be <= 64.
