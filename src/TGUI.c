#include <stdlib.h>
#include <GLES3/gl32.h>
#include <elm.h>
#include <guifh.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <utils.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

#define FPS 10.0

//Oggetto che rappresenta il display
struct _DisplayOscilloscopio{
  uint16_t width,height;    //Dimensioni display
  uint8_t hLines,vLines;    //Numero di linee della griglia
  unsigned gain;            //Guadagno del display
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
}gctx;

static gboolean render (GtkGLArea *area, GdkGLContext *context);
static void on_realize (GtkGLArea *area);

void* Thread_GUI(void* args){
  gctx.argsT=(struct argsThreadStruct *)args;
  gctx.draw=0;
  gctx.interval_GUI=1.0/FPS;
  
  //Installazione dell'azione per SIGALRM
  struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
  sigaction(SIGALRM, &newAct, NULL);
  
  //Creazione e start dell'applicazione
  GtkApplication *app=gtk_application_new("gtk.Oscilloscopio", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(G_APPLICATION(app), "activate", G_CALLBACK(InitApp), args);
  int status=g_application_run(G_APPLICATION(app), 0, NULL);
  g_print("exit status: %d\n",status);
  
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
  gctx.osc->gain=10.0;
  gctx.osc->dt=5e-3;
  gctx.osc->T_Window=3.0;
  gctx.osc->N=(unsigned)floor(gctx.osc->T_Window/gctx.osc->dt);
  gctx.osc->shiftFinestra=5;
  
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
  
  //Aggiunta della finestra principale all'applicazione
  gtk_application_add_window(self,MainWin);
  gtk_window_present(MainWin);
}

static void SEToggle(GtkToggleButton *self, gpointer user_data){
  float **bufferSignals=&(gctx.argsT->bufferSignals);

  if(gtk_toggle_button_get_active(self) && gctx.draw==0){    
    //Apertura del file per la comunicazione
    GtkDropDown* PortSelect=GTK_DROP_DOWN(gtk_grid_get_child_at(GTK_GRID(gtk_widget_get_parent(GTK_WIDGET(self))),2,0));
    GtkStringList* PortList=GTK_STRING_LIST(gtk_drop_down_get_model(PortSelect));
    const char* fileDev=gtk_string_list_get_string(PortList,gtk_drop_down_get_selected(PortSelect));
    //g_print("%s\n", fileDev);
    
    //Impostazione per il file tty
    int serial_fd = open(fileDev, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port: %s\n", strerror(errno));
        gtk_toggle_button_set_active(self,0);
    }else{
      struct termios tty;
      memset(&tty, 0, sizeof tty);

      if (tcgetattr(serial_fd, &tty) != 0) {
          fprintf(stderr, "Error from tcgetattr: %s\n", strerror(errno));
          close(serial_fd);
          gtk_toggle_button_set_active(self,0);
      }else{
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);

        // Configure settings: 8N1 mode (8 data bits, no parity, 1 stop bit)
        tty.c_cflag &= ~PARENB; // No parity
        tty.c_cflag &= ~CSTOPB; // One stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;     // 8 data bits
        tty.c_cflag &= ~CRTSCTS; // No hardware flow control
        tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
        tty.c_oflag &= ~OPOST; // Raw output

        // Set blocking read with a 1-second timeout
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10; // 1 second timeout

        if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
            fprintf(stderr, "Error from tcsetattr: %s\n", strerror(errno));
            close(serial_fd);
            gtk_toggle_button_set_active(self,0);
        }else{          
          //Flush del contenuto, sono sicuro che Start non viene toccato dato che Arduino sta aspettando il segnale
          tcflush(serial_fd,TCIOFLUSH);
          
          ssize_t bytesRead=0,bytesWrite=0;
          char buffer[10]={};
          char risposta='S';
          
          while(bytesRead < strlen("Start\n")){
            bytesRead += read(serial_fd,buffer+bytesRead,10-bytesRead);
          }
          //g_print("%s\n",buffer);
          if(strcmp(buffer,"Start\n") == 0){
            //Risposta per far continuare l'Arduino
            bytesWrite=write(serial_fd,&risposta,sizeof(char));
            tcdrain(serial_fd);
            bytesRead=0;
            risposta='K';
            bytesRead += read(serial_fd,&risposta,1);
            //g_print("%c\n",risposta);
            
            gtk_button_set_label(GTK_BUTTON(self),"Stop");
            //Creo il buffer per la condivisione dei segnali e di GL
            gctx.osc->k=0;
            gctx.osc->T=0.0;
            gctx.osc->k_camp=0;
            gctx.argsT->nElementi=(unsigned)floor(1.0/(FPS*gctx.osc->dt));
            *bufferSignals=(float*)calloc(gctx.argsT->nElementi,sizeof(GLfloat));
            signalMainLoop(gctx.argsT->mlc,START_RENDER,NULL);
            
            //Creo il timer per il campionamento e per la GUI
            timer_create(CLOCK_REALTIME, NULL, &(gctx.idTimerCamp));    
            long interval_ns=(long)floor( (gctx.osc->dt*1e9) );    
            struct itimerspec ts={.it_interval={.tv_sec=0L, .tv_nsec=interval_ns}, .it_value={.tv_sec=0L, .tv_nsec=interval_ns}};
            
            //Invio della conferma per far continuare l'Arduino
            risposta='C';
            bytesWrite=write(serial_fd,&risposta,sizeof(char));
            tcdrain(serial_fd);
            
            //Faccio partire il timer dell'applicazione
            timer_settime(gctx.idTimerCamp, 0, &ts, NULL);
            gctx.draw=1;
            gctx.argsT->fdSeriale=serial_fd;
          }else{
            fprintf(stderr, "Comando di start errato\n");
            close(serial_fd);
            gtk_toggle_button_set_active(self,0);
          }
        }
      }
    }
  }else if(gctx.draw==1){
    //Se sono passato da Stop a Start
    timer_delete(gctx.idTimerCamp);
    gctx.draw=0;
    
    //Segnale per interrompere l'Arduino
    char risposta='C';
    ssize_t bytesWrite=write(gctx.argsT->fdSeriale,&risposta,sizeof(char));
    tcdrain(gctx.argsT->fdSeriale);
    
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
  //Ad ogni SIGALARM mando un messaggio evento al mainloop per segnalare il thread della seriale
  signalMainLoop(gctx.argsT->mlc,REQUEST_SERIAL,NULL);
  
  ++gctx.osc->k_camp;
  if(gctx.osc->k_camp % gctx.argsT->nElementi == 0){
    //Richiesta di disegno al display per aggiornare
    signalMainLoop(gctx.argsT->mlc,RENDER_GL,NULL);
    gctx.osc->k_camp=0;
    gtk_gl_area_queue_render(gctx.Display);
  }
}

