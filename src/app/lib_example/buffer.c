#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "buffer/buffer.h"

// Initializes the Buffer buf.
static void initBuffer(char *database, Buffer *buf, int nPageBlocks,
      int nCacheBlocks, EvictPolicy Policy);

// Initializes the layer.
static void initLayer(Layer **layer, int maxBlocks, EvictPolicy evictPolicy);

// Frees all heap allocated space within the layer.
static void freeLayer(Layer **layer);

// Implements random eviction policies on a Buffer, returning the index of the
// Buffer block to place the next page or NO_EVICT_CANDIDATES if no blocks can
// be evicted.
static int32_t randomEvict(Layer *layer);

// Implements FIFO eviction policies on a Buffer, returning the index of the
// Buffer block to place the next page or NO_EVICT_CANDIDATES if no blocks can
// be evicted.
static int32_t fifoEvict(Layer *layer);

// Implements LRU eviction policies on a Buffer, returning the index of the
// Buffer block to place the next page or NO_EVICT_CANDIDATES if no blocks can
// be evicted.
static int32_t lruEvict(Layer *layer); 

// Attempts to locate a disk page within the Buffer. Returns the index of the
// disk page if found, otherwise FAILURE;
static int32_t findBufferedPage(Layer *layer, DiskAddress diskPage);

// Copies the data (and meta-data) of a block entry in srcLayer at srcIndex into
// the specified destIndex of destLayer. It is a way of moving an entry in one
// layer's buffer into another's.
static void copyPage(Layer *srcLayer, int srcIndex, Layer *destLayer,
      int destIndex);

// Function to test if a flag is set in the flags. Returns FALSE if the flag is
// not set.
static int8_t testFlag(uint8_t flagSet, uint8_t testFlag);

// Sets the flag within the buffer's flags array at the specified index.
static void setFlag(Layer *layer, int32_t index, uint8_t flag);

// Clears the flag within the buffer's flags array at the specified index.
static void clearFlag(Layer *layer, int32_t index, uint8_t flag);

// Returns TRUE if the page at the specified index is pinned.
static int pageIsPinnedIndex(Layer *layer, int32_t index);

// Returns TRUE if the page at the specified index is dirty.
static int pageIsDirtyIndex(Layer *layer, int32_t index);

// Returns TRUE if the page at the specified index is volatile.
static int pageIsVolatileIndex(Layer *layer, int32_t index);

// Clears the flags within the Buffer at the specified index.
static int clearFlagsIndex(Layer *layer, int32_t index);

// Sets the timestamp entry of buf at index to the current system time. Returns
// SUCCESS on success.
static int32_t updateTimestamp(Layer *layer, int32_t index);

int commence(char *database, Buffer **buf, int nPageBlocks, int nCacheBlocks,
      EvictPolicy evictPolicy) {

   ASSERT(database != NULL);
   ASSERT(buf != NULL);
   ASSERT(nPageBlocks > 0);
   ASSERT(nCacheBlocks > 0);

   int result = tfs_mount(database);

   // If the specified file does not represent a tinyFS disk image, make a
   // new disk image. 
   if (result != SUCCESS) {
      result = tfs_mkfs(database, nPageBlocks * BLOCKSIZE);
   }

   // Allocate space for the Buffer
   *buf = (Buffer *)calloc(1, sizeof(Buffer));
   ASSERT(*buf != NULL);

   // Initialize buf
   initBuffer(database, *buf, nPageBlocks, nCacheBlocks, evictPolicy);

   return result;
}

int squash(Buffer *buf) {
   ASSERT(buf != NULL);

   int i;
   int result;
   DiskAddress addr;
   Layer *pages = buf->pageLayer;

   // Flush all dirty pages to disk.
   for (i = 0; i < pages->maxBlocks; ++i) {
      addr = pages->blocks[i].addr;

      if (pageIsDirtyIndex(pages, i)) {
         // Unpin the page if its pinned.
         if (pageIsPinnedIndex(pages, i)) {
            unPinPage(buf, addr);
         }

         // Flush the page if its dirty.
         flushPage(buf, addr);
      }
   }

   // Free all layers.
   freeLayer(&buf->pageLayer);
   freeLayer(&buf->cacheLayer);

   // Unmount the tinyFS disk.
   result = tfs_unmount();

   // Free the allocated space for the database name.
   free(buf->database);

   // Free the Buffer
   free(buf);

   return result;
}

int readPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   int result;
   uint8_t temp[BLOCKSIZE] = {0};
   Layer *pages = buf->pageLayer;

   // Check the buffer to see if the page already exists.
   int32_t index = findBufferedPage(pages, diskPage);

   // If file exists in the buffer then update its timestamp and return SUCCESS.
   if (index >= 0) {
      updateTimestamp(pages, index);
      return SUCCESS;
   }

   // Otherwise, invoke eviction policy to determine which page to evict.
   index = pages->evictPolicy(pages);

   // If all pages are pinned, return error.
   if (index < 0) {
      return index;
   }

   // Attempt to read the page into temporary memory to see if the page
   // actually exists or not.
   result = tfs_readPage(diskPage.fd, diskPage.pageId, temp);

   // If a page is not found on disk, return error.
   if (result != SUCCESS) {
      return FAILURE;
   }

   // Flush the evictee to disk if its dirty.
   if (pageIsDirtyIndex(pages, index)) {
      result = flushPage(buf, pages->blocks[index].addr);
      if (result != SUCCESS) {
         return FAILURE;
      }
   }

   // Copy the contents of the disk page to the new buffer slot.
   memcpy(pages->blocks[index].data, temp, BLOCKSIZE);

   // Update bookkeeping within the buffer.
   pages->blocks[index].addr.fd = diskPage.fd;
   pages->blocks[index].addr.pageId = diskPage.pageId;

   // Increment numOccupied if this page is replacing a vacant slot.
   if (pages->numOccupied < pages->maxBlocks) {
      ++pages->numOccupied;
   }

   // Update the timestamp of the new page.
   updateTimestamp(pages, index);

   // Set flags accordingly.
   clearFlagsIndex(pages, index);

   return SUCCESS;
}

int writePage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;

   // Check the buffer to see if the page already exists.
   int32_t index = findBufferedPage(pages, diskPage);

   // If it does, update its timestamp, mark it as dirty and return SUCCESS.
   if (index != FAILURE) {
      setDirtyFlag(buf, diskPage);
      updateTimestamp(pages, index);
      return SUCCESS;
   }

   return FAILURE;
}

int flushPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   int result;
   Layer *pages = buf->pageLayer;

   // Check the buffer to see if the page already exists.
   int32_t index = findBufferedPage(pages, diskPage);

   // If the page isn't found
   if (index == FAILURE) {
      return FAILURE;
   }
   // If the page is dirty, write it to disk
   else if (pageIsDirtyIndex(pages, index)) {
      result = tfs_writePage(diskPage.fd, diskPage.pageId,
            pages->blocks[index].data);   

      // If the tinyFS call failed, return its error code.
      if (result != SUCCESS) {
         return result;
      }

      // Clear the page's dirty flag.
      clearDirtyFlag(buf, diskPage);
   }

   return SUCCESS;
}

int allocateCachePage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   int result;
   int cache_index;
   int pages_index;
   Layer *cache = buf->cacheLayer;
   Layer *pages = buf->pageLayer;

   // Attempt to find a slot in the volatile cache buffer.
   cache_index = cache->evictPolicy(cache);

   // If the cache buffer is full (so the returned index is marked as volatile
   // meaning that it contains data we need to hold onto).
   if (pageIsVolatileIndex(cache, cache_index)) {
      // Attempt to find a slot in the persistent page buffer that is unpinned
      // and non-volatile (this is a policy we are trying to enforce where we
      // try to only evict non-volatile unpinned pages).
      pages_index = pages->evictPolicy(pages);

      // If the persistent pages buffer cannot make space, return error.
      if (pages_index < 0) {
         return NO_EVICT_CANDIDATES;
      }

      // Otherwise, flush the page we found to disk in the persistent buffer.
      // Note that this could be us evicting a volatile page to disk as well
      // since they overflow into the persistent buffer.
      result = flushPage(buf, pages->blocks[pages_index].addr);

      // If the flush failed, don't write over the buffered data and return
      // FAILURE (this preserves the state of the system).
      if (result != SUCCESS) {
         return FAILURE;
      }

      // Copy the volatile page into the persistent buffer.
      copyPage(pages, pages_index, cache, cache_index);
   }

   // Update cache buffer data regarding the new page.
   cache->blocks[cache_index].addr.fd = diskPage.fd;
   cache->blocks[cache_index].addr.pageId = diskPage.pageId;

   // Clear data for this cache page in the buffer
   memset(cache->blocks[cache_index].data, '0', BLOCKSIZE);

   // Set the timestamp for this new cache page.
   result = updateTimestamp(cache, cache_index);
   ASSERT(result == SUCCESS);

   // Clear the flags for this cache page in the buffer.
   clearFlagsIndex(cache, cache_index);

   // Mark this entry as volatile
   setFlag(cache, cache_index, VOLATILE_FLAG);

   // Mark this entry as dirty
   setFlag(cache, cache_index, DIRTY_FLAG);

   // Increment the number of occupied buffer pages if this buffer slot
   // was originally unused.
   if (cache->numOccupied < cache->maxBlocks) {
      ++cache->numOccupied;
   }

   return SUCCESS;
}

