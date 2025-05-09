#include <gtk/gtk.h>

typedef struct _DisplayOscilloscopio DisplayOsc;

static void InitApp(GtkApplication *self, gpointer user_data);
static void SEToggle(GtkToggleButton *self, gpointer user_data);
void alrmHandler(int seg);
