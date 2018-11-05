BEGIN {
  cycleCount = 0;
  missCount = 0;
  hitCount = 0;
  writebackCount = 0;
  cpuIdleCount = 0;
  lineCount = 0;
  cacheLineSize = 32;
  fmax = 250000000;
}

{
  if (match($0, /(.*) C:(.*),H:(.*),M:(.*),W:(.*),I:(.*)/, fields)) {
    c=strtonum("0x"fields[2]);
    h=strtonum("0x"fields[3]);
    m=strtonum("0x"fields[4]);
    w=strtonum("0x"fields[5]);
    i=strtonum("0x"fields[6]);
    cycleCount = cycleCount + c;
    hitCount = hitCount + h;
    missCount = missCount + m;
    writebackCount = writebackCount + w;
    cpuIdleCount = cpuIdleCount + i;
    lineCount = lineCount+1;
  }
}

END {
  time = (cycleCount/lineCount)/fmax
  print "Time (s): ", time
  missRate = 100*(missCount/(hitCount+missCount))
  print "Miss rate (%): ", 100*(missCount/(hitCount+missCount))
  print "Hit rate (%): ", (100-missRate)
  bytes = cacheLineSize * (missCount + writebackCount)
  print "Off-chip memory (GBytes/s): ", ((1/time) * bytes)/1000000000
  print "CPU util (%): ", (1-(cpuIdleCount/cycleCount))*100
}
