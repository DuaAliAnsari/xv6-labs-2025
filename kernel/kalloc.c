// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Superpage allocator
// SUPERPAGE_SIZE should be defined in riscv.h or memlayout.h as (512 * PGSIZE) = 2MB
#ifndef SUPERPAGE_SIZE
#define SUPERPAGE_SIZE (512 * PGSIZE)  // 2MB
#endif

#define NSUPERPAGES 16

struct {
  struct spinlock lock;
  void *pages[NSUPERPAGES];
  int used[NSUPERPAGES];  // 0 = free, 1 = used
  int count;
} supermem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&supermem.lock, "supermem");
  
  // Initialize superpage allocator
  supermem.count = 0;
  for(int i = 0; i < NSUPERPAGES; i++) {
    supermem.pages[i] = 0;
    supermem.used[i] = 0;
  }
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  // Find first 2MB-aligned address
  uint64 super_start = (uint64)p;
  if(super_start % SUPERPAGE_SIZE != 0) {
    super_start = ((super_start / SUPERPAGE_SIZE) + 1) * SUPERPAGE_SIZE;
  }
  
  // Reserve NSUPERPAGES worth of 2MB chunks
  uint64 super_end = super_start;
  for(int i = 0; i < NSUPERPAGES && super_start + (i+1) * SUPERPAGE_SIZE <= (uint64)pa_end; i++) {
    supermem.pages[i] = (void*)(super_start + i * SUPERPAGE_SIZE);
    supermem.used[i] = 0;
    supermem.count++;
    super_end = super_start + (i+1) * SUPERPAGE_SIZE;
  }
  
  // Free everything before superpages as regular pages
  for(p = (char*)PGROUNDUP((uint64)pa_start); (uint64)p < super_start; p += PGSIZE) {
    kfree(p);
  }
  
  // Free everything after superpages as regular pages  
  for(p = (char*)super_end; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate a 2MB superpage
// Returns pointer to 2MB-aligned physical memory, or 0 if none available
void *
superalloc(void)
{
  void *pa = 0;
  
  acquire(&supermem.lock);
  
  for(int i = 0; i < supermem.count; i++) {
    if(supermem.used[i] == 0) {
      supermem.used[i] = 1;
      pa = supermem.pages[i];
      break;
    }
  }
  
  release(&supermem.lock);
  
  if(pa) {
    // Clear the entire 2MB superpage
    memset(pa, 0, SUPERPAGE_SIZE);
  }
    
  return pa;
}

// Free a 2MB superpage
// pa must be a valid superpage address returned by superalloc()
void
superfree(void *pa)
{
  if(((uint64)pa % SUPERPAGE_SIZE) != 0)
    panic("superfree: not aligned");
  
  acquire(&supermem.lock);
  
  int found = 0;
  for(int i = 0; i < supermem.count; i++) {
    if(supermem.pages[i] == pa) {
      if(supermem.used[i] == 0)
        panic("superfree: already free");
      supermem.used[i] = 0;
      found = 1;
      break;
    }
  }
  
  release(&supermem.lock);
  
  if(!found)
    panic("superfree: not a superpage");
    
  // Fill with junk to catch use-after-free bugs
  memset(pa, 1, SUPERPAGE_SIZE);
}

// Demote a superpage to regular pages
// This breaks up a 2MB superpage into 512 individual 4KB pages
// and adds them to the regular page allocator
// Returns 0 on success, -1 on failure
int
superdemote(void *pa)
{
  if(((uint64)pa % SUPERPAGE_SIZE) != 0)
    return -1;
    
  // Check if this is actually a superpage we manage
  acquire(&supermem.lock);
  int found = 0;
  for(int i = 0; i < supermem.count; i++) {
    if(supermem.pages[i] == pa && supermem.used[i] == 1) {
      found = 1;
      supermem.used[i] = 0;  // Mark as free in superpage allocator
      break;
    }
  }
  release(&supermem.lock);
  
  if(!found)
    return -1;
  
  // Add all 512 pages to the regular free list
  for(int j = 0; j < 512; j++) {
    void *page = (void*)((uint64)pa + j * PGSIZE);
    
    // Don't use kfree() directly as it calls memset and checks
    struct run *r = (struct run*)page;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  
  return 0;
}
