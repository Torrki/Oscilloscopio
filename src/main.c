#include <stdio.h>
#include <stdlib.h>
#include <elm.h>

//Dichiarazione del thread che gestisce la grafica
void* Thread_GUI(void* args);
void* Thread_Seriale(void* args);

int main(int argc, char* argv[]){
  //Creazione del contesto del Main-Loop
  struct _mlcontext *MLC;
  pthread_t idThreads[2];
  mlContextNew(&MLC);
  pthread_create(idThreads,NULL,Thread_Seriale,MLC);
  pthread_create(idThreads+1,NULL,Thread_GUI,MLC);
  
  //Ciclo di elaborazione degli eventi, termino con 2
  uint8_t lastCommand=0;
  while(lastCommand != EXIT_ML){
    if(GetNSignals(MLC)){
      struct _EQueueElement *event=PopEventQueue(MLC);
      lastCommand=GetCommandElement(event);
      switch(lastCommand){
      case REQUEST_SERIAL:
        printf("Thread seriale\n");
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
  PrintSignalQueue(MLC);
  mlContextDelete(MLC);
  return 0;
}
