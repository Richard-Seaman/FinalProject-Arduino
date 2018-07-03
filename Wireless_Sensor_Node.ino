// Date:           03-07-2018
// Author:         Richard Seaman
// Student Number: 17119111
// project:        Sensors & Sockets

// This is the Arduino sketch for the wireless sensor node
// It utilises the xbee-arduino library by Andrew Rapp
// The XBee Series 1 transmitter example was used as a basis for this sketch
// The example was accessed on 03/07/2018 at the followinf link:
// https://github.com/andrewrapp/xbee-arduino/blob/master/examples/Series1_Tx/Series1_Tx.pde

// SHIELD MUST BE IN UART TO WORK!!!
 
#include <XBee.h>
#include <dht.h>

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
}

// The continuous loop which begins after the setup
void loop() {

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
  delay(10000);
}
