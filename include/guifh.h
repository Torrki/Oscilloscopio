#include <gtk/gtk.h>

typedef struct _DisplayOscilloscopio DisplayOsc;

static void InitApp(GtkApplication *self, gpointer user_data);
static void DisplayDraw(GtkDrawingArea *area, cairo_t *cr,int width,int height,gpointer data);
static void SEToggle(GtkToggleButton *self, gpointer user_data);
void alrmHandler(int seg);