static gboolean render(GtkGLArea *area, GdkGLContext *context) {    
    //Se sono nella fase di disegno inizio a scrivere il buffer
    glClear(GL_COLOR_BUFFER_BIT);
    if(gctx.draw){
      unsigned nElementi=gctx.argsT->nElementi;
      // unsigned k=floor(gctx.osc->T/gctx.osc->dt);
      unsigned k_size=gctx.osc->k+nElementi;

      //Scrorrimento della finestra del display
      if(gctx.osc->T+gctx.interval_GUI > gctx.osc->T_Window){
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
      gctx.osc->T += gctx.interval_GUI;
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
  
  //Creazione dei buffer degli shaders, tempo e segnale
  GLuint bobjs[2];
  glGenBuffers(2,bobjs);
  unsigned N=floorf(gctx.osc->T_Window/gctx.osc->dt);
  
  glBindBuffer(GL_ARRAY_BUFFER,bobjs[0]);
  glBufferData(GL_ARRAY_BUFFER,N*sizeof(GLfloat),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,1,GL_FLOAT,GL_FALSE,0,NULL);
  glEnableVertexAttribArray(0);
  
  glBindBuffer(GL_ARRAY_BUFFER,bobjs[1]);
  glBufferData(GL_ARRAY_BUFFER,N*sizeof(GLfloat),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,0,NULL);
  glEnableVertexAttribArray(1);
  
  glClearColor(0.0, 0.0, 0.0, 1.0);
}

