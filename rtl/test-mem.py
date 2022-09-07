#!/usr/bin/python
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) Matthew Naylor
#
# This script:
#   * generates a random sequence of memory requests;
#   * feeds the requests to the trace generator (see TestMem.bsv);
#   * passes the resulting trace the axe consistency checker;
#   * reports an error if the trace is invalid;
#   * repeats these steps, gradually increasing the number of requests.

import os
import sys
import random
import subprocess

# =============================================================================
# Initialisation
# =============================================================================

# Command-line usage
def usage():
  print("Usage: test-mem.py")
  print("")
  print("  Environment variables:")
  print("    * SEED")
  print("    * NUM_ITERATIONS")
  print("    * INIT_DEPTH")
  print("    * INCR_DEPTH")
  print("    * TESTS_PER_DEPTH")
  print("    * NUM_THREADS")
  print("    * NUM_ADDRS")
  print("    * MAX_DELAY")
  print("    * ASSOC")
  print("    * LOG_DIR")

# Read options
try:
  seed          = int(os.environ.get("SEED", "0"))
  numIterations = int(os.environ.get("NUM_ITERATIONS", "10"))
  initDepth     = int(os.environ.get("INIT_DEPTH", "1000"))
  depthIncr     = int(os.environ.get("INCR_DEPTH", "1000"))
  testsPerDepth = int(os.environ.get("TESTS_PER_DEPTH", "100"))
  numThreads    = int(os.environ.get("NUM_THREADS", "16"))
  numAddrs      = int(os.environ.get("NUM_ADDRS", "3"))
  maxDelay      = int(os.environ.get("MAX_DELAY", "8"))
  assoc         = int(os.environ.get("ASSOC", "4"))
  insertFlushes = int(os.environ.get("FLUSHES", "1"))
  logDir        = os.environ.get("LOG_DIR", "test-mem-log")
except:
  print("Invalid options")
  usage()
  sys.exit()

# Create log directory
os.system("mkdir -p " + logDir)

# Set random seed
random.seed(seed)

# Initialise number of operations to generate
numOps = initDepth

# There are two modes of test:
#  1. LineGrain: arbitrary addresses but with a
#     1->1 mapping between addresses and lines
#  2. Exclusive: arbitrary addresses but threads
#     access exclusive lines
LineGrain = 1
Exclusive = 2
mode = random.choice([LineGrain, Exclusive])

# Generate addresses for LineGrain mode
addrSet = []
for i in range(0, numAddrs):
  offset = random.randint(0, 200)
  for j in range(0, assoc+1):
    addrSet.append(4*offset + j*1024)

# Generate addresses for Exclusive mode
addrMap = []
for t in range(0, numThreads):
  offset = 4 * random.randint(0, 7)
  addrMap.append([a+(1024*(assoc+1)*(t+1))+offset for a in addrSet])

# =============================================================================
# Functions
# =============================================================================

# Generate a random list of requests
def genReqs():
  reqs = ""
  uniqueVal = 1;
  for i in range(0, numOps):
    flushes = ['F'] if random.randint(1,20) <= insertFlushes else []
    op = random.choice(['S']*7 + ['L']*5 + ['D'] + ['B'] + flushes)
    thread = random.randint(0, numThreads-1)
    if mode == LineGrain:
      addr = random.choice(addrSet)
    else:
      addr = random.choice(addrMap[thread])
    if op == 'D':
      delay = random.randint(1, maxDelay)
      reqs = reqs + "D " + str(delay) + "\n"
    elif op == 'B':
      delay = random.randint(1, maxDelay)
      reqs = reqs + "B " + str(delay) + "\n"
    elif op == 'L':
      reqs = reqs + "L " + str(thread) + " " + str(addr) + "\n"
    elif op == 'S':
      reqs = reqs + "S " + str(thread) + " " + str(addr)
      reqs = reqs + " " + str(uniqueVal) + "\n"
      uniqueVal = uniqueVal+1
    elif op == 'F':
      lineNum = random.randint(0,64)
      way = random.randint(0,4)
      reqs = (reqs + "F " + str(thread) + " " + str(lineNum) + 
                " " + str(way) + "\n")
  reqs = reqs + "E\n"
  return reqs

# Generate a trace
def genTrace():
  reqs = genReqs()
  f = open(logDir + "/reqs.txt", 'w')
  f.write(reqs)
  f.close()
  try:
    reqsFile = logDir + "/reqs.txt"
    traceFile = logDir + "/trace.axe"
    cmd = "./testMem < " + reqsFile + "| grep -v Warn > " + traceFile
    #os.system(cmd)
    subprocess.call(cmd, shell=True);
  except:
    print("Problem invoking 'traceMem'")
    sys.exit()
  
# Generate a trace and check it
def genTraceAndCheck():
  genTrace()
  try:
    p = subprocess.Popen(['axe', 'check', 'wmo', logDir + "/trace.axe"],
          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
          stderr=subprocess.PIPE)
    out = p.stdout.read()
    if out == "OK\n": return True
    else: return False
  except:
    print("Problem invoking 'axe'")
    print("Ensure that 'axe' is in your PATH")
    sys.exit()

# =============================================================================
# Main
# =============================================================================

for i in range(0, numIterations):
  print("Depth", numOps)
  for t in range(0, testsPerDepth):
    print(t+1, "\r", end=' ')
    sys.stdout.flush()
    ok = genTraceAndCheck()
    if not ok:
      print(("\nTest failed.  For details, see directory '" + logDir + "/'"))
      sys.exit()
  print("OK, passed", testsPerDepth, "tests")
  numOps = numOps+depthIncr
