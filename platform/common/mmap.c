#include "3ds.h"
#include "3dshack.h"

int ctr_svchack_successful = 0;

typedef struct
{
   unsigned int requested_map;
   void* buffer;
}pico_mmap_t;

pico_mmap_t pico_mmaps[] = {
   {0x02000000, 0},
   {0x06000000, 0},
   {NULL,       0}
};

void ctr_flush_invalidate_cache()
{
    hack3dsInvalidateAndFlushCaches();
}

void cache_flush_d_inval_i(void *start, void *end)
{
#ifdef __arm__
   size_t len = (char *)end - (char *)start;
   (void)len;
#if defined(__BLACKBERRY_QNX__)
   msync(start, end - start, MS_SYNC | MS_CACHE_ONLY | MS_INVALIDATE_ICACHE);
#elif defined(__MACH__)
   sys_dcache_flush(start, len);
   sys_icache_invalidate(start, len);
#elif defined(_3DS)
   ctr_flush_invalidate_cache();
#elif defined(VITA)
   sceKernelSyncVMDomain(sceBlock, start, len);
#else
   __clear_cache(start, end);
#endif
#endif
}

void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed)
{
   (void)is_fixed;

   if (ctr_svchack_successful)
   {
      pico_mmap_t* pico_mmap;

      for (pico_mmap = pico_mmaps; pico_mmap->requested_map; pico_mmap++)
      {
         if ((pico_mmap->requested_map == addr))
         {
            unsigned int ptr_aligned, tmp;
            unsigned int currentHandle;
            unsigned int perm = 0b011;

            if (need_exec)
               perm = 0b111;

            size = (size + 0xFFF) & ~0xFFF;
            pico_mmap->buffer = malloc(size + 0x1000);
            ptr_aligned = (((unsigned int)pico_mmap->buffer) + 0xFFF) & ~0xFFF;

            svcDuplicateHandle(&currentHandle, 0xFFFF8001);

            if(svcControlProcessMemory(currentHandle, pico_mmap->requested_map, ptr_aligned, size, MEMOP_MAP, perm) < 0)
            {
               exit(1);
            }

            svcCloseHandle(currentHandle);
            return (void*)pico_mmap->requested_map;
         }
      }
   }

   return malloc(size);
}

void *plat_mremap(void *ptr, size_t oldsize, size_t newsize)
{
   if (ctr_svchack_successful)
   {
      pico_mmap_t* pico_mmap;

      for (pico_mmap = pico_mmaps; pico_mmap->requested_map; pico_mmap++)
      {
         if ((pico_mmap->requested_map == (unsigned int)ptr))
         {
            unsigned int ptr_aligned;
            unsigned int currentHandle;
            void* tmp;

            oldsize = (oldsize + 0xFFF) & ~0xFFF;
            newsize = (newsize + 0xFFF) & ~0xFFF;
            ptr_aligned = (((unsigned int)pico_mmap->buffer) + 0xFFF) & ~0xFFF;

            svcDuplicateHandle(&currentHandle, 0xFFFF8001);

            svcControlProcessMemory(currentHandle, pico_mmap->requested_map, ptr_aligned, oldsize, MEMOP_UNMAP, 0b011);

            tmp = realloc(pico_mmap->buffer, newsize + 0x1000);
            if(!tmp)
               return NULL;

            pico_mmap->buffer = tmp;
            ptr_aligned = (((unsigned int)pico_mmap->buffer) + 0xFFF) & ~0xFFF;

            svcControlProcessMemory(currentHandle, pico_mmap->requested_map, ptr_aligned, newsize, MEMOP_MAP, 0x3);

            svcCloseHandle(currentHandle);

            return ptr;
         }
      }
   }

   return realloc(ptr, newsize);

}
void plat_munmap(void *ptr, size_t size)
{
   if (ctr_svchack_successful)
   {
      pico_mmap_t* pico_mmap;

      for (pico_mmap = pico_mmaps; pico_mmap->requested_map; pico_mmap++)
      {
         if ((pico_mmap->requested_map == (unsigned int)ptr))
         {
            unsigned int ptr_aligned;
            unsigned int currentHandle;

            size = (size + 0xFFF) & ~0xFFF;
            ptr_aligned = (((unsigned int)pico_mmap->buffer) + 0xFFF) & ~0xFFF;

            svcDuplicateHandle(&currentHandle, 0xFFFF8001);

            svcControlProcessMemory(currentHandle, (void*)pico_mmap->requested_map, (void*)ptr_aligned, size, MEMOP_UNMAP, 0b011);

            svcCloseHandle(currentHandle);

            free(pico_mmap->buffer);
            pico_mmap->buffer = NULL;
            return;
         }
      }
   }

   free(ptr);
}

// if NULL is returned, static buffer is used
void *plat_mem_get_for_drc(size_t size)
{
   void *mem = NULL;
#ifdef VITA
   sceKernelGetMemBlockBase(sceBlock, &mem);
#endif
   return mem;
}

int plat_mem_set_exec(void *ptr, size_t size)
{
   int ret = -1;
#ifdef _WIN32
   ret = VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, 0);
   if (ret == 0 && log_cb)
      log_cb(RETRO_LOG_ERROR, "VirtualProtect(%p, %d) failed: %d\n", ptr, (int)size,
             GetLastError());
#elif defined(_3DS)

   if (ctr_svchack_successful)
   {
      unsigned int currentHandle;
      svcDuplicateHandle(&currentHandle, 0xFFFF8001);
      ret = svcControlProcessMemory(currentHandle, ptr, 0x0,
                              size, MEMOP_PROT, 0b111);
      svcCloseHandle(currentHandle);
      ctr_flush_invalidate_cache();
   }
   else
   {
      exit(1);
   }

#elif defined(VITA)
   ret = sceKernelOpenVMDomain();
#else
   ret = mprotect(ptr, size, PROT_READ | PROT_WRITE | PROT_EXEC);
   if (ret != 0 && log_cb)
      log_cb(RETRO_LOG_ERROR, "mprotect(%p, %zd) failed: %d\n", ptr, size, errno);
#endif
   return ret;
}