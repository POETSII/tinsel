# Results

## Old softswitch (without templates/accumulator etc):
```
Starting
Done
Time = 0.965256
```

## New softswitch:
```
Starting
Done
Time = 1.838475
```

## New softswitch (with if tinselCanRecv() rather than while()) 
```
Starting
Done
Time = 1.754674
```

## When actually enabling -O2, rather than -Os for the previous tests
```
Starting
Done
Time = 0.945482
```

## Changing the if canRecv() into while canRecv() 
```
Starting
Done
Time = 0.961798
```