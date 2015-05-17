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

//TVMTick threadTick;
TCB* currentThread=NULL;

vector<TCB*> highPriority;
vector<TCB*> normalPriority;
vector<TCB*> lowPriority;

vector<TCB*> ready;
//vector<TCB*> waiting;
//vector<TCB*> running;
//vector<TCB*> dead;
//vector<TCB*> sleeping;
vector<TCB*> allThreads;

SMachineContextRef old_context;
SMachineContextRef new_context;

TVMMainEntry VMLoadModule(const char *module);

#define VM_THREAD_PRIORITY_LOWLOW		((TVMThreadPriority)0x00)

//---------------------------------------------------------------------------//
//----------------------------   THREAD STUFF   -----------------------------//
//---------------------------------------------------------------------------//
extern "C" void AlarmCallback(void *param)
{

}


extern "C" void VMIdle(void*) //done
{
	while(1);
	{/*do nothing;*/}

} // end VMIdle ---------------------------------------------------//


extern "C" void VMThreadSkeleton(void *param)
{
	MachineEnableSignals();
  TVMThreadEntry entryPoint = ( ((TCB*)param)->threadEntryFnct );
  entryPoint( ((TCB*)param)->threadEntryParam );
  VMThreadTerminate( ((TCB*)param)->threadID);

} // end  VMThreadSkeleton ----------------------------------------//


extern "C" TVMStatus VMStart(int tickms, int machinetickms, int argc,
	char *argv[]) // DONE EXCEPT SLEEPCALLBACK
{

	MachineInitialize(machinetickms);
  MachineRequestAlarm(tickms*1000, AlarmCallback, NULL);
  //MachineRequestAlarm(tickms*1000, sleepCallBack, NULL);
  TCB* tIdle = new TCB;
  tIdle->threadID = 0;
  tIdle->threadStackState = VM_THREAD_STATE_READY;
  tIdle->threadEntryFnct = VMIdle;
  tIdle->threadEntryParam = NULL;
  tIdle->threadPriority = VM_THREAD_PRIORITY_LOWLOW;
  tIdle->threadStackSize = 100000;
  tIdle->threadBaseStackPtr = new uint8_t[100000];
	allThreads.push_back(tIdle);

	TCB* mainThread = new TCB;
	mainThread->threadStackState = VM_THREAD_STATE_RUNNING;
  mainThread->threadPriority = VM_THREAD_PRIORITY_NORMAL;
  mainThread->threadID = allThreads.size();
  allThreads.push_back(mainThread);
  currentThread = mainThread;

  MachineContextCreate(&(tIdle->threadContext),
  	VMThreadSkeleton, (void*)tIdle, tIdle->threadBaseStackPtr,
  	tIdle->threadStackState);

  TVMMainEntry VMMain = VMLoadModule(argv[0]);

  if (VMMain != NULL)
  {
    VMMain(argc, argv);
    MachineTerminate();
    return VM_STATUS_SUCCESS;
  }
  else return VM_STATUS_FAILURE;

} // end  VMFileClose ---------------------------------------------//

extern "C" void VMPrioPush(TCB *thread)
{
	// pushing to proper priorities vectors
	if(thread->threadPriority == VM_THREAD_PRIORITY_LOW)
		lowPriority.push_back(thread);
	if(thread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
		normalPriority.push_back(thread);
	if(thread->threadPriority == VM_THREAD_PRIORITY_HIGH)
		highPriority.push_back(thread);
}


extern "C" void VMSchedule()
{
	TCB* nextThread = NULL;
	// SCHEDULING
	if(!highPriority.empty())
	{
		if( (highPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}
			nextThread = highPriority.front();
			//highPriority.erase( nextThread );
			nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(currentThread->threadContext);
			new_context = &(nextThread->threadContext);
			currentThread = nextThread;
			MachineContextSwitch(old_context, new_context);
		}
		//check if high queue has same prio as current or if high queue
		//		has higher prio than current
	}
	if(!normalPriority.empty())
	{
		//check if normal queue has same prio as current or if normal queue
		//		has higher prio than current
	}
	if(!lowPriority.empty())
	{
		//check if low queue has same prio as current or if low queue
		//		has higher prio than current
	}

} // end  AlarmCallBack -------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadActivate(TVMThreadID thread)
{

	TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);
  // do some error checking for thread in size, non-null, and not dead
  MachineContextCreate( &(allThreads[thread]->threadContext),
    VMThreadSkeleton,
    (void*) allThreads[thread],
    allThreads[thread]->threadBaseStackPtr,
    allThreads[thread]->threadStackSize );

  allThreads[thread]->threadStackState = VM_THREAD_STATE_READY;
  VMPrioPush(allThreads[thread]);
  VMSchedule();
	return VM_STATUS_SUCCESS;
} // end  VMFileClose ---------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param,
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	if (tid == NULL || entry == NULL)
    return VM_STATUS_ERROR_INVALID_PARAMETER;
  TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);
  TCB* newThread = new TCB;
  *tid =  allThreads.size();
  newThread->threadID = allThreads.size();
  newThread->threadStackState = VM_THREAD_STATE_DEAD;
  newThread->threadEntryParam = param;
  newThread->threadPriority = prio;
  newThread->threadStackSize = memsize;
  newThread->threadEntryFnct = entry;
  newThread->threadBaseStackPtr = new uint8_t[memsize];
  allThreads.push_back(newThread);
  MachineResumeSignals(&OldState);
  return VM_STATUS_SUCCESS;
} // end  VMFileClose ---------------------------------------------//


extern "C" TVMStatus VMThreadSleep(TVMTick tick)
{

	return VM_STATUS_SUCCESS;

} // end  VMThreadSleep -------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef
	stateref)
{
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
} // end  VMThreadState -------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	// error checking first - check whehter threadid is valid
	//		and not already dead

  allThreads[thread]->threadStackState = VM_THREAD_STATE_DEAD;

  MachineResumeSignals(&OldState);
  VMSchedule();

  return VM_STATUS_SUCCESS;

} // end  VMThreadState -------------------------------------------//



//---------------------------------------------------------------------------//
//-----------------------------   FILE STUFF   ------------------------------//
//---------------------------------------------------------------------------//

	extern "C" TVMStatus VMFileClose(int filedescriptor)
	{
	  if (close(filedescriptor))
	  	return VM_STATUS_SUCCESS;
	  else return VM_STATUS_FAILURE;

	} // end FileClose -----------------------------------------------//


	extern "C" TVMStatus VMFileOpen(const char *filename, int flags, int mode,
		int *filedescriptor)
	{
	  if (filename == NULL || filedescriptor == NULL)
	  	return VM_STATUS_ERROR_INVALID_PARAMETER;
	  *filedescriptor = open(filename, flags, mode);

	  return VM_STATUS_SUCCESS;

	} // end FileOpen ------------------------------------------------//


	extern "C" TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
	{
	  return VM_STATUS_SUCCESS;
	} // end FileRead ------------------------------------------------//


	extern "C" TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, 
		int *newoffset)
	{
	  //fseek(*filedescriptor, offset, whence);
	  return VM_STATUS_SUCCESS;

	} // end FileSeek ------------------------------------------------//


	extern "C" TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
	{
	    int len = *length;                  
	    write(filedescriptor, data, len);
	    return VM_STATUS_SUCCESS;

	} // end FileWrite -----------------------------------------------//

//---------------------------------------------------------------------------//
//--------------------------------   END   ----------------------------------//
//---------------------------------------------------------------------------//