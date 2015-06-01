#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include "MemPool.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <cstring>
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
// dont do p3 version of fileopen and fileclose
// otherwise memory stuff from p3 is needed yes
// anything with shared mem should DEF need to be in this

// run like this: ./vm -f fat.ima ./app.so




///////////////////////////////     GLOBALS     //////////////////////////////
int currentThread=-1;

vector<TCB*> highPriority;
vector<TCB*> normalPriority;
vector<TCB*> lowPriority;

//vector<TCB*> ready;
vector<TCB*> sleeping;
vector<TCB*> allThreads;

vector<MemPool*> allMemPools;
void* sharedSpace;

extern "C"
{ TVMMainEntry VMLoadModule(const char *module); }

#define VM_THREAD_PRIORITY_LOWEST		((TVMThreadPriority)0x00)
const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;


//---------------------------------------------------------------------------//
//----------------------------   MEMORY STUFF   -----------------------------//
//---------------------------------------------------------------------------//

TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size,
	TVMMemoryPoolIDRef memory)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	if (base == NULL || memory == NULL || size == 0)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	*memory = (TVMMemoryPoolID)allMemPools.size();

	MemPool* newMemPool = new MemPool;
	newMemPool->memID = *memory;
	newMemPool->memStackPtr = (uint8_t*)base;
	newMemPool->memStackSize = size;

	MemBlock* newMemBlock = new MemBlock;
	newMemBlock->stackPtr = newMemPool->memStackPtr;
	newMemBlock->size = newMemPool->memStackSize;
	newMemPool->memFree.push_back(newMemBlock);

	allMemPools.push_back(newMemPool);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	if(allMemPools[memory] == NULL)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	else if(!allMemPools[memory]->memUsed.empty())
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_STATE;
	}
	else
	{
		delete allMemPools[memory];
		allMemPools[memory] = NULL;
		MachineResumeSignals(&OldState);
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	if (allMemPools[memory] == NULL || bytesleft == NULL)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_ID;
	}

	*bytesleft = allMemPools[memory]->memFreeSpace;

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	if (allMemPools[memory] == NULL || size == 0 || pointer == NULL)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	MemBlock* newMemBlock = new MemBlock;
	newMemBlock->size = (size + 0x3F) & (~0x3F);
	int counter = 0;

	for (int i = 0; i < (int)allMemPools[memory]->memFree.size(); i++)
	{
		counter=i;
		if (allMemPools[memory]->memFree[i]->size >= newMemBlock->size)
			break;
	}

	if (allMemPools[memory]->memFree[counter]->size < newMemBlock->size)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
	}/*
	int j=0;
	for(j=0; j < (int)allMemPools[memory]->memUsed.size(); j++)
		if(allMemPools[memory]->memUsed[j]->stackPtr > allMemPools[memory]->memFree[counter]->stackPtr)
			break;

	newMemBlock->stackPtr = allMemPools[memory]->memFree[counter]->stackPtr;
	*pointer = newMemBlock->stackPtr;
	allMemPools[memory]->memUsed.insert(allMemPools[memory]->memUsed[j], newMemBlock);
	allMemPools[memory]->memFreeSpace -= newMemBlock->size;
	if(allMemPools[memory]->memUsed[counter]->size == 0)
		allMemPools[memory]->memFree.erase(allMemPools[memory]->memUsed[counter]);*/


	*pointer = allMemPools[memory]->memFree[counter]->stackPtr;
	newMemBlock->stackPtr = allMemPools[memory]->memFree[counter]->stackPtr;
	allMemPools[memory]->memUsed.push_back(newMemBlock);
	allMemPools[memory]->memFree[counter]->stackPtr =
	allMemPools[memory]->memFree[counter]->stackPtr + newMemBlock->size;
	allMemPools[memory]->memFree[counter]->size =
	allMemPools[memory]->memFree[counter]->size - newMemBlock->size;

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer)
{
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	if (allMemPools[memory] == NULL || pointer == NULL)
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	MemPool* tempMemPool = allMemPools[memory];
	int counter = 0;
	for (int i = 0; i < (int)tempMemPool->memUsed.size(); i++)
	{
		if (tempMemPool->memUsed[i] != NULL)
		{
			counter = counter + 1;
			if (tempMemPool->memUsed[i]->stackPtr == (uint8_t*)pointer)
				break;
		}
	}

	if (counter == (int)tempMemPool->memUsed.size())
	{
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	int flag = 0;
	for (int i = 0; i < (int)tempMemPool->memFree.size(); i++)
	{
		flag = 0;
		if ((tempMemPool->memFree[i]->stackPtr + tempMemPool->memFree[i]->size)
			== tempMemPool->memUsed[counter]->stackPtr)
		{
			tempMemPool->memFree[i]->size = tempMemPool->memFree[i]->size
				+ tempMemPool->memUsed[counter]->size;
			flag = 1;
			break;
		}
		if (tempMemPool->memFree[i]->size != 0)
		{
			if ((tempMemPool->memUsed[counter]->stackPtr + tempMemPool->memUsed[counter]->size)
				== tempMemPool->memFree[i]->stackPtr)
			{
				tempMemPool->memFree[i]->size = tempMemPool->memFree[i]->size
					+ tempMemPool->memUsed[counter]->size;
				tempMemPool->memFree[i]->size = tempMemPool->memUsed[i]->size;
				flag = 1;
				break;
			}
		}
	}

	if (flag == 0)
		tempMemPool->memFree.push_back(tempMemPool->memUsed[counter]);

	tempMemPool->memFreeSpace = tempMemPool->memFreeSpace
		+ tempMemPool->memUsed[counter]->size;
	tempMemPool->memUsed.erase(tempMemPool->memUsed.begin() + counter);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}

//---------------------------------------------------------------------------//
//----------------------------   MUTEX STUFF   ------------------------------//
//---------------------------------------------------------------------------//



//---------------------------------------------------------------------------//
//-----------------------------   FILE STUFF   ------------------------------//
//---------------------------------------------------------------------------//

	extern "C" void VMSchedule();
	extern "C" void VMPrioPush(TCB *thread);

	extern "C" void FileCallback(void* param, int result)
	{
		TCB* tempThread = (TCB*)param;
		tempThread->threadStackState = VM_THREAD_STATE_READY;
		//allThreads.push_back(tempThread);
		VMPrioPush(tempThread);
		tempThread->fileResult = result;
		if ( allThreads[currentThread]->threadStackState == VM_THREAD_STATE_RUNNING
			 && ( allThreads[currentThread]->threadPriority < tempThread->threadPriority
			 || allThreads[currentThread]->threadStackState !=  VM_THREAD_STATE_RUNNING ) )
				VMSchedule();
	}

	extern "C" TVMStatus VMFileClose(int filedescriptor)
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);

		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;

		MachineFileClose(filedescriptor, FileCallback,
			allThreads[currentThread]);
		//filedescriptor = allThreads[currentThread]->fileResult;
		VMSchedule();

		if (allThreads[currentThread]->fileResult != -1)
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_FAILURE;
		}

	} // end FileClose -----------------------------------------------//


	extern "C" TVMStatus VMFileOpen(const char *filename, int flags, int mode,
		int *filedescriptor)																											// @925 on piazza
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);

		if (filename == NULL || filedescriptor == NULL)
		{
			MachineResumeSignals(&OldState);
		  	return VM_STATUS_ERROR_INVALID_PARAMETER;
		}
		else
		{
			//cout << "\tfiledescriptor before MFileOpen = " << *filedescriptor << endl;
			allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
			MachineFileOpen(filename, flags, mode, FileCallback,
				allThreads[currentThread]);
			//*filedescriptor = open(filename, flags, mode);
			VMSchedule();
			*filedescriptor = allThreads[currentThread]->fileResult;
			//cout <<"\tfiledescriptor after MFileOpen = " << *filedescriptor << endl;
			if (*filedescriptor != -1)
			{
				MachineResumeSignals(&OldState);
				return VM_STATUS_SUCCESS;
			}
			else
			{
				MachineResumeSignals(&OldState);
				return VM_STATUS_FAILURE;
			}
		}

	} // end FileOpen ------------------------------------------------//


	extern "C" TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);

		if(data == NULL || length == NULL)
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
		}

		void* base = NULL;
		if(*length > 512)
			VMMemoryPoolAllocate(1, 512, &base);
		else
			VMMemoryPoolAllocate(1, (TVMMemorySize)(*length), &base);
		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
		int tempLength = *length;
		*length = 0;
		while(tempLength > 512)
		{
			if(tempLength > 512)
			{
				MachineFileRead(filedescriptor, base, 512,
					FileCallback, allThreads[currentThread]);
				VMSchedule();
				memcpy(data, base, 512);
				*length += allThreads[currentThread]->fileResult;
			}
			else
			{
				MachineFileRead(filedescriptor, base, tempLength,
					FileCallback, allThreads[currentThread]);
				VMSchedule();
				memcpy(data, base, tempLength);
				*length += allThreads[currentThread]->fileResult;
			}
			tempLength -= 512;
			data += 512;
		}

		VMMemoryPoolDeallocate(1, base);


		/*while (len > 0)
		{
			VMMemoryPoolAllocate(allMemPools[1]->memID, 512, (void**)&base);

			buffer = len;
			if (len > 512)
				buffer = 512;

			MachineFileRead(filedescriptor, base, buffer, FileCallback, (void*)allThreads[currentThread]);
			allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
			VMSchedule();
			memcpy(data, base, buffer);

			VMMemoryPoolDeallocate(allMemPools[1]->memID, base);

			len = len - 512;
			data = data + 512;
		}
		//MachineFileRead(filedescriptor, data, *length, FileCallback, (void*)allThreads[currentThread]);

		//filedescriptor = allThreads[currentThread]->fileResult;

		//VMSchedule();*/

		if (*length > 0)
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_FAILURE;
		}

	} // end FileRead ------------------------------------------------//


	extern "C" TVMStatus VMFileSeek(int filedescriptor, int offset, int whence,
		int *newoffset)
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);

		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;

		MachineFileSeek(filedescriptor, offset, whence,
			FileCallback, allThreads[currentThread]);

		VMSchedule();

		if (newoffset != NULL)
		{
			*newoffset = allThreads[currentThread]->fileResult;
			MachineResumeSignals(&OldState);
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_FAILURE;
		}

	} // end FileSeek ------------------------------------------------//


	extern "C" TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
	{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);
		if(data == NULL || length == NULL)
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
		}

		void* base = NULL;
		if(*length > 512)
			VMMemoryPoolAllocate(1, 512, &base);
		else
			VMMemoryPoolAllocate(1, (TVMMemorySize)(*length), &base);
		allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
		int tempLength = *length;
		//*length = 0;
		while(tempLength > 0)
		{
			if(tempLength > 512)
			{
				memcpy(base, data, 512);
				MachineFileWrite(filedescriptor, base, 512,
					FileCallback, allThreads[currentThread]);
				VMSchedule();
			}
			else
			{
				memcpy(base, data, tempLength);
				MachineFileRead(filedescriptor, base, tempLength,
					FileCallback, allThreads[currentThread]);
				VMSchedule();
			}
			tempLength -= 512;
			data += 512;
		}

		VMMemoryPoolDeallocate(1, base);
		
		/*while(len > 0)
		{
			VMMemoryPoolAllocate(allMemPools[1]->memID, 512, (void**)&base);
			memset(base, '\0', 512);
			buffer = len;
			if (len > 512)
				buffer = 512;

			memcpy(base, data, buffer);
			MachineFileWrite(filedescriptor, base, buffer, FileCallback, (void*)allThreads[currentThread]);
			allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
			VMSchedule();

			VMMemoryPoolDeallocate(allMemPools[1]->memID, base);
			len = len - 512;
			data = data + 512;
		}

		//MachineFileWrite(filedescriptor, data, *length, FileCallback, (void*)allThreads[currentThread]);
		//VMSchedule();
		//filedescriptor = allThreads[currentThread]->fileResult;*/


		if (allThreads[currentThread]->fileResult != -1)
		{
			MachineResumeSignals(&OldState);
			*length = allThreads[currentThread]->fileResult;
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_FAILURE;
		}
	} // end FileWrite -----------------------------------------------//



