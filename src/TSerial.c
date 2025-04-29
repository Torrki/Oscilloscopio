#include <stdlib.h>
#include <elm.h>

//Implementazione del thread per la seriale
void* Thread_Seriale(void* args){
  struct _mlcontext *MLC=(struct _mlcontext *)args;
  signalMainLoop(MLC,1,NULL);
  return NULL;
}