int removeCachePage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   int32_t index;
   int result = FAILURE;
   uint8_t temp[BLOCKSIZE] = {0};
   Layer *cache = buf->cacheLayer;
   Layer *pages = buf->pageLayer;

   // Try and find the target page in the volatile buffer
   index = findBufferedPage(cache, diskPage);
   // If found, clear it out
   if (index != FAILURE) {
      clearFlagsIndex(cache, index);
      cache->blocks[index].addr.fd = VACANT_BLOCK; // So we know its vacant now
      result = SUCCESS;
   }

   // Try and find the target page in the persistent buffer
   index = findBufferedPage(pages, diskPage);
   // If found, clear it out
   if (index != FAILURE) {
      clearFlagsIndex(pages, index);
      pages->blocks[index].addr.fd = VACANT_BLOCK; // So we know its vacant now
      result = SUCCESS;
   }

   // Try and find the target page on disk
   if (SUCCESS == tfs_readPage(diskPage.fd, diskPage.pageId, temp)) {
      result = tfs_deleteFile(diskPage.fd);
   }

   return result;
}

int pinPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      setFlag(pages, index, PIN_FLAG);
      return SUCCESS;
   }

   return FAILURE;
}

int unPinPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      clearFlag(pages, index, PIN_FLAG);
      return SUCCESS;
   }

   return FAILURE;
}

int pageIsPinned(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      return testFlag(pages->flags[index], PIN_FLAG);
   }

   return FAILURE;
}

int setDirtyFlag(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      setFlag(pages, index, DIRTY_FLAG);
      return SUCCESS;
   }

   return FAILURE;
}

int clearDirtyFlag(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      clearFlag(pages, index, DIRTY_FLAG);
      return SUCCESS;
   }

   return FAILURE;
}

int pageIsDirty(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      return testFlag(pages->flags[index], DIRTY_FLAG);
   }

   return FAILURE;
}

int clearFlags(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   Layer *pages = buf->pageLayer;
   int32_t index = findBufferedPage(pages, diskPage);

   if (index != FAILURE) {
      return pages->flags[index] & 0;
   }

   return FAILURE;
}

int newPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   int index;
   int result;
   Layer *pages = buf->pageLayer;
   uint8_t temp[MAX_BUFFER_SIZE] = {0};

   // If the fd does not exist, return failure.
   if (tfs_numPages(diskPage.fd) < 0) {
      return FAILURE;
   }

   // Get buffer id to put the page by calling eviction policy
   index = pages->evictPolicy(pages);

   // If a buffer slot could not be obtained return error from policy call.
   if (index < 0) {
      return index;
   }

   // If the page is dirty, evict it before overwriting it with the new page.
   // Note that we assume that all pages (including volatile pages) will be
   // marked as dirty when created and when modified. This means that we cannot
   // have an event where a volatile page is not dirty & is not present on disk.
   if (pageIsDirtyIndex(pages, index)) {
      result = flushPage(buf, pages->blocks[index].addr);

      // If the page couldn't be evicted to disk, return FAILURE.
      if (result != SUCCESS) {
         return FAILURE;
      }
   }

   // Create the new page on disk
   result = tfs_writePage(diskPage.fd, diskPage.pageId, temp);

   // If the page couldn't be made on disk, return FAILURE.
   if (result != SUCCESS) {
      return FAILURE;
   }

   // Update buffer data regarding the new page.
   pages->blocks[index].addr.fd = diskPage.fd;
   pages->blocks[index].addr.pageId = diskPage.pageId;

   // Clear data for this page in the buffer
   memset(pages->blocks[index].data, '0', BLOCKSIZE);

   // Set the timestamp for this new page.
   result = updateTimestamp(pages, index);
   ASSERT(result == SUCCESS);

   // Clear the flags for this page in the buffer
   clearFlagsIndex(pages, index);

   // Increment the number of occupied buffer pages if this buffer slot
   // was originally unused.
   if (pages->numOccupied < pages->maxBlocks) {
      ++pages->numOccupied;
   }

   return SUCCESS;
}

