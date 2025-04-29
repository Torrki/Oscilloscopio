#include <stdlib.h>
#include <elm.h>

void* Thread_GUI(void* args){
  struct _mlcontext *MLC=(struct _mlcontext *)args;
  signalMainLoop(MLC,2,NULL);
  return NULL;
}
