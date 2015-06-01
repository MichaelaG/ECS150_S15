#ifndef MemPool_H
#define MemPool_H
#include <vector>
#include "VirtualMachine.h"

using namespace std;

extern "C" {

  typedef struct MemBlock
  {
    uint8_t *stackPtr;
    TVMMemorySize size;
  } MemBlock;

  class MemPool
  {
  public:
    MemPool();

    TVMMemorySize memStackSize;
    TVMMemorySize memFreeSpace;
    TVMMemoryPoolID memID;
    uint8_t* memStackPtr;

    vector<MemBlock*> memFree;
    vector<MemBlock*> memUsed;
  };

  MemPool::MemPool()
  {
    memStackSize = 0;
    memID = 0;
    memStackPtr = 0;
  }

} // end extern C

#endif
