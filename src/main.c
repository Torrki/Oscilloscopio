#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elm.h>
#include <utils.h>
#include <signal.h> 
#include <unistd.h>

//Dichiarazione del thread che gestisce la grafica
void* Thread_GUI(void* args);

int main(int argc, char* argv[]){
  //Creazione del contesto del Main-Loop
  MlContext *MLC;
  pthread_t idThread;
  mlContextNew(&MLC);
  
  struct argsThreadStruct *argsT=(struct argsThreadStruct*)calloc(1,sizeof(struct argsThreadStruct));
  argsT->mlc=MLC;
  
  pthread_create(&idThread,NULL,Thread_GUI,argsT);
  
  //Ciclo di elaborazione degli eventi, termino con 2
  uint8_t lastCommand=0;
  uint16_t *bufferCopy=NULL;
  
  while(lastCommand != EXIT_ML){
    if(GetNSignals(MLC)){
      struct _EQueueElement *event=PopEventQueue(MLC);
      lastCommand=GetCommandElement(event);
      switch(lastCommand){
      case RENDER_GL:
        //Copia del buffer per permettere di campionare anche durante il rendering
        ssize_t readByte= read(argsT->fdSeriale,bufferCopy,sizeof(uint16_t)*argsT->nElementi);
        int nElementiLetti=readByte/sizeof(uint16_t);
        printf("%lu\n",readByte);
        for(int k=0; k < nElementiLetti; ++k){
          argsT->bufferSignals[k]=((sampleType)(bufferCopy[k]>>6))*argsT->gainOsc;
        }
        argsT->nElementiLetti=nElementiLetti;
        break;
      case START_RENDER:
        //Allocazione della memoria per il campionamento
        bufferCopy=(uint16_t*)calloc(argsT->nElementi,sizeof(uint16_t));
        break;
      case END_RENDER:
        //Deallocazione della memoria del campionamento
        free(bufferCopy);
        bufferCopy=NULL;
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
        pthread_kill(idThread,SIGCONT);
        break;
      case OPEN_SERIAL:
        int* retAddr=(int*)GetResAddrElement(event);
        const char* filePar=(const char*)GetDataAddrElement(event);
        *retAddr=OpenSerial(filePar);
        pthread_kill(idThread,SIGCONT);
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
  pthread_join(idThread,NULL);
  
  free(argsT);
  //PrintSignalQueue(MLC);
  mlContextDelete(MLC);
  return 0;
}
