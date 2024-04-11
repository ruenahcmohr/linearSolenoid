/******************************************************************************
Title:    encoder -> PWM
Author:   rue_mohr
Date:     Aug 19 2005
Software: AVR-GCC 3.3 
Hardware: atmega32
  

                 
+-------------------------------------+
 |                          Sig Vd Gnd |
 |  +---------+   5V O     PB0 [o o o] | 
 |  | 7805  O |   Vd O     PB1 [o o o] | 
 |  +---------+   V+ .     PB2 [o o o] | 
 |                         PB3 [o o o] | -> side A pwm
 |                         PB4 [o o o] | 
 |                         PB5 [o o o] | 
 |                         PB6 [o o o] | 
 |                         PB7 [o o o] | 
 |                         PA0 [o o o] | <- up button
 |                         PA1 [o o o] | <- down button
 |        +----------+     PA2 [o o o] | 
 |        |O         |     PA3 [o o o] | 
 |        |          |     PA4 [o o o] | 
 |        |          |     PA5 [o o o] | 
 |        |          |     PA6 [o o o] | 
 |        |          |     PA7 [o o o] | 
 |        |          |     PC7 [o o o] |
 |        |          |     PC6 [o o o] |
 |        |          |     PC5 [o o o] |
 |        | ATMEGA32 |     PC4 [o o o] |
 |        |          |     PC3 [o o o] |
 |        |          |     PC2 [o o o] |
 |        |          |     PC1 [o o o] |
 |        |          |     PC0 [o o o] |
 |        |          |     PD7 [o o o] | -> side B pwm
 |        |          |     PD2 [o o o] | <- channel A
 |        |          |     PD3 [o o o] | <- channel B
 |        |          |     PD4 [o o o] |
 |        |          |     PD5 [o o o] |
 |        +----------+     PD6 [o o o] |
 |      E.D.S BABYBOARD III            |
 +-------------------------------------+


what this should do:
 A solenoid driver is put on side B pwm, the system should control the position
 as per the + - button inputs.
 
    
*******************************************************************************/
 
/****************************| INCLUDGEABLES |********************************/
 
#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/signal.h>
 
/*****************************| DEFINIATIONS |********************************/
 
#define Forward            1
#define Reverse            0
 
#define OUTPUT             1
#define INPUT              0

#define IsHigh(BIT, PORT)    ((PORT & (1<<BIT)) != 0)
#define IsLow(BIT, PORT)     ((PORT & (1<<BIT)) == 0)
#define SetBit(BIT, PORT)     PORT |= (1<<BIT)
#define ClearBit(BIT, PORT)   PORT &= ~(1<<BIT)

 // limit v to within low (l) and high (h) limits
#define limit(v, l, h)        ((v) > (h)) ? (h) : ((v) < (l)) ? (l) : (v)
 
/*****************************| VARIABLES |********************************/

 
 
volatile signed int position;           // note that this is signed
signed int control; 
volatile unsigned char oldstate; // quadrature decoding
 
#define ERROR 0
        signed char offsets[] = {
        /* 0000 */   0,
        /* 0001 */  +1,
        /* 0010 */  -1,
        /* 0011 */   ERROR,
        /* 0100 */  -1,
        /* 0101 */   0,
        /* 0110 */   ERROR,
        /* 0111 */  +1,
        /* 1000 */  +1,
        /* 1001 */   ERROR,
        /* 1010 */   0,
        /* 1011 */  -1,
        /* 1100 */   ERROR,
        /* 1101 */  -1,
        /* 1110 */  +1,
        /* 1111 */   0
};
 
/************************| FUNCTION DECLARATIONS |*************************/
 
void pwm_init     ( );
void SetPWM       ( uint8_t channel, uint8_t value) ;
void SetDir       ( uint8_t dir );
void SetSpeed     ( int Speed );
void setupEncoder ( void );
void updatePos    ( void );
 
/****************************| CODE... |***********************************/
 
