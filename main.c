/**
 * MAGIC CLOCK
 * by Claudius Boettcher, Frederik Brudy, Felix Praschak, Fabius Steinberger
 * LMU Munich, 2012
 *
 *  - TODOS -
 *  'L' fuer Listening klappt auch im Fehlerfall irgendwo! --> Alternative?!
 *
 *
 * - CONFIGURE WIFLY MODULE -
 *
 * factory R                        Set Factory Defaults
 * set uart baud 4800				AOK
 * save								Storing in config
 * reboot                           *READY*
 *
 * $$$                              CMD
 *
 * set wlan ssid leo191             AOK
 * set wlan pass praschak19551991	AOK
 * set wlan join 1                  AOK
 * save                             Storing in config
 * reboot                           READY*
 *
 * $$$                              CMD
 *
 * set ip proto 18                  AOK
 * set ip host 0                    AOK
 * set dns name www.leo191.de       AOK
 * set ip remote 80                 AOK
 * set comm remote GET$/led.php     AOK
 * set option format 1              AOK
 * save                             Storing in config
 * reboot                           *READY*
 */

#include <avr/io.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#define BAUD 4800UL      							// Baudrate max. 7200
#define UBRR_VAL ((F_CPU+BAUD*8UL)/(BAUD*16UL)-1)
volatile long secs = 0;								// globaler halb-sekunden Takt
const int timeout = 22;								// halb-sekunden Timeout (30 = 15sec)
volatile int errInRow = 0;

/* Payload-Array holds location and color data
 *
 * [0] color user1		[2] color user2			[4] color user3			[6] color user4
 * [1] position user1	[3] position user2		[5] position user3		[7] position user4
 *
 * Undefined/off: 0
 * Positions: 1-12
 * Colors: 1-7
 *
 * Colors:
 * OFF=00
 * RED=01
 * GREEN=02
 * BLUE=03
 * YELLOW=04
 * WHITE=05
 * VIOLET=06
 */
volatile int payload[8];

/* Function prototypes */
void wifly_init(unsigned int baud);
void wifly_putc(unsigned char c);
void wifly_puts (char *s);
char wifly_getc(void);
int wifly_gets(char* buffer, int max);
void wifly_reboot();
void setColor(int pin, int colorId);
void setPosition(int pin, int pos);

int main(){
    
	/* Init UART communication */
	wifly_init(UBRR_VAL);
    
	/* Set data directions */
	DDRB = 1<<PB4 | 1<<PB3 | 1<<PB2 | 1<<PB1 | 1<<PB5 | 1<<PB0 | 1<<PB6 | 1<<PB7; // servos & led
	DDRC = 1<<PC0 | 1<<PC1 | 1<<PC2 | 1<<PC3 | 1<<PC4 | 1<<PC5; // leds
	DDRD = 1<<PD2 | 1<<PD3 | 1<<PD4; // led
    
	/* Init output pins */
	PORTB = 1<<PB4 | 1<<PB3 | 1<<PB2 | 1<<PB1 | 1 <<PB0 | 1<<PB6 | 1<<PB7 | 1<<PB5;
	PORTC = 1<<PC0 | 1<<PC1 | 1<<PC2 | 1<<PC3 | 1<<PC4 | 1<<PC5;
	PORTD = 1<<PD2 | 1<<PD3 | 1<<PD4;
    
	/* LED chrosscheck */
	for(int p = 0; p < 10; p++){
		for(int i = 1; i <= 4; i++){
			for(int q = 1; q <= 3; q++){
				setColor(i, q);
				_delay_ms(50);
			}
			setColor(i, 0);
		}
	}
    
	/* Gong Test */
	for(int t = 0; t < 50; t++){//insg
		for (int r = 0; r < 10; r++) {//lauflaenge
			PORTB |= (1 << PB5);
			_delay_us(600);//richtung
			PORTB &= ~(1 << PB5);
			_delay_ms(20);
		}
		_delay_ms(50);
	}
    
	/* Wait for WiFly to listen (L = Listening) */
	// alternativ: while(wifly_getc() != 'L');
	_delay_ms(1200);
    
	/* Init Timeout-Timer */
	TIMSK = _BV(TOIE1);     // Timer Overflow Interrupt Enable
	TCNT1 = 0;              // Timer reset
	TCCR1B |= (1<<CS11); 	// Prescaler
	sei();
    
	/* Active Indicator */
	for(int i = 1; i <= 4; i++) setColor(i, 2);
	_delay_ms(2000);
	for(int i = 1; i <= 4; i++) setColor(i, 0);
    
	/* Begin establishing connection */
	wifly_puts("$$$"); // command mode
	_delay_ms(500);
	wifly_putc(13); //enter
    
	while (1){
        
		/* Open connection to server */
		wifly_puts("open");
		wifly_putc(13);//enter
        
		char line[30];
        
		/* Timeout handling */
		if( wifly_gets(line, sizeof(line) / sizeof(line[0])) < 0){
            
			wifly_puts("*TIMEOUT*");
            
			errInRow++; // error count
            
			if(errInRow > 3) {
				errInRow = 0;
				wifly_reboot();
			}
            
		}
		else
		{
			errInRow = 0; // error count
            
			/* Explode string input into int array */
			char *ptr = strtok(line, "|:");
            
			for(int i = 0; ptr != NULL; i++) {
				payload[i] = atoi(ptr);
				ptr = strtok(NULL, "|:");
			}
            
			/* Set colors and hand positions */
			_delay_ms(100);
			setColor(1, payload[0]);
			setPosition(1, payload[1]);
			//_delay_ms(10000);
			setColor(2, payload[2]);
			setPosition(2, payload[3]);
			//_delay_ms(10000);
			setColor(3, payload[4]);
			setPosition(3, payload[5]);
			//_delay_ms(10000);
			setColor(4, payload[6]);
			setPosition(4, payload[7]);
            
			_delay_ms(1000); // request frequence
		}
        
		wifly_putc(13); // enter
	}
}


