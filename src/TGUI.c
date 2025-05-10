#include <stdlib.h>
#include <GLES3/gl32.h>
#include <elm.h>
#include <guifh.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <utils.h>

#define TOLL_DOUBLE 1e-4
#define FPS 4.0

//Oggetto che rappresenta il display
struct _DisplayOscilloscopio{
  uint16_t width,height;
  uint8_t hLines,vLines;
  unsigned gain;
  double T_Window,dt,T;
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
  
  gctx.osc=(DisplayOsc*)calloc(1,sizeof(DisplayOsc));
  gctx.osc->width=900;
  gctx.osc->height=500;
  gctx.osc->hLines=7;
  gctx.osc->vLines=7;
  gctx.osc->gain=10.0;
  gctx.osc->dt=1e-2;
  gctx.osc->T_Window=3.0;
  gctx.osc->T=0.0;
  
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

  if(gtk_toggle_button_get_active(self)){
    //Se sono passato da Start a Stop
    gtk_button_set_label(GTK_BUTTON(self),"Stop");
    
    //Creo il buffer per la condivisione dei segnali e di GL
    double dimBuffer=ceil((1.0/gctx.osc->dt)/FPS);
    *bufferSignals=(float*)calloc((int)dimBuffer,sizeof(float));
    
    //Creo e faccio partire il timer per il campionamento e per la GUI
    timer_create(CLOCK_REALTIME, NULL, &(gctx.idTimerCamp));
    
    struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
    sigaction(SIGALRM, &newAct, NULL);
    
    long interval_ns=(long)floor( (gctx.osc->dt*1e9) );    
    struct itimerspec ts={.it_interval={.tv_sec=0L, .tv_nsec=interval_ns}, .it_value={.tv_sec=0L, .tv_nsec=interval_ns}};
    
    timer_settime(gctx.idTimerCamp, 0, &ts, NULL);
    gctx.draw=1;
  }else{
    //Se sono passato da Stop a Start
    gctx.draw=0;
    gtk_button_set_label(GTK_BUTTON(self),"Start");
    timer_delete(gctx.idTimerCamp);
    free(*bufferSignals);
  }
}

void alrmHandler(int seg){
  static double interval_GUI=0.0;
  
  //Ad ogni SIGALARM mando un messaggio evento al mainloop per segnalare il thread della seriale
  signalMainLoop(gctx.argsT->mlc,REQUEST_SERIAL,NULL);

  interval_GUI += gctx.osc->dt;
  if(fabs(interval_GUI - gctx.interval_GUI) <= TOLL_DOUBLE ){
    //Richiesta di disegno al display per aggiornare
    gtk_gl_area_queue_render(gctx.Display);
    interval_GUI=0.0;
    signalMainLoop(gctx.argsT->mlc,RENDER_GL,NULL);
  }
}

static gboolean render(GtkGLArea *area, GdkGLContext *context) {    
    //Se sono nella fase di disegno inizio a scrivere il buffer
    glClear(GL_COLOR_BUFFER_BIT);
    if(gctx.draw){
      double nElementi=ceil((1.0/gctx.osc->dt)/FPS);
      double k=floor(gctx.osc->T/gctx.osc->dt);
      double incremento_tempo=gctx.osc->dt*nElementi;
      int k_size=(GLsizei)(k+nElementi);
      g_print("%lf\n%lf\n%d\n",incremento_tempo,k,k_size);

      //Scrorrimento della finestra del display
      if(gctx.osc->T + incremento_tempo > gctx.osc->T_Window){
        double endElement=floor(gctx.osc->T/gctx.osc->dt);
        float off_Copy= floor(endElement*(2.0/3.0))*sizeof(GLfloat);
        float size_Copy=ceil(endElement*(1.0/3.0))*sizeof(GLfloat);
        glCopyBufferSubData(GL_ARRAY_BUFFER,GL_ARRAY_BUFFER,off_Copy,0,size_Copy);
        
        //Modifica del cursore e pulizia display
        gctx.osc->T = floor(gctx.osc->T_Window*(1.0/3.0));
        k=floor(gctx.osc->T/gctx.osc->dt);
        k_size=(GLsizei)(k+nElementi);
        glFinish();
      }
      
      const unsigned nElementi_u=(unsigned)nElementi;
      GLfloat tGL[nElementi_u];
      for(unsigned j=0; j<nElementi_u; ++j){
        tGL[j]=-1.0+((gctx.osc->T + ((double)j)*gctx.osc->dt)/gctx.osc->T_Window)*2.0;
      }
      
      glBufferSubData(GL_ARRAY_BUFFER,k*sizeof(GLfloat),nElementi_u*sizeof(GLfloat),gctx.argsT->bufferSignals);
      glBindBuffer(GL_ARRAY_BUFFER,1);
      glBufferSubData(GL_ARRAY_BUFFER,k*sizeof(GLfloat),nElementi_u*sizeof(GLfloat),&tGL);
      glBindBuffer(GL_ARRAY_BUFFER,2);
      
      glDrawArrays(GL_POINTS,0,k_size);
      
      gctx.osc->T += gctx.osc->dt*nElementi;
    }
    return TRUE;
}

static void on_realize (GtkGLArea *area){
  // We need to make the context current if we want to
  // call GL API
  gtk_gl_area_make_current(area);
  if(gtk_gl_area_get_error(area) != NULL) return;
  
  g_print("%s\n",glGetString(GL_VERSION));
  
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

