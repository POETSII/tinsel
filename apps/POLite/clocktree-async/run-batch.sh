#!/bin/bash

MHZ=250000000

POINTS="
1 1 2
2 1 3
3 1 4
4 1 5
5 1 6
6 1 7
7 1 8
8 1 9
9 1 10
10 1 11
12 1 13
14 1 15
16 1 17

1 2 3
2 2 7
3 2 15
4 2 31
5 2 63
6 2 127
7 2 255
8 2 511
9 2 1023
10 2 2047
12 2 8191
14 2 32767
16 2 131071

1 3 4
2 3 13
3 3 40
4 3 121
5 3 364
6 3 1093
7 3 3280
8 3 9841
9 3 29524
10 3 88573
12 3 797161
14 3 7174453

1 4 5
2 4 21
3 4 85
4 4 341
5 4 1365
6 4 5461
7 4 21845
8 4 87381
9 4 349525
10 4 1398101
12 4 22369621

1 5 6
2 5 31
3 5 156
4 5 781
5 5 3906
6 5 19531
7 5 97656
8 5 488281
9 5 2441406
10 5 12207031

1 6 7
2 6 43
3 6 259
4 6 1555
5 6 9331
6 6 55987
7 6 335923
8 6 2015539
9 6 12093235

1 7 8
2 7 57
3 7 400
4 7 2801
5 7 19608
6 7 137257
7 7 960800
8 7 6725601

1 8 9
2 8 73
3 8 585
4 8 4681
5 8 37449
6 8 299593
7 8 2396745
8 8 19173961

1 9 10
2 9 91
3 9 820
4 9 7381
5 9 66430
6 9 597871
7 9 5380840

1 10 11
2 10 111
3 10 1111
4 10 11111
5 10 111111
6 10 1111111
7 10 11111111

1 12 13
2 12 157
3 12 1885
4 12 22621
5 12 271453
6 12 3257437

1 14 15
2 14 211
3 14 2955
4 14 41371
5 14 579195
6 14 8108731

1 16 17
2 16 273
3 16 4369
4 16 69905
5 16 1118481
6 16 17895697
"

while read -r LINE; do
  if [ "$LINE" == "" ]; then
    echo
    continue
  fi
  D=$(echo $LINE | cut -d' ' -f 1)
  B=$(echo $LINE | cut -d' ' -f 2)
  N=$(echo $LINE | cut -d' ' -f 3)
  C=$(./run $D $B | grep Cycles | cut -d' ' -f 3)
  T=$(bc -l <<< "scale=8; $C/$MHZ")
  echo $D, $B, $N, $T
  sleep 6
done <<< $POINTS
