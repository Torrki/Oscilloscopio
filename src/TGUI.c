#include <stdlib.h>
#include <GLES3/gl32.h>
#include <elm.h>
#include <guifh.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <utils.h>

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
}gctx;

static gboolean render (GtkGLArea *area, GdkGLContext *context);
static void on_realize (GtkGLArea *area);

void* Thread_GUI(void* args){
  gctx.argsT=(struct argsThreadStruct *)args;
  gctx.draw=0;
  
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
  gctx.osc->T_Window=4.0;
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
    *bufferSignals=(void*)calloc(10,sizeof(float));
    
    //Creo e faccio partire il timer per il campionamento
    timer_create(CLOCK_REALTIME, NULL, &(gctx.idTimerCamp));
    struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
    sigaction(SIGALRM, &newAct, NULL);
    long interval_ns=(long)(gctx.osc->dt*1e9);
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
  //Ad ogni SIGALARM mando un messaggio evento al mainloop per segnalare il thread della seriale
  signalMainLoop(gctx.argsT->mlc,REQUEST_SERIAL,NULL);
  //Richiesta di disegno al display per aggiornare
  gtk_widget_queue_draw(GTK_WIDGET(gctx.Display)); 
}

static gboolean render(GtkGLArea *area, GdkGLContext *context) {
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    //Se sono nella fase di disegno inizio a scrivere il buffer
    if(gctx.draw){
      /*float w=2.0*G_PI*1.0;
      float d=0.25f*sinf(w*gctx.osc->T);*/
      float k=floorf(gctx.osc->T/gctx.osc->dt);
      GLfloat tGL=-1.0+(gctx.osc->T/gctx.osc->T_Window)*2.0;
      GLfloat d=gctx.argsT->bufferSignals[0];
      GLfloat newData[3]={tGL,d,0.0f};
      glBufferSubData(GL_ARRAY_BUFFER,k*3*sizeof(GLfloat),3*sizeof(GLfloat),newData);
      
      glDrawArrays(GL_LINE_STRIP,0,k);
      
      gctx.osc->T += gctx.osc->dt;
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
  "layout (location = 0) in vec3 aPos;\n"
  "void main()\n"
  "{\n"
  "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
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
  
  //Creazione del buffer degli shaders  
  GLuint boVertex;
  glGenBuffers(1,&boVertex);
  glBindBuffer(GL_ARRAY_BUFFER,boVertex);
  unsigned N=floorf(gctx.osc->T_Window/gctx.osc->dt);
  glBufferData(GL_ARRAY_BUFFER,N*3*sizeof(GLfloat),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,NULL);
  glEnableVertexAttribArray(0);
}

