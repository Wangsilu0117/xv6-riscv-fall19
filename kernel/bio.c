//缓冲缓存。
//
//缓冲区缓存是一个包含buf结构的链表磁盘块内容的缓存副本。
//缓存磁盘块内存减少了磁盘读取的次数，
//还提供了多个进程使用的磁盘块的同步点。
//
//接口：
//*要获取特定磁盘块的缓冲区，请调用bread。
//*更改缓冲区数据后，调用bwrite将其写入磁盘。
//*处理完缓冲区后，调用brelse。
//*调用brelse后不要使用缓冲区。
//*一次只能有一个进程使用缓冲区，
//所以不要把它们保存得比需要的时间长。

/**
 * 使用哈希表将各个块按块号blockno作为key分组，并为每个组分配一个专用的锁。
 * 在bget()中查找指定块时，锁上对应的锁（获取空闲块号须另作处理）。
 * 分组的数量使用质数组（如13组）以减少哈希争用。
 * 哈希表的搜索和空闲缓存块的查找保证原子性。
*/


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 /*素数个哈希桶*/

// 所有缓冲区的链接列表，通过prev/next
// head.next 是最近使用的。
/* 每个哈希桶为双向链表加一把锁 */

struct {
  struct spinlock lock[NBUCKETS];  //自旋锁
  struct buf buf[NBUF];
// -----------------------------------------------------wsl改  
  struct buf hashbucket[NBUCKETS]; //代替head
} bcache;

/* 哈希映射 */
// -----------------------------------------------------wsl改
int bhash(int blockno){
  return blockno % NBUCKETS;
}


void
binit(void)  //双向循环链表
{
  struct buf *b; //b->lock 睡眠锁

  for (int i = 0; i < NBUCKETS; i++)
  {
// -----------------------------------------------------wsl改    
    initlock(&bcache.lock[i], "bcache.bucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  
  // 此时因为buffer没有和磁盘块对应起来，所以blockno全部为初始值0，将其全部放在第一个bucket中
  // 至于其他bucket缺少buffer该怎么解决，在bget里阐述
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // 仍然是插在表头
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

//通过缓冲区缓存查找设备dev上的块。
//如果找不到，请分配一个缓冲区。
//在这两种情况下，返回锁定的缓冲区。
// 获取空闲缓存块
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
// --------------------------------------------------wsl改
  int h = bhash(blockno);   //得到一个对应的hash值，用这个代替了原来的head

  acquire(&bcache.lock[h]); //锁住这个锁

  // Is the block already cached?
  //首先在blockno对应的bucket中寻找，refcnt可能=0，也可能！=0
  for(b = bcache.hashbucket[h].next; b != &bcache.hashbucket[h]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  // 如果在h对应的bucket中没有找到，那么需要到其他bucket中找，这种情况不会少见，因为
  // binit中，我们就把所有的buffer都插入到了第一个bucket中（当时blockno都是0
  // 此时原来bucket的锁还没有释放，因为我们在其他bucket中找到buffer后，还要将其插入到原bucket中  
  // --------------------------------------------------wsl改
  int nh = (h+1) % NBUCKETS; //nh表示下一个要探索的bucket，当它重新变成h，说明所有的buffer都bussy（refcnt不为0），此时,panic
  while (nh!=h)
  {
    acquire(&bcache.lock[nh]);
    for(b = bcache.hashbucket[nh].prev; b != &bcache.hashbucket[nh]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      
      //从原来的bucket的链表中断开
      b->next->prev = b->prev;
      b->prev->next = b->next;
      release(&bcache.lock[nh]);
      //插入到blockno对应的bucket中去
      b->next = bcache.hashbucket[h].next;
      b->prev = &bcache.hashbucket[h];
      bcache.hashbucket[h].next->prev = b;
      bcache.hashbucket[h].next = b;
      release(&bcache.lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[nh]);  
  nh = (nh+1)%NBUCKETS;
  }
  panic("bget: no buffers");  
}

// 返回一个包含指定块内容的锁定buf。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// 将b的内容写入磁盘。必须锁定。
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// 释放锁定的缓冲区。
// 移到MRU列表的前面。

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
// --------------------------------------------------------wsl改 
  int h = bhash(b->blockno);  
  acquire(&bcache.lock[h]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // 修改了数据结构，所以也要修改原head
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[h].next;
    b->prev = &bcache.hashbucket[h];
    bcache.hashbucket[h].next->prev = b;
    bcache.hashbucket[h].next = b;
  }
  
  release(&bcache.lock[h]);
}

// ---------------------------------------------------------wsl改
void
bpin(struct buf *b) {
  int h = bhash(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt++;
  release(&bcache.lock[h]);
}

// ---------------------------------------------------------wsl改
void
bunpin(struct buf *b) {
  int h = bhash(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt--;
  release(&bcache.lock[h]);
}