//---------------------------------------------------------------------------//
//----------------------------   THREAD STUFF   -----------------------------//
//---------------------------------------------------------------------------//


extern "C" void printStuff()
{
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
	cout << endl;
}  // end  printStuff --------------------------------------------------//


extern "C" void VMPrioPush(TCB *thread)
{
	//cout << "\tStart VMPrioPush" << endl;
	// pushing to proper priorities vectors
	if(thread->threadPriority == VM_THREAD_PRIORITY_LOW)
	{
		lowPriority.push_back(thread);
		//cout << "\t   pushed to lowPrio" << endl;
	}
	if(thread->threadPriority == VM_THREAD_PRIORITY_NORMAL)
	{
		normalPriority.push_back(thread);
		//cout << "\t   pushed to normalPrio" << endl;
	}
	if(thread->threadPriority == VM_THREAD_PRIORITY_HIGH)
	{
		highPriority.push_back(thread);
		//cout << "\t   pushed to highPrio" << endl;
	}
	//cout << "\t-End VMPrioPush" << endl;
}  // end  VMPrioPush --------------------------------------------------//


extern "C" void VMSchedule()
{
	//cout << "\tStart VMShedule" << endl;
	//cout << "\t  <--BEFORE" << endl;
	//printStuff();

	if(!highPriority.empty())
	{
		TCB* currentThreadRef = allThreads[currentThread];
		//cout << "\t  highPriority is not empty" << endl;
		// if current thread is running then set it to ready and
		//    put it into vectors
		if (currentThreadRef->threadStackState == VM_THREAD_STATE_RUNNING)
		{
        	currentThreadRef->threadStackState = VM_THREAD_STATE_READY;
        	VMPrioPush(currentThreadRef);
    	}
    	// if current thread is waiting and ticks are done
    	/*else if ( currentThreadRef->threadStackState==VM_THREAD_STATE_WAITING
    		&& currentThreadRef->threadWaitTicks != 0 )
    	{
        	sleeping.push_back(currentThreadRef);
        }*/
    	// set current to next and remove from Q
    	//TCB* temp = highPriority.front();
    	currentThread = highPriority.front()->threadID;
    	highPriority.erase(highPriority.begin());
    	// set current to running
    	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_RUNNING;
    	MachineContextSwitch(&(currentThreadRef->threadContext), &(allThreads[currentThread]->threadContext));
    	//cout << "\t  finished MCC high" << endl;
	}// finish highprio scheduling

	else if(!normalPriority.empty())
	{
		TCB* currentThreadRef = allThreads[currentThread];
		//cout << "\t  normalPriority is not empty" << endl;
		// if current thread is running then set it to ready and
		//    put it into vectors
		if (currentThreadRef->threadStackState == VM_THREAD_STATE_RUNNING)
		{
        	currentThreadRef->threadStackState = VM_THREAD_STATE_READY;
        	VMPrioPush(currentThreadRef);
    	}
    	// if current thread is waiting and ticks are done
    	/*else if ( currentThreadRef->threadStackState==VM_THREAD_STATE_WAITING
    		&& currentThreadRef->threadWaitTicks != 0 )
    	{
        	sleeping.push_back(currentThreadRef);
        }*/
    	// set current to next and remove from Q
    	//TCB* temp = normalPriority.front();
    	currentThread = normalPriority.front()->threadID;
    	normalPriority.erase(normalPriority.begin());
    	// set current to running
    	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_RUNNING;
    	MachineContextSwitch(&(currentThreadRef->threadContext), &(allThreads[currentThread]->threadContext));
    	//cout << "\t  finished MCC normal" << endl;
	}// finish normalprio scheduling

	else if(!lowPriority.empty())
	{
		//TCB* currentThreadRef = allThreads[currentThread];
		//cout << "\t  lowPriority is not empty" << endl;
		// if current thread is running then set it to ready and
		//    put it into vectors
		if (allThreads[currentThread]->threadStackState == VM_THREAD_STATE_RUNNING)
		{
			allThreads[currentThread]->threadStackState = VM_THREAD_STATE_READY;
		/*}
    	if (allThreads[currentThread]->threadStackState == VM_THREAD_STATE_READY)
		{*/
			VMPrioPush(allThreads[currentThread]);
		}
    	// if current thread is waiting and ticks are done
    	/*else if ( currentThreadRef->threadStackState==VM_THREAD_STATE_WAITING
    		&& currentThreadRef->threadWaitTicks != 0 )
    	{
        	sleeping.push_back(currentThreadRef);
        }*/
    	// set current to next and remove from Q
		TCB* currentThreadRef = allThreads[currentThread];
    	currentThread = lowPriority.front()->threadID;
    	//currentThread = temp->threadID;
    	lowPriority.erase(lowPriority.begin());
    	// set current to running
    	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_RUNNING;
    	MachineContextSwitch(&(currentThreadRef->threadContext), &(allThreads[currentThread]->threadContext));
    	//cout << "\t  finished MCC low" << endl;
	}// finish lowprio scheduling

	else
	{
		//cout << "\t  nothing to schedule, scheduling Idle" << endl;
		TCB* currentThreadRef = allThreads[currentThread];
		currentThread = allThreads.front()->threadID;
    	// set current to running
    	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_RUNNING;
    	MachineContextSwitch(&(currentThreadRef->threadContext), &(allThreads[currentThread]->threadContext));

	}// finish idle scheduling

	//cout << "\t  -->AFTER" <<  endl;
	//printStuff();

	//cout << "\t-End VMSchedule" << endl;

} // end  VMSchedule --------------------------------------------------//


