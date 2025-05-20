#define EXIT_ML 1
#define RENDER_GL EXIT_ML+1
#define START_RENDER RENDER_GL+1
#define END_RENDER START_RENDER+1
#define SCAN_SERIAL END_RENDER+1
#define OPEN_SERIAL SCAN_SERIAL+1

#include <equeue.h>
#include <mlcontext.h>
#include <eventerr.h>
