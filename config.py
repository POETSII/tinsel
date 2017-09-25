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

# FPGA board being targetted
p["TargetBoard"] = quoted("DE5")

# The number of hardware threads per core
p["LogThreadsPerCore"] = 4

# The number of 32-bit instructions that fit in a core's instruction memory
p["LogInstrsPerCore"] = 11

# Log of number of multi-threaded cores sharing a DCache
p["LogCoresPerDCache"] = 2

# Log of number of caches per DRAM port
p["LogDCachesPerDRAM"] = 0

# Log of number of 32-bit words in a single memory transfer
p["LogWordsPerBeat"] = 3

# Log of number of beats in a cache line
p["LogBeatsPerLine"] = 0

# Log of number of sets per thread in set-associative data cache
p["DCacheLogSetsPerThread"] = 3

# Log of number of ways per set in set-associative data cache
p["DCacheLogNumWays"] = 2

# Number of DRAMs per FPGA board
p["LogDRAMsPerBoard"] = 1

# Max number of outstanding DRAM requests permitted
p["DRAMLogMaxInFlight"] = 4

# DRAM latency in cycles (simulation only)
p["DRAMLatency"] = 20

# Size of each DRAM
p["LogBeatsPerDRAM"] = 26

# Size of DRAM partition on each core
p["LogBytesPerDRAMPartition"] = 21

# Size of internal flit payload
p["LogWordsPerFlit"] = 2

# Max flits per message
p["LogMaxFlitsPerMsg"] = 2

# Space available per thread in mailbox scratchpad
p["LogMsgsPerThread"] = 4

# Number of cores sharing a mailbox
p["LogCoresPerMailbox"] = 2

# Number of mailboxes per board
p["LogMailboxesPerBoard"] = 1

# Maximum size of boot loader (in bytes)
p["MaxBootImageBytes"] = 512

# Size of transmit buffer in a reliable link
p["LogTransmitBufferSize"] = 10

# Size of receive buffer in a reliable link
p["LogReceiveBufferSize"] = 5

# Max number of 64-bit items to put in an ethernet packet
p["TransmitBound"] = 20

# Timeout in reliable link (for detecting dropped packets)
p["LinkTimeout"] = 1024

# Latency of 10G MAC in cycles (simulation only)
p["MacLatency"] = 100

# Number of bits in mesh X coord
p["MeshXBits"] = 2

# Number of bits in mesh Y coord
p["MeshYBits"] = 2

# Mesh X length
p["MeshXLen"] = 1

# Mesh Y length
p["MeshYLen"] = 1

# Number of cores per FPU
p["LogCoresPerFPU"] = 2

# Latencies of arithmetic megafunctions
p["IntMultLatency"] = 3
p["FPMultLatency"] = 11
p["FPAddSubLatency"] = 14
p["FPDivLatency"] = 14
p["FPConvertLatency"] = 6
p["FPCompareLatency"] = 3

#==============================================================================
# Derived Parameters
#==============================================================================

# (These should not be modified.)

# Log of number of 32-bit words per data cache line
p["LogWordsPerLine"] = p["LogWordsPerBeat"]+p["LogBeatsPerLine"]

# Log of number of bytes per data cache line
p["LogBytesPerLine"] = 2+p["LogWordsPerLine"]

# Number of 32-bit words per data cache line
p["WordsPerLine"] = 2**p["LogWordsPerLine"]

# Data cache line size in bits
p["LineSize"] = p["WordsPerLine"] * 32

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
p["BeatBurstWidth"] = p["LogBeatsPerLine"]+1

# Cores per DCache
p["CoresPerDCache"] = 2**p["LogCoresPerDCache"]

# Caches per DRAM
p["DCachesPerDRAM"] = 2**p["LogDCachesPerDRAM"]

# Flits per message
p["MaxFlitsPerMsg"] = 2**p["LogMaxFlitsPerMsg"]

# Words per flit
p["WordsPerFlit"] = 2**p["LogWordsPerFlit"]

# Bytes per flit
p["LogBytesPerFlit"] = p["LogWordsPerFlit"] + 2

# Words per message
p["LogWordsPerMsg"] = p["LogWordsPerFlit"] + p["LogMaxFlitsPerMsg"]

# Bytes per message
p["LogBytesPerMsg"] = p["LogWordsPerMsg"] + 2

# Number of bytes per message
p["LogBytesPerMsg"] = p["LogWordsPerMsg"] + 2

# Number of flits per thread in scratchpad
p["LogFlitsPerThread"] = p["LogMsgsPerThread"] + p["LogMaxFlitsPerMsg"]

# Number of cores sharing a mailbox
p["CoresPerMailbox"] = 2 ** p["LogCoresPerMailbox"]

# Number of threads sharing a mailbox
p["LogThreadsPerMailbox"] = p["LogCoresPerMailbox"]+p["LogThreadsPerCore"]

# Size of memory-mapped region for mailbox scratchpad in bytes
p["LogScratchpadBytes"] = (1+p["LogWordsPerFlit"]+2+
                             p["LogMaxFlitsPerMsg"]+
                             p["LogMsgsPerThread"])

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

# Number of threads per board
p["LogThreadsPerBoard"] = p["LogThreadsPerDRAM"] + p["LogDRAMsPerBoard"]
p["ThreadsPerBoard"] = 2 ** p["LogThreadsPerBoard"]

# Cores per board
p["LogCoresPerBoard"] = p["LogCoresPerMailbox"] + p["LogMailboxesPerBoard"]
p["CoresPerBoard"] = 2**p["LogCoresPerBoard"]

# Threads per core
p["ThreadsPerCore"] = 2**p["LogThreadsPerCore"]

# Max number of threads in cluster
p["MaxThreads"] = (2**p["MeshXBits"] *
                     2**p["MeshYBits"] *
                       p["ThreadsPerBoard"])

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

#==============================================================================
# Main 
#==============================================================================

if len(sys.argv) > 1:
  mode = sys.argv[1]
else:
  print "Usage: config.py <defs|envs|cpp>"
  sys.exit(-1)

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
    print("#define Tinsel" + var + " " + str(p[var]))
