// Get the hardware thread id
inline int me()
{
  int id;
  asm volatile("csrr %0, 0xf15" : "=r"(id));
  return id;
}

// Set output bits
inline void set(int bits)
{
  asm volatile("csrs 0x800, %0" : : "r"(bits));
}

// Clear output bits
inline void clear(int bits)
{
  asm volatile("csrc 0x800, %0" : : "r"(bits));
}

// Sleep (approximately) for a second
void sleep() {
  // Assuming 400MHz clock and 16 threads and 4 cycles per iteration
  volatile int delay = 6250000;
  while (delay > 0) delay--;
}

// Shared variable
volatile int active = 0;

// Main
int main()
{
  int id = me();

  for (;;) {
    if (id == active) {
      clear(-1); // Clear all output bits
      set(id);   // Put my id on output bits
      sleep();
      active = (id+1) & 0xf; // Activate next thread
    }  
  }

  return 0;
}
