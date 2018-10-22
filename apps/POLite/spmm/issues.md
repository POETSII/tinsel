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


# Comparison with x64
## scipy sparse
```
for s in range(2, 8):
    s = 10**s
    density = float(100) / (s**2)
    ms_sp = [sparse.random(s, s, density=density, format='csr') for _ in range(5)]
    ms_sp.append(sparse.random(1, s, density=density, format='csr'))

    def full_calc_sparse(res):
        for m in ms_sp:
            res = m.dot(res)
        return res

    print("size={}".format(s))
    res = sparse.random(s, 1, density=density, format='csr')
    %timeit full_calc_sparse(res)

size=100
693 µs ± 25.9 µs per loop (mean ± std. dev. of 7 runs, 1000 loops each)
size=1000
711 µs ± 4.42 µs per loop (mean ± std. dev. of 7 runs, 1000 loops each)
size=10000
921 µs ± 4.04 µs per loop (mean ± std. dev. of 7 runs, 1000 loops each)
size=100000
2.2 ms ± 22.4 µs per loop (mean ± std. dev. of 7 runs, 100 loops each)
size=1000000
16.3 ms ± 374 µs per loop (mean ± std. dev. of 7 runs, 100 loops each)
size=10000000
214 ms ± 1.6 ms per loop (mean ± std. dev. of 7 runs, 1 loop each)
```
## numpy dense
```
ms = [np.matrix(np.random.random((10,10))) for _ in range(5)]
ms.append(np.matrix(np.random.random((1,10))))

def full_calc(res):
    for m in ms:
        res = np.dot(m, res)
    return res

res = np.ones((10,1))
%timeit full_calc(res)
7.98 µs ± 26.6 ns per loop (mean ± std. dev. of 7 runs, 100000 loops each)
```
#Encountered issues


## Message reordering
In this case I'm only increasing the node values by setting the input to non zero (weights > 0)
Note that the bottom lines, the value is lower than the old value, meaning that there has been some message reordering somewhere down the line (either in the POETS fabric or in the sending event loop).
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
SOLUTION: adding a sequence count to every connection, and only accepting the newest connection rather than the oldest one.

