#!/usr/bin/python

import cgi
import cgitb
import socket

print "Content-type: text/html\n"

# Tinsel Job Daemon listens on this address & port
serverAddr = '127.0.0.1'
serverPort = 10101

# Function to send exactly n bytes on socket
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

# HTML snippet to auto-resubmit form
def autoResubmit(jobId, time, count):
  dots = "." * (3*count)
  print ("<p><b>Please wait" + dots + "</b></p>")
  print "<p>(Its takes 60s to program the FPGA)</p>"
  print ('<input type="hidden" name="resubmitCount" value="' +
            str(count+1) + '">')
  print ('<input type="hidden" name="inprogress" value="' + str(jobId) + '">')
  print '<script>setTimeout(function(){'
  print 'document.getElementById("myForm").submit();'
  print ('}, ' + str(time) + ');</script>')

# Form contents
form = cgi.FieldStorage() 

# Override example from form
example = "simple"
if "example" in form:
  validExamples = ["simple", "border", "stripes", "heat"]
  if form.getvalue("example") in validExamples:
    example = form.getvalue("example")

print """
<html>
<head><title>Tinsel Web</title></head>
<body>

<h1>Tinsel Web</h1>

<form id="myForm" method="post">
"""

# Should input widgets be hidden?
disableWidgets = False

if "submitjob" in form:
  program = ""
  if "program" in form:
    program = form.getvalue("program")

  try:
    # Connect to Tinsel Job Daemon
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((serverAddr, serverPort))

    # Send job
    msg = "A" + str(len(program)).zfill(6) + program
    sendAll(s, msg)

    # Get response
    resp = receive(s, 1)
    if resp == 'E':
      print "<p>Error: failed to create new job"
    elif resp == 'O':
      jobId = int(receive(s, 6))
      autoResubmit(jobId, 4000, 1)
    
    # Close socket
    s.close()

    disableWidgets = True
  except:
    print "Error: can't communicate with Tinsel Job Deamon"
elif "inprogress" in form:
  jobId = form.getvalue("inprogress")

  resubmitCount = 0
  if "resubmitCount" in form: resubmitCount = form.getvalue("resubmitCount")

  program = ""
  if "program" in form:
    program = form.getvalue("program")

  try:
    # Connect to Tinsel Job Daemon
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((serverAddr, serverPort))

    # Try to remove job from server
    sendAll(s, "R" + str(jobId).zfill(6))

    # Get response
    resp = receive(s, 1)
    if resp == 'U':
      autoResubmit(jobId, 4000, int(resubmitCount))
      disableWidgets = True
    if resp == 'E':
      print "<p>Job failed:</p>"
      numBytes = int(receive(s, 6))
      print ("<pre>" + receive(s, numBytes) + "</pre>")
    elif resp == 'O':
      print "<p>Output from your job:</p>"
      numBytes = int(receive(s, 6))
      print ('<img src="data:image/png;base64,' + receive(s, numBytes) + '">')

    # Close socket
    s.close()
  except:
    print "Error: can't communicate with Tinsel Job Deamon"
else:
  # Load example
  exampleFile = open("examples/" + example + ".c", "r")
  program = exampleFile.read()

print "<p>Example: "
print "<select ",
print 'id="example" name="example" onChange="pickExample()">'

def exampleOption(val, text):
  global example
  if example == val:
    print ('<option value="' + val + '" selected>' + text + "</option>")
  else:
    print ('<option value="' + val + '">' + text + "</option>")

exampleOption("simple" , "Simple")
exampleOption("border" , "Border")
exampleOption("stripes", "Stripes")
exampleOption("heat"   , "Heat Diffusion")

print """
</select></p>

<p><textarea id="program" name="program" rows=30 cols=80>"""

print (program + "</textarea></p>")

if disableWidgets: print "<button disabled ",
else: print "<button ",
print 'name="submitjob" value="yes">Submit Job</button>'

print """
</form>

<script>
function pickExample()
{
  document.getElementById("myForm").submit();
}
</script>

</body>
</html>
"""
