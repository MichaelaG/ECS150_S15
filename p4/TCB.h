#ifndef TCB_H
#define TCB_H
#include "VirtualMachine.h"

extern "C"
{

     class TCB
     {
       public:
          TCB();
          TVMThreadID         threadID;
          TVMThreadPriority   threadPriority;
          TVMThreadState      threadStackState;
          TVMMemorySize       threadStackSize;
          void *              threadBaseStackPtr;
          TVMThreadEntry      threadEntryFnct;
          void *              threadEntryParam;
          SMachineContext     threadContext;
          TVMTick             threadWaitTicks;
     };

     TCB::TCB()
     {
          //threadID =  0;
          //threadPriority = 0;
          threadStackState = 0;
          threadWaitTicks = 0;
     }

} // end extern c

#endif
