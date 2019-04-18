#!/usr/bin/python

# Tinsel Job Daemon
# =================

# Commands accepted
# -----------------
#
# 1. Add a program to the job queue.  Client sends:
#
#   Byte 0 | Bytes 1..6       | Bytes 7..7+N-1
#   -------+------------------+---------------
#   A      | 6-digit number N | N-byte program
#
# On success, job daemon responds with:
#
#   Byte 0 | Bytes 1..6   
#   -------+---------------
#   O      | 6-digit job id
#
# On failure, job deamon responds with a single byte 'E'.
#
# 2. Remove a job (if complete) from the queue.  Client sends:
#
#   Byte 0 | Bytes 1..6
#   -------+---------------
#   R      | 6-digit job id
#
# Job daemon responds with:
#
#   Byte 0 | Bytes 1..6       | Bytes 7..7+N-1
#   -------+------------------+-------------------------
#   O      | 6-digit number N | N bytes of job output
#   E      | 6-digit number N | N bytes of error message
#
# 3. Quit.  Client sends single byte 'Q'.

import os
import os.path
import sys
import socket
import subprocess
import time

# Daemon listens on this address & port
serverAddr = '127.0.0.1'
serverPort = 10102

# Job queue
maxQueueLen = 10
queue = []

# Counter for determining new job ids
nextJobId = 0

# Job arrays
job = [None]*maxQueueLen
jobResult = [None]*maxQueueLen
jobTime = [None]*maxQueueLen

# Create server side socket
serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serverSocket.bind((serverAddr, serverPort))
serverSocket.listen(5)

# Function to receive exactly n bytes on socket
def receive(sock, n):
  got = 0
  buff = ""
  while got < n:
    tmp = sock.recv(n-got)
    got = got + len(tmp)
    buff = buff + tmp
  return buff

# Daemon states
idleState    = 0
resetState   = 1
runState     = 2

# Initial state
state = idleState
runProc = None

while 1:
  # Service the job queue
  if len(queue) > 0:
    jobId = queue[0]
    if state == idleState:
      os.system("make clean")
      f = open(str(jobId) + ".c", "w")
      f.write(job[jobId])
      f.close()
      runProc = subprocess.Popen("timeout 20s make APP=" + str(jobId),
                  shell=True)
      state = runState
    elif state == runState and runProc.poll() is not None:
      if not os.path.isfile("out.ppm"):
        errFile = open("compiler.err", "r")
        err = errFile.read()
        errFile.close()
        if err == "":
          err = "Program timeout"
        else:
          err = "Compiler error\n" + err
        jobResult[jobId] = "E" + str(len(err)).zfill(6) + err
        jobTime[jobId] = time.time()
      else:
        imgFile = open(str(jobId) + ".png.64", "r")
        img = imgFile.read().rstrip()
        imgFile.close()
        jobResult[jobId] = "O" + str(len(img)).zfill(6) + img
        jobTime[jobId] = time.time()
      queue.pop(0)
      state = idleState

  # Put timeout on server socket when queue is non-empty
  if len(queue) > 0:
    serverSocket.settimeout(0.5)
  else:
    serverSocket.settimeout(None)

  # Try to accept a connection
  try:
    (clientSocket, address) = serverSocket.accept()
  except socket.timeout:
    continue

  cmd = receive(clientSocket, 1)
  if cmd == 'Q':
    clientSocket.close();
    sys.exit()
  elif cmd == 'A':
    # Receive program
    progLen = int(receive(clientSocket, 6))
    prog = receive(clientSocket, progLen);

    # Add job to queue
    if len(queue) == maxQueueLen:
      clientSocket.send("E")
    elif jobResult[nextJobId] is not None and (
           time.time() < jobTime[nextJobId]+100):
      clientSocket.send("E")
    else:
      jobResult[nextJobId] = None
      # Send response
      clientSocket.send("O" + str(nextJobId).zfill(6))
      # Create new job
      job[nextJobId] = prog
      queue.append(nextJobId)
      nextJobId = (nextJobId + 1) % maxQueueLen
  elif cmd == 'R':
    # Receive id
    jobId = int(receive(clientSocket, 6))

    # Remove job from queue
    if jobResult[jobId] is None:
      clientSocket.send("U")
    else:
      output = jobResult[jobId]
      clientSocket.send(output)
      jobResult[jobId] = None
