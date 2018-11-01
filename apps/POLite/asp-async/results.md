# Results

# Initial (N=448)
Max fan-out = 14
Started
Sum of subset of shortest paths = 1943502336
Time = 0.614373

# With repeated recv (N=448)
Max fan-out = 14
Started
Sum of subset of shortest paths = 1943502336
Time = 0.609871


# Async

# N= 16 (16 bit distances)
Max fan-out = 14
Started
Sum of subset of shortest paths = 37774192
Time = 0.157334

Max fan-out = 14
Started
Sum of subset of shortest paths = 38676605
Time = 0.170337

Max fan-out = 14
Started
Sum of subset of shortest paths = 38896876
Time = 0.160729

Max fan-out = 14
Started
Sum of subset of shortest paths = 39843347
Time = 0.171023

## fixed up integer overflow by not using the distance field for reporting
Max fan-out = 14
Started
Sum of subset of shortest paths = 66921661
Time = 0.163861

Max fan-out = 14
Started
Sum of subset of shortest paths = 66922474
Time = 0.162178

Max fan-out = 14
Started
Sum of subset of shortest paths = 65881254
Time = 0.277176

Max fan-out = 14
Started
Sum of subset of shortest paths = 66618179
Time = 0.283698

# N = 8
Max fan-out = 14
Started
Sum of subset of shortest paths = 35122131
Time = 0.159188

# Issue : 
cannot simply OR together the reaching flags - even though this is logically true, it does not result in the best solution, it only results in the first solution. Eg.

A
 \
  C - D   
 /     \
B - E - F

If first ACD arrives, and for some reason BEFD arrives before BCD, D will think the fastest path to B is BEFD, not BCD (which is quicker). This issue does not occur in GALS because the distance is equivalent to the timestep, which is synchronized hence BCD cannot arrive after BEFD. This could be solved with the tinselIdle condition, which will flag true when there is no more messages in the system.

## N = 8
### RT = 32
Max fan-out = 14
Started
Sum of subset of shortest paths = 36112597
Time = 0.149142

### RT = 64
Max fan-out = 14
Started
Sum of subset of shortest paths = 34074980
Time = 0.273596

## N = 16
### RT = 32
Max fan-out = 14
Started
Sum of subset of shortest paths = 66241321
Time = 0.284625

### RT = 64
Max fan-out = 14
Started
Sum of subset of shortest paths = 64060523
Time = 0.171687


## N = 24
### RT = 32
Max fan-out = 14
Started
Sum of subset of shortest paths = 99074872
Time = 0.188952

Max fan-out = 14
Started
Sum of subset of shortest paths = 97325710
Time = 0.188463

### RT = 64
Max fan-out = 14
Started
Sum of subset of shortest paths = 96695416
Time = 0.323904

Max fan-out = 14
Started
Sum of subset of shortest paths = 230331193
Time = 0.335338

Max fan-out = 14
Started
Sum of subset of shortest paths = 94745325
Time = 0.285296

## With idle detection (on byron)
- do not run the check on every event loop iteration - makes quite a significant difference


### triggerTime = 2
mb2054@byron:~/asp$ ./build/run edges.txt 
Max fan-out = 14
Started
sum=343277028 time=1162.23ms

### triggerTime = 40
mb2054@byron:~/asp$ ./build/run edges.txt 
Max fan-out = 14
Started
sum=61484934 time=415.666ms

### triggerTime = 200
mb2054@byron:~/asp$ ./build/run edges.txt 
Max fan-out = 14
Started
sum=68880950 time=417.242ms

Values do not match, probably because the idle detection kicks in before the problem is completed due to the larger 

## Falling back to no idle detection as Byron is being flashed
When implementing a fast path - forwarding updates as they become available the system chokes with messages and no longer converges within a reasonable time. The alternative is to do trigger-based forwarding, but this requires you to configure a batch size. 

## Keeping an explicit list of sources to update that can be enumerated rather than needs to be scanned for

### TriggerTime = 20 

#### sources=2 update_slots=1
mb2054@coleridge:~/tinsel/apps/POLite/asp-async$ ./build/run edges.txt 
Max fan-out = 14
Started
total_messages=393799 graph_size=279936 sum=9407403 time=502.525ms

#### With 4 update slots
Max fan-out = 14
Started
total_messages=357931 graph_size=279936 sum=9441440 time=472.828ms

#### With 14 update slots
Max fan-out = 14
Started
total_messages=363152 graph_size=279936 sum=9436923 time=467.05ms

Seems to make no big difference

### TriggerTime = 5 (14 update slots)
Max fan-out = 14
Started
total_messages=376647 graph_size=279936 sum=9252360 time=485.423ms

### TT = 50
Max fan-out = 14
Started
total_messages=333359 graph_size=279936 sum=9616307 time=439.905ms
                                            
### TT = 250
Max fan-out = 14
Started
total_messages=362138 graph_size=279936 sum=9619722 time=469.386ms

Note, these cause the sum to increase quite a lot - so there are just less messages being sent hence the shorter execution time.


Still - the sums do not match and until then there is little point comparing performance as the results are non-deterministic for deterministic problems.

## Conclusion
- it's very hard to reason about the cases in which POETS is programmed in a fully async fashion (eg. not GALS).
Messages go missing, 
