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

#define NBUCKET 13

struct {
  struct spinlock steal;//avoid dead lock
  struct buf bucket[NBUCKET];
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
} bcache;


void
binit(void)
{
  struct buf *b;
  initlock(&bcache.steal, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
    b = &bcache.bucket[i];
    b->prev = b;
    b->next = b;
  }
  int idx = 0,round;
  // pre allocate buf to bucket
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    round = (idx++) % NBUCKET;
    b->next = bcache.bucket[round].next;
    b->prev = &bcache.bucket[round];
    bcache.bucket[round].next->prev = b;
    bcache.bucket[round].next = b;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = blockno % NBUCKET;
  int exist_ref0 = 0;//whether this bucket contains a buf whose refcount is zero

  acquire(&bcache.lock[hash]);
  for (b = bcache.bucket[hash].next; b != &bcache.bucket[hash]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0) {
      exist_ref0 = 1;
    }
  }
  // no need to steal
  if (exist_ref0 == 1) {
    for (b = bcache.bucket[hash].prev; b != &bcache.bucket[hash]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[hash]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  } 
  release(&bcache.lock[hash]);
  acquire(&bcache.steal);
  acquire(&bcache.lock[hash]);
  //check again,while this process is waiting for the steal lock,other process modify the buf in this bucket
  for (b = bcache.bucket[hash].next; b != &bcache.bucket[hash]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[hash]);
      release(&bcache.steal);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0) {
      exist_ref0 = 1;
    }
  }
  // no need to steal
  if (exist_ref0 == 1) {
    for (b = bcache.bucket[hash].prev; b != &bcache.bucket[hash]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[hash]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
  // steal from other bucket
  uint newhash;
  for (int i = 1; i < NBUCKET;i++) {
    newhash = (hash + i) % NBUCKET;
    acquire(&bcache.lock[newhash]);
    // Not cached; recycle an unused buffer.
    for (b = bcache.bucket[newhash].prev; b != &bcache.bucket[newhash]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        //break
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.lock[newhash]);
        //insert
        b->next = bcache.bucket[hash].next;
        b->prev = &bcache.bucket[hash];
        b->next->prev = b;
        b->prev->next = b;

        release(&bcache.lock[hash]);
        release(&bcache.steal);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[newhash]);
  }
  release(&bcache.lock[hash]);
  release(&bcache.steal);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
  bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // move to head
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[hash].next;
    b->prev = &bcache.bucket[hash];
    bcache.bucket[hash].next->prev = b;
    bcache.bucket[hash].next = b;
  }

  release(&bcache.lock[hash]);
}

void
bpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);
  b->refcnt++;
  release(&bcache.lock[hash]);
}

void
bunpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);
  b->refcnt--;
  release(&bcache.lock[hash]);
}
