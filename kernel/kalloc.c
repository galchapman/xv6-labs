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
static uint8* cow_refrence_counts;
static uint64 first_page;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  uint64 num_pages = (PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / (PGSIZE + sizeof(uint8));
  cow_refrence_counts = (void*)PGROUNDUP((uint64)end) + 1;

  first_page = PGROUNDUP((uint64)&cow_refrence_counts[num_pages]);

  // reset refrence counters
  memset(cow_refrence_counts, 1, sizeof(uint8) * num_pages);

  freerange((void*)first_page, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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


  acquire(&kmem.lock);
  uint64 page_index = (PGROUNDDOWN((uint64)pa) - first_page) / PGSIZE;
  if ((--cow_refrence_counts[page_index]) > 0) {
    // wait for refrence count to go to zero
    release(&kmem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

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

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // set cow refence count to 1
    uint64 page_index = (PGROUNDDOWN((uint64)r) - first_page) / PGSIZE;
    uint64 num_pages = (PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / (PGSIZE + sizeof(uint8));
    if (page_index >= num_pages) {
      printf("invalid page index: %d > num_pages = %d\n", page_index, num_pages);
      panic("invalid page index\n");
    }
    cow_refrence_counts[page_index] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint64
lockless_kalloc() {
  struct run *r;

  if (!holding(&kmem.lock)) {
    panic("locked not owned\n");
  }

  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // set cow refence count to 1
    uint64 page_index = (PGROUNDDOWN((uint64)r) - first_page) / PGSIZE;
    uint64 num_pages = (PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / (PGSIZE + sizeof(uint8));
    if (page_index >= num_pages) {
      printf("invalid page index: %d > num_pages = %d\n", page_index, num_pages);
      panic("invalid page index\n");
    }
    cow_refrence_counts[page_index] = 1;
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (uint64)r;
}

void
cow_increfcnt(uint64 page) {
  acquire(&kmem.lock);
  uint64 page_index = (PGROUNDDOWN(page) - first_page) / PGSIZE;
  ++cow_refrence_counts[page_index];
  release(&kmem.lock);
}

uint64
cow_make_writeable(uint64 page) {
  uint64 new_page = page;
  acquire(&kmem.lock);
  page = PGROUNDDOWN(page);
  uint64 page_index = (page - first_page) / PGSIZE;
  if (cow_refrence_counts[page_index] > 1) {
    new_page = lockless_kalloc();
    if (new_page == 0) { // out of memory
      release(&kmem.lock);
      return new_page;
    }
    --cow_refrence_counts[page_index];
    memmove((void*)new_page, (void*)page, PGSIZE);
  } else if (cow_refrence_counts[page_index] == 1) {
    // Does nothing
  } else {
    panic("Invalid cow refrence counter\n");
  }
  release(&kmem.lock);
  return new_page;
}