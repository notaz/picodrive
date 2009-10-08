#include "../sh2.h"

void sh2_execute(SH2 *sh2, int cycles)
{
  unsigned int pc = sh2->pc;
  int op;

  op = p32x_sh2_read16(pc);
}

