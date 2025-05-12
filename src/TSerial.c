#include <stdlib.h>
#include <stdio.h>
#include <elm.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>

//Implementazione del thread per la seriale
void* Thread_Seriale(void* args){
  //struct _mlcontext *MLC=(struct _mlcontext *)args;
  
  //Codice per trovare tutte le seriali connesse, faccio la ricerca tramite il filesystem virtuale sys
  DIR *dir_sys_usb=opendir("/sys/bus/usb/drivers/usb");
  struct dirent* subdir=readdir(dir_sys_usb);
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
                      printf("file tty del device: /dev/%s\n", tty->d_name);
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
  //Attesa per la selezione della seriale
  return NULL;
}
