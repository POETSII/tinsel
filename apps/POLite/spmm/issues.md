# Issues

## Stampeding
Assume a graph of the form:

    M1 -- M3
  /   \ /   \
I      X     O
  \   / \   / 
    M2 -- M4

When I updates, it sends the new values to M1 and M2. These then update, and each send their updated values to M3 and M4. Both receive 2 updates, resulting in 4 updates in total that are sent to O.
This problem gets significantly worse the bigger the network gets (#messages = number of paths through which you can reach that node, eg, for densely connected, the product of all the layer widths, in this case 2 * 2 = 4).

### Solutions
1. when you know the network topology, you can infer from what input it was sent on what the triggered inputs are going to be, though this requires global knowledge so is not applicable.
2. only emit when all input values have changed 
    - ISSUE: would defeat the purpose of partial updates
3. emit when we receive the next input update tag (set tags on the inputs)
    - ISSUE: multiple layers, as layer 2 will not see the initial update as 1 will only be sent a message when 0 sees the 2nd update.
    - unless you send update_1 followed by update_2 but then you still get multiple sets of updates
4. fire off the updated values on a timer, layer by layer (this is essentially turning the process into a global batch system, applying synchronisation) 
    - ISSUE: this prevent low-latency signal forwarding, as the graph 
5. only send a message to the output when there is a sufficiently big change or when you haven't sent one in the last X seconds
    - ISSUE: this network will not converge to the final output
        - FIX: also emit values on a timer when they've changed less (ie. not every update)


Combination of 4 and 5? : send when either
1. on big change (can be tweaked based on how long the last time of sending a message was)
2. on trigger (when haven't sent an update in a while)


#Encountered issues


## Message reordering
In this case I'm only increasing the node values by setting the input to non zero (weights > 0)
Note that the bottom lines, the value is lower than the old value, meaning that there has been some message reordering somewhere down the line (either in the POETS fabric or in the sending event loop)
```
total=2000
Creating layer of size=5
Creating layer of size=5
Creating layer of size=5
Creating layer of size=1
2018-10-16 19:22:37
Running ./build/run
Run on (12 X 3501 MHz CPU s)
CPU Caches:
  L1 Data 32K (x6)
  L1 Instruction 32K (x6)
  L2 Unified 1024K (x6)
  L3 Unified 8448K (x1)
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
HOST: Sending value 1 to ptid=0x1580 uc=0x1
HOST: Received output dest=0 exp=2000 v=2 src=0x15800001 last_update=0x7cc9a18c
HOST: Received output dest=0 exp=2000 v=4 src=0x16800001 last_update=0x7cc9ac4f
HOST: Received output dest=0 exp=2000 v=4 src=0x14800001 last_update=0x7cc9ad48
HOST: Received output dest=0 exp=2000 v=4 src=0x15c00001 last_update=0x7cc9ae7d
HOST: Received output dest=0 exp=2000 v=4 src=0x15400001 last_update=0x7cc9af87
HOST: Received output dest=0 exp=2000 v=4 src=0x11c00001 last_update=0x7e362271
HOST: Received output dest=0 exp=2000 v=16 src=0x10800001 last_update=0x7e362f67
HOST: Received output dest=0 exp=2000 v=48 src=0x10800001 last_update=0x7e364065
HOST: Received output dest=0 exp=2000 v=40 src=0x10c00001 last_update=0x7e36488c
HOST: Received output dest=0 exp=2000 v=40 src=0x11400001 last_update=0x7e364b03
HOST: Received output dest=0 exp=2000 v=40 src=0x1a800001 last_update=0x7b4efc5b
HOST: Received output dest=0 exp=2000 v=40 src=0x11800001 last_update=0x7e364d1e
HOST: Received output dest=0 exp=2000 v=40 src=0x19800001 last_update=0x7b4f03f5
HOST: Received output dest=0 exp=2000 v=112 src=0x10800001 last_update=0x7e364fee
HOST: Received output dest=0 exp=2000 v=256 src=0x10800001 last_update=0x7e36610c
HOST: Received output dest=0 exp=2000 v=368 src=0x10800001 last_update=0x7e36736e
HOST: Received output dest=0 exp=2000 v=336 src=0x10800001 last_update=0x81037803
HOST: Received output dest=0 exp=2000 v=240 src=0x10800001 last_update=0x8104d7c5
HOST: Received output dest=0 exp=2000 v=304 src=0x10800001 last_update=0x810524b0
HOST: Received output dest=0 exp=2000 v=304 src=0x10800001 last_update=0x810571ab
0:1:8:0: Found existing contrib, d=ffffffc0 src=1a800001 val=00000008 ov=00000028 w=00000002
0:1:8:0: Emitting lower output, hi=00000170 cu=00000150
0:1:8:0: Found existing contrib, d=ffffffc0 src=11800001 val=00000008 ov=00000028 w=00000002
0:1:8:0: Found existing contrib, d=ffffffe0 src=11400001 val=00000018 ov=00000028 w=00000002
0:1:8:0: Emitting lower output, hi=00000170 cu=000000f0
0:1:8:0: Emitting lower output, hi=00000170 cu=00000130
0:1:8:0: Emitting lower output, hi=00000170 cu=00000130
```

# Further issues
- Not all messages seem to get delivered -> fix: you can probe the nodes to re-emit the values periodically, however, when you do this the ordering becomes vague. A message resulting from a probe might be re-ordered with one of the messages that isn't a probe, which is bad, as they might have older values if you probe while the network is still propagating.