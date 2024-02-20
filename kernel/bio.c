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

#define BCACHE_TABLE_SIZE NBUF

struct {
//   struct spinlock lock;
  struct buf buf[BCACHE_TABLE_SIZE];
} bcache;

void
binit(void)
{
//   initlock(&bcache.lock, "bcache");
  for (struct buf *b = bcache.buf; b < bcache.buf+BCACHE_TABLE_SIZE; b++) {
    b->refcnt = 0;
    initlock(&b->ht_lock, "bcache.ht_lock");
    initsleeplock(&b->lock, "buffer");
  }
}

uint
bcache_hash(uint dev, uint blockno) {
    return (blockno) % BCACHE_TABLE_SIZE;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
//   acquire(&bcache.lock);
  struct buf *b;

  // loop over buffers starting with hash
  b = bcache.buf + bcache_hash(dev, blockno);
  for (uint i = 0; i < BCACHE_TABLE_SIZE; ++i) {
    if (b == bcache.buf+BCACHE_TABLE_SIZE) {
        b = bcache.buf;
    }

    acquire(&b->ht_lock);
    if (b->blockno == blockno && b->dev == dev) {
        // Found entry
        ++b->refcnt;
        release(&b->ht_lock);
        // release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
    }

    release(&b->ht_lock);
    ++b;
  }

//   acquire(&bcache.lock);

  // Search for empty spot to insert buffer
  b = bcache.buf + bcache_hash(dev, blockno);
  for (uint i = 0; i < BCACHE_TABLE_SIZE; ++i) {
    if (b == bcache.buf+BCACHE_TABLE_SIZE) {
        b = bcache.buf;
    }
    acquire(&b->ht_lock);

    if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->refcnt = 1;
        b->valid = 0;
        release(&b->ht_lock);
        // release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
    }

    release(&b->ht_lock);
    ++b;
  }

//   release(&bcache.lock);

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


//   acquire(&bcache.lock);
  acquire(&b->ht_lock);
  --b->refcnt;
  release(&b->ht_lock);
//   release(&bcache.lock);
}

void
bpin(struct buf *b) {
//   acquire(&bcache.lock);
  acquire(&b->ht_lock);
  ++b->refcnt;
  release(&b->ht_lock);
//   release(&bcache.lock);
}

void
bunpin(struct buf *b) {
//   acquire(&bcache.lock);
  acquire(&b->ht_lock);
  --b->refcnt;
  release(&b->ht_lock);
//   release(&bcache.lock);
}