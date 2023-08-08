#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int e1000_transmit(struct mbuf *m)
{
  // 获取锁，保证线程安全
  acquire(&e1000_lock);

  // 获取当前的 TX 环描述符索引
  uint32 idx = regs[E1000_TDT];

  // 检查 TX 环是否已满
  if (tx_ring[idx].status != E1000_TXD_STAT_DD)
  {
    // TX 环已满，返回错误
    printf("e1000_transmit: tx队列已满\n");

    // 同步内存访问
    __sync_synchronize();

    // 释放锁
    release(&e1000_lock);

    return -1;
  }
  else
  {
    // 如果存在上一个已传输的 mbuf，则释放它
    if (tx_mbufs[idx] != 0)
    {
      mbuffree(tx_mbufs[idx]);
    }

    // 填充描述符
    tx_ring[idx].addr = (uint64) m->head;
    tx_ring[idx].length = (uint16) m->len;
    tx_ring[idx].cso = 0;
    tx_ring[idx].css = 0;

    // 设置必要的命令标志位
    tx_ring[idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;

    // 保存 mbuf 的指针，以便后续释放
    tx_mbufs[idx] = m;

    // 更新 TX 环的位置
    regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;
  }

  // 同步内存访问
  __sync_synchronize();

  // 释放锁
  release(&e1000_lock);

  return 0;
}


extern void net_rx(struct mbuf *);
static void e1000_recv(void)
{
  // 获取下一个期望接收的包的位置
  uint32 idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

  // 获取 RX 描述符
  struct rx_desc* dest = &rx_ring[idx];

  // 检查是否有新的数据包到达
  while (rx_ring[idx].status & E1000_RXD_STAT_DD)
  {
    acquire(&e1000_lock);

    // 获取接收缓冲区
    struct mbuf *buf = rx_mbufs[idx];

    // 将数据包长度添加到 mbuf 中
    mbufput(buf, dest->length);

    // 分配新的 mbuf
    if (!(rx_mbufs[idx] = mbufalloc(0)))
      panic("mbuf分配失败");

    // 更新 RX 描述符的地址和状态
    dest->addr = (uint64)rx_mbufs[idx]->head;
    dest->status = 0;

    // 更新 RDT 寄存器的值，表示接收到了一个新的数据包
    regs[E1000_RDT] = idx;

    // 同步内存访问
    __sync_synchronize();

    // 释放锁
    release(&e1000_lock);

    // 将接收到的数据包传递给网络层处理
    net_rx(buf);

    // 更新下一个期望接收的包的位置
    idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    dest = &rx_ring[idx];
  }
}


void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
