HARTID=0xf15

entry:

# Get hardware thread id
csrr a0, HARTID

# Set stack pointer to DATA_MEM_TOP - (id * 256)
la sp, DATA_MEM_TOP
sll a0, a0, 8
sub sp, sp, a0

# Allocate 32 bytes of stack space
add sp, sp, -32

# Jump-and-link to main
jal main

# Loop when program terminates
j .