extern "C" void AlarmCallback(void *param)
{
	//TMachineSignalState OldState;
	//MachineSuspendSignals(&OldState);
	//cout << "\tenter alarm" << endl;
	//cout << sleeping.empty() << endl;
	if (!sleeping.empty())
	{
		//cout << "\tnot empty" << endl;
		for (int i = 0; i < (int)sleeping.size(); i++)
		{
			//cout << "    sleeping idx=" << i << "\t";
			//cout << "ticks left=" << sleeping[i]->threadWaitTicks << " ";
			sleeping[i]->threadWaitTicks = sleeping[i]->threadWaitTicks - 1;
			//cout << "\tless than sleeping size" << endl;
			if (sleeping[i]->threadWaitTicks == 0)
			{
				//cout << "\tfound tick==0 at " << i << endl;
				sleeping[i]->threadStackState = VM_THREAD_STATE_READY;
				VMPrioPush(sleeping[i]);
				//cout << "\terase and schedule" << endl;
				sleeping.erase(sleeping.begin() + i);
				//MachineResumeSignals(&OldState);
				VMSchedule();
			}
			//cout << "\tdecrement wait ticks" << endl;
			//cout << i << ": " << sleeping[i]->threadWaitTicks << endl;
			}
		}

	//MachineResumeSignals(&OldState);
	//cout << endl;
	//cout << "\tend alarm" << endl;

}// end  AlarmCallback --------------------------------------------//


