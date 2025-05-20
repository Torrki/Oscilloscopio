#include <mlcontext.h>

typedef float sampleType;

//Struttura dati per il passaggio di argomenti
struct argsThreadStruct{
  MlContext* mlc;
  sampleType *bufferSignals;
  unsigned nElementi;
  unsigned comandoSeriale;
  const char** porte;
  int fdSeriale;
  float gainOsc;
  unsigned nElementiLetti;
};

const char** ScanPorts();
int OpenSerial(const char *fileSerial);
