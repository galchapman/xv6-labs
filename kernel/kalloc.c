// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

struct run;

void freerange(void *pa_start, void *pa_end, struct run** freelist);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
} mem_cpus[NCPU];

void
kinit()
{
  uint64 last_addr = PGROUNDUP((uint64)end);
  uint64 step = PGROUNDDOWN((PGROUNDDOWN(PHYSTOP) - last_addr) / NCPU);
  for (int i = 0; i < NCPU; ++i) {
    struct kmem* cpu_mem = &mem_cpus[i];

    initlock(&cpu_mem->lock, "kmem.lock");
    acquire(&cpu_mem->lock);
    freerange((void*)(last_addr + i * step),
            i == NCPU-1 ? (void*)PHYSTOP : (void*)(last_addr + (i+1) * step),
            &cpu_mem->freelist);
    release(&cpu_mem->lock);
  }
}

void
freerange(void *pa_start, void *pa_end, struct run** freelist)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  *freelist = (void*)0;
  if (p + PGSIZE > (char*)pa_end)
    return; // no memory

  struct run *r = (void*)p;
  *freelist = r;
  for(p += PGSIZE; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    r->next = (void*)p;
    r = r->next;
  }
  r->next = (void*)0;
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

  push_off();

  int id = cpuid();

  struct kmem *cpu_mem = &mem_cpus[id];


  acquire(&cpu_mem->lock);
  r->next = cpu_mem->freelist;
  cpu_mem->freelist = r;
  release(&cpu_mem->lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();

  int id = cpuid();

  // Pick from current cpu list if not empty
  // Otherwise borrow from next avilable cpu free list.
  for (int i = 0; i < NCPU; ++i) {
    struct kmem *cpu_mem = &mem_cpus[(id + i) % NCPU];
    acquire(&cpu_mem->lock);
    r = cpu_mem->freelist;
    if(r) {
        cpu_mem->freelist = r->next;
    }
    release(&cpu_mem->lock);
    if (r) {
        break;
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
