#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define NSPG 20
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  void *pages[NSPG];
  int free[NSPG];
} spg;

void freerange(void *pa_start, void *pa_end);
void spg_init(void);
void *spg_alloc(void);
void spg_free(void *pa);
extern char end[];

void kinit() {
  initlock(&kmem.lock, "kmem");
  spg_init();
  freerange(end, (void*)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    int is_spg = 0;
    for(int i = 0; i < NSPG; i++) {
      if(spg.pages[i] == 0)
        continue;
      char *s_start = (char*)spg.pages[i];
      char *s_end = s_start + SUPERPAGE_SIZE;
      if(p >= s_start && p < s_end) {
        is_spg = 1;
        break;
      }
    }
    if(!is_spg)
      kfree(p);
  }
}

void spg_init(void) {
  initlock(&spg.lock, "spg");
  uint64 pa = PGROUNDUP((uint64)end);
  pa = (pa + SUPERPAGE_SIZE - 1) & ~(uint64)(SUPERPAGE_SIZE - 1);
  int count = 0;
  for(int i = 0; i < NSPG; i++) {
    if(pa + SUPERPAGE_SIZE > PHYSTOP)
      break;
    spg.pages[i] = (void*)pa;
    spg.free[i] = 1;
    pa += SUPERPAGE_SIZE;
    count++;
  }
  for(int i = count; i < NSPG; i++) {
    spg.pages[i] = 0;
    spg.free[i] = 0;
  }
}

void *spg_alloc(void) {
  acquire(&spg.lock);
  for(int i = 0; i < NSPG; i++) {
    if(spg.free[i] && spg.pages[i] != 0) {
      spg.free[i] = 0;
      void *pa = spg.pages[i];
      release(&spg.lock);
      char *p = (char*)pa;
      for(int j = 0; j < SUPERPAGE_SIZE; j += PGSIZE) {
        memset(p + j, 0, PGSIZE);
      }
      return pa;
    }
  }
  release(&spg.lock);
  return 0;
}

void spg_free(void *pa) {
  acquire(&spg.lock);
  for(int i = 0; i < NSPG; i++) {
    if(spg.pages[i] == pa) {
      spg.free[i] = 1;
      release(&spg.lock);
      return;
    }
  }
  release(&spg.lock);
  panic("spg_free");
}
