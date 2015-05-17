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
vector<TCB*> sleeping;
vector<TCB*> allThreads;

//SMachineContextRef old_context;
//SMachineContextRef new_context;

TVMMainEntry VMLoadModule(const char *module);

#define VM_THREAD_PRIORITY_LOWLOW		((TVMThreadPriority)0x00)

//---------------------------------------------------------------------------//
//----------------------------   THREAD STUFF   -----------------------------//
//---------------------------------------------------------------------------//

extern "C" void VMPrioPush(TCB *thread)
{
	//TMachineSignalState OldState;
  //MachineSuspendSignals(&OldState);
	// pushing to proper priorities vectors
	if(thread->threadPriority == VM_THREAD_PRIORITY_LOW)
		lowPriority.push_back(thread);
	if(thread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
		normalPriority.push_back(thread);
	if(thread->threadPriority == VM_THREAD_PRIORITY_HIGH)
		highPriority.push_back(thread);
	//MachineResumeSignals(&OldState);
}

extern "C" void VMSchedule()
{
	cout << "enter Schedule" << endl;
	//TMachineSignalState OldState;
  //MachineSuspendSignals(&OldState);
	TCB* nextThread = currentThread;
	SMachineContextRef old_context;
	SMachineContextRef new_context;
	// SCHEDULING
	if(!highPriority.empty())
	{
		cout << "high priority" << endl;
		if( (highPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/
			currentThread = highPriority.front();
			highPriority.erase( highPriority.begin() );
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(nextThread->threadContext);
			new_context = &(currentThread->threadContext);
			MachineContextSwitch(old_context, new_context);
		}
		//check if high queue has same prio as current or if high queue
		//		has higher prio than current
	}
	else if(!normalPriority.empty())
	{
		cout << "normal priority" << endl;
		if( (normalPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/
			currentThread = normalPriority.front();
			normalPriority.erase( normalPriority.begin() );
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(nextThread->threadContext);
			new_context = &(currentThread->threadContext);
			MachineContextSwitch(old_context, new_context);
		}
		//check if normal queue has same prio as current or if normal queue
		//		has higher prio than current
	}
	else if(!lowPriority.empty())
	{
		cout << "low priority" << endl;
		if( (lowPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/
			currentThread = lowPriority.front();
			lowPriority.erase( lowPriority.begin() );
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(nextThread->threadContext);
			new_context = &(currentThread->threadContext);
			MachineContextSwitch(old_context, new_context);
		}
		//check if low queue has same prio as current or if low queue
		//		has higher prio than current
	}
	else {
		cout << "idle thread" << endl;
		currentThread = allThreads[1];
		currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
		old_context = &(nextThread->threadContext);
		new_context = &(currentThread->threadContext);
		MachineContextSwitch(old_context, new_context);
		cout << "end idle" << endl;
	}
	//MachineResumeSignals(&OldState);
	cout << "end Schedule" << endl;

} // end  VMSchedule --------------------------------------------------//

extern "C" void AlarmCallback(void *param)
{
	//TMachineSignalState OldState;
  //MachineSuspendSignals(&OldState);
	cout << "enter alarm" << endl;
	cout << sleeping.empty() << endl;
	if (!sleeping.empty()) {
		cout << "not empty" << endl;
		for (int i = 0; i < (int)sleeping.size(); i ++)
		{
			cout << "less than sleeping size" << endl;
			if (sleeping[i]->threadWaitTicks == 0)
			{
				cout << "done sleeping" << endl;
				sleeping[i]->threadStackState = VM_THREAD_STATE_READY;
				//MachineResumeSignals(&OldState);
				//VMSchedule();

				if (sleeping[i]->threadPriority == VM_THREAD_PRIORITY_HIGH)
				{
					cout << "put in high vector" << endl;
					highPriority.push_back(sleeping[i]);
				}
				else if (sleeping[i]->threadPriority == VM_THREAD_PRIORITY_NORMAL)
				{
					cout << "put in normal vector" << endl;
					normalPriority.push_back(sleeping[i]);
				}
				else if (sleeping[i]->threadPriority == VM_THREAD_PRIORITY_LOW)
				{
					cout << "put in low vector" << endl;
					lowPriority.push_back(sleeping[i]);
				}
				cout << "erase and schedule" << endl;
				sleeping.erase(sleeping.begin() + i);
				VMSchedule();
			}
			else {}
				cout << "decrement wait ticks" << endl;
				sleeping[i]->threadWaitTicks = sleeping[i]->threadWaitTicks - 1;
				cout << i << ": " << sleeping[i]->threadWaitTicks << endl;
			}
		}

	else return;
	cout << "end alarm" << endl;

}// end  AlarmCallback --------------------------------------------//


extern "C" void VMIdle(void*) //done
{
	//MachineEnableSignals();
	while(1)
	{/*do nothing;*/}

} // end VMIdle ---------------------------------------------------//


extern "C" void VMThreadSkeleton(void *param)
{
	//MachineEnableSignals();
  TCB* entryPoint = ( (TCB*)param );
  entryPoint->threadEntryParam = ( ((TCB*)param)->threadEntryParam );
  VMThreadTerminate( entryPoint->threadID );

} // end  VMThreadSkeleton ----------------------------------------//


extern "C" TVMStatus VMStart(int tickms, int machinetickms, int argc,
	char *argv[]) // DONE EXCEPT SLEEPCALLBACK
{
	//TMachineSignalState OldState;
	MachineInitialize(machinetickms);
  MachineRequestAlarm(tickms*1000, AlarmCallback, NULL);
  //MachineRequestAlarm(tickms*1000, sleepCallBack, NULL);
	MachineEnableSignals();

	TCB* mainThread = new TCB;
	mainThread->threadStackState = VM_THREAD_STATE_RUNNING;
  mainThread->threadPriority = VM_THREAD_PRIORITY_NORMAL;
  mainThread->threadID = allThreads.size();
  allThreads.push_back(mainThread);
  currentThread = mainThread;

	TCB* tIdle = new TCB;
  tIdle->threadID = allThreads.size();
  tIdle->threadStackState = VM_THREAD_STATE_READY;
  tIdle->threadEntryFnct = VMIdle;
  tIdle->threadEntryParam = NULL;
  tIdle->threadPriority = VM_THREAD_PRIORITY_LOWLOW;
  tIdle->threadStackSize = 0x100000;
  tIdle->threadBaseStackPtr = new uint8_t[0x100000];

  MachineContextCreate(&(tIdle->threadContext),
  	VMIdle, NULL, tIdle->threadBaseStackPtr,
  	tIdle->threadStackSize);

	allThreads.push_back(tIdle);

  TVMMainEntry VMMain = VMLoadModule(argv[0]);

  if (VMMain != NULL)
  {
		//MachineResumeSignals(&OldState);
    VMMain(argc, argv);
    MachineTerminate();
    return VM_STATUS_SUCCESS;
  }
  else {
		//MachineResumeSignals(&OldState);
		return VM_STATUS_FAILURE;
	}

} // end  VMFileClose ---------------------------------------------//

//DONE
extern "C" TVMStatus VMThreadActivate(TVMThreadID thread)
{
	cout << "in activate" << endl;
	TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);
	cout << "in activate" << endl;
	cout << (int)thread << endl;
	cout << allThreads.size() << endl;
  // do some error checking for thread in size, non-null, and not dead
  MachineContextCreate( &(allThreads[thread]->threadContext),
    VMThreadSkeleton,
    allThreads[thread],
    allThreads[thread]->threadBaseStackPtr,
    allThreads[thread]->threadStackSize );
	cout << "in activate" << endl;
  allThreads[thread]->threadStackState = VM_THREAD_STATE_READY;
	cout << "in activate" << endl;
	VMPrioPush(allThreads[thread]);
  //VMPrioPush(allThreads[thread]);
	//MachineEnableSignals();
	//VMPrioPush(allThreads[thread]);
	if (!highPriority.empty() && currentThread->threadPriority < VM_THREAD_PRIORITY_HIGH)
	{
		if (currentThread->threadPriority == VM_THREAD_PRIORITY_HIGH)
		{
			highPriority.push_back(currentThread);
		}
		else if (currentThread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
		{
			normalPriority.push_back(currentThread);
		}
		else if (currentThread->threadPriority == VM_THREAD_PRIORITY_LOW)
		{
			lowPriority.push_back(currentThread);
		}
		MachineResumeSignals(&OldState);
		VMSchedule();
		return VM_STATUS_SUCCESS;
	}
	if (!normalPriority.empty() && currentThread->threadPriority < VM_THREAD_PRIORITY_NORMAL)
	{
		if (currentThread->threadPriority == VM_THREAD_PRIORITY_HIGH)
		{
			highPriority.push_back(currentThread);
		}
		else if (currentThread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
		{
			normalPriority.push_back(currentThread);
		}
		else if (currentThread->threadPriority == VM_THREAD_PRIORITY_LOW)
		{
			lowPriority.push_back(currentThread);
		}
		MachineResumeSignals(&OldState);
		VMSchedule();
		return VM_STATUS_SUCCESS;
	}
	MachineResumeSignals(&OldState);
	cout << "in activate" << endl;
	cout << "end activate" << endl;
	return VM_STATUS_SUCCESS;
} // end  VMThreadActivate ---------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param,
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	if (tid == NULL || entry == NULL)
	{
		MachineResumeSignals(&OldState);
    return VM_STATUS_ERROR_INVALID_PARAMETER;
  }
	TCB* newThread = new TCB;
  //*tid =  allThreads.size();
  *tid = newThread->threadID;
  newThread->threadStackState = VM_THREAD_STATE_DEAD;
  newThread->threadEntryParam = param;
  newThread->threadPriority = prio;
  newThread->threadStackSize = memsize;
  newThread->threadEntryFnct = entry;
  newThread->threadBaseStackPtr = new uint8_t[memsize];
  allThreads.push_back(newThread);
  MachineResumeSignals(&OldState);
	cout << "state before: " << newThread->threadStackState << endl;
	cout << "id before: " << newThread->threadID << endl;
  return VM_STATUS_SUCCESS;
} // end  VMFileClose ---------------------------------------------//


extern "C" TVMStatus VMThreadSleep(TVMTick tick)
{
	TMachineSignalState OldState;
  MachineSuspendSignals(&OldState);
	cout << "in thread sleep" << endl;
	currentThread->threadStackState = VM_THREAD_STATE_WAITING;
	currentThread->threadWaitTicks = tick;
	sleeping.push_back(currentThread);
	//MachineEnableSignals();
	MachineResumeSignals(&OldState);
	VMSchedule();
	cout << "end sleep" << endl;
	return VM_STATUS_SUCCESS;

} // end  VMThreadSleep -------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef
	stateref)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	if(stateref == NULL) {
		MachineResumeSignals(&OldState);
    return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	for(int i=0; i<(int)allThreads.size(); i++)
	{
		cout << "state: " << allThreads[i]->threadStackState << endl;
		cout << "id: " << allThreads[i]->threadID << endl;
		if(allThreads[i]->threadID == thread)
		{
  		stateref = &(allThreads[i]->threadStackState);
			MachineResumeSignals(&OldState);
  		return VM_STATUS_SUCCESS;
		}
	}
	MachineResumeSignals(&OldState);
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
	if (currentThread->threadID == thread)
	{
		VMSchedule();
	}

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
		TMachineSignalState OldState;
    MachineSuspendSignals(&OldState);
	  int len = *length;
	  write(filedescriptor, data, len);
		MachineResumeSignals(&OldState);
	  return VM_STATUS_SUCCESS;

	} // end FileWrite -----------------------------------------------//

//---------------------------------------------------------------------------//
//--------------------------------   END   ----------------------------------//
//---------------------------------------------------------------------------//
