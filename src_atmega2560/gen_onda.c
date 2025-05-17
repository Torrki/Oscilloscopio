#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define BAUD 9600
#define START_COMMAND "Start\n"
#define START 1
#define WAIT 2
#define CAMPIONA 3
#define FINE 4

volatile uint8_t stato=START;
volatile uint8_t segnale=0;
void usart_pstr(char *s);
void usart_putchar(char data);
char usart_getchar(void);

ISR(TIMER0_COMPA_vect){
  while ( !(UCSR0A & (_BV(UDRE0))) );
  UDR0 = segnale;
  segnale++;
}

int main(){  
  /*
  Funzionamento a stati:
  1)  Quando non sta campionando è in attesa dei dati di impostazione del campionamento
  2)  Quando riceve i dati di campionamento inizia a generare l'onda ed entra in un ciclo di trasmissione dei dati
  3)  Il ciclo viene interrotto solo quando riceve un comando di stop, tornando allo stato iniziale
  Di deafult lo stato di sleep è IDLE
  */
  cli();
  //Impostazione della USART
  DDRB |= 0x80;
  PORTB &= ~0x80;
  uint16_t ubrr=F_CPU/16/BAUD-1;
  UBRR0H = (uint8_t)(ubrr>>8);
  UBRR0L = (uint8_t)ubrr;

  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  sei();
  
  //Segnalo che la seriale lato Arduino è pronta
  usart_pstr(START_COMMAND);
  while ( !(UCSR0A & (_BV(TXC0))) );
  UCSR0A |= _BV(TXC0);
  
  //Aspetto la risposta dell'applicazione
  char c=usart_getchar();
  UCSR0A |= _BV(UDRE0) | _BV(RXC0);
  
  //Impostazione del timer per la freq di campionamento e la generazione del segnale
  const uint8_t ocra=155;
  TCCR0A |= _BV(WGM01);  //CTC
  OCR0A=ocra;
  TIMSK0 |= _BV(OCIE0A); //Interrupt per la comparazione del timer
  
  //Segnalo che il timer è pronto a partire
  usart_putchar('K');
  while ( !(UCSR0A & (_BV(TXC0))) );
  UCSR0A |= _BV(UDRE0) | _BV(TXC0);
  //Aspetto la risposta dell'applicazione
  c=usart_getchar();
  TCCR0B |= _BV(CS02);   //256 di prescaler e start del timer
  PORTB |= 0x80;
  c=usart_getchar();
  return 0;
}

void usart_putchar(char data) {
    // Wait for empty transmit buffer
    while ( !(UCSR0A & (_BV(UDRE0))) );
    // Start transmission
    UDR0 = data; 
}

char usart_getchar(void) {
    // Wait for incoming data
    while ( !(UCSR0A & (_BV(RXC0))) );
    // Return the data
    return UDR0;
}

void usart_pstr(char *s) {
    // loop through entire string
    while (*s) { 
        usart_putchar(*s);
        s++;
    }
}
