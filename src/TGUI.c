#include <stdlib.h>
#include <GLES3/gl32.h>
#include <elm.h>
#include <guifh.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <utils.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

#define FPS 25.0

typedef GLfloat sampleTypeGL;

//Oggetto che rappresenta il display
struct _DisplayOscilloscopio{
  uint16_t width,height;    //Dimensioni display
  uint8_t hLines,vLines;    //Numero di linee della griglia
  double T_Window,dt,T;     //Dimensione finestra, intervallo di campionamento, cursore nel tempo
  unsigned N,k_camp,k;      //Cursore indice intero, elementi in totale
  unsigned shiftFinestra;   //Divisore per lo scorrimento della finestra
};

struct GUI_Context{
  struct argsThreadStruct *argsT;
  GtkGLArea* Display;
  DisplayOsc* osc;
  timer_t idTimerCamp;
  uint8_t draw;
  double interval_GUI;
  GLuint bobjs[2];
}gctx;

void* Thread_GUI(void* args){
  gctx.argsT=(struct argsThreadStruct *)args;
  gctx.draw=0;
  gctx.interval_GUI=1.0/FPS;
  gctx.argsT->gainOsc=1.0f/1300.0f;
  
  //Installazione dell'azione per SIGALRM
  struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
  sigaction(SIGALRM, &newAct, NULL);
  
  //Creazione e start dell'applicazione
  GtkApplication *app=gtk_application_new("gtk.Oscilloscopio", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(G_APPLICATION(app), "activate", G_CALLBACK(InitApp), args);
  int status=g_application_run(G_APPLICATION(app), 0, NULL);
  if(status == 0){
    g_print("Applicazione terminata correttamente\n");
  }else{
    g_print("Applicazione terminata con un errore\n");
  }
  
  //Segnalazione al Main-Loop di uscire dal ciclo
  signalMainLoop(gctx.argsT->mlc,EXIT_ML,NULL);
  return NULL;
}

static void InitApp(GtkApplication *self, gpointer user_data){
  //Ottengo i riferimenti agli oggetti nella finestra
  GtkBuilder *builder=gtk_builder_new_from_file("../GUI/WinUI.ui");
  GtkWindow *MainWin=GTK_WINDOW(gtk_builder_get_object(builder, "MainWin"));
  gctx.Display=GTK_GL_AREA(gtk_builder_get_object(builder, "DisplayArea"));
  GtkToggleButton *ButtonSE=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "SE"));
  GtkDropDown* PortSelect=GTK_DROP_DOWN(gtk_builder_get_object(builder,"PortSelect"));
  
  //Impostazione dell'oscilloscopio
  gctx.osc=(DisplayOsc*)calloc(1,sizeof(DisplayOsc));
  gctx.osc->width=900;
  gctx.osc->height=500;
  gctx.osc->hLines=7;
  gctx.osc->vLines=7;
  gctx.osc->T_Window=5.0;
  gctx.osc->shiftFinestra=5;
  gctx.osc->dt=2e-4;
  gctx.osc->N=(unsigned)floor(gctx.osc->T_Window/gctx.osc->dt);
  gctx.argsT->nElementi=(unsigned)floor(1.0/(FPS*gctx.osc->dt));
  
  //Popolazione del drop down con le porte trovate
  signalMainLoop(gctx.argsT->mlc,SCAN_SERIAL,NULL);
  waitSigCont();
  
  GtkStringList *portList=GTK_STRING_LIST(gtk_drop_down_get_model(PortSelect));
  unsigned k=0;
  while(gctx.argsT->porte[k]){
    gtk_string_list_append(portList,gctx.argsT->porte[k]);
    ++k;
  }
  gtk_drop_down_set_model(PortSelect,G_LIST_MODEL(portList));
  
  //Bind delle funzioni ai widget
  g_signal_connect(gctx.Display,"render",G_CALLBACK(render),NULL);
  g_signal_connect(gctx.Display,"realize",G_CALLBACK(on_realize),NULL);
  g_signal_connect(ButtonSE,"toggled",G_CALLBACK(SEToggle),NULL);
  g_signal_connect(MainWin,"close-request",G_CALLBACK(CloseRequestWindow),NULL);
  
  //Aggiunta della finestra principale all'applicazione
  gtk_application_add_window(self,MainWin);
  gtk_window_present(MainWin);
}

