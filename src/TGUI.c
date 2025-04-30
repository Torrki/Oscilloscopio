#include <stdlib.h>
#include <GL/gl.h>
#include <elm.h>
#include <guifh.h>
#include <cairo/cairo.h>
#include <math.h>
#include <signal.h>
#include <time.h>

//Oggetto che rappresenta il display
struct _DisplayOscilloscopio{
  uint16_t width,height;
  uint8_t hLines,vLines;
  unsigned gain;
};

MlContext *MLC;
void* Thread_GUI(void* args){
  MLC=(struct _mlcontext *)args;
  
  //Creazione e start dell'applicazione
  GtkApplication *app=gtk_application_new("GTK.Oscilloscopio", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(G_APPLICATION(app), "activate", G_CALLBACK(InitApp), NULL);
  int status=g_application_run(G_APPLICATION(app), 0, NULL);
  g_print("exit status: %d\n",status);
  
  //Segnalazione al Main-Loop di uscire dal ciclo
  signalMainLoop(MLC,EXIT_ML,NULL);
  return NULL;
}

static void InitApp(GtkApplication *self, gpointer user_data){
  //Ottengo i riferimenti agli oggetti nella finestra
  GtkBuilder *builder=gtk_builder_new_from_file("../GUI/WinUI.ui");
  GtkWindow *MainWin=GTK_WINDOW(gtk_builder_get_object(builder, "MainWin"));
  GtkDrawingArea *Display=GTK_DRAWING_AREA(gtk_builder_get_object(builder, "DisplayArea"));
  GtkButton *ButtonSE=GTK_BUTTON(gtk_builder_get_object(builder, "SE"));
  
  DisplayOsc* osc=(DisplayOsc*)calloc(1,sizeof(DisplayOsc));
  osc->width=gtk_drawing_area_get_content_width(Display);
  osc->height=gtk_drawing_area_get_content_height(Display);
  osc->hLines=7;
  osc->vLines=7;
  osc->gain=10.0;
  
  //Bind delle funzioni ai widget
  gtk_drawing_area_set_draw_func(Display,DisplayDraw,osc,NULL);
  g_signal_connect(ButtonSE,"toggled",G_CALLBACK(SEToggle), NULL);
  
  //Aggiunta della finestra principale all'applicazione
  gtk_application_add_window(self,MainWin);
  gtk_window_present(MainWin);
}

static void DisplayDraw(GtkDrawingArea *area, cairo_t *cr,int width,int height,gpointer data){
  cairo_set_source_rgb(cr,0.0,0.0,0.0);
  cairo_paint (cr);
  DisplayOsc* osc=(DisplayOsc*)data;
  
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
  
  //Disegno della curva
  double dt=1e-3;
  double T=2;
  double f=10;
  double w=2.0*M_PI*f;
  int N=floor(T/dt);
  double GainDisplay=(double)(osc->gain);
  double A=10.0;
  double x=0.0,y=(osc->height/2)-GainDisplay*A;
  cairo_move_to(cr,x,y);
  for(int k=0; k<N; ++k){
    static double t=0;
    t += dt/T;
    double x=osc->width*t;
    double y=(osc->height/2)-GainDisplay*A*cos(w*t);
    cairo_line_to(cr,x,y);
  }
  cairo_set_source_rgba(cr,1.0,0.0,0.0,1.0);
  cairo_set_line_width(cr,2.0);
  cairo_set_dash(cr,dLength,0,0.0);
  cairo_stroke(cr);
}

static void SEToggle(GtkToggleButton *self, gpointer user_data){
  static timer_t idTimerCamp;
  static int x=0;
  g_print("%d\n",++x);
  if(gtk_toggle_button_get_active(self)){
    //Se sono passato da Start a Stop
    gtk_button_set_label(GTK_BUTTON(self),"Stop");
    //Creo e faccio partire il timer per il campionamento
    timer_create(CLOCK_REALTIME, NULL, &idTimerCamp);
    struct sigaction newAct={.sa_flags=0, .sa_handler=alrmHandler};
    sigaction(SIGALRM, &newAct, NULL);
    struct itimerspec ts={.it_interval={.tv_sec=1L, .tv_nsec=0L}, .it_value={.tv_sec=1L, .tv_nsec=0L}};
    timer_settime(idTimerCamp, 0, &ts, NULL);
  }else{
    //Se sono passato da Stop a Start
    gtk_button_set_label(GTK_BUTTON(self),"Start");
    timer_delete(idTimerCamp);
  }
}

void alrmHandler(int seg){
  //Ad ogni SIGALARM mando un messaggio evento al mainloop per segnalare il thread della seriale
  signalMainLoop(MLC,REQUEST_SERIAL,NULL);
}
