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

#define VM_THREAD_PRIORITY_LOWLOW ((TVMThreadPriority)0x00)

using namespace std;

//TVMTick threadTick;
TCB* currentThread = 0;
vector<TCB*>::iterator itr;
vector<TCB*> highPriority;
vector<TCB*> mediumPriority;
vector<TCB*> lowPriority;
vector<TCB*> bufferPriority;
vector<TCB*> ready;
vector<TCB*> waiting;
vector<TCB*> running;
vector<TCB*> dead;
vector<TCB*> sleeping;
vector<TCB*> allThreads;

//=========================INCLUDE FROM OTHER FILES=========================//

TVMMainEntry VMLoadModule(const char *module);

extern "C"
{

//===============================ALARMCALLBACK===============================//

void AlarmCallback(void *param) {

  //cout << waiting.size() << endl;
  //cout << "wait length4 " << waiting.size() << endl;

  for (int i = 0; i < (int)waiting.size(); i++) {
    waiting[i]->threadWaitTicks = waiting[i]->threadWaitTicks - 1;
    //cout << i << " " << waiting[i]->threadWaitTicks << endl;
    if (waiting[i]->threadWaitTicks == 0) {

        waiting[i]->threadStackState = VM_THREAD_STATE_READY;
        if (waiting[i]->threadPriority == 3) {

          itr = highPriority.begin();
          highPriority.insert(itr, waiting[i]);

        }
        else if (waiting[i]->threadPriority == 2) {

          itr = mediumPriority.begin();
          mediumPriority.insert(itr, waiting[i]);

        }
        else {

          itr = lowPriority.begin();
          lowPriority.insert(itr, waiting[i]);

        }
        itr = waiting.begin() + i;
        waiting.erase(itr);
        i = i - 1;
    }
  }

  if (highPriority.empty() != 1) {

    TCB* firstInVector = new TCB;
    firstInVector = highPriority.back();
    firstInVector->threadStackState = VM_THREAD_STATE_READY;
    highPriority.pop_back();
    MachineContextSwitch(currentThread->threadContext, firstInVector->threadContext);
    currentThread = firstInVector;

  }
  else if (mediumPriority.empty() != 1) {

    TCB* firstInVector = new TCB;
    firstInVector = mediumPriority.back();
    firstInVector->threadStackState = VM_THREAD_STATE_READY;
    mediumPriority.pop_back();
    MachineContextSwitch(currentThread->threadContext, firstInVector->threadContext);
    currentThread = firstInVector;

  }
  else if (lowPriority.empty() != 1) {

    TCB* firstInVector = new TCB;
    firstInVector = lowPriority.back();
    firstInVector->threadStackState = VM_THREAD_STATE_READY;
    lowPriority.pop_back();
    MachineContextSwitch(currentThread->threadContext, firstInVector->threadContext);
    currentThread = firstInVector;

  }

  //cout << "wait length4 " << waiting.size() << endl;

  //printf("%d\n", (int)threadTick);
  //SHOULD CHECK IF
  // TCB.waitingticks is >0, if it is then decrement
  // if not, then make the state ready
}

//================================VMFILECLOSE================================//

TVMStatus VMFileClose(int filedescriptor) {

  if (close(filedescriptor)) return VM_STATUS_SUCCESS;
  else return VM_STATUS_FAILURE;

}

//================================VMFILEOPEN=================================//

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {

  if (filename == NULL || filedescriptor == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;

  *filedescriptor = open(filename, flags, mode);

  return VM_STATUS_SUCCESS;

}

//================================VMFILEREAD=================================//

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {

  return VM_STATUS_SUCCESS;

}

//================================VMFILESEEK=================================//

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {

  //fseek(*filedescriptor, offset, whence);

  return VM_STATUS_SUCCESS;

}

//================================VMFILEWRITE================================//

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

    int len = *length;                  // length stored in an int to use in write system call
    write(filedescriptor, data, len);
    return VM_STATUS_SUCCESS;

}

//==================================VMIDLE==================================//

void VMIdle(void*){
  while(1);
  {
    //do nothing;
  }

}

//================================VMSCHEDULE=================================//

void VMThreadSkeleton(void *param);

void VMSchedule(TVMThreadID thread) {

  int flag = 0;
  TCB* nextThreadToSchedule = NULL;
  //currentThread = allThreads[allThreads.size() - thread - 1];

  //cout << "middle" << endl;
  cout << "\tSTATE = " << allThreads[allThreads.size() - thread - 1]->threadStackState << endl;

  // if it is running
  if(allThreads[allThreads.size() - thread - 1]->threadStackState == VM_THREAD_STATE_RUNNING)
    allThreads[allThreads.size() - thread - 1]->threadStackState = VM_THREAD_STATE_READY;

  if (allThreads[allThreads.size() - thread - 1]->threadStackState == VM_THREAD_STATE_READY) {

    if (allThreads[allThreads.size() - thread - 1]->threadPriority == 3)
    {
      itr = highPriority.begin();
      highPriority.insert(itr, allThreads[allThreads.size() - thread - 1]);
      cout << "\tHIGH" << endl;
    }
    else if (allThreads[allThreads.size() - thread - 1]->threadPriority == 2)
    {
      itr = mediumPriority.begin();
      mediumPriority.insert(itr, allThreads[allThreads.size() - thread - 1]);
      cout << "\tMEDIUM" << endl;

    }

    else if (allThreads[allThreads.size() - thread - 1]->threadPriority == 1){

      itr = lowPriority.begin();
      lowPriority.insert(itr, allThreads[allThreads.size() - thread - 1]);
      cout << "\tLOW" << endl;
    }
    else
    {
      cout << "idle" << endl;
      itr = bufferPriority.begin();
      bufferPriority.insert(itr, allThreads[allThreads.size() - thread - 1]);
      cout << "end idle" << endl;
    }
  }

  cout << "\tADDED THREAD TO PRIORITY QUEUE" << endl;

  // FIND HIGHEST PRIORITY READY THREAD
  for (int i = 0; i < (int)highPriority.size() && flag == 0; i++) {
    if (highPriority[i]->threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = highPriority[i];
    }
  }

  for (int i = 0; i < (int)mediumPriority.size() && flag == 0; i++) {
    if (mediumPriority[i]->threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = mediumPriority[i];
    }
  }

  for (int i = 0; i < (int)lowPriority.size() && flag == 0; i++) {
    if (lowPriority[i]->threadStackState == VM_THREAD_STATE_READY) {
      flag = 1;
      nextThreadToSchedule = lowPriority[i];
    }
  }
  cout << "\tFLAG = " << flag << endl;
  if (flag == 0)
    nextThreadToSchedule = bufferPriority[0];
  cout << "\t" << currentThread << endl;
  cout << "\tFOUND NEXT THREAD TO SCHEDULE " << nextThreadToSchedule << endl;

  // SET THE TCB OF NEW THREAD TO RUNNING
  //nextThreadToSchedule->threadStackState = VM_THREAD_STATE_RUNNING;

  cout << "\tSWITCHED TO RUNNING STATE" << endl;
  if(allThreads[allThreads.size() - thread - 1]->threadContext == NULL) {
    cout << "\tCONTEXT DOESNT EXIST"<<endl;
    MachineContextCreate(nextThreadToSchedule->threadContext, VMThreadSkeleton,
      nextThreadToSchedule->threadEntryParam, nextThreadToSchedule->threadBaseStackPtr, nextThreadToSchedule->threadStackSize);
    //MachineContextCreate(nextThreadToSchedule->threadContext, nextThreadToSchedule->threadEntryFnct,
      //nextThreadToSchedule->threadEntryParam, nextThreadToSchedule->threadBaseStackPtr, nextThreadToSchedule->threadStackSize);
    cout << "create" << endl;
    allThreads[allThreads.size() - thread - 1] = nextThreadToSchedule;
    allThreads[allThreads.size() - thread - 1]->threadStackState = VM_THREAD_STATE_RUNNING;
  }
  else {
  // SWITCH CONTEXTS
    cout << "\tCONTEXT SWITCH FUNCTION REACHED"<<endl ;
    TCB* prev = currentThread;
    cout << "\tDUMB"<<endl;
    currentThread = nextThreadToSchedule;
    cout << "\tDUMB"<<endl;
    cout << "\t CURRENT THREAD CONTEXT " << prev->threadContext << endl;
    cout << "\t NEXT THREAD CONTEXT    " << currentThread->threadContext << endl;
    MachineContextSwitch(prev->threadContext, currentThread->threadContext);
    cout << "switch" << endl;
    prev->threadStackState = VM_THREAD_STATE_WAITING;
    itr = waiting.begin();
    waiting.insert(itr, prev);
    //currentThread = nextThreadToSchedule;
    currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
  }

}

//==================================VMSTART==================================//

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {

    MachineInitialize(machinetickms); // initialize the machine abstraction layer
    MachineRequestAlarm(tickms*1000, AlarmCallback, NULL); // request a machine alarm

    typedef void(*TVMMain)(int argc, char *argv[]);

    TVMThreadID tidIdle;// = new TVMThreadID;
    TVMMain VMMain;                   // variable of function main
    VMMain = VMLoadModule(argv[0]);   // finds function pointer and returns it, NULL if nothing
    if (VMMain != NULL) {
      VMThreadCreate(&VMIdle, NULL, 0x100000, VM_THREAD_PRIORITY_LOWLOW, &tidIdle);
      //cout << "\tACTIVATING IDLE THREAD\n";
      VMThreadActivate(tidIdle);
      //cout << "TID IDLE = " << *tidIdle << endl;

      VMMain(argc, argv);               // call the function the function pointer is pointing to

      return VM_STATUS_SUCCESS;         // function call was a function that is defined
      //cout << "wait length3 " << waiting.size() << endl;
    }
    else return VM_STATUS_FAILURE;      // could not find the function the pointer "points" to

}

//============================VMTHREADACTIVATE===============================//

TVMStatus VMThreadActivate(TVMThreadID thread){

  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);

  cout << "\tSTARTING SCHEDULING" << endl;
  cout << "\tthread = " << thread << endl;

  MachineContextCreate( allThreads[thread]->threadContext,
    VMThreadSkeleton,
    (void*) allThreads[thread],
    allThreads[thread]->threadBaseStackPtr,
    allThreads[thread]->threadStackSize );

  cout << "\tGOING TO VMSCHEDULE" << endl;
  allThreads[allThreads.size() - thread - 1]->threadStackState = VM_THREAD_STATE_READY;
  VMSchedule(thread);

  return VM_STATUS_SUCCESS;

}

