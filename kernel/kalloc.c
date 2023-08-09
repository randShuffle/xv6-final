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
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i<NCPU; i++)
  {
    // char buf[9];
    // snprintf(buf, 6, "kmem_%d", i);
    // initlock(&kmem[i].lock, buf);
    initlock(&kmem[i].lock, "kmem");
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
void kfree(void *pa)
{
  struct run *r;  // 定义指向内存块的指针r
  push_off();  // 关闭中断
  int c = cpuid();  // 获取当前CPU的ID
  pop_off();  // 开启中断

  // 检查内存块是否合法
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 用'1'填充内存块，以捕捉悬空引用
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;  // 将内存块指针转换为run结构体指针

  acquire(&kmem[c].lock);  // 获取当前CPU的内存锁
  r->next = kmem[c].freelist;  // 将内存块插入到自由内存块链表的头部
  kmem[c].freelist = r;  // 更新自由内存块链表的头指针
  release(&kmem[c].lock);  // 释放当前CPU的内存锁
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void)
{
  struct run *r;  // 定义指向自由内存块的指针
  push_off();  // 关闭中断
  int c = cpuid();  // 获取当前CPU的ID
  pop_off();  // 开启中断

  acquire(&kmem[c].lock);  // 获取当前CPU的内存锁
  r = kmem[c].freelist;  // 获取当前CPU的自由内存块链表头指针
  if (r)  // 如果自由内存块链表不为空
  {
    kmem[c].freelist = r->next;  // 更新自由内存块链表头指针为下一个内存块
    release(&kmem[c].lock);  // 释放当前CPU的内存锁
  }
  else  // 如果自由内存块链表为空
  {
    release(&kmem[c].lock);  // 释放当前CPU的内存锁
    for (int i = 0; i < NCPU; i++)  // 遍历其他CPU
    {
      acquire(&kmem[i].lock);  // 获取其他CPU的内存锁
      r = kmem[i].freelist;  // 获取其他CPU的自由内存块链表头指针
      if (r)  // 如果其他CPU的自由内存块链表不为空
      {
        kmem[i].freelist = r->next;  // 更新其他CPU的自由内存块链表头指针为下一个内存块
        release(&kmem[i].lock);  // 释放其他CPU的内存锁
        break;  // 跳出循环
      }
      else  // 如果其他CPU的自由内存块链表为空
      {
        release(&kmem[i].lock);  // 释放其他CPU的内存锁
      }
    }
  }
  if (r)  // 如果获取到了内存块
    memset((char *)r, 5, PGSIZE);  // 使用'5'填充内存块
  return (void *)r;  // 返回内存块指针
}

