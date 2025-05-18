#include <gtk/gtk.h>

typedef struct _DisplayOscilloscopio DisplayOsc;

static void InitApp(GtkApplication *self, gpointer user_data);
static void SEToggle(GtkToggleButton *self, gpointer user_data);
static gboolean CloseRequestWindow (GtkWindow* self, gpointer user_data);
static gboolean render (GtkGLArea *area, GdkGLContext *context);
static void on_realize (GtkGLArea *area);
void alrmHandler(int seg);
