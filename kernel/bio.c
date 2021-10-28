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
// we set the number of buckets 13
// to avoid collision
#define NBUCKETS 13
struct {
  // each bucket has a spin lock
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // now we don't need head, we use hashbuckets
  struct buf hashbuckets[NBUCKETS];
} bcache;


// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     // add buffer after head
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     // init the lock, their names are buffer
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

// rewrite the binit function
void
binit(void)
{

  int i;
  for(i=0; i<NBUCKETS; i++){
      initlock(&bcache.lock[i], "bcache.bucket");
  }
  

  // Create linked list of buffers
  for(i=0; i<NBUCKETS; i++){
    bcache.hashbuckets[i].prev = &bcache.hashbuckets[i];
    bcache.hashbuckets[i].next = &bcache.hashbuckets[i];
  }
  
  for(i=0; i<NBUF; i++){
    // add buffer after corresponding head
    int num = i % NBUCKETS;
    bcache.buf[i].next = bcache.hashbuckets[num].next;
    bcache.buf[i].prev = &bcache.hashbuckets[num];
    // init the lock, their names are buffer
    initsleeplock(&(bcache.buf[i].lock), "buffer");
    bcache.hashbuckets[num].next->prev = &bcache.buf[i];
    bcache.hashbuckets[num].next = &bcache.buf[i];
  }

}

// // Look through buffer cache for block on device dev.
// // If not found, allocate a buffer.
// // In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;
//   // acquire the cache lock
//   acquire(&bcache.lock);

//   // Is the block already cached?
//   // as we can see, head does not restore data
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     // if dev and blockno are equal to the param
//     // then we can say we the block is cached
//     // and we can use the block
//     if(b->dev == dev && b->blockno == blockno){
//       // add the buffer's refcnt to 1
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       // set the block is invalid
//       // no data
//       b->valid = 0;
//       // we later will use it, so set the refcnt to 1
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }

// rewrite bget()
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // fetch the number of bucket
  int num = blockno % NBUCKETS;

  // acquire the cache lock
  acquire(&bcache.lock[num]);

  // Is the block already cached?
  // as we can see, head does not restore data
  for(b = bcache.hashbuckets[num].next; b != &bcache.hashbuckets[num]; b = b->next){
    // if dev and blockno are equal to the param
    // then we can say we the block is cached
    // and we can use the block
    if(b->dev == dev && b->blockno == blockno){
      // add the buffer's refcnt to 1
      b->refcnt++;
      // update the timestamp
      b->tick = ticks;
      release(&bcache.lock[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbuckets[num].prev; b != &bcache.hashbuckets[num]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      // set the block is invalid
      // no data
      b->valid = 0;
      // we later will use it, so set the refcnt to 1
      b->refcnt = 1;
      // update the timestamp
      b->tick = ticks;
      release(&bcache.lock[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // if there is no refcnt == 0 block, get from other bucket
  int i,round;
  struct buf *lrubuf = 0;
  for(i=(num+1)%NBUCKETS, round=0; round < NBUCKETS; i++, round++){
    acquire(&bcache.lock[i]);
    // find the earlist refcnt==0 buf
    for(b=bcache.hashbuckets[i].next; b!=&bcache.hashbuckets[i]; b=b->next){
      if(b->refcnt == 0){
        // initial
        if(!lrubuf){
          lrubuf = b;
          continue;
        }
        // compare the timestamp;
        if(b->tick < lrubuf->tick){
          lrubuf = b;
        }
      }
    }
    // if found that buf, add this buf to
    // the bucket and quit this loop
    if(lrubuf){
      // add this buf to bucket
      lrubuf->next->prev = lrubuf->prev;
      lrubuf->prev->next = lrubuf->next;
      lrubuf->next = bcache.hashbuckets[num].next;
      lrubuf->prev = &bcache.hashbuckets[num];
      bcache.hashbuckets[num].next->prev = lrubuf;
      bcache.hashbuckets[num].next = lrubuf;

      // change infomation
      lrubuf->dev = dev;
      lrubuf->blockno = blockno;
      lrubuf->valid = 0;
      lrubuf->refcnt = 1;
      lrubuf->tick = ticks;
      
      release(&bcache.lock[i]);
      acquiresleep(&lrubuf->lock);
      return lrubuf;
    }
    release(&bcache.lock[i]);
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
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // move the block after to the head
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbuckets[num].next;
    b->prev = &bcache.hashbuckets[num];
    bcache.hashbuckets[num].next->prev = b;
    bcache.hashbuckets[num].next = b;
  }
  
  release(&bcache.lock[num]);
}

void
bpin(struct buf *b) {
  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt++;
  release(&bcache.lock[num]);
}

void
bunpin(struct buf *b) {
  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt--;
  release(&bcache.lock[num]);
}