// Prints out the state of the buffer including tinyFS blockID, timestamps,
// value of pin and dity flags
void checkpoint(Buffer * buf) {
   ASSERT(buf != NULL);

   int i;
   Layer *pages = buf->pageLayer;

   printf("\n");
   printf("==== Checkpoint ====\n");
   printf("Disk file: %s\n", buf->database);
   printf("Max buffer slots: %i\n", pages->maxBlocks);
   printf("Occupied pages: %i\n", pages->numOccupied);
   printf("Blocks:\n");

   // print each block
   for (i = 0; i < pages->maxBlocks; i++) {
      printf("  Block Number %d\n", i);
      printf("            fd %d\n", pages->blocks[i].addr.fd);
      printf("        pageId %d\n", pages->blocks[i].addr.pageId);
      printf("     timestamp %ld\n", pages->timestamps[i]);
      printf("         dirty %d\n", testFlag(pages->flags[i], DIRTY_FLAG));
      printf("        pinned %d\n", testFlag(pages->flags[i], PIN_FLAG));
      printf("      volatile %d\n", testFlag(pages->flags[i], VOLATILE_FLAG));
      printf("\n");
   }
}

// If the index exists, prints the contents of the page at the buffer slot
void pageDump(Buffer *buf, int index) {
   ASSERT(buf != NULL);
   ASSERT(index >= 0 && index < buf->pageLayer->maxBlocks);

   int i;
   Layer *pages = buf->pageLayer;

   // If block is vacant
   if (pages->blocks[index].addr.fd == VACANT_BLOCK) {
      printf("Block %i is vacant.\n", index);
   }
   else { // Else print data
      printf("Block %i fd %i pageId %d\n", index, pages->blocks[index].addr.fd,
            pages->blocks[index].addr.pageId);
      for (i = 0; i < BLOCKSIZE; i++){
         printf("%u", pages->blocks[index].data[i]);
      }
      printf("\n");
   }
}

// Finds the page with the given blockId (diskPage) in the buffer and
// prints its contents. If a page with the blockId does not exist on
// disk, reports an error. If the page page exists on disk, but not 
// in the buffer, also reports an error
void printPage(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   uint8_t data[BLOCKSIZE];
   int found = 0;
   Block block;
   Layer *pages = buf->pageLayer;

   // if it can't find the block on disk
   if (tfs_readPage(diskPage.fd, diskPage.pageId, data) == ERROR) {
      printf("Cannot find block on disk\n");
   }
   else { // It exists on the disk
      // loops through filled every block in buffer
      int i;
      for (i = 0; i < pages->numOccupied; i++) {
         block = pages->blocks[i];
         // This is the page we're looking for
         if (block.addr.fd == diskPage.fd && 
               block.addr.pageId == diskPage.pageId) {
            printf("Fd %i pageId %d\n", diskPage.fd, diskPage.pageId);
            found = 1;
            pageDump(buf, i);
            break;
         }
      }
      // The page was never found
      if (!found) {
         printf("Page not in buffer\n");
      }
   }
}

