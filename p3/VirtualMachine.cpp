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

#define VM_THREAD_PRIORITY_LOWEST		((TVMThreadPriority)0x00)

//---------------------------------------------------------------------------//
//----------------------------   THREAD STUFF   -----------------------------//
//---------------------------------------------------------------------------//

extern "C" void VMPrioPush(TCB *thread)
{
	cout << "\tStart VMPrioPush" << endl;
	// pushing to proper priorities vectors
	if(thread->threadPriority == VM_THREAD_PRIORITY_LOW)
	{
		lowPriority.push_back(thread);
		cout << "\t   pushed to lowPrio" << endl;
	}
	if(thread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
	{
		normalPriority.push_back(thread);
		cout << "\t   pushed to normalPrio" << endl;
	}
	if(thread->threadPriority == VM_THREAD_PRIORITY_HIGH)
	{
		highPriority.push_back(thread);
		cout << "\t   pushed to highPrio" << endl;
	}
	cout << "\t-End VMPrioPush" << endl;
}  // end  VMPrioPush --------------------------------------------------//

extern "C" void VMSchedule()
{
	cout << "\tStart VMShedule" << endl;

	cout << "currentThread: tid=" << currentThread->threadID;
	cout << " prio=" << currentThread->threadPriority << endl;
	cout << "allThreads:" << endl;
	for(int i=0; i<(int)allThreads.size();i++)
	{
		cout << "tID=" << allThreads[i]->threadID;
		cout << " prio=" << allThreads[i]->threadPriority << endl;
	}
	cout << "highPrio:" << endl;
	for(int i=0; i<(int)highPriority.size();i++)
	{
		cout << "tID=" << highPriority[i]->threadID;
		cout << " prio=" << highPriority[i]->threadPriority << endl;
	}
	cout << "normalPrio:" << endl;
	for(int i=0; i<(int)normalPriority.size();i++)
	{
		cout << "tID=" << normalPriority[i]->threadID;
		cout << " prio=" << normalPriority[i]->threadPriority << endl;
	}
	cout << "lowPrio:" << endl;
	for(int i=0; i<(int)lowPriority.size();i++)
	{
		cout << "tID=" << lowPriority[i]->threadID;
		cout << " prio=" << lowPriority[i]->threadPriority << endl;
	}
	cout << "sleeping:" << endl;
	for(int i=0; i<(int)sleeping.size();i++)
	{
		cout << "tID=" << sleeping[i]->threadID;
		cout << " prio=" << sleeping[i]->threadPriority << endl;
	}



	SMachineContextRef old_context;
	SMachineContextRef new_context;
	// SCHEDULING
	if(!highPriority.empty())
	{
		cout << "\t   Scheduling HighPrio Thread" << endl;
		if( (highPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/

			TCB* old = currentThread;
			currentThread = highPriority.front();
			highPriority.erase(highPriority.begin());
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch( &(old->threadContext), &(currentThread->threadContext) );

			/*
			TCB* nextThread = highPriority.front();
			highPriority.erase( highPriority.begin() );
			nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(currentThread->threadContext);
			new_context = &(nextThread->threadContext);
			currentThread = nextThread;
			MachineContextSwitch(old_context, new_context);*/
		}
		//check if high queue has same prio as current or if high queue
		//		has higher prio than current
	}
	else if(!normalPriority.empty())
	{
		cout << "\t   Scheduling NormalPrio Thread" << endl;
		if( (normalPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/

			TCB* old = currentThread;
			currentThread = normalPriority.front();
			normalPriority.erase(normalPriority.begin());
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch( &(old->threadContext), &(currentThread->threadContext) );

			/*TCB* nextThread = normalPriority.front();
			normalPriority.erase( normalPriority.begin() );
			nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(currentThread->threadContext);
			new_context = &(nextThread->threadContext);
			currentThread = nextThread;
			MachineContextSwitch(old_context, new_context);*/
		}
		//check if normal queue has same prio as current or if normal queue
		//		has higher prio than current
	}
	else if(!lowPriority.empty())
	{
		cout << "\t   Scheduling LowPrio Thread" << endl;
		if( (lowPriority.front()->threadPriority >= currentThread->threadPriority
			&& currentThread->threadStackState != VM_THREAD_STATE_RUNNING) )
		{
			/*if(currentThread->threadStackState == VM_THREAD_STATE_RUNNING)
			{
				currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
				VMPrioPush(currentThread);
			}*/
			TCB* old = currentThread;
			currentThread = lowPriority.front();
			lowPriority.erase(lowPriority.begin());
			currentThread->threadStackState = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch( &(old->threadContext), &(currentThread->threadContext) );

			/*TCB* nextThread = lowPriority.front();
			lowPriority.erase( lowPriority.begin() );
			nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
			old_context = &(currentThread->threadContext);
			new_context = &(nextThread->threadContext);
			MachineContextSwitch(old_context, new_context);
			currentThread = nextThread;*/
		}
		//check if low queue has same prio as current or if low queue
		//		has higher prio than current
	}
	else if(currentThread->threadStackState != VM_THREAD_STATE_RUNNING)
	{
		cout << "\t   idle thread" << endl;
		currentThread = allThreads[0];
		old_context = &(currentThread->threadContext);
		MachineContextSwitch( &(allThreads[currentThread->threadID]->threadContext), &(allThreads[0]->threadContext) );
		cout << "\t   end idle" << endl;
	}
	cout << "currentThread: tid=" << currentThread->threadID;
	cout << " prio=" << currentThread->threadPriority << endl;
	cout << "allThreads:" << endl;
	for(int i=0; i<(int)allThreads.size();i++)
	{
		cout << "tID=" << allThreads[i]->threadID;
		cout << " prio=" << allThreads[i]->threadPriority << endl;
	}
	cout << "highPrio:" << endl;
	for(int i=0; i<(int)highPriority.size();i++)
	{
		cout << "tID=" << highPriority[i]->threadID;
		cout << " prio=" << highPriority[i]->threadPriority << endl;
	}
	cout << "normalPrio:" << endl;
	for(int i=0; i<(int)normalPriority.size();i++)
	{
		cout << "tID=" << normalPriority[i]->threadID;
		cout << " prio=" << normalPriority[i]->threadPriority << endl;
	}
	cout << "lowPrio:" << endl;
	for(int i=0; i<(int)lowPriority.size();i++)
	{
		cout << "tID=" << lowPriority[i]->threadID;
		cout << " prio=" << lowPriority[i]->threadPriority << endl;
	}
	cout << "sleeping:" << endl;
	for(int i=0; i<(int)sleeping.size();i++)
	{
		cout << "tID=" << sleeping[i]->threadID;
		cout << " prio=" << sleeping[i]->threadPriority << endl;
	}





	cout << "\t-End VMSchedule" << endl;

} // end  VMSchedule --------------------------------------------------//

extern "C" void AlarmCallback(void *param)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	cout << "\tenter alarm" << endl;
	cout << sleeping.empty() << endl;
	if (!sleeping.empty()) {
		cout << "\tnot empty" << endl;
		for (int i = 0; i < (int)sleeping.size(); i++)
		{
			cout << "\tless than sleeping size" << endl;
			if (sleeping[i]->threadWaitTicks == 0)
			{
				cout << "\tfound tick==0" << endl;
				sleeping[i]->threadStackState = VM_THREAD_STATE_READY;
				VMPrioPush(sleeping[i]);
				cout << "\terase and schedule" << endl;
				sleeping.erase(sleeping.begin() + i);
				VMSchedule();
			}
			cout << "\tdecrement wait ticks" << endl;
			sleeping[i]->threadWaitTicks = sleeping[i]->threadWaitTicks - 1;
			//cout << i << ": " << sleeping[i]->threadWaitTicks << endl;
			}
		}

	else
		return;
	MachineResumeSignals(&OldState);
	cout << "\tend alarm" << endl;

}// end  AlarmCallback --------------------------------------------//


extern "C" void VMIdle(void*) //done
{
	while(1)
	{/*do nothing;*/}

} // end VMIdle ---------------------------------------------------//


extern "C" void VMThreadSkeleton(void *param)
{
  TCB* entryPoint = ( (TCB*)param );
  entryPoint->threadEntryParam = ( ((TCB*)param)->threadEntryParam );
  VMThreadTerminate( entryPoint->threadID );

} // end  VMThreadSkeleton ----------------------------------------//


extern "C" TVMStatus VMStart(int tickms, int machinetickms, int argc,
	char *argv[]) // DONE EXCEPT SLEEPCALLBACK
{
	MachineInitialize(machinetickms);
	MachineRequestAlarm(tickms*1000, AlarmCallback, NULL);
	MachineEnableSignals();

	TCB* tIdle = new TCB;
	tIdle->threadID = allThreads.size();
	tIdle->threadStackState = VM_THREAD_STATE_READY;
	tIdle->threadEntryFnct = VMIdle;
	tIdle->threadEntryParam = NULL;
	tIdle->threadPriority = VM_THREAD_PRIORITY_LOWEST;
	tIdle->threadStackSize = 0x100000;
	tIdle->threadBaseStackPtr = new uint8_t[0x100000];
	allThreads.push_back(tIdle);

	TCB* mainThread = new TCB;
	mainThread->threadStackState = VM_THREAD_STATE_RUNNING;
	mainThread->threadPriority = VM_THREAD_PRIORITY_NORMAL;
	mainThread->threadID = allThreads.size();
	allThreads.push_back(mainThread);
	currentThread = mainThread;
	VMPrioPush(mainThread);

	MachineContextCreate(&(tIdle->threadContext),
		VMIdle, NULL, tIdle->threadBaseStackPtr,
		tIdle->threadStackSize);

	TVMMainEntry VMMain = VMLoadModule(argv[0]);
	if (VMMain != NULL)
	{
		VMMain(argc, argv);
		MachineTerminate();
		return VM_STATUS_SUCCESS;
	}
	return VM_STATUS_FAILURE;

} // end  VMFileClose ---------------------------------------------//

//DONE
extern "C" TVMStatus VMThreadActivate(TVMThreadID thread)
{

	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	//cout << (int)thread << endl;
	//cout << allThreads.size() << endl;
	// do some error checking for thread in size, non-null, and not dead
	MachineContextCreate( &(allThreads[thread]->threadContext),
    VMThreadSkeleton,
    allThreads[thread],
    allThreads[thread]->threadBaseStackPtr,
    allThreads[thread]->threadStackSize );


	allThreads[thread]->threadStackState = VM_THREAD_STATE_READY;

	VMPrioPush(allThreads[thread]);
	cout << "\tPUSHED the active thread" << endl;
	VMSchedule();
	MachineResumeSignals(&OldState);

	return VM_STATUS_SUCCESS;
} // end  VMThreadActivate ---------------------------------------------//


//DONE
extern "C" TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param,
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	//cout << prio << (unsigned int)VM_THREAD_PRIORITY_LOW <<endl;
	if (tid == NULL || entry == NULL)
    	return VM_STATUS_ERROR_INVALID_PARAMETER;
   	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	TCB* newThread = new TCB;
	*tid =  allThreads.size();
	newThread->threadID = allThreads.size();
	newThread->threadStackState = VM_THREAD_STATE_DEAD;
	newThread->threadEntryParam = param;
	newThread->threadPriority = prio-1;
	newThread->threadStackSize = memsize;
	newThread->threadEntryFnct = entry;
	newThread->threadBaseStackPtr = new uint8_t[memsize];
	allThreads.push_back(newThread);
	MachineResumeSignals(&OldState);

	cout << "\tFINISHED CREATE THREAD - prio=" << (int)prio-1 << " - id="<< *tid << endl;
	return VM_STATUS_SUCCESS;
} // end  VMFileClose ---------------------------------------------//


extern "C" TVMStatus VMThreadSleep(TVMTick tick)
{
	cout << "\tStart VMThreadSleep" << endl;
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	if(tick == VM_TIMEOUT_IMMEDIATE)
		VMSchedule();

	if (currentThread->threadPriority == VM_THREAD_PRIORITY_LOW)
	{
		lowPriority.erase(lowPriority.begin());
		cout << "\t   Erased a lowPrio thread" << endl;
	}
	if (currentThread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
	{
		normalPriority.erase(normalPriority.begin());
		cout << "\t   Erased a normalPrio thread" << endl;
	}
	if (currentThread->threadPriority == VM_THREAD_PRIORITY_HIGH)
	{
		highPriority.erase(highPriority.begin());
		cout << "\t   Erased a highPrio thread" << endl;
	}

	currentThread->threadStackState = VM_THREAD_STATE_WAITING;
	currentThread->threadWaitTicks = tick;
	sleeping.push_back(currentThread);

	VMSchedule();
	MachineResumeSignals(&OldState);
	cout << "\t-End VMThreadSleep" << endl;
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
	//cout << "thread = " << (int)thread<< endl;
	for(int i=0; i<(int)allThreads.size(); i++)
	{
		//cout << "state: " << allThreads[i]->threadStackState << endl;
		//cout << "id: " << allThreads[i]->threadID << endl;
		if(allThreads[i]->threadID == thread)
		{
  		*stateref = (allThreads[i]->threadStackState);
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
