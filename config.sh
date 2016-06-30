# This file controls the parameters for the circuit generator

# The Altera device family being targetted
export DeviceFamily="Stratix V"

# The number of hardware threads per core
export LogThreadsPerCore=4

# The number of 32-bit instructions that fit in a core's instruction memory
export LogInstrsPerCore=9

# The number of 32-bit words that fit in a core's data memory
# Condition: LogDataWordsPerCore >= LogInstrsPerCore
export LogDataWordsPerCore=11
