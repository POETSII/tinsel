#!/usr/bin/python
# SPDX-License-Identifier: BSD-2-Clause

# Send job to Tinsel Job Daemon
# =============================

import sys
import socket

if len(sys.argv) != 2:
  print "Usage: tinjob-add [FILE]"
  sys.exit()

# Read file
f = open(sys.argv[1], "r")
prog = f.read()
f.close()

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

# Send job to server
prog = "A" + str(len(prog)).zfill(6) + prog
sendAll(s, prog)

# Get response
resp = receive(s, 1)
if resp == 'E':
  print "Failed to create new job"
elif resp == 'O':
  jobId = int(receive(s, 6))
  print "Ok, created job #", str(jobId)
