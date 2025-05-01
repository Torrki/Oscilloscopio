#include <stdlib.h>
#include <GL/gl.h>
#include <elm.h>
#include <guifh.h>
#include <cairo/cairo.h>
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
  GtkDrawingArea* Display;
  DisplayOsc* osc;
  timer_t idTimerCamp;
  uint8_t draw;
}gctx;

void* Thread_GUI(void* args){
  gctx.argsT=(struct argsThreadStruct *)args;
  gctx.draw=0;
  
  //Creazione e start dell'applicazione
  GtkApplication *app=gtk_application_new("GTK.Oscilloscopio", G_APPLICATION_DEFAULT_FLAGS);
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
  gctx.Display=GTK_DRAWING_AREA(gtk_builder_get_object(builder, "DisplayArea"));
  GtkToggleButton *ButtonSE=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "SE"));
  
  gctx.osc=(DisplayOsc*)calloc(1,sizeof(DisplayOsc));
  gctx.osc->width=gtk_drawing_area_get_content_width(gctx.Display);
  gctx.osc->height=gtk_drawing_area_get_content_height(gctx.Display);
  gctx.osc->hLines=7;
  gctx.osc->vLines=7;
  gctx.osc->gain=10.0;
  gctx.osc->dt=1e-3;
  gctx.osc->T_Window=2.0;
  gctx.osc->T=0.0;
  
  //Bind delle funzioni ai widget
  gtk_drawing_area_set_draw_func(gctx.Display,DisplayDraw,NULL,NULL);
  g_signal_connect(ButtonSE,"toggled",G_CALLBACK(SEToggle),NULL);
  //g_print("%p\n", &(gctx.argsT->bufferSignals));
  
  //Aggiunta della finestra principale all'applicazione
  gtk_application_add_window(self,MainWin);
  gtk_window_present(MainWin);
}

static void DisplayDraw(GtkDrawingArea *area, cairo_t *cr,int width,int height,gpointer data){
  cairo_set_source_rgb(cr,0.0,0.0,0.0);
  cairo_paint (cr);
  DisplayOsc* osc=gctx.osc;
  
  //Disegno della griglia
  uint8_t NLines=osc->hLines;
  double dh=osc->height/(double)(NLines+1);
  for(int i=1; i<=NLines; ++i){
    double h=dh *(double)i;
    cairo_move_to(cr,0.0,h);
    cairo_line_to(cr,osc->width,h);
  }
  double dx=osc->width/(double)(NLines+1);
  for(int i=1; i<=NLines; ++i){
    double x=dx *(double)i;
    cairo_move_to(cr,x,0.0);
    cairo_line_to(cr,x,osc->height);
  }
  cairo_set_source_rgba(cr,1.0,1.0,1.0,0.7);
  const double dLength[2]={6.0, 6.0};
  cairo_set_dash(cr,dLength,1,0.0);
  cairo_set_line_width(cr,1.0);
  cairo_stroke(cr);
  
  //Se il pulsante è passato da Start a Stop disegno la curva
  cairo_set_dash(cr,dLength,0,0.0);
  if(gctx.draw){
    cairo_set_source_rgba(cr,1.0,0.0,0.0,1.0);
    cairo_set_line_width(cr,2.0);
    float* buffer=(float*)(gctx.argsT->bufferSignals);
    
    int N=floor(osc->T/osc->dt);    
    //Primo dato
    double x=(osc->T/osc->T_Window)*osc->width;
    double y=*buffer;
    cairo_move_to(cr,x,y);
    cairo_line_to(cr,x+20.0,y);
        
    g_print("%f\n",*buffer);
    cairo_stroke(cr);
  }
}

static void SEToggle(GtkToggleButton *self, gpointer user_data){
  static int x=0;
  g_print("%d\n",++x);
  void **bufferSignals=(void**)(&(gctx.argsT->bufferSignals));
  
  //g_print("%p\n",bufferSignals);
  if(gtk_toggle_button_get_active(self)){
    //Se sono passato da Start a Stop
    gtk_button_set_label(GTK_BUTTON(self),"Stop");
    
    //Creo il buffer per la condivisione dei segnali
    *bufferSignals=(void*)calloc(10,sizeof(float));
    
    //Creo e faccio partire il timer per il campionamento
    timer_create(CLOCK_REALTIME, NULL, &(gctx.idTimerCamp));
    struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
    sigaction(SIGALRM, &newAct, NULL);
    struct itimerspec ts={.it_interval={.tv_sec=0L, .tv_nsec=10000000L}, .it_value={.tv_sec=0L, .tv_nsec=10000000L}};
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
