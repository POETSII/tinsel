#!/usr/bin/python
#
# Copyright (c) Matthew Naylor
#
# This script:
#   * generates a random sequence of enq & deq requests;
#   * feeds the requests to the ArrayOfQueue test bench;
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
  print "Usage: test-array-of-queue.py"
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
  numQueues     = int(os.environ.get("NUM_QUEUES", "4"))
  maxDelay      = int(os.environ.get("MAX_DELAY", "5"))
  logDir        = os.environ.get("LOG_DIR", "test-array-of-queue-log")
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
  reqs = ""
  for i in range(0, numOps):
    op = random.choice(['I']*2 + ['R']*2 + ['D', 'C'])
    index = random.randint(0, numQueues-1)
    if op == 'D':
      delay = random.randint(1, maxDelay)
      reqs = reqs + "D " + str(delay) + "\n"
    elif op == 'C':
      delay = random.randint(1, maxDelay)
      reqs = reqs + "C " + str(delay) + "\n"
    elif op == 'I':
      item = random.randint(1, 1000)
      reqs = reqs + "I " + str(index) + " " + str(item) + "\n"
    elif op == 'R':
      reqs = reqs + "R " + str(index) + "\n"
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
    traceFile = logDir + "/trace.txt"
    cmd = "./testArrayOfQueue < " + reqsFile + "| grep -v Warn > " + traceFile
    #os.system(cmd)
    subprocess.call(cmd, shell=True);
  except:
    print "Problem invoking 'testArrayOfQueue'"
    sys.exit()
 
# Check a trace
def checkTrace():
  traceFile = open(logDir + "/trace.txt", "rt")
  queues = [ [] for q in range(0, numQueues) ]
  # Perform all enqs
  for line in traceFile:
    fields = line.split()
    if fields[0] == 'I':
      index = int(fields[1])
      item = int(fields[2])
      queues[index].append(item)
  # Perform all deqs
  lineNum = 0
  for line in traceFile:
    lineNum = lineNum+1
    fields = line.split()
    if fields[0] == 'R':
      index = int(fields[1])
      item = int(fields[2])
      if len(queues[index]) == 0:
        print ("\nDequeue from empty queue on line " + str(lineNum))
        return False
      if item != queues[index].pop(0):
        print ("\nBad dequeue on line " + str(lineNum))
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