int main (void)  {
   signed long error;
   char oldbuttons;
   char changes;
  
  DDRA = (INPUT << PA0 | INPUT << PA1 |INPUT << PA2 |INPUT << PA3 |INPUT << PA4 |INPUT << PA5 |INPUT << PA6 |INPUT << PA7);
  DDRB = (INPUT << PB0 | INPUT << PB1 |INPUT << PB2 |OUTPUT << PB3 |INPUT << PB4 |INPUT << PB5 |INPUT << PB6 |INPUT << PB7);
  DDRC = (INPUT << PC0 | INPUT << PC1 |INPUT << PC2 |INPUT << PC3 |INPUT << PC4 |INPUT << PC5 |INPUT << PC6 |INPUT << PC7);
  DDRD = (INPUT << PD0 | INPUT << PD1 |INPUT << PD2 |INPUT << PD3 |INPUT << PD4 |INPUT << PD5 |INPUT << PD6 |OUTPUT << PD7);        
         
  pwm_init();
  setupEncoder(); 
  sei();
  
while(1) {

    // Filter bits that changed, and are now high
    if ((changes = ((PINA ^ oldbuttons) & oldbuttons)) != 0) {
      if (changes & 0x01)  control++;  // button 1
      if (changes & 0x02)  control--;  // button 2          
    }
    oldbuttons = PINA;

    error = position - control;
    SetSpeed ( limit(error*2, 10, 255));     	
   }
}
    
//------------------------| FUNCTIONS |------------------------
 
//SIGNAL (SIG_INTERRUPT0) {  // fix this signal name!!! INT0
ISR(INT0_vect) {
        updatePos();
}
 
//SIGNAL (SIG_INTERRUPT1) { // fix this signal name!!! INT1
ISR(INT1_vect) {
        updatePos();
}
 
void SetSpeed ( int Speed ) {

  if (Speed > 0) {
      SetPWM( 0, 0);     // make sure one side is stopped
      SetPWM( 1, Speed); // set other side
    } else {
      SetPWM( 1, 0);
      SetPWM( 0, -Speed);
    }
}
 
 
 
void SetPWM( uint8_t channel, uint8_t value) {
   if (channel == 0)   
      OCR0 = value;
   if (channel == 1)   
      OCR2 = value;
}
 
void pwm_init() {
  // clear pwm levels
  OCR0 = 0; 
  OCR2 = 0;
  
  // set up WGM, clock, and mode for timer 0
  TCCR0 = 0 << FOC0  | 
          1 << WGM00 | /* fast pwm */
          1 << COM01 | /* inverted */
          0 << COM00 | /*   this bit 1 for inverted, 0 for normal  */
          1 << WGM01 | /* fast pwm */
          0 << CS02  | /* prescale by 1024 */
          1 << CS01  |
          1 << CS00  ;
  
  // set up WGM, clock, and mode for timer 2
  TCCR2 = 0 << FOC2  | 
          1 << WGM20 |
          1 << COM21 |
          0 << COM20 | /*   this bit 1 for inverted, 0 for normal  */
          1 << WGM21 |
          1 << CS22  |
          0 << CS21  |
          0 << CS20  ;
  
 }
 
void setupEncoder() {
        // clear position
        position = 0; 
 
        // we need to set up int0 and int1 to 
        // trigger interrupts on both edges
        MCUCR = (1<<ISC10) | (1<<ISC00);
 
        // then enable them.
        GICR  = (1<<INT0) | (1<<INT1);
}
 
 
 

 
void updatePos() {
/*      get the bits        PIND     = XXXXiiXX
        clear space       & 0x0C     = 0000ii00
        or in old status  | oldstate = XX00iijj
       clean up          & 0x0F     = 0000iijj  */  
       
       unsigned char temp;
       
        temp = oldstate | (PIND & 0x0C); // Update Oldstate 
        
        position += *(offsets+temp);       // Update Position
        
        oldstate = temp >> 2; 
}





























