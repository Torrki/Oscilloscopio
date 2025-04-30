#include <stdlib.h>
#include <GL/gl.h>
#include <elm.h>
#include <guifh.h>
#include <cairo/cairo.h>

void* Thread_GUI(void* args){
  struct _mlcontext *MLC=(struct _mlcontext *)args;
  
  //Creazione e start dell'applicazione
  GtkApplication *app=gtk_application_new("GTK.Oscilloscopio", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(G_APPLICATION(app), "activate", G_CALLBACK(InitApp), NULL);
  int status=g_application_run(G_APPLICATION(app), 0, NULL);
  g_print("exit status: %d\n",status);
  
  //Segnalazione al Main-Loop di uscire dal ciclo
  signalMainLoop(MLC,2,NULL);
  return NULL;
}

static void InitApp(GtkApplication *self, gpointer user_data){
  //Ottengo i riferimenti agli oggetti nella finestra
  GtkBuilder *builder=gtk_builder_new_from_file("../GUI/WinUI.ui");
  GtkWindow *MainWin=GTK_WINDOW(gtk_builder_get_object(builder, "MainWin"));
  GtkDrawingArea *Display=GTK_DRAWING_AREA(gtk_builder_get_object(builder, "DisplayArea"));
  
  //Bind delle funzioni ai widget
  gtk_drawing_area_set_draw_func(Display,DisplayDraw,NULL,NULL);
  
  //Aggiunta della finestra principale all'applicazione
  gtk_application_add_window(self,MainWin);
  gtk_window_present(MainWin);
}

static void DisplayDraw(GtkDrawingArea *area, cairo_t *cr,int width,int height,gpointer data){
  cairo_set_source_rgb(cr,0.0,0.0,0.0);
  cairo_paint (cr);
}