static void SEToggle(GtkToggleButton *self, gpointer user_data){
  sampleType **bufferSignals=&(gctx.argsT->bufferSignals);

  if(gtk_toggle_button_get_active(self) && gctx.draw==0){
    //Apertura del file per la comunicazione
    GtkDropDown* PortSelect=GTK_DROP_DOWN(gtk_grid_get_child_at(GTK_GRID(gtk_widget_get_parent(GTK_WIDGET(self))),2,0));
    GtkStringList* PortList=GTK_STRING_LIST(gtk_drop_down_get_model(PortSelect));
    
    //Allocazione per il parametro del comando
    const char* fileDev=gtk_string_list_get_string(PortList,gtk_drop_down_get_selected(PortSelect));
    char *filePar=(char*)calloc(strlen(fileDev)+1,sizeof(char));
    int *retValue=(int*)malloc(sizeof(int));
    memcpy(filePar,fileDev,strlen(fileDev)*sizeof(char));
    
    //Faccio eseguire il comando al ML per non far bloccare per troppo tempo la GUI
    signalMainLoopReply(gctx.argsT->mlc,OPEN_SERIAL,filePar,retValue);
    
    if(*retValue == -1){
      g_print("Seriale non aperta\n");
      gtk_toggle_button_set_active(self,0);
    }else{
      //Codice per l'avviamento del campionamento
      int serial_fd=*retValue;
      ssize_t bytesRead=0,bytesWrite=0,bytesRead_tmp=0;
      char buffer[10]={};
      char risposta='S';
      
      tcflush(serial_fd,TCIOFLUSH);
      while(bytesRead < strlen("Start\n")){
        bytesRead_tmp = read(serial_fd,buffer+bytesRead,10-bytesRead);
        if(bytesRead_tmp == -1){
          fprintf(stderr, "Error from read: %s\n", strerror(errno));
          break;
        }else bytesRead += bytesRead_tmp;
      }
      
      //Se non ci sono stati errori e il comando è esatto
      if(bytesRead_tmp > -1 && strcmp(buffer,"Start\n") == 0){ 
        //Risposta per far continuare l'Arduino
        bytesWrite = write(serial_fd,&risposta,sizeof(char));
        if(bytesWrite == -1){
          fprintf(stderr, "Error from write: %s\n", strerror(errno));
        }else{
          tcdrain(serial_fd);
        }
        bytesRead=0;
        risposta='K';
        bytesRead += read(serial_fd,&risposta,1);
        if(bytesRead == -1){
          fprintf(stderr, "Error from read: %s\n", strerror(errno));
        }else{
          g_print("%c\n",risposta);
          //Leggo la frequenza di campionamento dell'Arduino
          unsigned long freqTimer=0;
          void* addrFreqTimer=(void*)&freqTimer;
          bytesRead=0;
          ssize_t bytesRead_tmp=1;
          while(bytesRead_tmp > 0 && bytesRead < sizeof(unsigned long)){
            bytesRead_tmp = read(serial_fd,addrFreqTimer+bytesRead,sizeof(unsigned long)-bytesRead);
            bytesRead += bytesRead_tmp;
          }
          bytesRead=0;
          double freqTimerDouble=(double)freqTimer;
          g_print("Frequenza timer: %lu\nIntervallo timer: %le\n",freqTimer,1.0/freqTimerDouble);
          gctx.osc->dt = 1.0/freqTimerDouble;
          gtk_button_set_label(GTK_BUTTON(self),"Stop");
          
          //Creo il buffer per la condivisione dei segnali e di GL
          gctx.osc->k=0;
          gctx.osc->T=0.0;
          gctx.osc->k_camp=0;
          g_print("N: %u\nnElementi: %u\n", gctx.osc->N, gctx.argsT->nElementi);
          *bufferSignals=(sampleType*)calloc(gctx.argsT->nElementi,sizeof(sampleType));
          signalMainLoop(gctx.argsT->mlc,START_RENDER,NULL);
          
          //Creo il timer per il campionamento e per la GUI
          timer_create(CLOCK_REALTIME, NULL, &(gctx.idTimerCamp));    
          long interval_ns=(long)floor( (gctx.interval_GUI*1e9) );    
          struct itimerspec ts={.it_interval={.tv_sec=0L, .tv_nsec=interval_ns}, .it_value={.tv_sec=0L, .tv_nsec=interval_ns}};
          
          //Invio della conferma per far continuare l'Arduino
          risposta='C';
          bytesWrite=write(serial_fd,&risposta,sizeof(char));
          if(bytesWrite == -1){
            fprintf(stderr, "Error from write: %s\n", strerror(errno));
          }else{
            tcdrain(serial_fd);
          }
          
          //Faccio partire il timer dell'applicazione
          timer_settime(gctx.idTimerCamp, 0, &ts, NULL);
          gctx.draw=1;      
          gctx.argsT->fdSeriale=serial_fd;
        }
      }
    
      free(filePar);
      free(retValue);
    }
  }else if(gctx.draw){
    //Se sono passato da Stop a Start
    timer_delete(gctx.idTimerCamp);
    gctx.draw=0;
    
    //Segnale per interrompere l'Arduino
    char risposta='C';
    ssize_t bytesWrite=write(gctx.argsT->fdSeriale,&risposta,sizeof(char));
    if(bytesWrite == -1){
      fprintf(stderr, "Error from write: %s\n", strerror(errno));
    }else{
      tcdrain(gctx.argsT->fdSeriale);
    }
    
    //Segnalazione al mainloop, l'Arduino ha il tempo di terminare
    signalMainLoop(gctx.argsT->mlc,END_RENDER,NULL);
    gtk_button_set_label(GTK_BUTTON(self),"Start");
    free(*bufferSignals);
    *bufferSignals=NULL;
    
    //Arrivo al flush con l'Arduino che ha termonato e non trasmette più dati
    tcflush(gctx.argsT->fdSeriale,TCIOFLUSH);
    close(gctx.argsT->fdSeriale);
  }
}