/* Timer 1 interrupt */
SIGNAL (TIMER1_OVF_vect){
	secs++;
	return;
}

/* Init UART */
void wifly_init(unsigned int baud){
    
	UBRRH = (unsigned char)(baud>>8);
	UBRRL = (unsigned char)baud;
	UCSRB |= (1<<TXEN) | (1<<RXEN);
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
}

/* Send character */
void wifly_putc(unsigned char c){
    
	while (!(UCSRA & (1<<UDRE)));
    
	UDR = c;
}

/* Send string */
void wifly_puts (char *s){
    
	while (*s){
		wifly_putc(*s);
		s++;
	}
}

/* Read character */
char wifly_getc(void){
    
	long secs_ref = secs;
    
	while (1){
		if(secs-secs_ref > 2*timeout) break;
		if((UCSRA & (1<<RXC))) break;
	}
    
	return UDR;
}

/* Read string */
int wifly_gets(char* buffer, int max){
    
	int nextChar;
	int length = 0;
	long secs_ref = secs;
	int err = 0;
    
	_delay_ms(400);    // important!
    
	while(1){
		if(secs-secs_ref > timeout){ // check for timeout
			err++; // error count
			break;
		}
        
		if(wifly_getc() == '{') break;
        
	}
    
	nextChar = wifly_getc();
    
	while(1){
		if(nextChar == '}') break;
        
		if(length >= max -1){
			err++;
			break;
		}
        
		if(secs-secs_ref > timeout){
			err++;
			break;
		}
        
		*buffer++ = nextChar;
		length++;
		nextChar = wifly_getc();
	}
    
	wifly_puts("$$$"); // command mode
	_delay_ms(500); // important!
	wifly_putc(13); //enter
    
	*buffer = '\0'; // close string
    
	_delay_ms(200); // important!
    
	if(err > 0) return -1; // error count
    
	return 0;
}

/* Reboot Wifly module */
void wifly_reboot(){
    
	wifly_putc(13); // enter
	wifly_puts("$$$"); // command mode
	_delay_ms(500);
	wifly_putc(13);//enter
    
	wifly_puts("reboot");
	wifly_putc(13); // enter
    
	while(wifly_getc() != '*');
}

