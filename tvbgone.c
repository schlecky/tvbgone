#include <legacymsp430.h>
#include <msp430g2553.h>

#include "codes.c"

#define DELAY_CNT 13

#define IR_OUT P1OUT
#define IR_DIR P1DIR
#define IR_PIN BIT2

#define LED_PIN BIT5
#define LED_OUT P3OUT
#define LED_DIR P3DIR

#define BTNOFF_PIN BIT6
#define BTNOFF_DIR P1DIR
#define BTNOFF_IE P1IE
#define BTNOFF_IES P1IES
#define BTNOFF_REN P1REN
#define BTNOFF_OUT  P1OUT

unsigned char* code_ptr;
int bitsleft_r = 0;
unsigned char bits_r=0;
int fin=0;

NAKED(_reset_vector__)
{
	/* place your startup code here */

	/* Make sure, the branch to main (or to your start
	   routine) is the last line in the function */
	__asm__ __volatile__("br #main"::);
}

// This function delays the specified number of 10 microseconds
// it is 'hardcoded' and is calibrated by adjusting DELAY_CNT 
// in main.h Unless you are changing the crystal from 8mhz, dont
// mess with this.

// Shortcut to insert single, non-optimized-out nop
#define NOP __asm__ __volatile__ ("nop")
void delay_ten_us(int us) {
  int timer;
  while (us != 0) {
    // for 8MHz we want to delay 80 cycles per 10 microseconds
    // this code is tweaked to give about that amount.
    for (timer=0; timer <= DELAY_CNT; timer++) {
      NOP;
      NOP;
    }
    NOP;
    NOP;
    NOP;
    us--;
  }
}

// we cant read more than 16 bits at a time so dont try!
int read_bits(int count)
{
  int i;
  int tmp=0;
  
  // we need to read back count bytes
  for (i=0; i<count; i++) {
    // check if the 8-bit buffer we have has run out
    if (bitsleft_r == 0) {
      // in which case we read a new byte in
      bits_r = *(code_ptr++);
      // and reset the buffer size (8 bites in a byte)
      bitsleft_r = 8;
    }
    // remove one bit
    bitsleft_r--;
    // and shift it off of the end of 'bits_r'
    tmp |= (((bits_r >> (bitsleft_r)) & 1) << (count-1-i));
  }
  // return the selected bits in the LSB part of tmp
  return tmp; 
}

void flash_led()
{
  LED_OUT |= LED_PIN;
  delay_ten_us(1000);
  LED_OUT &= ~LED_PIN;
}

void xmitCodeElement(int ontime, int offtime, int PWM_code )
{
  //allume
  if(PWM_code)
    TA0CCTL1 = OUTMOD_4;
  else
    TA0CCTL1 = OUTMOD_0 | OUT;
  delay_ten_us(ontime);
  
  //eteint
  TA0CCTL1 = OUTMOD_0;
  delay_ten_us(offtime);
}

void main (void)
{
  
  WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer
  BCSCTL1 = CALBC1_8MHZ;          //run at 8Mhz
  DCOCTL = CALDCO_8MHZ;
  
  LED_DIR |= LED_PIN;
  LED_OUT &= ~LED_PIN;

  // bouton off
  BTNOFF_REN |= BTNOFF_PIN;
  BTNOFF_OUT |= BTNOFF_PIN;
  BTNOFF_IES |= BTNOFF_PIN;
  BTNOFF_IE |= BTNOFF_PIN;
  WRITE_SR(GIE); 		            //Enable global interrupts

  P1SEL |= IR_PIN;
  P1DIR |= IR_PIN;

  TA0CTL = TASSEL_2 | MC_1 ;   //Selectionne l'horloge rapide et mode up

  TA0CCTL1 = OUTMOD_4;
 
  int i=0;

  for(i=0;(i<num_EUcodes) && (fin==0);i++)
  {
    code_ptr = (unsigned char*)(*(EUpowerCodes+i)); 
    TA0CCR0 = *(code_ptr++);
    unsigned char numpairs = *(code_ptr++);
    unsigned char bitcompression = *(code_ptr++);
    code_ptr++; //allignement
    int* time_ptr = (int*) *((int*)code_ptr);
    code_ptr+=2;
    // traite chaque paire du code
    int k=0;
    for (k=0; k<numpairs; k++) {
      int ti = (read_bits(bitcompression)) * 2;
      //ti=0;
      int on_ti = time_ptr[ti];     // read word 1 - ontime
      int off_ti = time_ptr[ti+1];  // read word 2 - offtime   
      xmitCodeElement(on_ti, off_ti, (TA0CCR0!=0)); 
    }
   
    //Flush remaining bits, so that next code starts
    //with a fresh set of 8 bits.
    bitsleft_r=0;	
    // delay 250 milliseconds before transmitting next POWER code
    flash_led();
    delay_ten_us(25000);
   // if(i==5) break;
  }
  
  fin = 0;
  
  for(i=0;(i<num_NAcodes) && (fin==0);i++)
  {
    code_ptr = (unsigned char*)(*(NApowerCodes+i)); 
    TA0CCR0 = *(code_ptr++);
    unsigned char numpairs = *(code_ptr++);
    unsigned char bitcompression = *(code_ptr++);
    code_ptr++; //allignement
    int* time_ptr = (int*) *((int*)code_ptr);
    code_ptr+=2;
    // traite chaque paire du code
    int k=0;
    for (k=0; k<numpairs; k++) {
      int ti = (read_bits(bitcompression)) * 2;
      //ti=0;
      int on_ti = time_ptr[ti];     // read word 1 - ontime
      int off_ti = time_ptr[ti+1];  // read word 2 - offtime   
      xmitCodeElement(on_ti, off_ti, (TA0CCR0!=0)); 
    }
   
    //Flush remaining bits, so that next code starts
    //with a fresh set of 8 bits.
    bitsleft_r=0;	
    // delay 250 milliseconds before transmitting next POWER code
    flash_led();
    delay_ten_us(25000);
   // if(i==5) break;
  }
  LPM4;
}

// Port 2 interrupt service routine
interrupt (PORT1_VECTOR) Port_1(void)
{
  fin=1;
  P1IFG=0;
}

