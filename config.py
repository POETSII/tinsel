#!/usr/bin/env python2

# SPDX-License-Identifier: BSD-2-Clause
# This file controls the parameters for the circuit generator

import sys

#==============================================================================
# Prelude
#==============================================================================

def quoted(s): return "'\"" + s + "\"'"

# Intialise parameter map
p = {}

#==============================================================================
# Default config
#==============================================================================

# The Altera device family being targetted
p["DeviceFamily"] = quoted("Stratix V")

# The number of hardware threads per core
p["LogThreadsPerCore"] = 4

# The number of 32-bit instructions that fit in a core's instruction memory
p["LogInstrsPerCore"] = 11

# Share instruction memory between two cores?
p["SharedInstrMem"] = True

# Log of number of multi-threaded cores sharing a DCache
p["LogCoresPerDCache"] = 2

# Log of number of caches per DRAM port
p["LogDCachesPerDRAM"] = 1

# Log of number of 32-bit words in a single memory transfer
p["LogWordsPerBeat"] = 3

# Log of number of beats in a cache line
p["LogBeatsPerLine"] = 0

# Log of number of sets per thread in set-associative data cache
p["DCacheLogSetsPerThread"] = 2

# Log of number of ways per set in set-associative data cache
p["DCacheLogNumWays"] = 4

# Number of DRAMs per FPGA board
p["LogDRAMsPerBoard"] = 1

# Number of SRAMs per FPGA board (or None if 0)
p["SRAMsPerBoard"] = None
# Max number of outstanding DRAM requests permitted
p["DRAMLogMaxInFlight"] = 5

# DRAM latency in cycles (simulation only)
p["DRAMLatency"] = 20

# Size of each DRAM
p["LogBeatsPerDRAM"] = 26 # need sufficent DRAM capacity for progRouters...

# Size of internal flit payload
p["LogWordsPerFlit"] = 2

# Max flits per message
p["LogMaxFlitsPerMsg"] = 2

# Space available in mailbox scratchpad
p["LogMsgsPerMailbox"] = 9

# Number of cores sharing a mailbox
p["LogCoresPerMailbox"] = 2

# Number of bits in mailbox mesh X coord
p["MailboxMeshXBits"] = 1

# Number of bits in mailbox mesh Y coord
p["MailboxMeshYBits"] = 1

# Length of mailbox mesh X dimension
p["MailboxMeshXLen"] = 2 ** p["MailboxMeshXBits"]

# Length of mailbox mesh Y dimension
p["MailboxMeshYLen"] = 2 ** p["MailboxMeshYBits"]

# Number of mailboxes per board
p["LogMailboxesPerBoard"] = p["MailboxMeshXBits"] + p["MailboxMeshYBits"]

# Size of multicast queues in mailbox
p["LogMsgPtrQueueSize"] = 6

# Size of multicast serialisation buffer
p["LogMulticastBufferSize"] = 9

# Maximum size of boot loader (in bytes)
p["MaxBootImageBytes"] = 1024

# Size of transmit buffer in a reliable link
p["LogTransmitBufferSize"] = 10

# Size of receive buffer in a MAC
p["LogMacRecvBufferSize"] = 5

# Size of receive buffer in a reliable link
p["LogReliableLinkRecvBufferSize"] = 9

# Max number of 64-bit items to put in an ethernet packet
p["TransmitBound"] = 20

# Timeout in reliable link (for detecting dropped packets)
p["LinkTimeout"] = 1024

# Latency of 10G MAC in cycles (simulation only)
p["MacLatency"] = 100

# Number of bits in mesh X coord (board id)
p["MeshXBits"] = 3
p["MeshXBits1"] = p["MeshXBits"] + 1

# Number of bits in mesh Y coord (board id)
p["MeshYBits"] = 3
p["MeshYBits1"] = p["MeshYBits"] + 1

# Number of bits in mesh X coord within a box (DIP switches)
p["MeshXBitsWithinBox"] = 2

# Number of bits in mesh Y coord within a box (DIP switches)
p["MeshYBitsWithinBox"] = 2

# Mesh X length within a box
p["MeshXLenWithinBox"] = 1

# Mesh Y length within a box
p["MeshYLenWithinBox"] = 1

# Number of cores per FPU
p["LogCoresPerFPU"] = 2

