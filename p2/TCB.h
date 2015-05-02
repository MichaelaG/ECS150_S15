#ifndef TCB_H
#define TCB_H

#include "VirtualMachine.h"

class TCB {

  public:
          TVMThreadIDRef threadID;
          TVMThreadPriority threadPriority;
          TVMThreadState threadStackState;
          TVMMemorySize threadStackSize;
          uint8_t * threadBaseStackPtr;
          TVMThreadEntry threadEntryFnct;
          void * threadEntryParam;
          SMachineContext *threadContext;
          TVMTick threadWaitTicks;
};

#endif
