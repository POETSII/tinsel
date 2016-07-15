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

# The number of 32-bit words that fit in a core's data memory
# Condition: LogDataWordsPerCore >= LogInstrsPerCore
export LogDataWordsPerCore=11

# Log of number of multi-threaded cores per data cache
export LogCoresPerDCache=2

# Log of number of 32-bit words per data cache line
export LogWordsPerLine=3

# Log of number of sets per thread in set-associative data cache
export DCacheLogSetsPerThread=5

# Log of number of ways per set in set-associative data cache
export DCacheLogNumWays=2

# Log of number of data caches present
export LogNumDCaches=0

# DRAM latency in cycles (simulation only)
export DRAMLatency=20

# Max number of in-flight DRAM requests at any time (simulation only)
export DRAMPipelineLen=4

#==============================================================================
# Derived Parameters
#==============================================================================

# (These must NOT be modified.)

# Log of number of bytes per data cache line
export LogBytesPerLine=$((2+$LogWordsPerLine))

# Number of 32-bit words per data cache line
export WordsPerLine=$((2**$LogWordsPerLine))

# Data cache line size and memory bus width
export LineSize=$(($WordsPerLine * 32))