# Number of inter-FPGA links on north edge
# Number of inter-FPGA links on south edge
p["LogNorthSouthLinks"] = 0

# Number of inter-FPGA links on east edge
# Number of inter-FPGA links on west edge
p["LogEastWestLinks"] = 0

# Latencies of arithmetic megafunctions
p["IntMultLatency"] = 3
p["FPMultLatency"] = 11
p["FPAddSubLatency"] = 7
p["FPDivLatency"] = 14
p["FPConvertLatency"] = 6
p["FPCompareLatency"] = 3

# SRAM parameters
p["SRAMAddrWidth"] = 20
p["LogBytesPerSRAMBeat"] = 3
p["SRAMBurstWidth"] = 3
p["SRAMLatency"] = 8
p["SRAMLogMaxInFlight"] = 5
p["SRAMStoreLatency"] = 2

# Programmable router parameters:
p["LogRoutingEntryLen"] = 5 # Number of beats in a routing table entry
p["ProgRouterMaxBurst"] = 4
p["FetcherLogIndQueueSize"] = 1
p["FetcherLogBeatBufferSize"] = 5
p["FetcherLogFlitBufferSize"] = 5
p["FetcherLogMsgsPerFlitBuffer"] = (
  p["FetcherLogFlitBufferSize"] - p["LogMaxFlitsPerMsg"])
p["FetcherMsgsPerFlitBuffer"] = 2 ** p["FetcherLogMsgsPerFlitBuffer"]

# Enable performance counters
p["EnablePerfCount"] = True

# Box mesh
p["BoxMeshXLen"] = 1
p["BoxMeshYLen"] = 1
p["BoxMesh"] = ('{'
    '{"asdex"}'
  '}')

# Enable custom accelerators (experimental feature)
p["UseCustomAccelerator"] = False

# Clock frequency (in MHz)
p["ClockFreq"] = 210

#==============================================================================
# Derived Parameters
#==============================================================================

# (These should not be modified.)

# The number of 32-bit instructions that fit in a core's instruction memory
p["InstrsPerCore"] = 2**p["LogInstrsPerCore"]

# Number of sets per thread in set-associative data cache
p["DCacheSetsPerThread"] = 2**p["DCacheLogSetsPerThread"]

# Number of ways per set in set-associative data cache
p["DCacheNumWays"] = 2**p["DCacheLogNumWays"]

# Log of number of 32-bit words per data cache line
p["LogWordsPerLine"] = p["LogWordsPerBeat"]+p["LogBeatsPerLine"]

# Log of number of bytes per data cache line
p["LogBytesPerLine"] = 2+p["LogWordsPerLine"]

# Number of 32-bit words per data cache line
p["WordsPerLine"] = 2**p["LogWordsPerLine"]

# Data cache line size in bits
p["BitsPerLine"] = p["WordsPerLine"] * 32

# Number of beats per cache line
p["BeatsPerLine"] = 2**p["LogBeatsPerLine"]

# Number of 32-bit words in a memory transfer
p["WordsPerBeat"] = 2**p["LogWordsPerBeat"]

# Number of bytes in a memory transfer
p["BytesPerBeat"] = 4 * p["WordsPerBeat"]

# Number of bytes in a DRAM
p["BytesPerDRAM"] = 2**p["LogBeatsPerDRAM"] * p["BytesPerBeat"]

# Log of number of bytes in a memory transfer
p["LogBytesPerBeat"] = p["LogWordsPerBeat"] + 2

# Data cache beat width in bits
p["BeatWidth"] = p["WordsPerBeat"] * 32

# Longest possible burst transfer is 2^BeatBurstWidth-1
p["BeatBurstWidth"] = 3
assert p["LogBeatsPerLine"] < p["BeatBurstWidth"]

# Cores per DCache
p["CoresPerDCache"] = 2**p["LogCoresPerDCache"]

# Caches per DRAM
p["DCachesPerDRAM"] = 2**p["LogDCachesPerDRAM"]

# Flits per message
p["MaxFlitsPerMsg"] = 2**p["LogMaxFlitsPerMsg"]

