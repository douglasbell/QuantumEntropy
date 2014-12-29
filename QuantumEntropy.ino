/**
 * Uses a geiger counter to provide entropy for true random number generation
 * 
 * Author: Doug Bell (douglas.bell@gmail.com)
 *
 * 3rd Party Libaries
 *
 * http://playground.arduino.cc/Code/QueueList
 * https://github.com/ivanseidel/ArduinoThread
 * http://playground.arduino.cc/Code/LCDi2c
 */
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <QueueList.h>
#include <ThreadController.h>
#include <Thread.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

// LCD
#define I2C_ADDR         0x3F
#define BACKLIGHT_PIN    3
#define EN_PIN           2
#define RW_PIN           1
#define RS_PIN           0
#define D4_PIN           4
#define D5_PIN           5
#define D6_PIN           6
#define D7_PIN           7
    
// TTL for recorded events
#define TTL_IN_MS       60000

// Constant for the number of bytes in a kilobyte
const int k = 1000;
  
// Constant defining memory measurement sizes
const String sizes[5] = {"B", "KB", "MB", "GB", "TB"};

// Thread Controller
ThreadController controller = ThreadController();

// Update Thread
Thread updateThread = Thread();

// LCD setup of address and pins
LiquidCrystal_I2C lcd(I2C_ADDR,EN_PIN,RW_PIN,RS_PIN,D4_PIN,D5_PIN,D6_PIN,D7_PIN);

// The QueueList containing the event millis received for the last 60 seconds 
QueueList <long> timingQueue;

// The total number of bits received
long bitCount = 0;

// The index for the bit loop
int idx = 0;

// The time between the last 4 events for unbiased bit output
long t[4];

long lastMillis = 0;

/**
 * Setup the sketch
 */
void setup() {                
  Serial.begin(9600);
  
  // Setup LCD
  lcd.begin (20,4,LCD_5x8DOTS);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  
  // Initialize Update Thread
  updateThread.enabled = true; // Default enabled value is true
  updateThread.setInterval(100); // Sets the wanted interval to be 50ms
  updateThread.onRun(update); 

  // Add the Threads to the ThreadController
  controller.add(&updateThread);
}

/** 
 * The processing loop
 */
void loop() { 
  // Start the Thread Controller Run
  controller.run();
  
  // See if we have received a bit from the geiger counter and push it on the timingQueue
  while (Serial.available()) {
    int incomingByte = Serial.read();
    if (isdigit(incomingByte)) {
      long t1 = millis();
      
      timingQueue.push(t1);
      bitCount++;
      
      // Calc the diff in time between two distinct events t1 and t2 + t3 and t4
      // XOR the bits to remove bias
      t[idx] = t1 - lastMillis;
      idx++;
 
      if (idx == 4) {
        
        int unbiasedBit;

        int bit1 = (long)t[1] < (long)t[0] ? 0 : 1;
        int bit2 = (long)t[3] > (long)t[2] ? 1 : 0;
        
        // XOR both bits to remove bias
        if ((bit1 == 1 && bit2 == 1) || (bit1 == 0 && bit2 == 0)) {
	  unbiasedBit = 0;
        } else if ((bit1 == 1 && bit2 == 0) || (bit1 == 0 && bit2 == 1)) {
          unbiasedBit = 1;
        } // XOR Complete
        
        Serial.print(unbiasedBit);	
        
        idx = 0; // Reset idx
      }
      
      lastMillis = t1; // Set the current millis() reading for the next pass
    }
  }
}

/** 
 * Complete the update by clearing any entries that have execced TTL
 */ 
void update() {
  
   // Clean the queue if an event has lingered for more than the TTL_IN_MS value 
  if (timingQueue.count() > 0 && millis() > timingQueue.peek() + TTL_IN_MS) {
    while (timingQueue.count() > 0 && millis() > timingQueue.peek() + TTL_IN_MS) {
      timingQueue.pop(); // Remove the entry from the queue
    } 
  } 
  
  // Update the LCD display
  lcd.home ();
  
  // Line 1
  lcd.setCursor (0, 0); 
  lcd.print(timingQueue.count());
  lcd.print(" CPM");
  lcd.print(" ");
  lcd.print((float)timingQueue.count() / 1000.0);
  lcd.print(" mR/hr"); 

  // Line 2
  lcd.setCursor(0, 1); 
 
  int index = !bitCount ? 0: (int)floor(log(bitCount) / log(k));
 
  lcd.print((int)(bitCount / pow(k, index)));
  lcd.print(sizes[index]);
  lcd.print(" in ");
 
  long seconds = millis() / 1000;
 
  lcd.print(seconds);
  lcd.print((seconds == 1 ? " sec " : " secs ")); 
  
  // Line 3
  lcd.setCursor(0, 2);  
  lcd.print(((float)bitCount / ((float)millis() / 1000.0)));
  lcd.print(" bits/sec");

  // Line 4
  //lcd.setCursor(0,3);
}