//=============================VMTHREADCREATE================================//

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize,
  TVMThreadPriority prio, TVMThreadIDRef tid) {
    //cout << "wait length1 " << waiting.size() << endl;
  if (tid == NULL || entry == NULL)
    return VM_STATUS_ERROR_INVALID_PARAMETER;

  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);

  // SET STATE OF THE THREAD ENTERING
  TCB* newThread = new TCB;
  //newThread->threadContext = machineCtx;
  newThread->threadStackState = VM_THREAD_STATE_DEAD;
  newThread->threadEntryParam = param;
  //TVMThreadID *threadRef = new TVMThreadID;
  //*threadRef = allThreads.size();
  newThread->threadID = allThreads.size();//threadRef;
  cout << "\tTHREAD ID = " << allThreads.size() << endl;
  *tid =  allThreads.size();
  newThread->threadPriority = prio;
  newThread->threadStackSize = memsize;
  newThread->threadEntryFnct = entry;
  newThread->threadBaseStackPtr = new uint8_t[memsize];
  
  allThreads.push_back(newThread);

  MachineResumeSignals(&OldState);

  return VM_STATUS_SUCCESS;

}

//==============================VMTHREADSLEEP================================//

TVMStatus VMThreadSleep(TVMTick tick){
  

    if (tick == VM_TIMEOUT_INFINITE) return VM_STATUS_ERROR_INVALID_PARAMETER;
    TMachineSignalState OldState;
    MachineSuspendSignals(&OldState);
    if (tick == VM_TIMEOUT_IMMEDIATE) {

      currentThread->threadStackState = VM_THREAD_STATE_WAITING;
      itr = waiting.begin();
      waiting.insert(itr, currentThread);
      //cout << "wait length " << waiting.size() << endl;
      if (currentThread->threadPriority == 3) {

        TCB* firstInVector = new TCB;
        firstInVector = highPriority[highPriority.size() - 1];
        firstInVector->threadStackState = VM_THREAD_STATE_READY;
        highPriority.pop_back();
        itr = highPriority.begin();
        highPriority.insert(itr, currentThread);
        currentThread = firstInVector;
      }
      else if (currentThread->threadPriority == 2) {

        TCB* firstInVector = new TCB;
        firstInVector = mediumPriority[mediumPriority.size() - 1];
        firstInVector->threadStackState = VM_THREAD_STATE_READY;
        mediumPriority.pop_back();
        itr = mediumPriority.begin();
        mediumPriority.insert(itr, currentThread);
        currentThread = firstInVector;
      }
      else {

        TCB* firstInVector = new TCB;
        firstInVector = lowPriority[lowPriority.size() - 1];
        firstInVector->threadStackState = VM_THREAD_STATE_READY;
        lowPriority.pop_back();
        itr = lowPriority.begin();
        lowPriority.insert(itr, currentThread);
        currentThread = firstInVector;
      }
    }
  else {
    currentThread->threadWaitTicks = tick;
    //printf("%d\n", (int)threadTick);
    currentThread->threadStackState = VM_THREAD_STATE_WAITING;
    itr = waiting.begin();
    waiting.insert(itr, currentThread);
    //cout << "wait length " << waiting.size() << endl;
    while (currentThread->threadWaitTicks > 0) {    // check the tick time to see if sleep is over
      //cout << "outside" << currentThread->threadWaitTicks << endl;
      AlarmCallback(NULL);         // get another alarm tick since not awake yet
      //cout << "outside" << currentThread->threadWaitTicks << endl;
    }

    return VM_STATUS_SUCCESS;
    //else return VM_STATUS_ERROR_INVALID_PARAMETER;
  }
  MachineResumeSignals(&OldState);
  return VM_STATUS_SUCCESS;
    // schedule now that the thread is awake

}

//==============================VMTHREADSKELETON=============================//

void VMThreadSkeleton(void *param) {

  MachineEnableSignals();
  TVMThreadEntry entryPoint = ( ((TCB*)param)->threadEntryFnct );
  entryPoint( ((TCB*)param)->threadEntryParam );
  VMThreadTerminate( ((TCB*)param)->threadID);

}

//===============================VMTHREADSTATE===============================//


TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref){

  if(stateref == NULL)
    return VM_STATUS_ERROR_INVALID_PARAMETER;
  for(int i=0; i<(int)allThreads.size(); i++)
  {
    if(allThreads[i]->threadID == thread)
    {
      *stateref = allThreads[i]->threadStackState;
      return VM_STATUS_SUCCESS;
    }
  }
  return VM_STATUS_ERROR_INVALID_ID;

}

//=============================VMTHREADTERMINATE=============================//


TVMStatus VMThreadTerminate(TVMThreadID thread){

  allThreads[allThreads.size() - thread - 1]->threadStackState = VM_THREAD_STATE_DEAD;

  return VM_STATUS_SUCCESS;

}
}
