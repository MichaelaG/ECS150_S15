#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H
#include "VirtualMachine.h"

extern "C"
{

  struct memBlock {
    uint8_t *basePtr;
    int lenght;
  }memBlock;

     class MemoryPool
     {
       public:
          MemoryPool();

          TVMMemorySize memPoolSize;
          TVMMemoryPoolID memPoolID;
          list<memBlock*> freeBlocks;
          list<memBlock*> abailableBlocks;
          uint8_t *base;
          int free;
     };

     MemoryPool::MemoryPool()
     {}

} // end extern c

#endif
