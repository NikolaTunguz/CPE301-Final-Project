// CPE301 Final Project
// Nikola Tunguz
// December 15, 2023

#include <LiquidCrystal.h>

#define RDA 0x80
#define TBE 0x20

// UART Pointers
volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

// ADC Pointers
volatile unsigned char *my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int  *my_ADC_DATA = (unsigned int*) 0x78;

// GPIO Pointers
volatile unsigned char *portE = 0x2E; //Start/Stop Button
volatile unsigned char *portDDRE = 0x2D; 

volatile unsigned char *portF = 0x31; //LEDs
volatile unsigned char *portDDRF = 0x30; 

// Timer Pointers
volatile unsigned char *myTCCR1A  = 0x80;
volatile unsigned char *myTCCR1B  = 0x81;
volatile unsigned char *myTCCR1C  = 0x82;
volatile unsigned char *myTIMSK1  = 0x6F;
volatile unsigned char *myTIFR1   = 0x36;
volatile unsigned int  *myTCNT1   = 0x84;

//LCD
const int RS = 52, EN = 53, D4 = 50, D5 = 51, D6 = 48, D7 = 49; //LCD pins to digital pins
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

//global variables
bool disabled = true;
bool error, idle, running;
unsigned int waterThreshold = 25;
unsigned int waterLevel;

void setup() {
  U0Init(9600);
  //Serial.begin(9600);
  setup_timer_regs();
  adc_init();
  lcd.begin(16, 2);

  //set LEDs to output and LOW (PF0-3)
  *portDDRF |= 0x0F;
  *portF &= ~(0x0F);

  //set PE4 to input (Start/Disable Button)
  *portDDRE *= 0x10;
  attachInterrupt(digitalPinToInterrupt(2), button, FALLING);
}

void loop() {

  if(disabled){
    disableState();
  }
  else if (error){
    errorState();
  }
  else if(idle){
    idleState();
  }
  else if (running){
    runningState();
  }
  
  
}
//state functions
void disableState(){
  lightYellow();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write("System Disabled");
}
void errorState(){
  lightRed();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write("Water level is");
  lcd.setCursor(0, 1);
  lcd.write("too low.");
  
  waterLevel = adc_read(5);
  if(waterLevel > waterThreshold){
    idle = true;
    error = false;
    disabled = false; 
    running = false;
  }
}
void idleState(){
  lightGreen();
  writeData();
  checkWater();
}
void runningState(){
  lightBlue();
  writeData();
  checkWater();
}

//LED light up functions
void lightYellow(){
  
  *portF &= ~(0x0F); // turn others off
  *portF |= 0x01; // turn on port 0
}
void lightRed(){
  *portF &= ~(0x0F); // turn others off
  *portF |= 0x02; // turn on port 1
}
void lightGreen(){
  *portF &= ~(0x0F); // turn others off
  *portF |= 0x04; // turn on port 2
}
void lightBlue(){
  *portF &= ~(0x0F); // turn others off
  *portF |= 0x08; // turn on port 3
}

//updates temperature and humidity values
void writeData(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write("Temperature:");
  lcd.setCursor(0, 1);
  lcd.write("Humidity:");
}
void checkWater(){
  waterLevel = adc_read(5);
  if(waterLevel < waterThreshold){
    idle = false;
    error = true;
    disabled = false; 
    running = false;
  }
}
// Timer setup function
void setup_timer_regs(){
  // setup the timer control registers
  *myTCCR1A= 0x80;
  *myTCCR1B= 0X81;
  *myTCCR1C= 0x82;
  
  // reset the TOV flag
  *myTIFR1 |= 0x01;
  
  // enable the TOV interrupt
  *myTIMSK1 |= 0x01;
}

// TIMER OVERFLOW ISR       ->>>> IDK IF THIS IS USEFUL FOR THIS PROJECT PROBABLY REMOVE LATER
unsigned int currentTicks;
ISR(TIMER1_OVF_vect)
{
  // Stop the Timer
  *myTCCR1B &= 0xF8;
  // Load the Count
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  // Start the Timer
  *myTCCR1B |= 0x01;
  // if it's not the STOP amount
  if(currentTicks != 65535)
  {
    // XOR to toggle PB6
    //*portB ^= 0x40;
  }
}

// enable/disable button interrupt
void button(){
  if( disabled ){
    disabled = false;
    idle = true;
  }
  else{
    disabled = true;
    error = false;
    idle = false;
    running = false;
  }
}

//UART FUNCTIONS
void U0Init(int U0baud){
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char kbhit(){
  return *myUCSR0A & RDA;
}
unsigned char getChar(){
  return *myUDR0;
}
void putChar(unsigned char U0pdata){
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}
//ADC functions
void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}