# Mailbox size
p["LogFlitsPerMailbox"] = p["LogMsgsPerMailbox"] + p["LogMaxFlitsPerMsg"]
p["LogWordsPerMailbox"] = p["LogFlitsPerMailbox"] + p["LogWordsPerFlit"]
p["LogBytesPerMailbox"] = p["LogWordsPerMailbox"] + 2

# Words per flit
p["WordsPerFlit"] = 2**p["LogWordsPerFlit"]

# Bytes per flit
p["LogBytesPerFlit"] = p["LogWordsPerFlit"] + 2

# Bits per flit
p["BitsPerFlit"] = p["WordsPerFlit"] * 32

# Words per message
p["LogWordsPerMsg"] = p["LogWordsPerFlit"] + p["LogMaxFlitsPerMsg"]

# Bytes per message
p["LogBytesPerMsg"] = p["LogWordsPerMsg"] + 2

# Number of cores sharing a mailbox
p["CoresPerMailbox"] = 2 ** p["LogCoresPerMailbox"]

# Number of threads sharing a mailbox
p["LogThreadsPerMailbox"] = p["LogCoresPerMailbox"]+p["LogThreadsPerCore"]
p["ThreadsPerMailbox"] = 2**p["LogThreadsPerMailbox"]

# Base of off-chip memory-mapped region in bytes
p["LogOffChipRAMBaseAddr"] = (1+p["LogWordsPerFlit"]+2+
                                p["LogMaxFlitsPerMsg"]+
                                p["LogMsgsPerMailbox"])

# Size of mailbox transmit buffer
p["LogTransmitBufferLen"] = (p["LogMaxFlitsPerMsg"]
                               if p["LogMaxFlitsPerMsg"] > 1 else 1)

# Number of mailboxes per board
p["MailboxesPerBoard"] = 2 ** p["LogMailboxesPerBoard"]

# Number of DRAMs per FPGA board
p["DRAMsPerBoard"] = 2 ** p["LogDRAMsPerBoard"]

# Size of each DRAM
p["LogLinesPerDRAM"] = p["LogBeatsPerDRAM"] - p["LogBeatsPerLine"]
p["LogBytesPerDRAM"] = p["LogBeatsPerDRAM"] + p["LogBytesPerBeat"]

# Number of threads per DRAM
p["LogThreadsPerDRAM"] = (p["LogThreadsPerCore"] +
                            p["LogCoresPerDCache"] +
                              p["LogDCachesPerDRAM"])
p["ThreadsPerDRAM"] = 2 ** p["LogThreadsPerDRAM"]

# Size of DRAM partition on each thread
p["LogBytesPerDRAMPartition"] = (
  p["LogBeatsPerDRAM"]-1 + p["LogWordsPerBeat"]+2 - p["LogThreadsPerDRAM"])

# Number of threads per board
p["LogThreadsPerBoard"] = p["LogThreadsPerDRAM"] + p["LogDRAMsPerBoard"]
p["ThreadsPerBoard"] = 2 ** p["LogThreadsPerBoard"]

# Cores per board
p["LogCoresPerBoard"] = p["LogCoresPerMailbox"] + p["LogMailboxesPerBoard"]
p["LogCoresPerBoard1"] = p["LogCoresPerBoard"] + 1
p["CoresPerBoard"] = 2**p["LogCoresPerBoard"]

# Threads per core
p["ThreadsPerCore"] = 2**p["LogThreadsPerCore"]

# Max number of threads in cluster
p["MaxThreads"] = (2**p["MeshXBits"] *
                     2**p["MeshYBits"] *
                       p["ThreadsPerBoard"])

# Size of off-chip memory
# Twice the size of DRAM
# Top half and bottom half map to the same DRAM memory
# But the top half has the partition-interlaving translation applied
p["LogBeatsPerMem"] = p["LogBeatsPerDRAM"] + 1
p["LogBytesPerMem"] = p["LogBytesPerDRAM"] + 1
p["LogLinesPerMem"] = p["LogLinesPerDRAM"] + 1

# Cores per FPU
p["CoresPerFPU"] = 2 ** p["LogCoresPerFPU"]

# Threads per FPU
p["LogThreadsPerFPU"] = p["LogThreadsPerCore"] + p["LogCoresPerFPU"]

# FPUs per board
p["LogFPUsPerBoard"] = p["LogCoresPerBoard"] - p["LogCoresPerFPU"]
p["FPUsPerBoard"] = 2 ** p["LogFPUsPerBoard"]

