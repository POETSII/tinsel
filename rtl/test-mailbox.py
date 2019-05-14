#!/usr/bin/python
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) Matthew Naylor
#
# This script:
#   * generates a random sequence of mailbox requests;
#   * feeds the requests to the mailbox test bench;
#   * reads back the resulting trace;
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
  print "Usage: test-mailbox.py"
  print ""
  print "  Environment variables:"
  print "    * SEED"
  print "    * NUM_ITERATIONS"
  print "    * INIT_DEPTH"
  print "    * INCR_DEPTH"
  print "    * TESTS_PER_DEPTH"
  print "    * NUM_THREADS"
  print "    * MAX_DELAY"
  print "    * LOG_DIR"

# Read options
try:
  seed          = int(os.environ.get("SEED", "0"))
  numIterations = int(os.environ.get("NUM_ITERATIONS", "10"))
  initDepth     = int(os.environ.get("INIT_DEPTH", "1000"))
  depthIncr     = int(os.environ.get("INCR_DEPTH", "1000"))
  testsPerDepth = int(os.environ.get("TESTS_PER_DEPTH", "100"))
  numThreads    = int(os.environ.get("NUM_THREADS", "4"))
  maxDelay      = int(os.environ.get("MAX_DELAY", "16"))
  logDir        = os.environ.get("LOG_DIR", "test-mailbox-log")
except:
  print "Invalid options"
  usage()
  sys.exit()

# Create log directory
subprocess.call("mkdir -p " + logDir, shell=True);

# Set random seed
random.seed(seed)

# Initialise number of operations to generate
numOps = initDepth

# =============================================================================
# Functions
# =============================================================================

# Generate a random list of requests
def genReqs():
  reqs = "D 100\n"
  for i in range(0, numOps):
    op = random.choice(['S']*4 + ['D'])
    thread = random.randint(0, numThreads-1)
    if op == 'D':
      delay = random.randint(1, maxDelay)
      reqs = reqs + "D " + str(delay) + "\n"
    elif op == 'W':
      word1 = random.randint(1, 1000)
      word2 = random.randint(1, 1000)
      reqs = reqs + "W " + str(thread) + " " + str(word1)
      reqs = reqs + " " + str(word2) + "\n"
    elif op == 'S':
      dest = random.randint(1, numThreads-1)
      reqs = reqs + "S " + str(thread) + " " + str(dest) + "\n"
  reqs = reqs + "D 100\nE\n"
  return reqs

# Generate a trace
def genTrace():
  reqs = genReqs()
  f = open(logDir + "/reqs.txt", 'w')
  f.write(reqs)
  f.close()
  try:
    reqsFile = logDir + "/reqs.txt"
    traceFile = logDir + "/trace.txt"
    cmd = "./testMailbox < " + reqsFile + "| grep -v Warn > " + traceFile
    #os.system(cmd)
    subprocess.call(cmd, shell=True);
  except:
    print "Problem invoking 'testMailbox'"
    sys.exit()
 
# Check a trace
def checkTrace():
  lineNum = 0
  traceFile = open(logDir + "/trace.txt", "rt")
  scratchpad = [ [t, 100+t] for t in range (0, numThreads) ]
  inbox = [ [] for t in range(0, numThreads) ]
  for line in traceFile:
    lineNum = lineNum+1
    fields = line.split()
    if fields[0] == 'S':
      src = int(fields[1])
      dst = int(fields[2])
      word1 = scratchpad[src][0]
      word2 = scratchpad[src][1]
      inbox[dst].append([word1, word2])
    elif fields[0] == 'R':
      thread = int(fields[1])
      word1 = int(fields[2])
      word2 = int(fields[3])
      found = False
      for i in range(0, len(inbox[thread])):
        msg = inbox[thread][i]
        if msg[0] == word1 and msg[1] == word2:
          found = True
          inbox[thread].pop(i)
          break
      if not found:
        print ("\nBad receive on line " + str(lineNum))
        return False
  return True
 
# =============================================================================
# Main
# =============================================================================

try:
  for i in range(0, numIterations):
    print "Depth", numOps
    for t in range(0, testsPerDepth):
      print t+1, "\r",
      sys.stdout.flush()
      genTrace()
      ok = checkTrace()
      if not ok:
        print ("Test failed.  For details, see directory '" + logDir + "/'")
        sys.exit()
    print "OK, passed", testsPerDepth, "tests"
    numOps = numOps+depthIncr
except:
  print "Exception. Exiting..."
  sys.exit();
