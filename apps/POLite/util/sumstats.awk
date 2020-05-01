#!/usr/bin/awk -f
# SPDX-License-Identifier: BSD-2-Clause

BEGIN {
  cycleCount = 0;
  missCount = 0;
  hitCount = 0;
  writebackCount = 0;
  cpuIdleCount = 0;
  cacheCount = 0;
  coreCount = 0;
  cacheLineSize = 32;
  msgsReceived = 0;
  msgsSent = 0;
  progRouterSent = 0;
  progRouterSentInter = 0;
  fmax = 220000000;
  if (boardsX == "" || boardsY == "") {
    boardsX = 3;
    boardsY = 2;
  }
}

{
  if (match($0, /(.*):(.*):(.*):(.*): /, coords)) {
    bx = strtonum(coords[1]);
    by = strtonum(coords[2]);
    if (bx < boardsX && by < boardsY) {
      # Cache hit/miss/writeback counts
      if (match($0, /(.*) H:(.*),M:(.*),W:(.*)/, fields)) {
        h=strtonum("0x"fields[2]);
        m=strtonum("0x"fields[3]);
        w=strtonum("0x"fields[4]);
        hitCount = hitCount + h;
        missCount = missCount + m;
        writebackCount = writebackCount + w;
        cacheCount = cacheCount+1;
      }
      # CPU cycle/idle counts
      else if (match($0, /(.*) C:(.*) (.*),I:(.*) (.*)/, fields)) {
        c1=strtonum("0x"fields[2]);
        c0=strtonum("0x"fields[3]);
        c = c1 * 4294967296 + c0
        i1=strtonum("0x"fields[4]);
        i0=strtonum("0x"fields[5]);
        i = i1 * 4294967296 + i0
        cycleCount = cycleCount + c;
        cpuIdleCount = cpuIdleCount + i;
        coreCount = coreCount+1;
      }
      # Per-thread message counts
      else if (match($0, /(.*) MS:(.*),MR:(.*),PR:(.*),PRI:(.*)/, fields)) {
        ms=strtonum("0x"fields[2]);
        mr=strtonum("0x"fields[3]);
        pr=strtonum("0x"fields[4]);
        pri=strtonum("0x"fields[5]);
        msgsSent = msgsSent + ms;
        msgsReceived = msgsReceived + mr;
        progRouterSent = progRouterSent + pr;
        progRouterSentInter = progRouterSentInter + pri;
      }
    }
  }
}

END {
  print "Assuming", (boardsX*boardsY), "boards: ", boardsX, "x", boardsY
  time = (cycleCount/coreCount)/fmax
  print "Time (s): ", time
  missRate = 100*(missCount/(hitCount+missCount))
  print "Miss rate (%): ", 100*(missCount/(hitCount+missCount))
  print "Hit rate (%): ", (100-missRate)
  bytes = cacheLineSize * (missCount + writebackCount)
  print "Off-chip memory (GBytes/s): ", ((1/time) * bytes)/1000000000
  print "CPU util (%): ", (1-(cpuIdleCount/cycleCount))*100
  print "Msgs received: ", msgsReceived
  print "Msgs sent by threads: ", msgsSent
  print "Msgs injected by ProgRouter:", progRouterSent
  print "Inter-board msgs:", progRouterSentInter
  print ""
  print "Notes:"
  print "  * ProgRouter injections includes inter-board msgs"
  print "  * Memory bandwidth does not include lookups by ProgRouter"
  print "  * If runtime > 40s approx, hit/miss counts may overflow"
}
