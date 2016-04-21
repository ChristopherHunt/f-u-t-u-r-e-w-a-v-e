#include "buffer/buffer.c"

static void printBlockInfo(Buffer *buf) {
   ASSERT(buf != NULL);

   int i;
   Layer *pages = buf->pageLayer;

   printf("\n");
   for (i = 0; i < pages->maxBlocks; ++i) {
      printf("block[%d]: fd: %d pageId: %d\n", i, pages->blocks[i].addr.fd,
            pages->blocks[i].addr.pageId);
   }
   printf("\n");
}

static void createTestPage(Buffer *buf, DiskAddress diskPage, int maxPageCount) {
   ASSERT(buf != NULL);

   int index;
   int result;
   Layer *pages = buf->pageLayer;
   int oldNumOccupied = buf->pageLayer->numOccupied;

   // Create some pages to test against
   result = newPage(buf, diskPage);
   ASSERT(result == SUCCESS);

   // Test whether or not the page was created in the buffer
   if (oldNumOccupied == pages->maxBlocks) {
      ASSERT(pages->numOccupied == oldNumOccupied);
   }
   else {
      ASSERT(pages->numOccupied == oldNumOccupied + 1);
   }

   // Test findBufferedPage
   index = findBufferedPage(pages, diskPage);
   ASSERT(index >= 0 && index < maxPageCount);

   // Ensure the page's timestamp was updated.
   ASSERT(pages->timestamps[index] > 0);

   // Ensure the page's flags were updated.
   ASSERT(pages->flags[index] == 0);
}

int main(int argc, char **argv) {
   Buffer *buf;
   DiskAddress diskPage;
   int fd;
   int i;
   int index;
   int result;
   int maxPageCount = 3;
   Layer *pages;

   // Make a filesystem
   result = tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);
   ASSERT(result == SUCCESS);

   // Mount the filesystem
   result = tfs_mount(DEFAULT_DISK_NAME);
   ASSERT(result == SUCCESS);

   // Test commence (and indirectly initBuffer).
   result = commence(DEFAULT_DISK_NAME, &buf, maxPageCount, maxPageCount, LRU);
   ASSERT(result == SUCCESS);
   ASSERT(strcmp(buf->database, DEFAULT_DISK_NAME) == 0);
   for (i = 0; i < buf->pageLayer->maxBlocks; ++i) {
      ASSERT(buf->pageLayer->blocks[i].addr.fd == VACANT_BLOCK);
   }
   ASSERT(buf->pageLayer->maxBlocks == maxPageCount);
   ASSERT(buf->pageLayer->numOccupied == 0);

   pages = buf->pageLayer;

   // Test flag functionality
   setFlag(pages, 0, DIRTY_FLAG);
   ASSERT(pages->flags[0] == DIRTY_FLAG);
   ASSERT(testFlag(pages->flags[0], DIRTY_FLAG));

   clearFlag(pages, 0, DIRTY_FLAG);
   ASSERT(pages->flags[0] == 0);
   ASSERT(testFlag(pages->flags[0], DIRTY_FLAG) == 0);

   setFlag(pages, 0, PIN_FLAG);
   ASSERT(pages->flags[0] == PIN_FLAG);
   ASSERT(testFlag(pages->flags[0], PIN_FLAG));

   clearFlag(pages, 0, PIN_FLAG);
   ASSERT(pages->flags[0] == 0);
   ASSERT(testFlag(pages->flags[0], PIN_FLAG) == 0);

   setFlag(pages, 0, DIRTY_FLAG);
   ASSERT(pages->flags[0] == DIRTY_FLAG);

   setFlag(pages, 0, PIN_FLAG);
   ASSERT(pages->flags[0] == (DIRTY_FLAG | PIN_FLAG));
   ASSERT(testFlag(pages->flags[0], PIN_FLAG));

   clearFlag(pages, 0, DIRTY_FLAG);
   ASSERT(pages->flags[0] == PIN_FLAG);

   clearFlag(pages, 0, PIN_FLAG);
   ASSERT(pages->flags[0] == 0);

   // Open a file on the disk.
   fd = tfs_openFile("file1");
   ASSERT(fd >= 0);
   diskPage.fd = fd;
   printf("fd: %d\n", fd);

   printf("checkpoint0\n");
   checkpoint(buf);
   printBlockInfo(buf);
   diskPage.pageId = 1;
   // Create first page
   createTestPage(buf, diskPage, maxPageCount);

   printBlockInfo(buf);
   diskPage.pageId = 2;
   // Create second page
   createTestPage(buf, diskPage, maxPageCount);

   printf("checkpoint after 2\n");
   checkpoint(buf);
   printBlockInfo(buf);
   diskPage.pageId = 3;
   // Create third page
   createTestPage(buf, diskPage, maxPageCount);

   printBlockInfo(buf);
   diskPage.pageId = 4;
   // Create fourth page
   createTestPage(buf, diskPage, maxPageCount);

   // Try reading a page that is in the buffer
   diskPage.pageId = 2;
   printBlockInfo(buf);
   result = readPage(buf, diskPage);
   ASSERT(result == SUCCESS);
   printBlockInfo(buf);

   // Try reading a page that isn't in the buffer but is on disk
   diskPage.pageId = 1;
   printBlockInfo(buf);
   result = readPage(buf, diskPage);
   ASSERT(result == SUCCESS);
   printBlockInfo(buf);

   // Try reading a page that isn't in the buffer but isn't on disk
   diskPage.pageId = 5;
   printBlockInfo(buf);
   result = readPage(buf, diskPage);
   ASSERT(result == FAILURE);
   printBlockInfo(buf);

   // Write a page to test it marking the dirty flag
   diskPage.pageId = 1;
   result = writePage(buf, diskPage);
   ASSERT(result == SUCCESS);
   ASSERT(pageIsDirty(buf, diskPage));

   // Test if pin works
   // Find the index of the next to be evicted
   index = pages->evictPolicy(pages);
   diskPage.fd = pages->blocks[index].addr.fd;
   diskPage.pageId = pages->blocks[index].addr.pageId;

   // Pin that page
   pinPage(buf, diskPage);

   // Create a new page and make sure the pinned page isn't evicted
   printBlockInfo(buf);
   diskPage.pageId = 5;
   // Create fifth page
   createTestPage(buf, diskPage, maxPageCount);
   printBlockInfo(buf);

   printf("checkpoint after 5\n");
   checkpoint(buf);
   printf("pageDump,0\n");
   pageDump(buf,0);
   printf("pageDump,1\n");
   pageDump(buf,1);

   printf("printPage when page exists\n");
   printPage(buf, diskPage);

   printf("printBlock when block exists\n");
   printBlock(buf, diskPage);

   diskPage.pageId = -1;
   printf("printPage when page does not exist\n");
   printPage(buf, diskPage);

   printf("printBlock when block does not exist\n");
   printBlock(buf, diskPage);

   result = squash(buf);
   ASSERT(result == SUCCESS);
   return 0;
}
