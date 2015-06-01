#ifndef TCB_H
#define TCB_H
#include "VirtualMachine.h"
#include <vector>
#include <queue>
#include <deque>

using namespace std;

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
          int                 fileResult;
     };

     TCB::TCB()
     {
          //threadID =  0;
          //threadPriority = 0;
          threadStackState = 0;
          threadWaitTicks = 0;
     }

     class MCB
     {
       public:
          TVMMutexIDRef mutexID;
          TVMThreadIDRef threadIDRef;
          deque<TCB*> lowMutexQ;
          deque<TCB*> normalMutexQ;
          deque<TCB*> highMutexQ;
     };

} // end extern c

#endif
