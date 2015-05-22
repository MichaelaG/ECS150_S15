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
//TCB* currentThread=NULL;
int currentThread=-1;

vector<TCB*> highPriority;
vector<TCB*> normalPriority;
vector<TCB*> lowPriority;

vector<TCB*> ready;
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



	cout << "\tBEFORE SCHEDULING" << endl;
	cout << "\t\tcurrentThread:\n\t\t tid=" << allThreads[currentThread]->threadID;
	cout << " prio=" << allThreads[currentThread]->threadPriority << endl;
	cout << "\t\tallThreads:" << endl;
	for(int i=0; i<(int)allThreads.size();i++)
	{
		cout << "\t\t tID=" << allThreads[i]->threadID;
		cout << " prio=" << allThreads[i]->threadPriority << endl;
	}
	cout << "\t\thighPrio:" << endl;
	for(int i=0; i<(int)highPriority.size();i++)
	{
		cout << "\t\t tID=" << highPriority[i]->threadID;
		cout << " prio=" << highPriority[i]->threadPriority << endl;
	}
	cout << "\t\tnormalPrio:" << endl;
	for(int i=0; i<(int)normalPriority.size();i++)
	{
		cout << "\t\t tID=" << normalPriority[i]->threadID;
		cout << " prio=" << normalPriority[i]->threadPriority << endl;
	}
	cout << "\t\tlowPrio:" << endl;
	for(int i=0; i<(int)lowPriority.size();i++)
	{
		cout << "\t\t tID=" << lowPriority[i]->threadID;
		cout << " prio=" << lowPriority[i]->threadPriority << endl;
	}
	cout << "\t\tsleeping:" << endl;
	for(int i=0; i<(int)sleeping.size();i++)
	{
		cout << "\t\t tID=" << sleeping[i]->threadID;
		cout << " prio=" << sleeping[i]->threadPriority << endl;
	}




	//SMachineContextRef old_context;
	//SMachineContextRef new_context;
	// SCHEDULING
	if(!highPriority.empty())
	{
		if( (highPriority.front()->threadPriority == allThreads[currentThread]->threadPriority
        && allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING)
		|| highPriority.front()->threadPriority > allThreads[currentThread]->threadPriority)
		{
			cout << "\t   Scheduling HighPrio Thread" << endl;
			TCB* nextThread = highPriority.front();
			TCB* currentThreadRef = allThreads[currentThread];
			if(nextThread->threadID != allThreads[currentThread]->threadID)
			{
	            highPriority.erase(highPriority.begin());
	            nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
	            //old_context = &(allThreads[currentThread]->threadContext);
	            //new_context = &(nextThread->threadContext);
	            currentThread = nextThread->threadID;
				MachineContextSwitch( &(currentThreadRef->threadContext),&(nextThread->threadContext) );
			}
		}
	}
	else if(!normalPriority.empty())
	{
		if((normalPriority.front()->threadPriority == allThreads[currentThread]->threadPriority
		&& allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING)
		|| allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING
		|| normalPriority.front()->threadPriority > allThreads[currentThread]->threadPriority)
		{
			cout << "\t   Scheduling NormalPrio Thread" << endl;
			TCB* nextThread = normalPriority.front();
			TCB* currentThreadRef = allThreads[currentThread];
			cout << "\t  "<< nextThread->threadID << " " << allThreads[currentThread]->threadID << endl;
			if(nextThread->threadID != allThreads[currentThread]->threadID)
			{
				cout << "\tNORMAL PRIO ERASE" << endl;
	            normalPriority.erase(normalPriority.begin());
	            nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
	            //old_context = &(allThreads[currentThread]->threadContext);
	            //new_context = &(nextThread->threadContext);
	            currentThread = nextThread->threadID;
				MachineContextSwitch( &(currentThreadRef->threadContext),&(nextThread->threadContext) );
			}
		}
	}
	else if(!lowPriority.empty())
	{
		if((lowPriority.front()->threadPriority == allThreads[currentThread]->threadPriority
        && allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING)
        || allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING
		|| lowPriority.front()->threadPriority > allThreads[currentThread]->threadPriority)
		{
			cout << "\t   Scheduling LowPrio Thread" << endl;
			TCB* nextThread = lowPriority.front();
			TCB* currentThreadRef = allThreads[currentThread];
			cout << "\t  "<< nextThread->threadID << " " << allThreads[currentThread]->threadID << endl;
			if(nextThread->threadID != allThreads[currentThread]->threadID)
			{
				cout << "\tLOW PRIO ERASE" << endl;
	            lowPriority.erase(lowPriority.begin());
	            nextThread->threadStackState = VM_THREAD_STATE_RUNNING;
	            //old_context = &(allThreads[currentThread]->threadContext);
	            //new_context = &(nextThread->threadContext);
	            currentThread = nextThread->threadID;
	            cout << "\t   " << &(currentThreadRef->threadContext) << " " << &(nextThread->threadContext) << endl;
				MachineContextSwitch( &(currentThreadRef->threadContext),&(nextThread->threadContext) );
			}
		}
	}
	else if(allThreads[currentThread]->threadStackState != VM_THREAD_STATE_RUNNING)
	{
		cout << "\t   idle thread" << endl;
		//old_context = &(currentThread->threadContext);
		TCB* currentThreadRef = allThreads[currentThread];
		currentThread=0;
		MachineContextSwitch( &(currentThreadRef->threadContext), &(allThreads[0]->threadContext) );
		cout << "\t   end idle" << endl;
	}





	cout << "\tAFTER SCHEDULING" << endl;
	cout << "\t\tcurrentThread:\n\t\t tid=" << allThreads[currentThread]->threadID;
	cout << " prio=" << allThreads[currentThread]->threadPriority << endl;
	cout << "\t\tallThreads:" << endl;
	for(int i=0; i<(int)allThreads.size();i++)
	{
		cout << "\t\t tID=" << allThreads[i]->threadID;
		cout << " prio=" << allThreads[i]->threadPriority << endl;
	}
	cout << "\t\thighPrio:" << endl;
	for(int i=0; i<(int)highPriority.size();i++)
	{
		cout << "\t\t tID=" << highPriority[i]->threadID;
		cout << " prio=" << highPriority[i]->threadPriority << endl;
	}
	cout << "\t\tnormalPrio:" << endl;
	for(int i=0; i<(int)normalPriority.size();i++)
	{
		cout << "\t\t tID=" << normalPriority[i]->threadID;
		cout << " prio=" << normalPriority[i]->threadPriority << endl;
	}
	cout << "\t\tlowPrio:" << endl;
	for(int i=0; i<(int)lowPriority.size();i++)
	{
		cout << "\t\t tID=" << lowPriority[i]->threadID;
		cout << " prio=" << lowPriority[i]->threadPriority << endl;
	}
	cout << "\t\tsleeping:" << endl;
	for(int i=0; i<(int)sleeping.size();i++)
	{
		cout << "\t\t tID=" << sleeping[i]->threadID;
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
				MachineResumeSignals(&OldState);
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
	MachineEnableSignals();
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
	currentThread = mainThread->threadID;
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
	MachineResumeSignals(&OldState);
	VMSchedule();

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
	//MachineSuspendSignals(&OldState);
	if(tick == VM_TIMEOUT_IMMEDIATE)
	{
		VMPrioPush(allThreads[currentThread]);
		//later make it so that it searches through and find the right
		//thread to erase from the sleeping vector
		sleeping.erase(sleeping.begin());
		cout << "\t   Erased a sleeping thread" << endl;
		MachineResumeSignals(&OldState);
		//VMSchedule();
	}
	if (allThreads[currentThread]->threadPriority == VM_THREAD_PRIORITY_LOW)
	{
		lowPriority.erase(lowPriority.begin());
		cout << "\t   Erased a lowPrio thread" << endl;
	}
	if (allThreads[currentThread]->threadPriority == VM_THREAD_PRIORITY_NORMAL)
	{
		normalPriority.erase(normalPriority.begin());
		cout << "\t   Erased a normalPrio thread" << endl;
	}
	if (allThreads[currentThread]->threadPriority == VM_THREAD_PRIORITY_HIGH)
	{
		highPriority.erase(highPriority.begin());
		cout << "\t   Erased a highPrio thread" << endl;
	}

	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	allThreads[currentThread]->threadWaitTicks = tick;
	sleeping.push_back(allThreads[currentThread]);

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
	if (currentThread == (int)thread)
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
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);

		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
		//MachineFileClose(filedescriptor, fileCallBack, (void*)allThreads[currentThread]);

		MachineResumeSignals(&OldState);
		return VM_STATUS_SUCCESS;

	} // end FileClose -----------------------------------------------//


	extern "C" TVMStatus VMFileOpen(const char *filename, int flags, int mode,
		int *filedescriptor)
	{
	  if (filename == NULL || filedescriptor == NULL)
	  	return VM_STATUS_ERROR_INVALID_PARAMETER;
	  allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	  *filedescriptor = open(filename, flags, mode);

	  return VM_STATUS_SUCCESS;

	} // end FileOpen ------------------------------------------------//


	extern "C" TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
	{
		if(data == NULL || length == NULL)
			return VM_STATUS_ERROR_INVALID_PARAMETER;

		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);
		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;

		// do read

		// schedule

		MachineResumeSignals(&OldState);
	  	return VM_STATUS_SUCCESS;
	} // end FileRead ------------------------------------------------//


	extern "C" TVMStatus VMFileSeek(int filedescriptor, int offset, int whence,
		int *newoffset)
	{
	  TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);
		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;

		// do seek

		// schedule

		MachineResumeSignals(&OldState);
	  return VM_STATUS_SUCCESS;

	} // end FileSeek ------------------------------------------------//


	extern "C" TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);
		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	  	int len = *length;
	  	write(filedescriptor, data, len);
		MachineResumeSignals(&OldState);

	  	return VM_STATUS_SUCCESS;
	} // end FileWrite -----------------------------------------------//

//---------------------------------------------------------------------------//
//--------------------------------   END   ----------------------------------//
//---------------------------------------------------------------------------//
