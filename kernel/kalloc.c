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

// now you not only create a kmem, but a type kmem
struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  // rewrite
  int i;
  // initial every CPU's freelist
  for(i=0; i<NCPU; i++){
    initlock(&kmems[i].lock, "kmem");
  }
  // the lock's name is set as kmem
  // the lock's initial cpu is set 0
  // the lock is set 0, lock now is free
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  // promise the initial virtual address is end
  p = (char*)PGROUNDUP((uint64)pa_start);
  // until the p reach the PHYSTOP, stop kfree(p)
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
  // write
  // close INTERRUPPTION
  push_off();
  // get CPUID
  int cpuID = cpuid();
  r = (struct run*)pa;
  // acquire spinlock
  acquire(&kmems[cpuID].lock);
  // add the page to the header of freelist
  r->next = kmems[cpuID].freelist;
  kmems[cpuID].freelist = r;
  // realese the spinlock
  release(&kmems[cpuID].lock);
  // open INTERRUPPTION
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
  int cpuID = cpuid();
  // acquire the spinlock
  acquire(&kmems[cpuID].lock);
  // acquire the header of freelist
  r = kmems[cpuID].freelist;
  if(r)
    // acquire successed, let the header of freelist move
    kmems[cpuID].freelist = r->next;
  else{
    // else steal free page from other CPU's freelist
    // steal form the next cpu
    int i,j;
    // j:looping times, and it must less than NCPU-1 
    for(i=(cpuID+1)%NCPU,j=0; j<NCPU-1; i=(i+1)%NCPU, j++){
      acquire(&kmems[i].lock);
      // if the freelist is not null
      if(kmems[i].freelist){
        r = kmems[i].freelist;
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        // successed, loop exit
        break;
      }
      else
        release(&kmems[i].lock);
    }
  }
  pop_off();
  // realease the spinlock
  release(&kmems[cpuID].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