// Prints the contents of a block directly from the disk
void printBlock(Buffer *buf, DiskAddress diskPage) {
   ASSERT(buf != NULL);

   uint8_t data[BLOCKSIZE];

   // If block is found
   if (tfs_readPage(diskPage.fd, diskPage.pageId, data) == SUCCESS) {
      printf("Fd %i pageId %d\n", diskPage.fd, diskPage.pageId);
      int i;
      for (i = 0; i < BLOCKSIZE; i++) {
         printf("%u", data[i]);
      }
      printf("\n");
   }
   else { // couldn't find the block on the disk
      printf("Cannot find block on disk\n");
   }
}

static void initBuffer(char *database, Buffer *buf, int nPageBlocks,
      int nCacheBlocks, EvictPolicy evictPolicy) {
   ASSERT(database != NULL);
   ASSERT(buf != NULL);
   ASSERT(nPageBlocks > 0);
   ASSERT(nCacheBlocks >= 0);

   // Copy name of database over.
   size_t len = strlen(database) + 1; // Account for \0.
   buf->database = (char *)calloc(len, sizeof(char));
   ASSERT(buf->database != NULL);
   strcpy(buf->database, database);

   initLayer(&buf->pageLayer, nPageBlocks, evictPolicy);
   initLayer(&buf->cacheLayer, nCacheBlocks, evictPolicy);
}

static void initLayer(Layer **layer, int maxBlocks, EvictPolicy evictPolicy) {
   ASSERT(layer != NULL);
   ASSERT(maxBlocks <= MAX_BUFFER_SIZE);

   // Allocate space for the Layer itself.
   *layer = (Layer *)calloc(1, sizeof(Layer));
   ASSERT(*layer != NULL);

   // Allocate space for the block array within this layer.
   (*layer)->blocks = (Block *)calloc(maxBlocks, sizeof(Block));
   ASSERT((*layer)->blocks != NULL);

   // Allocate space for the timestamps array within this layer.
   (*layer)->timestamps = (long *)calloc(maxBlocks, sizeof(long));
   ASSERT((*layer)->timestamps != NULL);

   // Allocate space for the flags array within this layer.
   (*layer)->flags = (uint8_t *)calloc(maxBlocks, sizeof(uint8_t));
   ASSERT((*layer)->flags != NULL);

   int i;
   // Mark all blocks vacant.
   for (i = 0; i < maxBlocks; ++i) {
      memset((*layer)->blocks[i].data, '\0', BLOCKSIZE);
      (*layer)->blocks[i].addr.fd = VACANT_BLOCK;
      (*layer)->flags[i] = 0;
   }

   (*layer)->maxBlocks = maxBlocks;
   (*layer)->numOccupied = 0;

   // Bind the eviction policy function to this Layer.
   switch (evictPolicy) {
      case RANDOM:
         (*layer)->evictPolicy = randomEvict;
         break;

      case FIFO:
         (*layer)->evictPolicy = fifoEvict;
         break;

      case LRU:
         (*layer)->evictPolicy = lruEvict;
         break;

         // Should never get here.
      default:
         ASSERT(FALSE);
   }
}

static void freeLayer(Layer **layer) {
   free((*layer)->blocks);
   free((*layer)->timestamps);
   free((*layer)->flags);
   free((*layer));
   *layer = NULL;
}

// TODO: IMPLEMENT
static int32_t randomEvict(Layer *layer) {
   ASSERT(layer != NULL);

   printf("randomEvict not implemented yet!\n");
   ASSERT(FALSE);
   return SUCCESS;
}

// TODO: IMPLEMENT
static int32_t fifoEvict(Layer *layer) {
   ASSERT(layer != NULL);

   printf("fifoEvict not implemented yet!\n");
   ASSERT(FALSE);
   return SUCCESS;
}

