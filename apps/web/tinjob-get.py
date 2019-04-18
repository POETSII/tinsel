#!/usr/bin/python

# Get job result from Tinsel Job Daemon
# =====================================

import sys
import socket

if len(sys.argv) != 2:
  print "Usage: tinjob-get [ID]"
  sys.exit()

# Parse job id
jobId = int(sys.argv[1][0:6])

# Daemon listens on this address & port
serverAddr = '127.0.0.1'
serverPort = 10102

# Connect to server
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((serverAddr, serverPort))

# Function to send exactly n bytes
def sendAll(sock, data):
  sent = 0
  n = len(data)
  while sent < n:
    tmp = sock.send(data)
    data = data[tmp:]
    sent = sent + tmp

# Function to receive exactly n bytes on socket
def receive(sock, n):
  got = 0
  buff = ""
  while got < n:
    tmp = sock.recv(n-got)
    got = got + len(tmp)
    buff = buff + tmp
  return buff

# Try to remove job from server
sendAll(s, "R" + str(jobId).zfill(6))

# Get response
resp = receive(s, 1)
if resp == 'U':
  print "Job has not yet completed"
if resp == 'E':
  print "Job failed"
  numBytes = int(receive(s, 6))
  print receive(s, numBytes)
elif resp == 'O':
  print "Job succeeded"
  numBytes = int(receive(s, 6))
  print receive(s, numBytes)
