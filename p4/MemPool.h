#ifndef MemPool_H
#define MemPool_H
#include <vector>
#include "VirtualMachine.h"

using namespace std;

extern "C"
{
  typedef struct MemBlock
  {
    uint8_t *stackPtr; //*base
    TVMMemorySize size; //length
  } MemBlock;

  class MemPool
  {
  public:
    MemPool();

    TVMMemorySize memStackSize; //memory_pool_size
    TVMMemorySize memFreeSpace;
    TVMMemoryPoolID memID;  //memory_pool_id
    uint8_t* memStackPtr;  //*base

    vector<MemBlock*> memFree;  //free_list
    vector<MemBlock*> memUsed;  //alloc_list
  };

  MemPool::MemPool()
  {
    memStackSize = 0;
    memID = 0;
    memStackPtr = 0;
  }


  //BPB Struct Defn



  //Root Entry Struct Defn


} // end extern C

#endif
