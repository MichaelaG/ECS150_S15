#include "VirtualMachine.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

//==================================VMSTART==================================//

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {

    

}

//================================VMFILEOPEN=================================//

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {



}

//================================VMFILECLOSE================================//

TVMStatus VMFileClose(int filedescriptor) {



}

//================================VMFILEREAD=================================//

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {



}

//================================VMFILEWRITE================================//

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {



}

//================================VMFILESEEK=================================//

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {



}
