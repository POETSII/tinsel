#include <stdio.h>
#include <stdlib.h>
#include <HostLink.h>

const char* tests[] = {
  "fldst",    "fadd",   "fdiv",    "fcmp",
  "fcvt_w",   "fcvt",
  "addi",     "beq",    "bne",     "lh",
  "mulhsu",   "sb",     "slti",    "sra",
  "xori",     "add",    "bge",     "jalr",
  "lhu",      "mulhu",  "sh",      "sltiu",
  "srli",     "xor",    "andi",    "bgeu",
  "jal",      "lui",    "mul",     "simple",
  "slt",      "srl",    "and",     "blt",
  "lb",       "lw",     "ori",     "slli",
  "sltu",     "sub",    "auipc",   "bltu",
  "lbu",      "mulh",   "or",      "sll",
  "srai",     "sw",     NULL
};

int runOne(HostLink* hostLink, const char* test)
{
  // Compute code filename
  char codeFilename[1024];
  snprintf(codeFilename, sizeof(codeFilename), "%s.code.v", test);

  // Compute data filename
  char dataFilename[1024];
  snprintf(dataFilename, sizeof(dataFilename), "%s.data.v", test);

  // Boot and run test
  hostLink->bootOne(codeFilename, dataFilename);
  hostLink->goOne();

  // Get test result
  uint32_t coreId, threadId;
  uint8_t byte;
  hostLink->mesh[0][0]->get(&coreId, &threadId, &byte);

  return byte;
}

int main()
{
  HostLink hostLink;

  for (int i = 0; ; i++) {
    if (tests[i] == NULL) break;
    printf("%s\t", tests[i]);
    int result = runOne(&hostLink, tests[i]);
    if (result&1)
      printf("PASSED\n");
    else
      printf("FAILED #%i\n", result>>1);
  }

  return 0;
}
