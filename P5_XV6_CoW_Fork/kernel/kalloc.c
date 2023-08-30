// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;

  /*
  P5 changes
  */
  uint free_pages; //track free pages
  uint ref_cnt[PHYSTOP / PGSIZE]; //track reference count

} kmem;

extern char end[]; // first address after kernel loaded from ELF file

void
kfree_init(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  kmem.ref_cnt[PADDR(v) / PGSIZE] = 0;
  kmem.free_pages++;
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");
  acquire(&kmem.lock);
  kmem.free_pages = 0;
  release(&kmem.lock);
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree_init(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  acquire(&kmem.lock);
  int ref_cnt = (int) kmem.ref_cnt[PADDR(v) / PGSIZE];
  // make sure when kfree is called, only one process is using the page
  if(ref_cnt > 1){
    kmem.ref_cnt[PADDR(v) / PGSIZE]--;
  }
  else if(ref_cnt == 1){
    kmem.ref_cnt[PADDR(v) / PGSIZE]--;

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);
    kmem.free_pages++;
    r = (struct run*)v;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  else {
    panic("kfree");
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.ref_cnt[PADDR(r) / PGSIZE] = 1;
    kmem.free_pages--;
  }
  release(&kmem.lock);
  return (char*)r;
}

int getFreePagesCount(void)
{
  int free_pages;
  acquire(&kmem.lock);
  free_pages = (int)kmem.free_pages;
  release(&kmem.lock);
  return free_pages;
}

void inc_ref_cnt(char *r)
{
  acquire(&kmem.lock);
  kmem.ref_cnt[PADDR(r) / PGSIZE]++;
  release(&kmem.lock);
}

void dec_ref_cnt(char *r)
{
  acquire(&kmem.lock);
  kmem.ref_cnt[PADDR(r) / PGSIZE]--;
  release(&kmem.lock);
}

int get_ref_cnt(char *r)
{
  int rst;
  acquire(&kmem.lock);
  rst = (int) kmem.ref_cnt[PADDR(r) / PGSIZE];
  release(&kmem.lock);
  return rst;
}
