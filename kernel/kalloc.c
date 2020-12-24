// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

/**
 * 实验3-1重新设计内存分配器Memory Allocator
 * 为每个CPU分配独有的内存分配器空闲链表
*/

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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

// 修改结构体的定义，因为实际只有3个，所以这里就直接定义3个
struct kmem kmems[3];

//-----------------------------------------------------wsl
int getcpu(){
  push_off();
  int cpu = cpuid();
  pop_off();
  return cpu;
}

void
kinit()
{
  for (int i = 0; i < 3; i++)
  {
//-----------------------------------------------------wsl    
    initlock(&kmems[i].lock, "kmem"); 
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

//释放v指向的物理内存页
//通常应该通过调用kalloc（）返回。在初始化异常时，请参阅上面的kinit
//作用：将kalloc获取得到的物理内存加到空闲链表头

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

//-----------------------------------------------------wsl 
  int hart = getcpu();  //当前调用这个函数的CPU

  acquire(&kmems[hart].lock);
  r->next = kmems[hart].freelist;
  kmems[hart].freelist = r;
  release(&kmems[hart].lock);
}

// 窃取别人的空闲链表
void *
steal(){
  struct run *rs = 0;        //链表
  for (int i = 0; i < 3; i++)
  {
//-----------------------------------------------------wsl    
    acquire(&kmems[i].lock);  //调用的次数
    if (kmems[i].freelist!=0) //找到r!=0的就可以了
    {
      rs = kmems[i].freelist;
      kmems[i].freelist = rs->next;
      release(&kmems[i].lock);
      return (void *)rs;
    }
    release(&kmems[i].lock); // 是当前要窃取别人freelist的那个cpu
  }
  return (void*)rs;  //没有办法窃取的时候还是返回这个指针
}


//分配一个4096字节的物理内存页。
//返回内核可以使用的指针。
//如果无法分配内存，则返回0。
//作用：分配物理内存
void *
kalloc(void)
{
  struct run *r;        //链表
  
//---------------------------------------要修改  
  int hart;             //cpu id
  hart = getcpu();

  acquire(&kmems[hart].lock);  //调用的次数
  r = kmems[hart].freelist;
  if(r)
    kmems[hart].freelist = r->next;
  release(&kmems[hart].lock);
  
  if(!r)
    r = steal();  //r = 0窃取别人的空闲链表，可以自动进行类型转换的

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

