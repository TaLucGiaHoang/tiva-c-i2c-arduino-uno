#include <Wire.h>

void setup() {
  Wire.begin(0x3B);                // join i2c bus with address 0x3B
  Wire.onRequest(requestEvent); // register event when master request to send 
  Wire.onReceive(receiveEvent); // register event when arduino receives
  Serial.begin(115200);         // start serial for output
  Serial.println("I2C Slave Sender + Receiver - addr:0x08, baud:100000(Hz)");
}

void loop() {
  delay(100);
}

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent() {
  Wire.write("hello "); // respond with message of 6 bytes
  //Serial.println("send to master"); // debug only, print in even handler can miss data
                                      // expected by master
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  while (Wire.available()) { // loop through all but the last
    char c = Wire.read(); // receive byte as a character
    Serial.print(c);         // print the character
  }
}
