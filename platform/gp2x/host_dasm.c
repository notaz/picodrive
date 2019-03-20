#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/disarm.c"


/* symbols */
typedef struct { const char *name; void *value; } asymbol;

static asymbol **symbols;
static long symcount, symstorage = 8;

static const char *lookup_name(void *addr)
{
  asymbol **sptr = symbols;
  int i;

  for (i = 0; i < symcount; i++) {
    asymbol *sym = *sptr++;

    if (addr == sym->value)
      return sym->name;
  }

  return NULL;
}

void host_dasm(void *addr, int len)
{
  void *end = (char *)addr + len;
  const char *name;
  char buf[64];
  long insn, symaddr;

  while (addr < end) {
    name = lookup_name(addr);
    if (name != NULL)
      printf("%s:\n", name);

    insn = *(long *)addr;
    printf("   %08lx %08lx ", (long)addr, insn);
    if(disarm((unsigned)addr, insn, buf, sizeof(buf))) {
      symaddr = 0;
      if ((insn & 0xe000000) == 0xa000000) {
        symaddr = (long)addr + 8 + ((long)(insn << 8) >> 6);
        name = lookup_name((void *)symaddr);
      }
      if (symaddr && name)
        printf("%s <%s>\n", buf, name);
      else if (symaddr && !name)
        printf("%s <unknown>\n", buf);
      else
        printf("%s\n", buf);
    } else
      printf("unknown\n");
    addr = (char *)addr + sizeof(long);
  }
}

void host_dasm_new_symbol_(void *addr, const char *name)
{
  asymbol *sym, **tmp;

  if (symbols == NULL)
    symbols = malloc(symstorage);
  if (symstorage <= symcount * sizeof(symbols[0])) {
    tmp = realloc(symbols, symstorage * 2);
    if (tmp == NULL)
      return;
    symstorage *= 2;
    symbols = tmp;
  }

  symbols[symcount] = calloc(sizeof(*symbols[0]), 1);
  if (symbols[symcount] == NULL)
    return;

  // a HACK (should use correct section), but ohwell
  sym = symbols[symcount];
  sym->value = addr;
  sym->name = name;
  symcount++;
}

