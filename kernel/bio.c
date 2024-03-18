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

#define BCACHE_TABLE_SIZE 19
#define BCACHE_BUFFERS 100

struct bcache_hash_entry {
    struct spinlock lock;
    struct buf *head;
    struct buf *tail;
};

struct {
  struct spinlock lock;
  struct buf *free_list;
  struct buf buf[BCACHE_BUFFERS];
  struct bcache_hash_entry ht[BCACHE_TABLE_SIZE];
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");
  struct buf* last = 0;
  for (struct buf *b = bcache.buf; b < bcache.buf+BCACHE_BUFFERS; b++) {
    b->refcnt = 0;
    b->prev = 0; // bufs in free list dont need prev
    if (last != 0) {
        last->next = b;
    }
    last = b;
    initsleeplock(&b->lock, "buffer");
  }
  last->next = 0;
  bcache.free_list = &bcache.buf[0];

  for (struct bcache_hash_entry *he = bcache.ht; he < bcache.ht + BCACHE_TABLE_SIZE; ++he) {
    initlock(&he->lock, "bcache.ht.lock");
    he->head = he->tail = 0;
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
  struct buf **b;

  // loop over buffers starting with hash
  entry = &bcache.ht[bcache_hash(dev, blockno)];
  acquire(&entry->lock);

  for (b = &(entry->head); *b != 0; b = &((*b)->next)) {
    if ((*b)->next != 0 && (*b)->next->prev != *b) {
        panic("Invalid link list\n");
    }
    if((*b)->dev == dev && (*b)->blockno == blockno){
      (*b)->refcnt++;
      release(&entry->lock);
      acquiresleep(&(*b)->lock);
      return *b;
    }
  }

  // alocate new buffer (b points to last next pointer)
  acquire(&bcache.lock);
  *b = bcache.free_list;
  bcache.free_list = bcache.free_list->next;
  if (*b == 0) {
    panic("bget: no buffers");
  }

  release(&bcache.lock);

  (*b)->next = 0;
  (*b)->prev = entry->tail;
  entry->tail = *b;

  (*b)->dev = dev;
  (*b)->blockno = blockno;
  (*b)->valid = 0;
  (*b)->refcnt = 1;

  release(&entry->lock);

  acquiresleep(&(*b)->lock);

  return *b;
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

  if (b->refcnt == 0) {
    // update hash entry list
    if (b->prev)
        b->prev->next = b->next;
    else // same as if (entry->head == b)
        entry->head = b->next;

    if (b->next)
        b->next->prev = b->prev;
    else
        entry->tail = b->prev;

    // update free list
    acquire(&bcache.lock);

    b->next = bcache.free_list;
    b->prev = 0;
    bcache.free_list = b;

    release(&bcache.lock);
  }

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