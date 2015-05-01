#include "VirtualMachine.h"
#include "Machine.h"
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

TVMTick threadTick;

//=========================INCLUDE FROM OTHER FILES=========================//

TVMMainEntry VMLoadModule(const char *module);

extern "C" {

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

//================================VMFILEWRITE================================//

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

    int len = *length;                  // length stored in an int to use in write system call
    write(filedescriptor, data, len);
    return VM_STATUS_SUCCESS;

}
}
