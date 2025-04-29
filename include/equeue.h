/**   \file equeue.h
      \brief Header per la coda degli eventi.
*/
/**   \var typedef struct _EQueueElement EQueueElement
      \brief Tipo di dato per un elemento della coda degli eventi.
*/
/**   \var typedef struct _EQueue EQueue
      \brief Tipo di dato per la coda degli eventi.
*/
/**   \fn int EQueueNew(EQueue** q)
      \brief Funzione per la creazione di una coda degli eventi.
            
      \param q Indirizzo a un puntatore EQueue*.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR o MEMORY_ERR.
*/
/**   \fn int EQueueDelete(EQueue* q)
      \brief Funzione per la distruzione di una coda degli eventi.
            
      \param q Puntatore a una coda.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare il codice NULL_POINTER_ERR.
*/
/**   \fn int EQueuePush(EQueue* q, EQueueElement* e)
      \brief Funzione per aggiungere un elemento alla coda degli eventi.
            
      \param q Puntatore a una coda.
      \param e Puntatore all'elemento da aggiungere alla coda q.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare il codice NULL_POINTER_ERR.
*/
/**   \fn int EQueuePop(EQueue* q, EQueueElement** e)
      \brief Funzione per rimuovere un elemento dalla coda degli eventi.
            
      \param q Puntatore a una coda.
      \param e Indirizzo a un puntatore EQueueElement* per memorizzare l'elemento estratto.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare il codice NULL_POINTER_ERR.
*/
/**   \fn int EQueueElementNew(EQueueElement** e, pthread_t id, uint8_t c, void* data, void* res)
      \brief Funzione per la creazione di un elemento EQueueElement.      
      
      \param e Indirizzo a un puntatore EQueueElement* per memorizzare il nuovo elemento.
      \param id pthread_t del thread mittente.
      \param c  Codice del comando.
      \param data Dati ausiliari da passare insieme al comando.
      \param res Indirizzo di memoria in cui memorizzare la risposta.
      \return Il valore di ritorno è 0 nel caso di successo, altrimenti può tornare i codici NULL_POINTER_ERR o MEMORY_ERR.
*/
/**   \fn int GetEQueueElements(EQueue* q)
      \brief Funzione per ottenere il numero di elementi di una coda degli eventi.
      
      \param q Una coda degli eventi.
*/
/**   \fn pthread_t GetIdThreadElement(EQueueElement* e)
      \brief Funzione per ottenere il pthread_t associato all'elemento.
      
      \param e Un elemento valido.
*/
/**   \fn uint8_t GetCommandElement(EQueueElement* e)
      \brief Funzione per ottenere il comando associato all'elemento.
      
      \param e Un elemento valido.
*/
/**   \fn void* GetResAddrElement(EQueueElement* e)
      \brief Funzione per ottenere l'indirizzo associato al risultato del comando dell'elemento.
      
      \param e Un elemento valido.
*/
/**   \fn void* GetDataAddrElement(EQueueElement* e)
      \brief Funzione per ottenere il l'indirizzo associato ai dati per eseguire il comando dell'elemento.
      
      \param e Un elemento valido.
*/
/**   \fn void PrintQueue(EQueue* q)
      \brief Funzione per stampare la coda degli eventi nello stdout.
      
      \param q Una coda degli eventi.
*/

#include <stdint.h>
#include <pthread.h>

typedef struct _EQueueElement EQueueElement;
typedef struct _EQueue EQueue;

int EQueueNew(EQueue** q);
int EQueueDelete(EQueue* q);
int EQueuePush(EQueue* q, EQueueElement* e);
int EQueuePop(EQueue* q, EQueueElement** e);
int EQueueElementNew(EQueueElement** e, pthread_t id, uint8_t c, void* data, void* res);
int GetEQueueElements(EQueue* q);
pthread_t GetIdThreadElement(EQueueElement* e);
uint8_t GetCommandElement(EQueueElement* e);
void* GetResAddrElement(EQueueElement* e);
void* GetDataAddrElement(EQueueElement* e);
void PrintQueue(EQueue* q);
