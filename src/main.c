#include <stdio.h>
#include <stdlib.h>
#include <elm.h>
#include <utils.h>

//Dichiarazione del thread che gestisce la grafica
void* Thread_GUI(void* args);
void* Thread_Seriale(void* args);

int main(int argc, char* argv[]){
  //Creazione del contesto del Main-Loop
  MlContext *MLC;
  pthread_t idThreads[2];
  mlContextNew(&MLC);
  
  struct argsThreadStruct *argsT=(struct argsThreadStruct*)calloc(1,sizeof(struct argsThreadStruct));
  argsT->mlc=MLC;
  
  pthread_create(idThreads,NULL,Thread_Seriale,argsT);
  pthread_create(idThreads+1,NULL,Thread_GUI,argsT);
  
  //Ciclo di elaborazione degli eventi, termino con 2
  uint8_t lastCommand=0;
  float c=-0.9f;
  while(lastCommand != EXIT_ML){
    if(GetNSignals(MLC)){
      struct _EQueueElement *event=PopEventQueue(MLC);
      lastCommand=GetCommandElement(event);
      switch(lastCommand){
      case REQUEST_SERIAL:
        float *buffer=argsT->bufferSignals;
        *buffer=c;
        c += 1e-3;
      default:
        break;
      }
      free(event);
    }else{
      //Punto di sospensione se non ci sono eventi
      waitNewEvent(MLC);
    }
  }
  
  pthread_join(*idThreads,NULL);
  pthread_join(*(idThreads+1),NULL);
  
  free(argsT);
  PrintSignalQueue(MLC);
  mlContextDelete(MLC);
  return 0;
}
