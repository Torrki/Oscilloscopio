#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define BAUD 9600
#define START_COMMAND "Start\n"
#define WAIT 1
#define CAMPIONA 2
#define FINE 3

volatile uint8_t stato=WAIT;
volatile uint8_t segnale=0;
void usart_pstr(char *s);
void usart_putchar(char data);

ISR(USART0_RX_vect){
  uint8_t dato=UDR0;
  if(stato==WAIT && dato=='K'){
    stato=CAMPIONA;
  }else if(stato == CAMPIONA && dato=='S'){
    stato=FINE;
  }
}

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
  DDRB |= 0x80;
  PORTB &= ~0x80;
  uint16_t ubrr=F_CPU/16/BAUD-1;
  UBRR0H = (uint8_t)(ubrr>>8);
  UBRR0L = (uint8_t)ubrr;

  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8-bit data */ 
  UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);   /* Enable RX and TX */
  sei();
  
  //Segnalo che la seriale lato Arduino è pronta
  usart_pstr(START_COMMAND);
  while ( !(UCSR0A & (_BV(TXC0))) );
  UCSR0A |= _BV(UDRE0) | _BV(TXC0);
  while(stato == WAIT);
  usart_putchar('K');
  while ( !(UCSR0A & (_BV(TXC0))) );
  UCSR0A |= _BV(UDRE0) | _BV(TXC0);
  
  //TODO: Impostare il timer per la freq di campionamento e la generazione del segnale
  const uint8_t ocra=155;
  TCCR0A |= _BV(WGM01);  //CTC
  OCR0A=ocra;
  TIMSK0 |= _BV(OCIE0A); //Interrupt per la comparazione del timer
  TCCR0B |= _BV(CS02);   //256 di prescaler e start del timer
  while(1);
  return 0;
}

void usart_putchar(char data) {
    // Wait for empty transmit buffer
    while ( !(UCSR0A & (_BV(UDRE0))) );
    // Start transmission
    UDR0 = data; 
}

void usart_pstr(char *s) {
    // loop through entire string
    while (*s) { 
        usart_putchar(*s);
        s++;
    }
}
