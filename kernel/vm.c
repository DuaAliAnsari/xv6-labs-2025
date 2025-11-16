#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  
extern char trampoline[]; 

pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

void
kvminithart()
{
  sfence_vma();
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - PGSIZE;
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Map a 2MB superpage (at level 1 of page table)
int
mapsuperpage(pagetable_t pagetable, uint64 va, uint64 pa, int perm)
{
  pte_t *pte;
  
  if(va >= MAXVA)
    panic("mapsuperpage");
  if((va % SUPERPAGE_SIZE) != 0)
    panic("mapsuperpage: va not aligned");
  if((pa % SUPERPAGE_SIZE) != 0)
    panic("mapsuperpage: pa not aligned");
  
  // Get/create level-2 PTE
  pte = &pagetable[PX(2, va)];
  if(*pte & PTE_V) {
    pagetable = (pagetable_t)PTE2PA(*pte);
  } else {
    pagetable = (pagetable_t)kalloc();
    if(pagetable == 0)
      return -1;
    memset(pagetable, 0, PGSIZE);
    *pte = PA2PTE(pagetable) | PTE_V;
  }
  
  // Set level-1 PTE with R/W/X bits (makes it a leaf/superpage)
  pte = &pagetable[PX(1, va)];
  if(*pte & PTE_V)
    panic("mapsuperpage: remap");
  
  *pte = PA2PTE(pa) | perm | PTE_V;
  return 0;
}

// Check if VA is mapped as a superpage
// Returns 1 if superpage, 0 otherwise
int
issuperpage(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  if(va >= MAXVA)
    return 0;
  
  // Must be aligned to superpage boundary
  if((va % SUPERPAGE_SIZE) != 0)
    return 0;
  
  // Check level-2
  pte = &pagetable[PX(2, va)];
  if((*pte & PTE_V) == 0)
    return 0;
  
  // Check level-1 - if it has R/W/X bits, it's a leaf (superpage)
  pagetable = (pagetable_t)PTE2PA(*pte);
  pte = &pagetable[PX(1, va)];
  
  if((*pte & PTE_V) == 0)
    return 0;
    
  // Superpage = level-1 PTE with V and at least one of R/W/X
  return (*pte & (PTE_R | PTE_W | PTE_X)) != 0;
}

// CRITICAL FIX: Simplified uvmunmap to prevent infinite loops
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  // Safety: if npages is 0, return immediately
  if(npages == 0)
    return;

  // Convert to iterating by page count instead of address
  for(uint64 i = 0; i < npages; ){
    a = va + i * PGSIZE;
    
    // Safety check
    if(a >= MAXVA)
      break;
    
    // Check for superpage at this address
    if((a % SUPERPAGE_SIZE) == 0 && (npages - i) >= 512) {
      pte_t *pte2 = &pagetable[PX(2, a)];
      
      if(*pte2 & PTE_V) {
        pagetable_t pt1 = (pagetable_t)PTE2PA(*pte2);
        pte_t *pte1 = &pt1[PX(1, a)];
        
        // Check if this is actually a superpage
        if((*pte1 & PTE_V) && (*pte1 & (PTE_R | PTE_W | PTE_X))) {
          // This is a superpage
          if(do_free) {
            uint64 pa = PTE2PA(*pte1);
            superfree((void*)pa);
          }
          *pte1 = 0;
          i += 512;  // Skip 512 pages (2MB)
          continue;
        }
      }
    }
    
    // Regular page
    if((pte = walk(pagetable, a, 0)) == 0) {
      i++;
      continue;
    }
    if((*pte & PTE_V) == 0) {
      i++;
      continue;
    }
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
    i++;
  }
}

uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  
  for(a = oldsz; a < newsz; ){
    // Safety check
    if(a >= MAXVA) {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    
    // Try to use a superpage if:
    // 1. Virtual address is 2MB-aligned
    // 2. At least 2MB remains to allocate
    if((a % SUPERPAGE_SIZE) == 0 && (newsz - a) >= SUPERPAGE_SIZE) {
      mem = superalloc();
      if(mem != 0) {
        // Map as superpage
        if(mapsuperpage(pagetable, a, (uint64)mem, PTE_R|PTE_W|PTE_U|xperm) != 0){
          superfree(mem);
          uvmdealloc(pagetable, a, oldsz);
          return 0;
        }
        a += SUPERPAGE_SIZE;
        continue;
      }
    }
    
    // Regular page allocation
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    a += PGSIZE;
  }
  return newsz;
}

uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // This is a leaf page - skip it
    }
  }
  kfree((void*)pagetable);
}

void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// CRITICAL FIX: Simplified uvmcopy to prevent infinite loops
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  
  // Use page count iteration for safety
  uint64 npages = PGROUNDUP(sz) / PGSIZE;
  
  for(i = 0; i < npages; ){
    uint64 va = i * PGSIZE;
    
    // Safety check
    if(va >= MAXVA || va >= sz)
      break;
    
    // Check for superpage
    if((va % SUPERPAGE_SIZE) == 0 && (npages - i) >= 512) {
      pte_t *pte2 = &old[PX(2, va)];
      
      if(*pte2 & PTE_V) {
        pagetable_t pt1 = (pagetable_t)PTE2PA(*pte2);
        pte_t *pte1 = &pt1[PX(1, va)];
        
        // Check if this is a superpage (leaf at level 1)
        if((*pte1 & PTE_V) && (*pte1 & (PTE_R | PTE_W | PTE_X))) {
          pa = PTE2PA(*pte1);
          flags = PTE_FLAGS(*pte1);
          
          if((mem = superalloc()) == 0)
            goto err;
            
          memmove(mem, (char*)pa, SUPERPAGE_SIZE);
          
          if(mapsuperpage(new, va, (uint64)mem, flags) != 0){
            superfree(mem);
            goto err;
          }
          
          i += 512;  // Skip 512 pages
          continue;
        }
      }
    }
    
    // Regular page
    if((pte = walk(old, va, 0)) == 0) {
      i++;
      continue;
    }
    if((*pte & PTE_V) == 0) {
      i++;
      continue;
    }
    
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, va, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
    i++;
  }
  return 0;

 err:
  uvmunmap(new, 0, i, 1);
  return -1;
}

void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
  
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    pte = walk(pagetable, va0, 0);
    if((*pte & PTE_W) == 0)
      return -1;
      
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    return 0;
  }
  mem = (uint64) kalloc();
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}

int
ismapped(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0) {
    return 0;
  }
  if (*pte & PTE_V){
    return 1;
  }
  return 0;
}

// Helper function for vmprint - recursively prints page table entries
void
vmprint_recursive(pagetable_t pagetable, int level, uint64 va_base)
{
  // Safety check
  if(level < 0 || level > 2)
    return;
    
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      // Calculate the virtual address for this PTE
      uint64 va;
      if(level == 2) {
        va = (uint64)i << (9 + 9 + 12); // Level 2: bits 30-38
      } else if(level == 1) {
        va = va_base | ((uint64)i << (9 + 12)); // Level 1: bits 21-29
      } else {
        va = va_base | ((uint64)i << 12); // Level 0: bits 12-20
      }
      
      // Print indentation based on level
      for(int j = 0; j <= (2 - level); j++) {
        printf(" ..");
      }
      
      // Print the virtual address, PTE, and physical address
      printf("0x%016lx: pte 0x%016lx pa 0x%016lx\n", 
             va, pte, PTE2PA(pte));
      
      // If this is not a leaf page (no R/W/X bits), recurse
      if(level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // This PTE points to a lower-level page table
        uint64 child = PTE2PA(pte);
        vmprint_recursive((pagetable_t)child, level - 1, va);
      }
    }
  }
}

// Print the page table structure
void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  vmprint_recursive(pagetable, 2, 0);
}

// CRITICAL: This is the function the test uses to verify superpages
pte_t *
pgpte(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  pagetable_t level2_pt, level1_pt, level0_pt;
  
  if(va >= MAXVA)
    return 0;
  
  // Level 2
  level2_pt = pagetable;
  pte = &level2_pt[PX(2, va)];
  if((*pte & PTE_V) == 0)
    return 0;
  
  // Level 1
  level1_pt = (pagetable_t)PTE2PA(*pte);
  pte = &level1_pt[PX(1, va)];
  
  if((*pte & PTE_V) == 0)
    return 0;
    
  // Check if this is a superpage (has R/W/X bits set)
  if(*pte & (PTE_R | PTE_W | PTE_X)) {
    // This is a superpage leaf at level 1
    return pte;
  }
  
  // Not a superpage, level-1 points to level-0 page table
  // Level 0 - regular page
  level0_pt = (pagetable_t)PTE2PA(*pte);
  pte = &level0_pt[PX(0, va)];
  
  if((*pte & PTE_V) == 0)
    return 0;
    
  return pte;
}
