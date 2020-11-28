// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 /*素数个哈希桶*/

// Linked list of all buffers, through prev/next.
// head.next is most recently used.
/* 每个哈希桶为双向链表加一把锁 */
// -----------------------------------------------------wsl改
struct {
  struct spinlock lock[NBUCKETS];  //自旋锁
  struct buf buf[NBUF];

  struct buf hashbucket[NBUCKETS]; //代替的head
} bcache;

/* 哈希映射 */
// -----------------------------------------------------wsl改
int bhash(int blockno){
  return blockno%NBUCKETS;
}

// -----------------------------------------------------wsl改
void
binit(void)  //双向循环链表
{
  struct buf *b; //b->lock 睡眠锁

  for (int i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache.bucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  
  // initlock(&bcache.lock[i], "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;

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

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 获取空闲缓存块
// --------------------------------------------------wsl改
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int h = bhash(blockno); //得到一个对应的hash值

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
  int nh = (h+1)%NBUCKETS; //// nh表示下一个要探索的bucket，当它重新变成h，说明所有的buffer都bussy（refcnt不为0），此时,如之前设计的，panic
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

// Return a locked buf with the contents of the indicated block.

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

// Write b's contents to disk.  Must be locked.
// 
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
// --------------------------------------------------------wsl改
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
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


