BEGIN {
  cycleCount = 0;
  missCount = 0;
  hitCount = 0;
  writebackCount = 0;
  cpuIdleCount = 0;
  lineCount = 0;
  cacheLineSize = 32;
  intraThreadSendCount = 0;
  interThreadSendCount = 0;
  interBoardSendCount = 0;
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
  else if (match($0, /(.*) LS:(.*),TS:(.*),BS:(.*)/, fields)) {
    ls=strtonum("0x"fields[2]);
    ts=strtonum("0x"fields[3]);
    bs=strtonum("0x"fields[4]);
    intraThreadSendCount = intraThreadSendCount+ls;
    interThreadSendCount = interThreadSendCount+ts;
    interBoardSendCount = interBoardSendCount+bs;
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
  print "Intra-thread messages: ", intraThreadSendCount
  print "Inter-thread messages: ", interThreadSendCount
  print "Inter-board messages: ", interBoardSendCount
}
