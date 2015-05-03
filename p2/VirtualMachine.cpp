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
  //SHOULD CHECK IF
  // TCB.waitingticks is >0, if it is then decrement
  // if not, then make the state ready
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

//==================================VMIDLE==================================//

void VMIdle(void*){
  while(1);
  {
    //do nothing;
  }
  
}

//==================================VMSTART==================================//

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {

    typedef void(*TVMMain)(int argc, char *argv[]);

    TVMThreadIDRef tidIdle = new TVMThreadID;
    TVMMain VMMain;                   // variable of function main
    VMMain = VMLoadModule(argv[0]);   // finds function pointer and returns it, NULL if nothing
    if (VMMain != NULL) {
      VMThreadCreate(&VMIdle, NULL, 0x100, 1, tidIdle);
      //cout << "\tACTIVATING IDLE THREAD\n";
      VMThreadActivate(*tidIdle);
      //cout << "TID IDLE = " << *tidIdle << endl;

      VMMain(argc, argv);               // call the function the function pointer is pointing to
      MachineInitialize(machinetickms); // initialize the machine abstraction layer
      MachineRequestAlarm(machinetickms, AlarmCallback, NULL); // request a machine alarm
      //MachineContextCreate(allThreads[*tidIdle].threadContext, allThreads[*tidIdle].threadEntryFnct,
      //  allThreads[*tidIdle].threadEntryParam, allThreads[*tidIdle].threadBaseStackPtr, allThreads[*tidIdle].threadStackSize);
      //currentThread = allThreads[*tidIdle];
      //NEED TO CHANGE PRIORITY OF IDLE TO 0 LATER

      return VM_STATUS_SUCCESS;         // function call was a function that is defined

    }
    else return VM_STATUS_FAILURE;      // could not find the function the pointer "points" to

}

//============================VMTHREADACTIVATE===============================//


TVMStatus VMThreadActivate(TVMThreadID thread){

  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);

  int flag = 0;
  TCB nextThreadToSchedule;

  allThreads[thread].threadStackState = VM_THREAD_STATE_READY;

  //cout << "\tSTARTING SCHEDULING\n";

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

    else if (allThreads[thread].threadPriority == 1){

      itr = lowPriority.begin();
      lowPriority.insert(itr, allThreads[thread]);
    }
  }

  //cout << "\tADDED THREAD TO PRIORITY QUEUE\n";

  // FIND HIGHEST PRIORITY READY THREAD
  for (int i = 0; i < (int)highPriority.size() && flag == 0; i++) {
    if (highPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = highPriority[i];
    }
  }

  for (int i = 0; i < (int)mediumPriority.size() && flag == 0; i++) {
    if (mediumPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = mediumPriority[i];
    }
  }

  for (int i = 0; i < (int)lowPriority.size() && flag == 0; i++) {
    if (lowPriority[i].threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = lowPriority[i];
    }
  }

  //cout << "\tFOUND NEXT THREAD TO SCHEDULE\n";

  // SET THE TCB OF NEW THREAD TO RUNNING
  nextThreadToSchedule.threadStackState = VM_THREAD_STATE_RUNNING;

  //cout << "\tSWITCHED TO RUNNING STATE\n";
  if(currentThread.threadContext == NULL) {
    //cout << "\tCONTEXT DOESNT EXIST\n";
    MachineContextCreate(nextThreadToSchedule.threadContext, nextThreadToSchedule.threadEntryFnct,
      nextThreadToSchedule.threadEntryParam, nextThreadToSchedule.threadBaseStackPtr, nextThreadToSchedule.threadStackSize);
    currentThread = nextThreadToSchedule;
    currentThread.threadStackState = VM_THREAD_STATE_RUNNING;
  }
  else {
  // SWITCH CONTEXTS
    //cout << "\tCONTEXT SWITCH FUNCTION REACHED\n";
    MachineContextSwitch(currentThread.threadContext, nextThreadToSchedule.threadContext);
    currentThread.threadStackState = VM_THREAD_STATE_WAITING;
    currentThread = nextThreadToSchedule;
    currentThread.threadStackState = VM_THREAD_STATE_RUNNING;
  }
  return VM_STATUS_SUCCESS;


  //WHEN ACTIVATING A THREAD, TURN ON ALARM CALLBACK SO THREAD IS LIMITED TO
  //CERTAIN QUANTUM OF TIME
}

//=============================VMTHREADCREATE================================//

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize,
  TVMThreadPriority prio, TVMThreadIDRef tid) {

  if (tid == NULL || entry == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;

  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);

  SMachineContext *machineCtx = new SMachineContext();

  // SET STATE OF THE THREAD ENTERING
  TCB newThread;
  newThread.threadContext = machineCtx;
  newThread.threadStackState = VM_THREAD_STATE_DEAD;
  newThread.threadEntryParam = param;
  //TVMThreadID *threadRef = new TVMThreadID;
  //*threadRef = allThreads.size();
  newThread.threadID = allThreads.size();//threadRef;
  *tid =  allThreads.size();
  newThread.threadPriority = prio;
  newThread.threadStackSize = memsize;
  newThread.threadEntryFnct = entry;
  newThread.threadBaseStackPtr = new uint8_t[memsize];

  //if(newThread.threadStackState == VM_THREAD_STATE_DEAD)
  //  cout << newThread.threadID << " DEAD\n";

  itr = allThreads.begin();
  allThreads.insert(itr, newThread);

  //if(newThread.threadStackState == VM_THREAD_STATE_DEAD)
  //  cout << newThread.threadID <<" DEAD\n";

  MachineResumeSignals(&OldState);

  //if(newThread.threadStackState == VM_THREAD_STATE_DEAD)
  //  cout << newThread.threadID <<" DEAD\n";

  return VM_STATUS_SUCCESS;

}

//==============================VMTHREADSLEEP================================//

TVMStatus VMThreadSleep(TVMTick tick) {

    threadTick = tick;
    //printf("%d\n", (int)threadTick);

    while (threadTick > 0) {    // check the tick time to see if sleep is over
      //printf("good\n");
      AlarmCallback(NULL);         // get another alarm tick since not awake yet
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

  if(stateref == NULL)
    return VM_STATUS_ERROR_INVALID_PARAMETER;
  for(int i=0; i<(int)allThreads.size(); i++)
  {
    if(allThreads[i].threadID == thread)
    {
      *stateref = allThreads[i].threadStackState;
      return VM_STATUS_SUCCESS;
    }
  }
  return VM_STATUS_ERROR_INVALID_ID;

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
