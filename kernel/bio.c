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
#define NBUCKET 13

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock[NBUCKET];
  struct buf bucket[NBUCKET];
  struct buf buf[NBUF];

  struct spinlock steal;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  struct buf *b;
  int idx = 0,round;
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }
  //pre allocate bufs into the bucket
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    round = (idx++) % NBUCKET;
    b->next = bcache.bucket[round].next;
    b->prev = &bcache.bucket[round];
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[round].next->prev = b;
    bcache.bucket[round].next = b;
  }
  initlock(&bcache.steal, "bcache");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hash = (blockno) % NBUCKET;
  int buf_ref0_exist;//if not cached ,does this bucket have a buf whose refcount is zero

  acquire(&bcache.lock[hash]);

  for (b = bcache.bucket[hash].next; b != &bcache.bucket[hash]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0) {
      buf_ref0_exist = 1;
    }
  }
  //no need to steal from other bucket
  if (buf_ref0_exist == 1) {
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
  //check one more time,before you get the steallock,other process may modify this bucket,brelease a buf in this bucket,
  //so there is a buf whose ref is zero 

  for (b = bcache.bucket[hash].next; b != &bcache.bucket[hash]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[hash]);
      release(&bcache.steal);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0) {
      buf_ref0_exist = 1;
    }
  }
  //no need to steal from other bucket
  if (buf_ref0_exist == 1) {
    for (b = bcache.bucket[hash].prev; b != &bcache.bucket[hash]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[hash]);
        release(&bcache.steal);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }

  //steal from other bucket
  if (buf_ref0_exist == 0) {
    for (int i = 1; i < NBUCKET; i++) {
      int newhash = (hash + i) % NBUCKET;
      acquire(&bcache.lock[newhash]);
      for (b = bcache.bucket[newhash].prev; b != &bcache.bucket[newhash]; b = b->prev) {
        if (b->refcnt == 0) {
          //break buf
          b->prev->next = b->next;
          b->next->prev = b->prev;
          release(&bcache.lock[newhash]);
          //merge into bucket
          b->prev = &bcache.bucket[hash];
          b->next = bcache.bucket[hash].next;
          bcache.bucket[hash].next->prev = b;
          bcache.bucket[hash].next = b;

          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          release(&bcache.lock[hash]);
          release(&bcache.steal);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.lock[newhash]);
    }
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
  int hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);

  b->refcnt--;
  if (b->refcnt == 0) {
    // break
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //merge
    b->next = bcache.bucket[hash].next;
    b->prev = &bcache.bucket[hash];
    bcache.bucket[hash].next->prev = b;
    bcache.bucket[hash].next = b;
  }
  release(&bcache.lock[hash]);
}

void
bpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);
  b->refcnt++;
  release(&bcache.lock[hash]);
}

void
bunpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;
  acquire(&bcache.lock[hash]);
  b->refcnt--;
  release(&bcache.lock[hash]);
}