void alrmHandler(int seg){
  //Ad ogni SIGALARM mando un messaggio evento al mainloop per segnalare il rendering
  signalMainLoop(gctx.argsT->mlc,RENDER_GL,NULL);
  gtk_gl_area_queue_render(gctx.Display);
}

static gboolean render(GtkGLArea *area, GdkGLContext *context) {    
    //Se sono nella fase di disegno inizio a scrivere il buffer
    glClear(GL_COLOR_BUFFER_BIT);
    if(gctx.draw){
      unsigned nElementi=gctx.argsT->nElementiLetti;
      unsigned k_size=gctx.osc->k+nElementi;
      //double interval_GUI_now=nElementi*gctx.osc->dt;
      double interval_GUI_now=gctx.interval_GUI;
      //g_print("k_size: %u\nk: %u\n",k_size,gctx.osc->k);

      //Scrorrimento della finestra del display
      if(gctx.osc->T+interval_GUI_now > gctx.osc->T_Window){
        // unsigned endElement=floor(gctx.osc->T/gctx.osc->dt);
        unsigned off_Copy= ((gctx.osc->k*(gctx.osc->shiftFinestra-1))/gctx.osc->shiftFinestra)*sizeof(GLfloat);
        unsigned size_Copy= (gctx.osc->k/gctx.osc->shiftFinestra)*sizeof(GLfloat);
        glCopyBufferSubData(GL_ARRAY_BUFFER,GL_ARRAY_BUFFER,off_Copy,0,size_Copy);
        
        //Modifica del cursore e pulizia display
        gctx.osc->T /= (double)gctx.osc->shiftFinestra;
        gctx.osc->k /= gctx.osc->shiftFinestra;
        k_size=(GLsizei)(gctx.osc->k+nElementi);
        glFinish();
      }
      
      GLfloat tGL[nElementi];
      double t=gctx.osc->T;
      for(unsigned j=0; j<nElementi; ++j){
        tGL[j]=-1.0+(t/gctx.osc->T_Window)*2.0;
        t += gctx.osc->dt;
      }
      
      glBufferSubData(GL_ARRAY_BUFFER,gctx.osc->k*sizeof(GLfloat),nElementi*sizeof(GLfloat),gctx.argsT->bufferSignals);
      glBindBuffer(GL_ARRAY_BUFFER,1);
      glBufferSubData(GL_ARRAY_BUFFER,gctx.osc->k*sizeof(GLfloat),nElementi*sizeof(GLfloat),&tGL);
      glBindBuffer(GL_ARRAY_BUFFER,2);
      
      glDrawArrays(GL_POINTS,0,k_size);
      
      gctx.osc->k += nElementi;
      gctx.osc->T += interval_GUI_now;
    }
    return TRUE;
}

