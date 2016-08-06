# This file controls the parameters for the circuit generator

#==============================================================================
# Configurable parameters
#==============================================================================

# The Altera device family being targetted
export DeviceFamily="Stratix V"

# The number of hardware threads per core
export LogThreadsPerCore=4

# The number of 32-bit instructions that fit in a core's instruction memory
export LogInstrsPerCore=9

# Log of number of multi-threaded cores per data cache
export LogCoresPerDCache=2

# Log of number of 32-bit words in a single memory transfer
export LogWordsPerBeat=3

# Log of number of beats in a cache line
export LogBeatsPerLine=0

# Log of number of sets per thread in set-associative data cache
export DCacheLogSetsPerThread=5

# Log of number of ways per set in set-associative data cache
export DCacheLogNumWays=2

# Log of number of data caches present
export LogNumDCaches=0

# Max number of outstanding DRAM requests permitted
export DRAMLogMaxInFlight=4

# DRAM latency in cycles (simulation only)
export DRAMLatency=20

# If set to 1, reduces logic usage, but each DRAM port is limited to
# half throughput (one response every other cycle).  This may be
# acceptable for various reasons, most notably when using a multi-port
# front-end to DRAM, e.g. on a Cyclone V board.
export DRAMPortHalfThroughput=1

# DRAM byte-address width
export DRAMAddrWidth=30

#==============================================================================
# Derived Parameters
#==============================================================================

# (These should not be modified.)

# Log of number of 32-bit words per data cache line
export LogWordsPerLine=$(($LogWordsPerBeat+$LogBeatsPerLine))

# Log of number of bytes per data cache line
export LogBytesPerLine=$((2+$LogWordsPerLine))

# Number of 32-bit words per data cache line
export WordsPerLine=$((2**$LogWordsPerLine))

# Data cache line size in bits
export LineSize=$(($WordsPerLine * 32))

# Number of beats per cache line
export BeatsPerLine=$((2**$LogBeatsPerLine))

# Number of 32-bit words in a memory transfer
export WordsPerBeat=$((2**$LogWordsPerBeat))

# Log of number of bytes per data transfer
export LogBytesPerBeat=$((2+$LogWordsPerBeat))

# Number of data bytes in a memory transfer
export BytesPerBeat=$((2**$LogBytesPerBeat))

# Memory transfer bus width in bits
export BusWidth=$(($WordsPerBeat * 32))

# Longest possible burst transfer is 2^BurstWidth-1
export BurstWidth=$(($LogBeatsPerLine+1))
