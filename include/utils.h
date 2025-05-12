//Struttura dati per il passaggio di argomenti
struct argsThreadStruct{
  MlContext* mlc;
  float *bufferSignals;
  unsigned nElementi;
  unsigned comandoSeriale;
  const char** porte;
  int fdSeriale;
};

const char** ScanPorts();
