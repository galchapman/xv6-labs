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

#define BCACHE_TABLE_SIZE 13

struct bcache_hash_entry {
  struct spinlock lock;
  struct buf buf[10];
};

struct {
  struct bcache_hash_entry ht[BCACHE_TABLE_SIZE];
} bcache;

void
binit(void)
{
  for (struct bcache_hash_entry *entry = bcache.ht; entry < bcache.ht + BCACHE_TABLE_SIZE; ++entry) {
    initlock(&entry->lock, "bcache.ht.lock");
    for (struct buf *b = entry->buf; b < entry->buf+10; ++b) {
      b->refcnt = 0;
      initsleeplock(&b->lock, "buffer");
    }
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
  struct bcache_hash_entry *entry;

  // loop over buffers starting with hash
  entry = &bcache.ht[bcache_hash(dev, blockno)];
  acquire(&entry->lock);

  for (struct buf *b = entry->buf; b < entry->buf+NBUF; ++b) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&entry->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (struct buf *b = entry->buf; b < entry->buf+NBUF; ++b) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&entry->lock);
      acquiresleep(&b->lock);
      return b;
    }
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

  struct bcache_hash_entry * entry = &bcache.ht[bcache_hash(b->dev, b->blockno)];

  acquire(&entry->lock);
  --b->refcnt;
  release(&entry->lock);
}

void
bpin(struct buf *b) {
  struct bcache_hash_entry * entry = &bcache.ht[bcache_hash(b->dev, b->blockno)];
  acquire(&entry->lock);
  ++b->refcnt;
  release(&entry->lock);
}

void
bunpin(struct buf *b) {
  struct bcache_hash_entry * entry = &bcache.ht[bcache_hash(b->dev, b->blockno)];
  acquire(&entry->lock);
  --b->refcnt;
  release(&entry->lock);
}