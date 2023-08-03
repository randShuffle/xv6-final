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

// COW 中一个物理页面不仅被映射给一个页表，故而需要一个数据结构维护其引用的数量
struct spinlock reflock;
uint8 referencecount[PHYSTOP/PGSIZE];

void kinit()
{
  initlock(&kmem.lock, "kmem");  // 初始化内核内存管理的锁
  initlock(&reflock, "ref");  // 初始化引用计数的锁
  freerange(end, (void*)PHYSTOP);  // 释放物理内存空间
}


void freerange(void *pa_start, void *pa_end)
{
  char *p;
  
  // 将pa_start向上对齐到下一个页面的起始地址
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  // 从p开始，每次增加PGSIZE（页面大小）直到达到pa_end
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    // 获取reflock（引用锁），防止并发访问引用计数
    acquire(&reflock);
    
    // 将当前页面的引用计数置为0
    referencecount[(uint64)p / PGSIZE] = 0;
    
    // 释放reflock（引用锁）
    release(&reflock);
    
    // 释放物理内存页面p
    kfree(p);
  }
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
  if(r)
    referencecount[PGROUNDUP((uint64)r)/PGSIZE] = 1; 
  // 查到：https://blog.miigon.net/posts/s081-lab6-copy-on-write-fork/
  // 说 kalloc() 可以不用加锁，但还没搞懂原理
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