## Not all messages get delivered
- Not all messages seem to get delivered -> fix: you can probe the nodes to re-emit the values periodically, however, when you do this the ordering becomes vague. A message resulting from a probe might be re-ordered with one of the messages that isn't a probe, which is bad, as they might have older values if you probe while the network is still propagating. Occasionally it works however:
```
total=2000
Creating layer of size=5
Creating layer of size=5
Creating layer of size=5
Creating layer of size=1
2018-10-16 20:22:49
Running ./build/run
Run on (12 X 3501 MHz CPU s)
CPU Caches:
  L1 Data 32K (x6)
  L1 Instruction 32K (x6)
  L2 Unified 1024K (x6)
  L3 Unified 8448K (x1)
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
HOST: Sending value 1 to ptid=0x1580 uc=0x1
HOST: Sending value 1 to ptid=0x19c0 uc=0x2
HOST: Sending value 1 to ptid=0x12c0 uc=0x3
HOST: Sending value 1 to ptid=0x1ac0 uc=0x4
HOST: Sending value 1 to ptid=0x1900 uc=0x5
HOST: Received output dest=0 exp=2000 v=16 src=0x10800001 last_update=0x0
HOST: Received output dest=0 exp=2000 v=48 src=0x10800001 last_update=0x1
HOST: Received output dest=0 exp=2000 v=176 src=0x10800001 last_update=0x2
HOST: Received output dest=0 exp=2000 v=448 src=0x10800001 last_update=0x3
HOST: Received output dest=0 exp=2000 v=656 src=0x10800001 last_update=0x4
HOST: Received output dest=0 exp=2000 v=1120 src=0x10800001 last_update=0x5
HOST: Received output dest=0 exp=2000 v=1264 src=0x10800001 last_update=0x6
HOST: Received output dest=0 exp=2000 v=1392 src=0x10800001 last_update=0x7
HOST: Received output dest=0 exp=2000 v=1584 src=0x10800001 last_update=0x8
HOST: Received output dest=0 exp=2000 v=1728 src=0x10800001 last_update=0x9
HOST: Received output dest=0 exp=2000 v=1808 src=0x10800001 last_update=0xa
HOST: Received output dest=0 exp=2000 v=2000 src=0x10800001 last_update=0xb
HOST: Sending value 0 to ptid=0x1580 uc=0x6
HOST: Sending value 0 to ptid=0x19c0 uc=0x7
HOST: Sending value 0 to ptid=0x12c0 uc=0x8
HOST: Sending value 0 to ptid=0x1ac0 uc=0x9
HOST: Sending value 0 to ptid=0x1900 uc=0xa
HOST: Received output dest=0 exp=0 v=1984 src=0x10800001 last_update=0xc
HOST: Received output dest=0 exp=0 v=1952 src=0x10800001 last_update=0xd
HOST: Received output dest=0 exp=0 v=1888 src=0x10800001 last_update=0xe
HOST: Received output dest=0 exp=0 v=1632 src=0x10800001 last_update=0xf
HOST: Received output dest=0 exp=0 v=1296 src=0x10800001 last_update=0x10
HOST: Received output dest=0 exp=0 v=880 src=0x10800001 last_update=0x11
HOST: Received output dest=0 exp=0 v=784 src=0x10800001 last_update=0x12
HOST: Received output dest=0 exp=0 v=656 src=0x10800001 last_update=0x13
HOST: Received output dest=0 exp=0 v=528 src=0x10800001 last_update=0x14
HOST: Received output dest=0 exp=0 v=272 src=0x10800001 last_update=0x15
HOST: Received output dest=0 exp=0 v=0 src=0x10800001 last_update=0x16
HOST: Sending value 1 to ptid=0x1580 uc=0xb
HOST: Sending value 1 to ptid=0x19c0 uc=0xc
HOST: Sending value 1 to ptid=0x12c0 uc=0xd
HOST: Sending value 1 to ptid=0x1ac0 uc=0xe
HOST: Sending value 1 to ptid=0x1900 uc=0xf
HOST: Received output dest=0 exp=2000 v=16 src=0x10800001 last_update=0x17
HOST: Received output dest=0 exp=2000 v=48 src=0x10800001 last_update=0x18
HOST: Received output dest=0 exp=2000 v=112 src=0x10800001 last_update=0x19
HOST: Received output dest=0 exp=2000 v=288 src=0x10800001 last_update=0x1a
HOST: Received output dest=0 exp=2000 v=672 src=0x10800001 last_update=0x1b
HOST: Received output dest=0 exp=2000 v=1072 src=0x10800001 last_update=0x1c
HOST: Received output dest=0 exp=2000 v=1216 src=0x10800001 last_update=0x1d
HOST: Received output dest=0 exp=2000 v=1360 src=0x10800001 last_update=0x1e
HOST: Received output dest=0 exp=2000 v=1520 src=0x10800001 last_update=0x1f
HOST: Received output dest=0 exp=2000 v=1792 src=0x10800001 last_update=0x20
```
SOLUTION: implementing a different POLite semantics on top of tinsel that allows you to interrupt multicast

## The execution time seems to be exponential with respect to the number of layers
----------------------------------------------------------
Benchmark                   Time           CPU Iterations
----------------------------------------------------------
standard test/1/10        502 us        502 us       1393
standard test/2/10       1484 us       1484 us        473
standard test/3/10       2304 us       2303 us        304
standard test/4/10       4794 us       4793 us        146
standard test/5/10      18814 us      18812 us         40
standard test/6/10      34823 us      34819 us         20

This is most likely caused by the number of messages sent/received, which could be lots less.
Question - how to reduce this? Only send values on triggers rather than on every update?

When implementing the triggering - send an update (if changed) every Nth iteration of the event loop (with trigger time 8 iters):

(num_layers, layer_size, trigger_time)
------------------------------------------------------------
Benchmark                     Time           CPU Iterations
------------------------------------------------------------
standard test/1/10/8        501 us        501 us       1393
standard test/2/10/8        666 us        666 us       1050
standard test/3/10/8        800 us        800 us        878
standard test/4/10/8        937 us        937 us        743
standard test/5/10/8       1101 us       1101 us        633
standard test/6/10/8       1542 us       1542 us        463

This does shift the growth from the previous exponential to what appears to be roughly linear. However, the total results are still roughly in the order of 1ms for the 5 10x10 matrix mults. This, compared to the numpy dense matrix multiplication, which runs in about 7 us. However, it is roughly comparable to the scipy sparse matrix implementation, which takes about the same time when the values are spread across a 10000x10000 matrix, which could be the case in big sparse networks. Furthermore, the POETS approach will scale fine as the network gets wider due to the parallelism, whereas scipy will start to deteriorate.