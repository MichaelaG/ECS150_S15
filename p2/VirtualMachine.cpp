#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <string>
#include <iostream>
#include <vector>

using namespace std;

TVMTick threadTick;
TCB currentThread;
vector<TCB>::iterator itr;
vector<TCB> highPriority;
vector<TCB> mediumPriority;
vector<TCB> lowPriority;
vector<TCB> bufferPriority;
vector<TCB> ready;
vector<TCB> waiting;
vector<TCB> running;
vector<TCB> dead;
vector<TCB> sleeping;
vector<TCB> allThreads;

//=========================INCLUDE FROM OTHER FILES=========================//

TVMMainEntry VMLoadModule(const char *module);

extern "C"
{

//===============================ALARMCALLBACK===============================//

void AlarmCallback(void *param) {

  threadTick = threadTick - 1;
  //printf("%d\n", (int)threadTick);

}

//================================VMFILECLOSE================================//

TVMStatus VMFileClose(int filedescriptor) {

  return VM_STATUS_SUCCESS;

}

//================================VMFILEOPEN=================================//

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {

  return VM_STATUS_SUCCESS;

}

//================================VMFILEREAD=================================//

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {

  return VM_STATUS_SUCCESS;

}

//================================VMFILESEEK=================================//

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {

  return VM_STATUS_SUCCESS;

}

//==================================VMSTART==================================//

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {

    typedef void(*TVMMain)(int argc, char *argv[]);

    TVMMain VMMain;                   // variable of function main
    VMMain = VMLoadModule(argv[0]);   // finds function pointer and returns it, NULL if nothing
    if (VMMain != NULL) {

      VMMain(argc, argv);               // call the function the function pointer is pointing to
      MachineInitialize(machinetickms); // initialize the machine abstraction layer
      MachineRequestAlarm(machinetickms, AlarmCallback, NULL); // request a machine alarm
      return VM_STATUS_SUCCESS;         // function call was a function that is defined

    }
    else return VM_STATUS_FAILURE;      // could not find the function the pointer "points" to

}

//============================VMTHREADACTIVATE===============================//


TVMStatus VMThreadActivate(TVMThreadID thread){

  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);

  int flag = 0;
  TCB contextSwitchThread;

  //--------------Scheduling--------------// @495 stuff
  // IF STATE IS STILL READY, ADD TO THAT QUEUE
  if (allThreads[thread].threadStackState == VM_THREAD_STATE_READY) {

    if (allThreads[thread].threadPriority == 3) {

      itr = highPriority.begin();
      highPriority.insert(itr, allThreads[thread]);

    }
    else if (allThreads[thread].threadPriority == 2) {

      itr = mediumPriority.begin();
      mediumPriority.insert(itr, allThreads[thread]);

    }

    else {

      itr = lowPriority.begin();
      lowPriority.insert(itr, allThreads[thread]);
    }

  }

  // FIND HIGHEST PRIORITY READY THREAD
  for (int i = 0; i < (int)highPriority.size() && flag == 0; i++) {
    if (highPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      contextSwitchThread = highPriority[i];
    }
  }

  for (int i = 0; i < (int)mediumPriority.size() && flag == 0; i++) {
    if (mediumPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      contextSwitchThread = mediumPriority[i];
    }
  }

  for (int i = 0; i < (int)lowPriority.size() && flag == 0; i++) {
    if (lowPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      contextSwitchThread = lowPriority[i];
    }
  }

  // SET THE TCB OF NEW THREAD TO RUNNING
  contextSwitchThread.threadStackState = VM_THREAD_STATE_RUNNING;

  // SWITCH CONTEXTS
  MachineContextSwitch(currentThread.threadContext, contextSwitchThread.threadContext);
  currentThread = contextSwitchThread;

}

//=============================VMTHREADCREATE================================//

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize,
  TVMThreadPriority prio, TVMThreadIDRef tid) {

      //cout << prio << endl;
      //int length = 0;

      if (tid == NULL || entry == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;

      TMachineSignalState OldState;
      MachineSuspendSignals(&OldState);

      SMachineContext *machineCtx = new SMachineContext();

      // SET STATE OF THE THREAD ENTERING
      TCB newThread;
      newThread.threadContext = machineCtx;
      newThread.threadStackState = VM_THREAD_STATE_READY;
      newThread.threadEntryParam = param;
      newThread.threadID = tid;
      newThread.threadPriority = prio;
      newThread.threadStackSize = memsize;
      newThread.threadEntryFnct = entry;

      itr = allThreads.begin();
      allThreads.insert(itr, newThread);

      MachineResumeSignals(&OldState);

      return VM_STATUS_SUCCESS;

}

//==============================VMTHREADSLEEP================================//

TVMStatus VMThreadSleep(TVMTick tick) {

    threadTick = tick;
    //printf("%d\n", (int)threadTick);

    while (threadTick > 0) {            // check the tick time to see if sleep is over

      //printf("good\n");
      AlarmCallback(NULL);              // get another alarm tick since not awake yet

    }

    if (threadTick == 0) return VM_STATUS_SUCCESS;
    else return VM_STATUS_ERROR_INVALID_PARAMETER;

    // schedule now that the thread is awake

}

//==============================VMTHREADSKELETON=============================//

void VMThreadSkeleton(TVMThreadID thread) {

  allThreads[thread].threadEntryFnct(allThreads[thread].threadEntryParam);
  VMThreadTerminate(thread);

}


//===============================VMTHREADSTATE===============================//


TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref){

  *stateref = allThreads[thread].threadStackState;

}

//=============================VMTHREADTERMINATE=============================//


TVMStatus VMThreadTerminate(TVMThreadID thread){

  allThreads[thread].threadStackState = VM_THREAD_STATE_DEAD;

  return VM_STATUS_SUCCESS;

}

//================================VMFILEWRITE================================//

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

    int len = *length;                  // length stored in an int to use in write system call
    write(filedescriptor, data, len);
    return VM_STATUS_SUCCESS;

}
}
