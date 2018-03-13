#include <HostLink.h>

// Benchmarks to run
const char* benchmarks[] = {
    "loadLoop"
  , "storeLoop"
  , "modifyLoop"
  , "copyLoop"
  , "cacheLoop"
  , "scratchpadLoop"
  , "messageLoop"
  , NULL
};

int main()
{
  HostLink hostLink;

  for (int i = 0; benchmarks[i] != NULL; i++) {
    // Boot benchmark
    char codeFile[256], dataFile[256];
    snprintf(codeFile, sizeof(codeFile), "%s-code.v", benchmarks[i]);
    snprintf(dataFile, sizeof(dataFile), "%s-data.v", benchmarks[i]);
    hostLink.boot(codeFile, dataFile);

    // Trigger execution
    hostLink.go();

    // Wait for response
    const int numThreads = 1 << TinselLogThreadsPerBoard;
    uint32_t flit[4];
    uint32_t total = 0;
    for (int j = 0; j < numThreads; j++) {
      hostLink.recv(flit);
      total += flit[0];
    }
    printf("%s: %d cycles on average\n", benchmarks[i], total/numThreads);
  }

  return 0;
}
