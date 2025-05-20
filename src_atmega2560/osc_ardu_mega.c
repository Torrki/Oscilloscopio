#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define BAUD 57600
#define START_COMMAND "Start\n"
#define START 1
#define WAIT 2
#define CAMPIONA 3
#define FINE 4

void usart_pstr(char *s);
void usart_putchar(char data);
char usart_getchar(void);

ISR(TIMER0_COMPA_vect){
  //Aspetto che la seriale e l'ADC siano disponibili
  while( !( (UCSR0A & _BV(UDRE0)) | (ADCSRA & _BV(ADIF)) ) );
  UDR0 = ADCL;
  while( !(UCSR0A & _BV(UDRE0)) );
  UDR0 = ADCH;
  ADCSRA |= _BV(ADIF);
}

int main(){  
  /*
  Funzionamento a stati:
  1)  Quando non sta campionando è in attesa dei dati di impostazione del campionamento
  2)  Quando riceve i dati di campionamento inizia a generare l'onda ed entra in un ciclo di trasmissione dei dati
  3)  Il ciclo viene interrotto solo quando riceve un comando di stop, tornando allo stato iniziale
  */
  cli();
  //Impostazione della USART e dell'ADC
  //USART
  DDRB |= 0x80;
  PORTB &= ~0x80;
  uint16_t ubrr=F_CPU/16/BAUD-1;
  UBRR0H = (uint8_t)(ubrr>>8);
  UBRR0L = (uint8_t)ubrr;

  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  //ADC
  ADCSRA |= _BV(ADEN);
  ADMUX |= _BV(REFS0) | _BV(ADLAR); //AVCC, Right adj,0 channel signle ended
  ADCSRA |= _BV(ADATE); //Auto-trigger, 2 di prescaler
  ADCSRB |= _BV(ADTS1) | _BV(ADTS0); //trigger del campionamento con la comparazione del timer
  sei();
  
  //Faccio partire il primo campionamento per l'inizializzazione del circuito ADC
  ADCSRA |= _BV(ADSC);
  
  //Segnalo che la seriale lato Arduino è pronta
  usart_pstr(START_COMMAND);
  while ( !(UCSR0A & (_BV(TXC0))) );
  UCSR0A |= _BV(TXC0);
  ADCSRA |= _BV(ADIF);
  
  //Aspetto la risposta dell'applicazione
  char c=usart_getchar();
  UCSR0A |= _BV(UDRE0) | _BV(RXC0);
  
  //Impostazione del timer per la freq di campionamento e la generazione del segnale, 1250 Hz
  unsigned long CS=64UL;
  uint8_t bitCS=0x00;
  switch(CS){
    case 1UL:
      bitCS |= _BV(CS00);
      break;
    case 8UL:
      bitCS |= _BV(CS01);
      break;
    case 64UL:
      bitCS |= _BV(CS01)|_BV(CS00);
      break;
    case 256UL:
      bitCS |= _BV(CS02);
      break;
    case 1024UL:
      bitCS |= _BV(CS02)|_BV(CS00);
      break;
    default:
      //Di default se il prescaler non è corretto il timer non parte
      break;
  }
  
  const uint8_t ocra=24;
  TCCR0A |= _BV(WGM01);  //CTC
  OCR0A=ocra;
  TIMSK0 |= _BV(OCIE0A); //Interrupt per la comparazione del timer
  unsigned long freqTimer=F_CPU/(2UL*CS*(1UL+(unsigned long)ocra));
  
  //Segnalo che il timer è pronto a partire, trasmetto anche la frequenza di campionamento
  usart_putchar('K');
  while ( !(UCSR0A & (_BV(TXC0))) );
  unsigned long maskFreqTimer=0xFF;
  for(int k=0; k<sizeof(unsigned long); ++k){
    uint8_t dato=(freqTimer & maskFreqTimer) >> (k*8);
    usart_putchar(dato);
    maskFreqTimer <<= 8;
  }
  UCSR0A |= _BV(UDRE0) | _BV(TXC0);
  
  //Aspetto la risposta dell'applicazione
  c=usart_getchar();
  TCCR0B |= _BV(CS01) | _BV(CS00);   //64 di prescaler e start del timer
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