extern "C" void VMIdle(void*) //done
{
	MachineEnableSignals();
	while(1)
	{/*do nothing;*/}

} // end VMIdle ---------------------------------------------------//


extern "C" void VMThreadSkeleton(void *param)
{
	MachineEnableSignals();
	TCB* entryPoint = ( (TCB*)param );
	//entryPoint->threadEntryParam = ( ((TCB*)param)->threadEntryParam );
	entryPoint->threadEntryFnct(entryPoint->threadEntryParam);
	VMThreadTerminate( entryPoint->threadID );

} // end  VMThreadSkeleton ----------------------------------------//


TVMStatus VMStart(int tickms, TVMMemorySize heapsize, int machinetickms,
	TVMMemorySize sharedsize, const char *mount, int argc, char *argv[])
{
	uint8_t *image;
	sharedsize = (sharedsize + 0x1000) & (~0x1000);
	sharedSpace = MachineInitialize(machinetickms, sharedsize);
	//MachineInitialize(machinetickms);
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

	MemPool* mainMemPool = new MemPool;
	mainMemPool->memStackSize = heapsize;
	mainMemPool->memStackPtr = new uint8_t[heapsize];
	mainMemPool->memFreeSpace = heapsize;
	mainMemPool->memID = allMemPools.size();

	MemBlock* mainMemBlock = new MemBlock;
	mainMemBlock->stackPtr = mainMemPool->memStackPtr;
	mainMemBlock->size = mainMemPool->memStackSize;
	mainMemPool->memFree.push_back(mainMemBlock);
	allMemPools.push_back(mainMemPool);

	MemPool* sharedMemPool = new MemPool;
	sharedMemPool->memStackSize = sharedsize;
	sharedMemPool->memStackPtr = (uint8_t*)sharedSpace;
	sharedMemPool->memFreeSpace = sharedsize;
	sharedMemPool->memID = allMemPools.size();
	allMemPools.push_back(sharedMemPool);

	MemBlock* sharedMemBlock = new MemBlock;
	sharedMemBlock->stackPtr = sharedMemPool->memStackPtr;
	sharedMemBlock->size = sharedMemPool->memStackSize;
	sharedMemPool->memFree.push_back(sharedMemBlock);

	MachineFileOpen(mount, O_RDWR, 0644, FileCallback, (void*)allThreads[currentThread]);
	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	VMSchedule();

	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	void* basePtr = NULL;
	VMMemoryPoolAllocate(allMemPools[1]->memID, 512, (void**)&basePtr);

	MachineFileRead(allThreads[currentThread]->fileResult, basePtr, 512, FileCallback,
		(void*)allThreads[currentThread]);
	VMSchedule();

	VMMemoryPoolDeallocate(allMemPools[1]->memID, basePtr);


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
	//cout << "\tStart VMThreadActivate" << endl;
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
	//cout << "\tPUSHED the active thread" << endl;
	MachineResumeSignals(&OldState);
	VMSchedule();
	//cout << "\t-End VMThreadActivate" << endl;

	return VM_STATUS_SUCCESS;
} // end  VMThreadActivate ---------------------------------------------//


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
	newThread->threadID = *tid;
	newThread->threadStackState = VM_THREAD_STATE_DEAD;
	newThread->threadEntryParam = param;
	newThread->threadPriority = prio-1; // FIGURE THIS SHIT OUT
	newThread->threadStackSize = memsize;
	newThread->threadEntryFnct = entry;
	newThread->threadWaitTicks = 0;
	newThread->threadBaseStackPtr = new uint8_t[memsize];
	allThreads.push_back(newThread);
	MachineResumeSignals(&OldState);

	//cout << "\tFINISHED CREATE THREAD - prio=" << (int)prio-1;
	//cout << " - id="<< *tid << endl;
	return VM_STATUS_SUCCESS;
} // end  VMFileClose ---------------------------------------------//


