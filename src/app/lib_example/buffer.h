#ifndef __BUFFER__H__
#define __BUFFER__H__

#include <stdint.h>
#include "tinyfs/tiny.h"
#include "utils/utils.h"

#define MAX_BUFFER_SIZE 5         // Max number of pages a Buffer can support.
#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAILURE -1
#define NO_EVICT_CANDIDATES -2
#define VACANT_BLOCK -1

#define DIRTY_FLAG    0x1          // Mark to set a page as dirty.
#define PIN_FLAG      0x2          // Mark to pin a page in the Buffer.
#define VOLATILE_FLAG 0x4          // Mark if a page is volatile.

// Enums representing the different eviction policies available for the Buffer.
typedef enum {RANDOM, FIFO, LRU} EvictPolicy;

typedef struct DiskAddress {       // Specifies lookup info for disk pages.
   int fd;                         // File descriptor associated with the block.
   int pageId;                     // Page ID of the block.
} DiskAddress;

typedef struct Block {             // Define a disk block.
   uint8_t data[BLOCKSIZE];        // Content stored in disk block.
   DiskAddress addr;               // Specifies the disk lookup info for a page.
} Block;

typedef struct Layer Layer;                                                    

struct Layer {                     // Abstraction of block layers in buffer.
   int maxBlocks;                  // Max number of blocks in this layer.         
   Block *blocks;                  // Actual array of buffered blocks.       
   uint8_t *flags;                 // Flags for the buffered blocks.         
   long *timestamps;               // Timestamps for the buffered blocks.    
   int32_t numOccupied;            // Number of occupied blocks in the array.     
   int32_t (*evictPolicy)(Layer *layer); // Pointer to eviction policy function.  
};                                                                         

typedef struct Buffer {                                                                  char *database;                 // Name of disk file used with this buffer.
   Layer *cacheLayer;              // Buffered volatile pages.              
   Layer *pageLayer;               // Buffered persistent pages.            
} Buffer;       

// Call to create and initialize a Buffer buf with a specified number of page
// slots and cache slots (cache being for volatile data in the buffer) from
// a tinyFS disk image named database. Also tells the Buffer to use the
// specified eviction policy as specified in the call. Returns the error code
// associated with the call to tfs_mkfs() or tfs_mount().
int commence(char *database, Buffer **buf, int nPageBlocks,
      int nCacheBlocks, EvictPolicy policy);

// Call to flush all dirty pages to disk and cleanup a Buffer buf. Returns
// the error code associated with the call to tfs_unmount().
int squash(Buffer *buf);

// Ensures that a specified page is read into the Buffer buf. Returns SUCCESS on
// success, FAILURE if the diskPage does not exist and ALL_PINNED_FAILURE if
// the Buffer is fully pinned (and so no eviction could occur).
int readPage(Buffer *buf, DiskAddress diskPage);

// Marks the diskPage as dirty within the Buffer buf. Returns SUCCESS if the
// page is found and updated.
int writePage(Buffer *buf, DiskAddress diskPage);

// Forces a diskPage in the Buffer to be written to disk. Returns SUCCESS if
// the page was found in the Buffer and successfully flushed to disk.
int flushPage(Buffer *buf, DiskAddress diskPage);

// Allocates a page in the cache layer of buf for the specified diskPage. If the
// cache layer is full then a cache page is evicted to the persistent layer of
// the buffer. If that layer is full of volatile pages then a page is evicted to
// disk to make room for it. Returns SUCCESS if a cache page is placed in the
// cache layer's buffer.
int allocateCachePage(Buffer *buf, DiskAddress diskPage);

// Searches for the specified diskPage within the cache layer's buffer, the
// persistent layer's buffer and the disk itself, removing whatever copy it
// finds. Returns SUCCESS if the diskPage is found and deleted.
int removeCachePage(Buffer *buf, DiskAddress diskPage);

// Marks a diskPage in the Buffer as being "pinned", so that it cannot be
// evicted until "unpinned" later. Returns SUCCESS on success.
int pinPage(Buffer *buf, DiskAddress diskPage);

// Removes the "pin flag" from the diskPage in the Buffer so that it can later
// be evicted if needed by the Buffer Manager's eviction policy. Returns SUCCESS
// on success.
int unPinPage(Buffer *buf, DiskAddress diskPage);

// Returns FALSE if the page at the specified index is not pinned.
int pageIsPinned(Buffer *buf, DiskAddress diskPage);

// Marks a diskPage in the Buffer as "dirty" so that the Buffer Manager knows
// to write the page to disk. Returns SUCCESS on success.
int setDirtyFlag(Buffer *buf, DiskAddress diskPage);

// Removes the "dirty flag" from a diskPage in the Buffer so that the Buffer
// Manager knows it can evict the page without first persisting it to disk.
// Returns SUCCESS on success.
int clearDirtyFlag(Buffer *buf, DiskAddress diskPage);

// Returns FALSE if the page at the specified index is not dirty.
int pageIsDirty(Buffer *buf, DiskAddress diskPage);

// Clears all flags associated with the Buffer entry at the specified index.
// Returns SUCCESS if the flags are able to be cleared.
int clearFlags(Buffer *buf, DiskAddress diskPage);

// Creates a new disk page on the tinyFS disk image.
int newPage(Buffer *buf, DiskAddress diskPage);

// Prints out the state of the buffer including tinyFS blockID, timestamps,
// value of pin and dity flags
void checkpoint(Buffer * buf);

// If the index exists, prints the contents of the page at the buffer slot
void pageDump(Buffer *buf, int index);

// Finds the page with the given blockId (diskPage) in the buffer and
// prints its contents. If a page with the blockId does not exist on
// disk, reports an error. If the page page exists on disk, but not 
// in the buffer, also reports an error
void printPage(Buffer *buf, DiskAddress diskPage);

// Prints the contents of a block directly from the disk
// NEVER USES BUFFER
void printBlock(Buffer *buf, DiskAddress diskPage);

#endif
