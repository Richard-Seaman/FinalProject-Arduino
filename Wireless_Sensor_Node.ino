// Date:           03-07-2018
// Author:         Richard Seaman
// Student Number: 17119111
// project:        Sensors & Sockets

// This is the Arduino sketch for the wireless sensor node
// It utilises the xbee-arduino library by Andrew Rapp
// The XBee Series 1 transmitter example was used as a basis for this sketch
// The example was accessed on 03/07/2018 at the followinf link:
// https://github.com/andrewrapp/xbee-arduino/blob/master/examples/Series1_Tx/Series1_Tx.pde

// A youtube tutorial by Ralph Bacon was followed in order to allow the Arduino to sleep between transmissions
// The youtube tutorial was accessed on 4/07/2018 at the following link:
// https://www.youtube.com/watch?v=jqFl8ydUzZM
// The tutorial code was accessed on 4/07/2018 at the following link:
// https://github.com/RalphBacon/Arduino-Deep-Sleep/blob/master/Sleep_ATMEGA328P_Timer.ino

// NB: SHIELD MUST BE IN UART TO WORK!!!
 
#include <XBee.h>
#include <dht.h>

// For sleeping
#include "Arduino.h"
#include <avr/sleep.h>
#include <avr/wdt.h>

// Volatile as it's used within interrupt
volatile char sleepCnt = 0;

XBee xbee = XBee();
dht DHT;

// Create a payload to send with the frame
// Only need to send a single byte for temperature and another for humidty
// Range of 0-255 is acceptable
uint8_t payload[2] = {0, 0};  // {humidity, temperature}

// Configure the transmission request and response
// 16-bit addressing (Note: coordinator 16-bit Source Address = 1234)
Tx16Request tx = Tx16Request(0x1234, payload, sizeof(payload));
TxStatusResponse txStatus = TxStatusResponse();

// LED pins
// LEDs are used as indicators for status / error / success
int statusLed = 11;
int errorLed = 12;
int successLed = 10;

// DHT sensor pin
// (blue type used)
#define DHT11_PIN 7

// Simple function to flash an LED at the given pin, a given number of times and duration
// (unchanged from Andrew Rapp's function)
void flashLed(int pin, int times, int wait) {
    
    for (int i = 0; i < times; i++) {
      digitalWrite(pin, HIGH);
      delay(wait);
      digitalWrite(pin, LOW);
      
      if (i + 1 < times) {
        delay(wait);
      }
    }
}

// Do the initial setup for the sketch
void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  pinMode(successLed, OUTPUT);
  Serial.begin(9600);
  xbee.setSerial(Serial);
  Serial.println("Setup completed.");
}

// The continuous loop which begins after the setup
void loop() {

  // NB: not original code!!
  // code for sleeping is mostly copied from Ralph Bacon's tutorial
  // with only minor modifications made as required

  ////////////////////////////
  // Start of Ralph Bacon's code
  
  // Disable the ADC (Analog to digital converter, pins A0 [14] to A5 [19])
  static byte prevADCSRA = ADCSRA;
  ADCSRA = 0;

  // Set the type of sleep mode we want. (deep sleep)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Note, sleepCnt * 1s = actual sleep time  (900 = 15min)
  while (sleepCnt < 900) {

    // Turn of Brown Out Detection (low voltage). 
    // This is automatically re-enabled upon timer interrupt
    sleep_bod_disable();
    
    // Ensure we can wake up again by first disabling interrupts (temporarily) so
    // the wakeISR does not run before we are asleep and then prevent interrupts,
    // and then defining the ISR (Interrupt Service Routine) to run when poked awake by the timer
    noInterrupts();

    // clear various "reset" flags
    MCUSR = 0;  // allow changes, disable reset
    WDTCSR = bit (WDCE) | bit(WDE); // set interrupt mode and an interval
    WDTCSR = bit (WDIE) | bit(WDP2) | bit(WDP1); //| bit(WDP0);    // set WDIE, and 1 second delay
    wdt_reset();

    // Send a message just to show we are about to sleep
    //Serial.println("Good night!");
    //Serial.flush();

    // Allow interrupts now
    interrupts();

    // And enter sleep mode as set above
    sleep_cpu();
  
  }

  // --------------------------------------------------------
  // µController is now asleep until woken up by an interrupt
  // --------------------------------------------------------

  // Prevent sleep mode, so we don't enter it again, except deliberately, by code
  sleep_disable();

  // Wakes up at this point when timer wakes up µC
  Serial.println("I'm awake!");

  // Reset sleep counter
  sleepCnt = 0;

  // Re-enable ADC if it was previously running
  ADCSRA = prevADCSRA;

  // End of Ralph Bacon's code
  ////////////////////////////

  // Code to do when awoken from sleep below...

  // Read the humidity and temperature
  int chk = DHT.read11(DHT11_PIN);
  int h = DHT.humidity;
  int t = DHT.temperature;
  // And print them out
  Serial.print("Temperature = ");
  Serial.println(t);
  Serial.print("Humidity = ");
  Serial.println(h);

  // Make sure they're okay before continuing
  if (!isnan(t) && !isnan(h)) {

    // convert humidity into a byte and assign to payload
    payload[0] = (char) h;
    //Serial.print("Humidity Payload: ");
    //Serial.println(payload[0]);
    
    // and temperature
    payload[1] = (char) t;
    //Serial.print("Temperature Payload: ");
    //Serial.println(payload[1]);
     
    //Serial.println("");
    Serial.println("Transmitting...");
    xbee.send(tx);

    // flash TX indicator
    flashLed(statusLed, 1, 100);

    // after sending a tx request, we expect a status response
    // wait up to 5 seconds for the status response
    if (xbee.readPacket(5000)) {
        // got a response!
      Serial.println("Got a response!");
      
        // should be a znet tx status              
      if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
         xbee.getResponse().getTxStatusResponse(txStatus);
        
         // get the delivery status, the fifth byte
           if (txStatus.getStatus() == SUCCESS) {
              // success.  time to celebrate
              flashLed(successLed, 5, 50);
           } else {
              // the remote XBee did not receive our packet. is it powered on?
              flashLed(errorLed, 3, 500);
           }
        }      
    } else if (xbee.getResponse().isError()) {
      Serial.println("Error reading packet.  Error code: ");
      Serial.println(xbee.getResponse().getErrorCode());
    } else {
      // local XBee did not provide a timely TX Status Response.  
      // Radio is not configured properly or connected
      Serial.println("local XBee did not provide a timely TX Status Response");
      // Flash the error LED
      flashLed(errorLed, 2, 50);
    }
      
  }
  
  // Delay the loop  
  //delay(300000);
}

// NB: required function for wdt (watchdog timer)
// When WatchDog timer causes µC to wake it comes here
ISR (WDT_vect) {

  // Turn off watchdog, we don't want it to do anything (like resetting this sketch)
  wdt_disable();

  // Increment the WDT interrupt count
  sleepCnt++;

  // Now we continue running the main Loop() just after we went to sleep
}
