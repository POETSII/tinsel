# Baremetal GALS Heat

Performance from [Baremetal GALS Heat code](heat.c)
running 50,000 time steps on Tinsel:

  FPGAs       | Cells      | Cells/FPGA | Runtime (s)
  ----------- | ---------- | ---------- | -----------
  1 (1x1)     | 65,536     | 65,536     | 6.2
  4 (2x2)     | 262,144    | 65,536     | 6.2
  9 (3x3)     | 589,824    | 65,536     | 6.4
  12 (3x4)    | 786,432    | 65,536     | 6.5
  16 (4x4)    | 1,048,576  | 65,536     | 6.6
  24 (6x4)    | 1,572,864  | 65,536     | 6.8
  48 (6x8)    | 3,145,728  | 65,536     | 7.8

These runtimes include the time to transfer the final cell states to
the host PC, which probably accounts for some of the slight increase in
execution time for larger grids.  HostLink does not yet support bulk
receives, and that would probably help.

Baseline performance from a [single-thread PC implementation](pc/scalar.cpp)
running 50,000 time steps on an Intel i9:

  Cells       | Runtime (s)
  ----------- | ----------
  65,536      | 2.4
  262,144     | 9.7
  589,824     | 21.7
  786,432     | 29.7
  1,048,576   | 38.0
  1,572,864   | 58.5
  3,145,728   | 126.2
