#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <bfd.h>
#include <dis-asm.h>

#include "host_dasm.h"

extern char **g_argv;

static struct disassemble_info di;

#ifdef ARM
#define print_insn_func print_insn_little_arm
#define BFD_ARCH bfd_arch_arm
#define BFD_MACH bfd_mach_arm_4T
#else
#define print_insn_func print_insn_i386_intel
#define BFD_ARCH bfd_arch_i386
#define BFD_MACH bfd_mach_i386_i386_intel_syntax
#endif

/* symbols */
static asymbol **symbols;
static long symcount;

/* Filter out (in place) symbols that are useless for disassembly.
   COUNT is the number of elements in SYMBOLS.
   Return the number of useful symbols.  */
static long
remove_useless_symbols (asymbol **symbols, long count)
{
  asymbol **in_ptr = symbols, **out_ptr = symbols;

  while (--count >= 0)
    {
      asymbol *sym = *in_ptr++;

      if (sym->name == NULL || sym->name[0] == '\0')
        continue;
      if (sym->flags & (BSF_DEBUGGING | BSF_SECTION_SYM))
        continue;
      if (bfd_is_und_section (sym->section)
          || bfd_is_com_section (sym->section))
        continue;
      if (sym->value + sym->section->vma == 0)
        continue;
/*
      printf("sym: %08lx %04x %08x v %08x \"%s\"\n",
        (unsigned int)sym->value, (unsigned int)sym->flags, (unsigned int)sym->udata.i,
        (unsigned int)sym->section->vma, sym->name);
*/
      *out_ptr++ = sym;
    }
  return out_ptr - symbols;
}

static void slurp_symtab(const char *filename)
{
  bfd *abfd;
  long storage;

  symcount = 0;

  abfd = bfd_openr(filename, NULL);
  if (abfd == NULL) {
    fprintf(stderr, "failed to open: %s\n", filename);
    goto no_symbols;
  }

  if (!bfd_check_format(abfd, bfd_object))
    goto no_symbols;

  if (!(bfd_get_file_flags(abfd) & HAS_SYMS))
    goto no_symbols;

  storage = bfd_get_symtab_upper_bound(abfd);
  if (storage <= 0)
    goto no_symbols;

  symbols = malloc(storage);
  symcount = bfd_canonicalize_symtab(abfd, symbols);
  if (symcount < 0)
    goto no_symbols;

  symcount = remove_useless_symbols(symbols, symcount);
//  bfd_close(abfd);
  return;

no_symbols:
  fprintf(stderr, "no symbols in %s\n", bfd_get_filename(abfd));
  if (abfd != NULL)
    bfd_close(abfd);
}


/* Like target_read_memory, but slightly different parameters.  */
static int
dis_asm_read_memory(bfd_vma memaddr, bfd_byte *myaddr, unsigned int len,
                     struct disassemble_info *info)
{
  memcpy(myaddr, (void *)(int)memaddr, len);
  return 0;
}

static void
dis_asm_memory_error(int status, bfd_vma memaddr,
                      struct disassemble_info *info)
{
  fprintf(stderr, "memory_error %p\n", (void *)(int)memaddr);
}

static void
dis_asm_print_address(bfd_vma addr, struct disassemble_info *info)
{
  asymbol **sptr = symbols;
  int i;

  printf("%08x", (int)addr);

  for (i = 0; i < symcount; i++) {
    asymbol *sym = *sptr++;

    if (addr == sym->value + sym->section->vma) {
      printf(" <%s>", sym->name);
      break;
    }
  }
}

static int insn_printf(void *f, const char *format, ...)
{
  va_list args;
  size_t n;

  va_start(args, format);
  n = vprintf(format, args);
  va_end(args);

  return n;
}

static void host_dasm_init(void)
{
  slurp_symtab(g_argv[0]);

  init_disassemble_info(&di, NULL, insn_printf);
  di.flavour = bfd_target_unknown_flavour;
  di.memory_error_func = dis_asm_memory_error; 
  di.print_address_func = dis_asm_print_address;
//  di.symbol_at_address_func = dis_asm_symbol_at_address;
  di.read_memory_func = dis_asm_read_memory;
  di.arch = BFD_ARCH;
  di.mach = BFD_MACH;
  di.endian = BFD_ENDIAN_LITTLE;
  disassemble_init_for_target(&di);
}

void host_dasm(void *addr, int len)
{
  bfd_vma vma_end, vma = (bfd_vma)(int)addr;
  static int init_done = 0;

  if (!init_done) {
    host_dasm_init();
    init_done = 1;
  }

  vma_end = vma + len;
  while (vma < vma_end) {
    printf("  %p ", (void *)(long)vma);
    vma += print_insn_func(vma, &di);
    printf("\n");
  }
}