/* Set LED color between 0 - 7 */
void setColor(int pin, int colorId){
    
	switch(pin){
            
        case 1:
            switch(colorId){
                case 1:
                    PORTB |= (1 << PB0); // r=1
                    PORTB &= ~(1 << PB6); // g=0
                    PORTB &= ~(1 << PB7); // b=0
                    break;
                    
                case 2:
                    PORTB &= ~(1 << PB0); // r=0
                    PORTB |= (1 << PB6); // g=1
                    PORTB &= ~(1 << PB7); // b=0
                    break;
                    
                case 3:
                    PORTB &= ~(1 << PB0); // r=0
                    PORTB &= ~(1 << PB6); // g=0
                    PORTB |= (1 << PB7); // b=1
                    break;
                    
                case 4:
                    PORTB |= (1 << PB0); // r=1
                    PORTB |= (1 << PB6); // g=1
                    PORTB &= ~(1 << PB7); // b=0
                    break;
                    
                case 5:
                    PORTB |= (1 << PB0); // r=1
                    PORTB |= (1 << PB6); // g=1
                    PORTB |= (1 << PB7); // b=1
                    break;
                    
                case 6:
                    PORTB |= (1 << PB0); // r=1
                    PORTB &= ~(1 << PB6); // g=0
                    PORTB |= (1 << PB7); // b=1
                    break;
                    
                default:
                    PORTB &= ~(1 << PB0); // r=0
                    PORTB &= ~(1 << PB6); // g=0
                    PORTB &= ~(1 << PB7); // b=0
                    break;
            }
            break;
            
        case 2:
            switch(colorId){
                case 1:
                    PORTC |= (1 << PC3); // r=1
                    PORTC &= ~(1 << PC4); // g=0
                    PORTC &= ~(1 << PC5); // b=0
                    break;
                    
                case 2:
                    PORTC &= ~(1 << PC3); // r=0
                    PORTC |= (1 << PC4); // g=1
                    PORTC &= ~(1 << PC5); // b=0
                    break;
                    
                case 3:
                    PORTC &= ~(1 << PC3); // r=0
                    PORTC &= ~(1 << PC4); // g=0
                    PORTC |= (1 << PC5); // b=1
                    break;
                    
                case 4:
                    PORTC |= (1 << PC3); // r=1
                    PORTC |= (1 << PC4); // g=1
                    PORTC &= ~(1 << PC5); // b=0
                    break;
                    
                case 5:
                    PORTC |= (1 << PC3); // r=1
                    PORTC |= (1 << PC4); // g=1
                    PORTC |= (1 << PC5); // b=1
                    break;
                    
                case 6:
                    PORTC |= (1 << PC3); // r=1
                    PORTC &= ~(1 << PC4); // g=0
                    PORTC |= (1 << PC5); // b=1
                    break;
                    
                default:
                    PORTC &= ~(1 << PC3); // r=0
                    PORTC &= ~(1 << PC4); // g=0
                    PORTC &= ~(1 << PC5); // b=0
                    break;
            }
            break;
            
        case 3:
            switch(colorId){
                case 1:
                    PORTD |= (1 << PD2); // r=1
                    PORTD &= ~(1 << PD3); // g=0
                    PORTD &= ~(1 << PD4); // b=0
                    break;
                    
                case 2:
                    PORTD &= ~(1 << PD2); // r=0
                    PORTD |= (1 << PD3); // g=1
                    PORTD &= ~(1 << PD4); // b=0
                    break;
                    
                case 3:
                    PORTD &= ~(1 << PD2); // r=0
                    PORTD &= ~(1 << PD3); // g=0
                    PORTD |= (1 << PD4); // b=1
                    break;
                    
                case 4:
                    PORTD |= (1 << PD2); // r=1
                    PORTD |= (1 << PD3); // g=1
                    PORTD &= ~(1 << PD4); // b=0
                    break;
                    
                case 5:
                    PORTD |= (1 << PD2); // r=1
                    PORTD |= (1 << PD3); // g=1
                    PORTD |= (1 << PD4); // b=1
                    break;
                    
                case 6:
                    PORTD |= (1 << PD2); // r=1
                    PORTD &= ~(1 << PD3); // g=0
                    PORTD |= (1 << PD4); // b=1
                    break;
                    
                default:
                    PORTD &= ~(1 << PD2); // r=0
                    PORTD &= ~(1 << PD3); // g=0
                    PORTD &= ~(1 << PD4); // b=0
                    break;
            }
            break;
            
        case 4:
            switch(colorId){
                case 1:
                    PORTC |= (1 << PC0); // r=1
                    PORTC &= ~(1 << PC1); // g=0
                    PORTC &= ~(1 << PC2); // b=0
                    break;
                    
                case 2:
                    PORTC &= ~(1 << PC0); // r=0
                    PORTC |= (1 << PC1); // g=1
                    PORTC &= ~(1 << PC2); // b=0
                    break;
                    
                case 3:
                    PORTC &= ~(1 << PC0); // r=0
                    PORTC &= ~(1 << PC1); // g=0
                    PORTC |= (1 << PC2); // b=1
                    break;
                    
                case 4:
                    PORTC |= (1 << PC0); // r=1
                    PORTC |= (1 << PC1); // g=1
                    PORTC &= ~(1 << PC2); // b=0
                    break;
                    
                case 5:
                    PORTC |= (1 << PC0); // r=1
                    PORTC |= (1 << PC1); // g=1
                    PORTC |= (1 << PC2); // b=1
                    break;
                    
                case 6:
                    PORTC |= (1 << PC0); // r=1
                    PORTC &= ~(1 << PC1); // g=0
                    PORTC |= (1 << PC2); // b=1
                    break;
                    
                default:
                    PORTC &= ~(1 << PC0); // r=0
                    PORTC &= ~(1 << PC1); // g=0
                    PORTC &= ~(1 << PC2); // b=0
                    break;
            }
            break;
	}
}

/* Set servo position between 1 - 12 */
void setPosition(int pin, int pos){
    
	int i = 0;
	int pinName;
    
	switch(pin){
		case 1: pinName = PB1; break;
		case 2: pinName = PB2; break;
		case 3: pinName = PB3; break;
		case 4: pinName = PB4; break;
		case 5: pinName = PB5; break;
		default: pinName = PB1; break;
	}
    
	/* PWM */
	while(i < 30) {
        
		PORTB |= (1<<pinName);
        
		switch(pos){
			case 1: _delay_us(1950); break; // home
			case 2: _delay_us(2370);  break; // uni
			case 3: _delay_us(700);  break; // party
			case 4: _delay_us(1330); break; // prison
			case 5:  _delay_us(560); break; // work
			case 6:  _delay_us(1800); break; // sport
			case 7:  _delay_us(2060);  break; // on the way
			case 8:  _delay_us(1000); break; // travelling
			case 9:  _delay_us(840); break; // relax
			case 10: _delay_us(1480); break; // hospital
			case 11: _delay_us(1650); break; // lunch
			case 12: _delay_us(1180); break; // lost
			default:break;
		}
        
		PORTB &= ~(1<<pinName);
		_delay_ms(20);
		i++;
	}
}
