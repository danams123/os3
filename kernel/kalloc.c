// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

extern uint64 cas(volatile void *addr, int expected, int newval); // cas.S

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint64 page_counter[NUM_PYS_PAGES];

int reference_change(uint64 pa, uint64 to_add)
{
  uint64 tmp;
  do
  {
    tmp = page_counter[INDEX((uint64)pa)];
  } while (!cas(&page_counter[INDEX((uint64)pa)], tmp, tmp + to_add));

  return tmp;
}
int get_counter(uint64 pa)
{
  return page_counter[INDEX((uint64)pa)];
}

void kinit()
{
  initlock(&kmem.lock, "kmem");
  memset(page_counter, 0, sizeof(uint64) * NUM_PYS_PAGES);
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (page_counter[INDEX((uint64)pa)] > 1)
  {
    reference_change((uint64)pa, -1);
    return;
  }

  page_counter[INDEX((uint64)pa)] = 0;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

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

  if (r)
  {
    kmem.freelist = r->next;
    page_counter[INDEX((uint64)r)] = 1;
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