# Max latency of any FPU operation
p["FPUOpMaxLatency"] = max(
  [ p["IntMultLatency"]
  , p["FPMultLatency"]
  , p["FPAddSubLatency"]
  , p["FPDivLatency"]
  , p["FPConvertLatency"]
  , p["FPCompareLatency"]
  ])

# Number of inter-FPGA links
p["NumNorthSouthLinks"] = 2 ** p["LogNorthSouthLinks"]
p["NumEastWestLinks"] = 2 ** p["LogEastWestLinks"]

# SRAM parameters
if p["SRAMsPerBoard"]:
    p["BytesPerSRAMBeat"] = 2 ** p["LogBytesPerSRAMBeat"]
    p["WordsPerSRAMBeat"] = p["BytesPerSRAMBeat"] / 4
    p["SRAMDataWidth"] = 32 * p["WordsPerSRAMBeat"]
    p["SRAMsPerBoard"] = 2 * p["DRAMsPerBoard"]
    p["LogThreadsPerSRAM"] = p["LogThreadsPerDRAM"] - 1
    p["LogBeatsPerSRAM"] = (
      (p["SRAMAddrWidth"] + p["LogBytesPerSRAMBeat"]) - p["LogBytesPerBeat"])
    p["LogBytesPerSRAM"] = p["LogBeatsPerSRAM"] + p["LogBytesPerBeat"]
    p["LogBytesPerSRAMPartition"] = p["LogBytesPerSRAM"] - p["LogThreadsPerSRAM"]
else:
    p["BytesPerSRAMBeat"] = 0
    p["WordsPerSRAMBeat"] = 0
    p["SRAMDataWidth"] = 0
    # p["LogThreadsPerSRAM"] = None
    # p["LogBeatsPerSRAM"] = None
    # p["LogBytesPerSRAM"] = None
    # p["LogBytesPerSRAMPartition"] = None

# DRAM base and length
if p["SRAMsPerBoard"]:
    p["DRAMBase"] = 3 * (2 ** p["LogBytesPerSRAM"])
else:
    p["DRAMBase"] = 512 # TDDO FIXME; first word does not write correctly.
p["DRAMGlobalsLength"] = 2 ** (p["LogBytesPerDRAM"] - 1) - p["DRAMBase"]
p["POLiteDRAMGlobalsLength"] = 2 ** 14
p["POLiteProgRouterBase"] = p["DRAMBase"] + p["POLiteDRAMGlobalsLength"]
p["POLiteProgRouterLength"] = (p["DRAMGlobalsLength"] -
                                 p["POLiteDRAMGlobalsLength"])

# POLite globals

# Number of FPGA boards per box (including bridge board)
#p["BoardsPerBox"] = p["MeshXLenWithinBox"] * p["MeshYLenWithinBox"] + 1
p["BoardsPerBox"] = p["MeshXLenWithinBox"] * p["MeshYLenWithinBox"]

# Parameters for programmable routers
# (and the routing-record fetchers they contain)
p["FetchersPerProgRouter"] = 4 + p["MailboxMeshXLen"]
p["LogFetcherFlitBufferSize"] = 5

#==============================================================================
# Main
#==============================================================================

def to_cpp_string(convertee):
  """Returns a string of convertee appropriate for cpp output. Mostly just for
  converting booleans."""
  if convertee is True:
    return "true"
  elif convertee is False:
    return "false"
  else:
    return str(convertee)


if len(sys.argv) > 1:
  mode = sys.argv[1]
else:
  print "Usage: config.py <defs|envs|cpp|vpp>"
  sys.exit(-1)

# The BoxMesh parameter is only meant for cpp mode
if (mode != "cpp"): del p["BoxMesh"]

if mode == "defs":
  for var in p:
    if isinstance(p[var], bool):
      if p[var]: print("-D " + var),
    else:
      print("-D " + var + "=" + str(p[var])),
elif mode == "envs":
  for var in p:
    print("export " + var + "=" + str(p[var]))
elif mode == "cpp":
  for var in p:
    print("#define Tinsel" + var + " " + to_cpp_string(p[var]))
elif mode == "vpp":
  for var in p:
    print("`define Tinsel" + var + " " + str(p[var]))