static int32_t lruEvict(Layer *layer) {
   ASSERT(layer != NULL);

   int32_t i;
   int result;
   int32_t oldestIndex = FAILURE;
   int32_t oldestVolatileIndex = FAILURE;
   long oldestTimestamp = FAILURE;
   long oldestVolatileTimestamp = FAILURE;
   struct timespec spec;

   // First see if any pages are available.
   if (layer->numOccupied < layer->maxBlocks) {
      // See if there are any vacant pages.
      for (i = 0; i < layer->maxBlocks; ++i) {
         if (layer->blocks[i].addr.fd == VACANT_BLOCK) {
            return i;   
         }
      }
   }

   // Initialize the timestamp to the current time.
   result = clock_gettime(CLOCK_REALTIME, &spec);
   ASSERT(result == SUCCESS);
   oldestTimestamp = spec.tv_nsec;

   // Otherwise, find the oldest unpinned page
   for (i = 0; i < layer->maxBlocks; ++i) {
      // If the page isn't pinned (and thus could be evicted if needed)
      if (pageIsPinnedIndex(layer, i) == FALSE) {

         // If the page is old and is volatile
         if (layer->timestamps[i] < oldestVolatileTimestamp &&
               pageIsVolatileIndex(layer, i)) {
            oldestVolatileTimestamp = layer->timestamps[i];
            oldestVolatileIndex = i;
         }
         // If the page is old and non-volatile
         else if (layer->timestamps[i] < oldestTimestamp &&
               pageIsVolatileIndex(layer, i) == FALSE) {
            oldestTimestamp = layer->timestamps[i];
            oldestIndex = i;
         }
      }
   }

   // If a unpinned persistent page was found to be evicted, return its index.
   if (oldestIndex >= 0) {
      return oldestIndex;
   }
   // Otherwise, if a volatile page was found to be evicted, return its index.
   else if (oldestVolatileIndex >= 0) {
      return oldestVolatileIndex;
   }
   // Otherwise, no pages can be evicted so return appropriate error.
   else {
      return NO_EVICT_CANDIDATES;
   }
}

// TODO: This is just doing linear search along the buffer. I know this is
// inefficient but I didn't want to futz with the hashtable stuff tonight
// so I just threw this together. However, since its a function it can be
// easily replaced without impacting the logic of the code that's using it.
static int32_t findBufferedPage(Layer *layer, DiskAddress diskPage) {
   ASSERT(layer != NULL);

   int i;
   DiskAddress addr;

   for (i = 0; i < layer->maxBlocks; ++i) {
      addr = layer->blocks[i].addr;
      if (addr.fd == diskPage.fd && addr.pageId == diskPage.pageId) {
         return i; 
      }
   }

   return FAILURE;
}

static void copyPage(Layer *srcLayer, int srcIndex, Layer *destLayer,
      int destIndex) {
   ASSERT(srcLayer != NULL);
   ASSERT(destLayer != NULL);

   // Copy over DiskAddress
   destLayer->blocks[destIndex].addr.fd = srcLayer->blocks[srcIndex].addr.fd;
   destLayer->blocks[destIndex].addr.pageId =
      srcLayer->blocks[srcIndex].addr.pageId;

   // Copy over data.
   memcpy(destLayer->blocks[destIndex].data, srcLayer->blocks[srcIndex].data,
         BLOCKSIZE);

   // Copy over flags.
   memcpy(&destLayer->flags[destIndex], &srcLayer->flags[srcIndex],
      sizeof(uint8_t));

   // Copy over timestamp.
   memcpy(&destLayer->timestamps[destIndex], &srcLayer->timestamps[srcIndex],
      sizeof(long));
}

static int32_t updateTimestamp(Layer *layer, int32_t index) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   int result;
   struct timespec spec;
   result = clock_gettime(CLOCK_REALTIME, &spec);
   if (result == SUCCESS) {
      layer->timestamps[index] = spec.tv_nsec;
   }
   return result;
}

static void setFlag(Layer *layer, int32_t index, uint8_t flag) {
   ASSERT(layer!= NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   layer->flags[index] |= flag;
}

static void clearFlag(Layer *layer, int32_t index, uint8_t flag) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   layer->flags[index] &= ~flag;
}

static int8_t testFlag(uint8_t flagSet, uint8_t testFlag) {
   return flagSet & testFlag;
}

static int clearFlagsIndex(Layer *layer, int32_t index) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   return layer->flags[index] & 0;
}

static int pageIsPinnedIndex(Layer *layer, int32_t index) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   return testFlag(layer->flags[index], PIN_FLAG);
}

static int pageIsDirtyIndex(Layer *layer, int32_t index) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   return testFlag(layer->flags[index], DIRTY_FLAG);
}

static int pageIsVolatileIndex(Layer *layer, int32_t index) {
   ASSERT(layer != NULL);
   ASSERT(index >= 0 && index < layer->maxBlocks);

   return testFlag(layer->flags[index], VOLATILE_FLAG);
}
