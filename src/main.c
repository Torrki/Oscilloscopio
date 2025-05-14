#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elm.h>
#include <utils.h>
#include <signal.h> 
#include <unistd.h>

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
  unsigned i=0;
  float *bufferCopy=NULL;
  
  while(lastCommand != EXIT_ML){
    if(GetNSignals(MLC)){
      struct _EQueueElement *event=PopEventQueue(MLC);
      lastCommand=GetCommandElement(event);
      switch(lastCommand){
      case REQUEST_SERIAL:
        //Lettura dalla seriale del dato attuale
        uint8_t dato=0;
        read(argsT->fdSeriale,&dato,sizeof(char));
        bufferCopy[i++]=((float)dato)/500.0f;
        c += 2e-3;
        break;
      case RENDER_GL:
        //Copia del buffer per permettere di campionare anche durante il rendering
        memcpy(argsT->bufferSignals,bufferCopy,sizeof(float)*argsT->nElementi);
        i=0;
        break;
      case START_RENDER:
        //Allocazione della memoria per il campionamento
        bufferCopy=(float*)calloc(argsT->nElementi,sizeof(float));
        i=0;
        break;
      case END_RENDER:
        //Deallocazione della memoria del campionamento
        free(bufferCopy);
        break;
      case SCAN_SERIAL:
        //Comando per la scansione delle seriali CDC disponibili
        argsT->comandoSeriale=SCAN_SERIAL;
        //Se ho già fatto una scansione cancello il risultato
        if(argsT->porte){
          unsigned k=0;
          while(argsT->porte[k]){
            free((void*)(argsT->porte[k]));
            ++k;
          }
          free(argsT->porte);
        }
        argsT->porte=ScanPorts();
        pthread_kill(idThreads[0],SIGCONT);
        pthread_kill(idThreads[1],SIGCONT);
        break;
      default:
        break;
      }
      free(event);
    }else{
      //Punto di sospensione se non ci sono eventi
      waitNewEvent(MLC);
    }
  }
  argsT->comandoSeriale=EXIT_ML;
  pthread_kill(idThreads[0],SIGCONT);
  
  pthread_join(*idThreads,NULL);
  pthread_join(*(idThreads+1),NULL);
  
  free(argsT);
  PrintSignalQueue(MLC);
  mlContextDelete(MLC);
  return 0;
}
