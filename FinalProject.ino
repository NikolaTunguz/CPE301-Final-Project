// CPE301 Final Project
// Nikola Tunguz
// December 15, 2023

#include <LiquidCrystal.h>
#include <dht.h>
#include <Stepper.h>
#include <uRTCLib.h>

#define RDA 0x80
#define TBE 0x20

//DHT
dht DHT;
#define DHT11_PIN 7

//RTC
uRTCLib rtc(0x68);
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

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

volatile unsigned char *portG = 0x34; //Fan motor 
volatile unsigned char *portDDRG = 0x33;

volatile unsigned char *portD = 0x2B; //Stepper buttons
volatile unsigned char *portDDRD = 0x2A; 
volatile unsigned char *pinD = 0x29; 

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

//Fan Motor
int in4 = 4; //portG5
int in3 = 3; //portE5

//Stepper Motor
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 8, 10, 9,11);

//global variables
bool disabled = true;
bool error, idle, running;

unsigned int waterThreshold = 25;
unsigned int waterLevel = 0;
unsigned int tempThreshold = 28;

bool changedState = false;
char* state = "Disabled";

unsigned long previousMillis = 0;
const long interval = 1000; 

void setup() {
  U0Init(9600);
  adc_init();
  lcd.begin(16, 2);

  URTCLIB_WIRE.begin();
  //rtc.set(0,48,13,6,15,12,23);
  //second, minute, hour, dayofweek, day of month, month, year

  //set LEDs to output and LOW (PF0-3)
  *portDDRF |= 0x0F;
  *portF &= ~(0x0F);

  //set PE4 to input (Start/Disable Button)
  *portDDRE *= 0x10;
  attachInterrupt(digitalPinToInterrupt(2), button, FALLING);

  //set PD2/3 to input for stepper motor
  *portDDRD *= 0x0C; 

  //set PG5 & PE5 to output and low for fan motor
  *portDDRG |= 0x20;
  *portG &= ~(0x20);
  *portDDRE |= 0x20;
  *portE &= ~(0x20);

  putChar('\n');
  writeWord("Program Started: ");
  displayTime();
}

void loop() {
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;
    writeData();
  }
  if(changedState == true){
    writeWord(state);
    displayTime();
  }

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
  myDelay(250);
}

//state functions
void disableState(){
  changedState = false;
  lightYellow();
  adjustVent();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write("System Disabled");
}
void errorState(){
  changedState = false;
  lightRed();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write("Water level is");
  lcd.setCursor(0, 1);
  lcd.write("too low.");
  //turn fan off
  *portG &= ~(0x20);
  *portE &= ~(0x20);
  //see if to leave error
  waterLevel = adc_read(5);
  if(waterLevel > waterThreshold){
    idle = true;
    error = false;
    //disabled = false; 
    running = false;
    if(state == "Idle: "){
      changedState = false;
    }
    else{
      state = "Idle: ";
      changedState = true;
    }
  }
}
void idleState(){
  if(changedState){
    writeData();
  }
  changedState = false;
  lightGreen();
  adjustVent();
  checkWater();
  if(error == false){
    checkTemp();
  }
}
void runningState(){
  if(changedState){
    writeData();
  }
  changedState = false;
  lightBlue();
  adjustVent();
  checkWater();
  if(error == false){
    checkTemp();
  }
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
  int chk = DHT.read11(DHT11_PIN);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.write("Temp: ");
  lcd.print(DHT.temperature); 
  
  lcd.setCursor(0, 1);
  lcd.write("Humidity: "); 
  lcd.print(DHT.humidity);
}

//RTC print time
void displayTime(){
  rtc.refresh();
  
  writeNumber(rtc.day());
  putChar('/'); 
  writeNumber(rtc.month());
  putChar('/'); 
  writeNumber(rtc.year());

  putChar(' ');

  putChar('(');
  writeWord(daysOfTheWeek[rtc.dayOfWeek() - 1]) ;
  putChar(')'); 

  putChar(' ');
  
  writeNumber(rtc.hour());
  putChar(':');
  writeNumber(rtc.minute());
  putChar(':');
  writeNumber(rtc.second());
  putChar('\n');
  
}
void checkTemp(){
  if(DHT.temperature > tempThreshold ){
    //turn fan on & go into running
    *portG |= 0x20;
    *portE |= 0x20;
    idle = false;
    error = false;
    //disabled = false; 
    running = true;
    if(state == "Running: "){
      changedState = false;
    }
    else{
      state = "Running: ";
      changedState = true;
    }
  }
  else{
    //turn fan off & go into idle
    *portG &= ~(0x20);
    *portE &= ~(0x20);
    idle = true;
    error = false;
    //disabled = false; 
    running = false;
    if(state == "Idle: "){
      changedState = false;
    }
    else{
      state = "Idle: ";
      changedState = true;
    }
  }
}

void checkWater(){
  waterLevel = adc_read(5);
  if(waterLevel < waterThreshold){
    idle = false;
    error = true;
    //disabled = false; 
    running = false;
    if(state == "Error: "){
      changedState = false;
    }
    else{
      state = "Error: ";
      changedState = true;
    }
  }
}

// enable/disable button interrupt
void button(){
  if( disabled ){
    changedState = true;
    state = "Idle: ";

    disabled = false;
    idle = true;
  }
  else{
    changedState = true;
    state = "Disabled: ";

    disabled = true;
    error = false;
    idle = false;
    running = false;
  }
}

void adjustVent(){
  if(*pinD & 0x04){
    myStepper.step(100);
    writeWord("Vent Turned: ");
    displayTime();
    myDelay(250);
    
  }
  else if(*pinD & 0x08){
    myStepper.step(-100);
    writeWord("Vent Turned: ");
    displayTime();
    myDelay(250);
    
  }
  
}

void myDelay(int seconds){
  // number of ticks needed for the delay
  int freq = 9600 / seconds;
  double halfPeriod = 1.0/ double(freq) / 2.0;
  double clock = 0.0000000625;
  unsigned long currentTicks = halfPeriod / clock;

  // Configure the timer registers
  *myTCCR1A = 0x00;  
  *myTCCR1B = 0x00;  
  *myTCNT1 = 0xFFFF - currentTicks;  
  // Start the timer
  *myTCCR1B |= 0x01;
  // Wait for the timer overflow flag
  while ((*myTIFR1 & 0x01) == 0);
  // Stop the timer
  *myTCCR1B &= 0xF8;
  // Clear the overflow flag
  *myTIFR1 |= 0x01;
}

//UART FUNCTIONS
void writeWord(char message[]){
  for(int i = 0; message[i] != '\0' ; i++){
    putChar(message[i]);
  }
}
void writeNumber(int number){
  //convert number to ascii values
  if (number > 9){
    int upperDigits = number / 10;
    int onesPlace = number % 10;
    writeNumber(upperDigits);
    writeNumber(onesPlace);
  }
  else{
    putChar(number + 48);
  }
}

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
void adc_init(){
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
unsigned int adc_read(unsigned char adc_channel_num){
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
