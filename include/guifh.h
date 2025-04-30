#include <gtk/gtk.h>

static void InitApp(GtkApplication *self, gpointer user_data);
static void DisplayDraw(GtkDrawingArea *area, cairo_t *cr,int width,int height,gpointer data);