static void on_realize (GtkGLArea *area){
  // We need to make the context current if we want to
  // call GL API
  gtk_gl_area_make_current(area);
  if(gtk_gl_area_get_error(area) != NULL) return;
  
  g_print("%s\n",glGetString(GL_VERSION));
  //unsigned nElementi=(unsigned)floor(gctx.interval_GUI/gctx.osc->dt);
  //g_print("%u\n%lf\n%lf\n",nElementi,gctx.interval_GUI/gctx.osc->dt,gctx.interval_GUI);
  
  /*
  1.  Creazione degli shaders
  2.  Creazione del programma
  3.  Installazione del programma
  4.  Creazione del VBO per i vertici
  5.  Installazione del VBO nel contesto corrente
  */
  GLuint vertexShader=glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader=glCreateShader(GL_FRAGMENT_SHADER);
  
  const char *vertexShaderSource = 
  "#version 320 es\n"
  "layout (location = 0) in float t;\n"
  "layout (location = 1) in float signal;\n"
  "void main()\n"
  "{\n"
  "   gl_Position = vec4(t, signal, 0.0, 1.0);\n"
  "   gl_PointSize = 2.0;\n"
  "}\n";
  glShaderSource(vertexShader,1,&vertexShaderSource,NULL);
  glCompileShader(vertexShader);
  
  //Verifica della compilazione
  GLint status;
  glGetShaderiv(vertexShader,GL_COMPILE_STATUS,&status);
  if(status == GL_FALSE){
    //Se la compilazione è fallita allora prendo i log
    GLint infoLength;
    glGetShaderiv(vertexShader,GL_INFO_LOG_LENGTH,&infoLength);
    GLchar* shaderLog=(GLchar*)malloc((infoLength+10)*sizeof(GLchar));
    glGetShaderInfoLog(vertexShader,infoLength,&infoLength,shaderLog);
    g_print("%s\n",shaderLog);
    free(shaderLog);
    return;
  }
  
  const char* fragmentShaderSource =
  "#version 320 es\n"
  "out highp vec4 FragColor;\n"
  "void main()\n"
  "{\n"
  "    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
  "}\n";
  glShaderSource(fragmentShader,1,&fragmentShaderSource,NULL);
  glCompileShader(fragmentShader);
  
  //Verifica della compilazione
  glGetShaderiv(fragmentShader,GL_COMPILE_STATUS,&status);
  if(status == GL_FALSE){
    //Se la compilazione è fallita allora prendo i log
    GLint infoLength;
    glGetShaderiv(fragmentShader,GL_INFO_LOG_LENGTH,&infoLength);
    GLchar* shaderLog=(GLchar*)malloc((infoLength+10)*sizeof(GLchar));
    glGetShaderInfoLog(fragmentShader,infoLength,&infoLength,shaderLog);
    g_print("%s\n",shaderLog);
    free(shaderLog);
    return;
  }
  
  //Se la compilazione avviene con successo creo il programma
  GLuint prog=glCreateProgram();
  glAttachShader(prog,vertexShader);
  glAttachShader(prog,fragmentShader);
  
  //Fase di linking
  glLinkProgram(prog);
  //Verifica del linking
  glGetProgramiv(prog,GL_LINK_STATUS,&status);
  if(status == GL_FALSE){
    //Se il linking è fallito allora prendo i log
    GLint infoLength;
    glGetProgramiv(prog,GL_INFO_LOG_LENGTH,&infoLength);
    GLchar* progLog=(GLchar*)malloc((infoLength+10)*sizeof(GLchar));
    glGetProgramInfoLog(prog,infoLength,&infoLength,progLog);
    g_print("%s\n",progLog);
    free(progLog);
    return;
  }
  glUseProgram(prog);
  
  //Rilascio delle risorse
  glReleaseShaderCompiler();
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glDeleteProgram(prog);
  
  //Creazione dei buffer degli shaders, tempo e segnale
  glGenBuffers(2,gctx.bobjs);
  
  glBindBuffer(GL_ARRAY_BUFFER,gctx.bobjs[0]);
  glBufferData(GL_ARRAY_BUFFER,gctx.osc->N*sizeof(GLfloat),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,1,GL_FLOAT,GL_FALSE,0,NULL);
  glEnableVertexAttribArray(0);
  
  glBindBuffer(GL_ARRAY_BUFFER,gctx.bobjs[1]);
  glBufferData(GL_ARRAY_BUFFER,gctx.osc->N*sizeof(sampleTypeGL),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,0,NULL);
  glEnableVertexAttribArray(1);
  
  glClearColor(0.0, 0.0, 0.0, 1.0);
}

static gboolean CloseRequestWindow (GtkWindow* self, gpointer user_data){
  sampleType **bufferSignals=&(gctx.argsT->bufferSignals);
  
  //Deallocazione delle risorse
  glDeleteBuffers(2,gctx.bobjs);
  
  //Se chiudo la finestra mentre sto ancora disegnando
  if(gctx.draw){
    timer_delete(gctx.idTimerCamp);
    gctx.draw=0;
    
    //Segnale per interrompere l'Arduino
    char risposta='C';
    ssize_t bytesWrite=write(gctx.argsT->fdSeriale,&risposta,sizeof(char));
    if(bytesWrite == -1){
      fprintf(stderr, "Error from write: %s\n", strerror(errno));
    }else{
      tcdrain(gctx.argsT->fdSeriale);
    }
    
    //Segnalazione al mainloop, l'Arduino ha il tempo di terminare
    signalMainLoop(gctx.argsT->mlc,END_RENDER,NULL);
    free(*bufferSignals);
    *bufferSignals=NULL;
    
    //Arrivo al flush con l'Arduino che ha termonato e non trasmette più dati
    tcflush(gctx.argsT->fdSeriale,TCIOFLUSH);
    close(gctx.argsT->fdSeriale);
  }
  return FALSE;
}
