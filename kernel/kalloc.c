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

int 
cpuid_interoff() {
  int idx;
  push_off();
  idx = cpuid();
  pop_off();
  return idx;
}

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem.lock[i], "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
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

  int curid = cpuid_interoff();

  acquire(&kmem.lock[curid]);
  r->next = kmem.freelist[curid];
  kmem.freelist[curid] = r;
  release(&kmem.lock[curid]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int curid = cpuid_interoff();

  r = kmem.freelist[curid];

  if (r) {
    acquire(&kmem.lock[curid]);
    kmem.freelist[curid] = r->next;
    release(&kmem.lock[curid]);
  }else {
    for (int i = 1; i < NCPU; i++) {
      int nextcpuid = (curid + i) % NCPU;
      if (kmem.freelist[nextcpuid]) {
        r = kmem.freelist[nextcpuid];
        acquire(&kmem.lock[nextcpuid]);
        kmem.freelist[nextcpuid] = r->next;
        release(&kmem.lock[nextcpuid]);

        //add this page to curid cpu's freelist,not needed
        //acquire(&kmem.lock[curid]);
        //kmem.freelist[curid] = r;
        //r->next = 0;
        //kmem.freelist[curid] = 0;
        //release(&kmem.lock[curid]);
      }
    }
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
