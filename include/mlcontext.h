/**   \file mlcontext.h
      \brief Header per il contesto del main loop.
*/
/**   \var typedef struct _mlcontext MlContext
      \brief Tipo di dato per il contesto del main loop.
*/
/**   \fn int mlContextNew(MlContext **mlc)
      \brief Funzione per la creazione di un nuovo contesto per un main loop.
      
      \param mlc Indirizzo a un puntatore MlContext*.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR,MEMORY_ERR,MUTEX_INIT_ERR,SIGMASK_ERR.
*/
/**   \fn int mlContextDelete(MlContext *mlc)
      \brief Funzione per la distruzione di una coda degli eventi.
      
      \param mlc Puntatore a un contesto di un main loop.
      \return  Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR,MUTEX_UNLOCK_ERR,MUTEX_LOCK_ERR,MUTEX_DESTROY_ERR.
*/
/**   \fn int signalMainLoop(MlContext *mlc, uint8_t c, void* data)
      \brief Funzione con cui un thread può segnalare un main loop.
      
      La segnalazione è senza blocco del thread chiamante.
      
      \param mlc Puntatore a un contesto di un main loop.
      \param c comando da inviare al main loop.
      \param data dati associati al comando da eseguire.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR,MUTEX_UNLOCK_ERR,MUTEX_LOCK_ERR,THREAD_KILL_ERR.
*/
/**   \fn int signalMainLoopReply(MlContext *mlc, uint8_t c, void* data, void* res)
      \brief Funzione con cui un thread può segnalare un main loop.
      
      La segnalazione è con blocco del thread chiamante, in attesa della risposta da parte del main loop.
      
      \param mlc Puntatore a un contesto di un main loop.
      \param c comando da inviare al main loop.
      \param data dati associati al comando da eseguire.
      \param res puntatore a un buffer in cui il main loop memorizza la risposta.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR,MUTEX_UNLOCK_ERR,MUTEX_LOCK_ERR,THREAD_KILL_ERR,SIGMASK_ERR.
*/
/**   \fn int waitNewEvent(MlContext *mlc)
      \brief Funzione con cui il main loop si mette in attesa per ricevere nuovi eventi.
            
      \param mlc Puntatore a un contesto di un main loop.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR,THREAD_NOT_ML_ERR,SIGMASK_ERR.
*/
/**   \fn int GetNSignals(MlContext *mlc)
      \brief Funzione con cui un main loop recupera il numero di eventi in coda.
    
      \param mlc Puntatore a un contesto di un main loop.
*/
/**   \fn EQueueElement* PopEventQueue(MlContext *mlc)
      \brief Funzione con cui un main loop estrae un evento dalla cima della coda.
            
      \param mlc Puntatore a un contesto di un main loop.
      \return Il valore di ritorno è un puntatore alla struttura dati che rappresenta l'evento.
*/
/**
      \fn void PrintSignalQueue(MlContext *mlc)
      \brief Funzione per stampare nello stdout una rappresentazione della coda degli eventi associata al main loop.
    
      \param mlc Puntatore a un contesto di un main loop.
*/

/**   \mainpage Event Main Loop
      \section Introduzione
      Una libreria statica per l'implementazione di un sistema di main loop guidato da eventi.
      In un progetto multi-threading, un thread viene nominato come un main loop che risponde a dei comandi (eventi) generati da altri thread che hanno il main loop come riferimento
      per la gestione degli eventi.
      Ogni main loop ha una coda degli eventi associata, che raccoglie tutti i comandi che sono inviati, quando il main loop viene eseguito verifica se ci sono nuovi eventi nella coda, se
      si estrae quello in cima e lo elabora, altrimenti si mette in attesa; nella struttura multi-thread il thread del main loop viene eseguito solo quando ci sono degli eventi da
      elaborare, normalmente si trova in stato di sleep, viene attivato tramite una segnalazione da parte dei thread che richiedono di eseguire un comando.
      
      \subsection Implementazione Implementazione con POSIX.
      Trattandosi di uno sviluppo con Ubuntu, i thread usati rispettano lo standard POSIX, e dunque la segnalazione tra thread avviene tramite segnali e modalità che sono descritte nello
      standard. I thread che inviano comandi al main loop possono attendere per una risposta oppure continuare l'esecuzione, nel primo caso il thread chiamante si autosospende tramite
      sigwait, e viene risvegliato quando riceve un segnale SIGCONT, possibilmente inviato dal thread main loop.
      Per poter intercettare tutti i SIGCONT in maniera ordinata, i thread prima di autosospendersi modificano la propria maschera dei segnali impostando SIGCONT come un segnale da
      bloccare al suo arrivo, questa procedura è fatta preliminarmente per il thread main loop al momento della sua creazione in modo tale da bloccare anche i segnali SIGCONT che arrivano
      quando il thread è sveglio, il risultato è che il thread main loop entra in stato di sleep, tramite la funzione waitNewEvent, solo quando non ci sono più segnali SIGCONT accodati e
      la coda degli eventi è vuota.
      
      I thread POSIX sono molto rigidi per quanto riguarda la condivisione della memoria, l'unica zona in comune in un processo multi-thread è lo heap, perciò tutte le funzioni che
      richiedono che ci sia una comunicazione diretta tra thread chiamante e main loop, come passaggio di argomenti o ottenere la risposta del main loop, devono usare delle zone di memoria
      che sono allocate nello heap del processo.
*/

#include <stdint.h>
#include "equeue.h"

typedef struct _mlcontext MlContext;

int mlContextNew(MlContext **mlc);
int mlContextDelete(MlContext *mlc);
int signalMainLoop(MlContext *mlc, uint8_t c, void* data);
int signalMainLoopReply(MlContext *mlc, uint8_t c, void* data, void* res);
int waitNewEvent(MlContext *mlc);
int GetNSignals(MlContext *mlc);
EQueueElement* PopEventQueue(MlContext *mlc);
void PrintSignalQueue(MlContext *mlc);
