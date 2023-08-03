#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

#ifdef LAB_PGTBL
extern pte_t *walk(pagetable_t, uint64, int);
int sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 srcva, st; // 源虚拟地址和目标地址
  int len; // 长度
  uint64 buf = 0; // 缓冲区
  struct proc *p = myproc(); // 获取当前进程

  acquire(&p->lock); // 获取进程锁

  argaddr(0, &srcva); // 获取第一个参数，源虚拟地址
  argint(1, &len); // 获取第二个参数，长度
  argaddr(2, &st); // 获取第三个参数，目标地址
  if ((len > 64) || (len < 1)) // 长度超出范围则返回错误
    return -1;
  pte_t *pte; // 页表项指针
  for (int i = 0; i < len; i++)
  {
    pte = walk(p->pagetable, srcva + i * PGSIZE, 0); // 通过虚拟地址获取页表项
    if(pte == 0){ // 如果页表项为空，则返回错误
      return -1;
    }
    if((*pte & PTE_V) == 0){ // 如果页表项无效，则返回错误
      return -1;
    }
    if((*pte & PTE_U) == 0){ // 如果页表项不属于用户空间，则返回错误
      return -1;
    }
    if(*pte & PTE_A){ // 如果页表项的访问位被设置
      *pte = *pte & ~PTE_A; // 清除访问位
      buf |= (1 << i); // 将对应位置为1
    }
  }
  release(&p->lock); // 释放进程锁
  copyout(p->pagetable, st, (char *)&buf, ((len -1) / 8) + 1); // 将buf拷贝到用户空间
  return 0; // 返回成功
}

#endif

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