extern "C" TVMStatus VMThreadSleep(TVMTick tick)
{
	//cout << "\tStart VMThreadSleep" << endl;
	TMachineSignalState OldState;
	//MachineSuspendSignals(&OldState);
	if(tick == VM_TIMEOUT_IMMEDIATE)
	{
		//VMPrioPush(allThreads[currentThread]);
		//later make it so that it searches through and find the right
		//thread to erase from the sleeping vector
		//sleeping.erase(sleeping.begin());
		//cout << "\t   Erased a sleeping thread" << endl;
		MachineResumeSignals(&OldState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
		//VMSchedule();
	}

	allThreads[currentThread]->threadStackState = VM_THREAD_STATE_WAITING;
	allThreads[currentThread]->threadWaitTicks = tick;
	sleeping.push_back(allThreads[currentThread]);

	//cout << "\t  Finished pushing to sleeping" << endl;

	VMSchedule();
	MachineResumeSignals(&OldState);

	//cout << "\t-End VMThreadSleep" << endl;
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

	// error checking first - check whether threadID is valid
	//		and not already dead

  allThreads[thread]->threadStackState = VM_THREAD_STATE_DEAD;

	if (allThreads[thread]->threadPriority == VM_THREAD_PRIORITY_HIGH)
	{
		for (int i = 0; i < (int)highPriority.size(); i++)
		{
			if (i == (int)thread)
			{
				highPriority.erase(highPriority.begin() + i);
			}
		}
	}
	else if (allThreads[thread]->threadPriority == VM_THREAD_PRIORITY_NORMAL)
	{
		for (int i = 0; i < (int)normalPriority.size(); i++)
		{
			if (i == (int)thread)
			{
				normalPriority.erase(normalPriority.begin() + i);
			}
		}
	}
	else if (allThreads[thread]->threadPriority == VM_THREAD_PRIORITY_LOW)
	{
		for (int i = 0; i < (int)lowPriority.size(); i++)
		{
			if (i == (int)thread)
			{
				lowPriority.erase(lowPriority.begin() + i);
			}
		}
	}
	//MachineResumeSignals(&OldState);
	//if (currentThread == (int)thread)
	//{
		VMSchedule();
	//}
	MachineResumeSignals(&OldState);
  return VM_STATUS_SUCCESS;

} // end  VMThreadState -------------------------------------------//


//---------------------------------------------------------------------------//
//--------------------------------   END   ----------------------------------//
//---------------------------------------------------------------------------//
