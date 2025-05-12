#include <stdlib.h>
#include <stdio.h>
#include <elm.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <utils.h>

//Implementazione del thread per la seriale
void* Thread_Seriale(void* args){
  struct argsThreadStruct *argsT=(struct argsThreadStruct *)args;
  while(waitSigCont() == 0 && argsT->comandoSeriale != EXIT_ML){
    printf("comado seriale: %u\n",argsT->comandoSeriale);
    switch(argsT->comandoSeriale){
    case REQUEST_SERIAL:
      
      break;
    }
  }
  //Attesa per la selezione della seriale
  return NULL;
}

const char** ScanPorts(){
  //Codice per trovare tutte le seriali connesse, faccio la ricerca tramite il filesystem virtuale sys
  //Prima ottengo il numero delle possibili seriali
  DIR *dir_sys_usb=opendir("/sys/bus/usb/drivers/usb");
  struct dirent* subdir=readdir(dir_sys_usb);
  unsigned nSerials=0;
  while(subdir){
    if(subdir->d_type == DT_LNK){
      char path[PATH_MAX]="/sys/bus/usb/drivers/usb/";
      
      //File per ottenere la classe del dispositivo
      strcat(path,subdir->d_name);
      strcat(path,"/bDeviceClass");
      // printf("%s\n",path);
      FILE *devClass=fopen(path,"r");
      if(devClass){
        unsigned BaseClass=0xFF;
        if(fscanf(devClass,"%x",&BaseClass) == 1){
          //printf("%x\n",BaseClass);
          //Se è un dispositivo legato alla comunicazione e CDC esploro le sue interfaccie per trovare quella con il terminale tty
          if(BaseClass == 2){
            ++nSerials;
          }
        }
        fclose(devClass);
      }
    }
    subdir=readdir(dir_sys_usb);
  }
  closedir(dir_sys_usb);
  
  //Ora cerco per i tty
  char** ports=(char**)calloc(nSerials+1,sizeof(char*));
  unsigned i=0;
  dir_sys_usb=opendir("/sys/bus/usb/drivers/usb");
  subdir=readdir(dir_sys_usb);
  while(subdir){
    if(subdir->d_type == DT_LNK){
      char path[PATH_MAX]="/sys/bus/usb/drivers/usb/";
      
      //File per ottenere la classe del dispositivo
      strcat(path,subdir->d_name);
      strcat(path,"/bDeviceClass");
      // printf("%s\n",path);
      FILE *devClass=fopen(path,"r");
      if(devClass){
        unsigned BaseClass=0xFF;
        if(fscanf(devClass,"%x",&BaseClass) == 1){
          //printf("%x\n",BaseClass);
          //Se è un dispositivo legato alla comunicazione e CDC esploro le sue interfaccie per trovare quella con il terminale tty
          if(BaseClass == 2){
            strcpy(path,"/sys/bus/usb/drivers/usb/");
            strcat(path,subdir->d_name);
            DIR *dir_subdir=opendir(path);
            
            struct dirent* interface=readdir(dir_subdir);
            while(interface){
              strcpy(path,"/sys/bus/usb/devices/");
              if(strstr(interface->d_name,subdir->d_name)){
                
                //Devo aprire le cartelle delle interfaccie per vedere il nome del file tty in dev
                strcat(path,interface->d_name);
                strcat(path,"/tty");
                //printf("\t\t%s\n",path);
                DIR *dir_interface=opendir(path);
                
                //Se è presente il protocollo tty
                if(dir_interface){
                  struct dirent* tty=readdir(dir_interface);
                  while(tty){
                    if(strstr(tty->d_name,"tty")){
                      char* porta=(char*)calloc(strlen(tty->d_name)+6,sizeof(char));
                      sprintf(porta,"/dev/%s",tty->d_name);
                      ports[i]=porta;
                      ++i;
                    }
                    tty=readdir(dir_interface);
                  }
                  closedir(dir_interface);
                }
              }
              interface=readdir(dir_subdir);
            }
            closedir(dir_subdir);
          }
        }
        fclose(devClass);
      }
    }
    subdir=readdir(dir_sys_usb);
  }
  closedir(dir_sys_usb);
  return (const char**)ports;
}
