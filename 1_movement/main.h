#include "mbed.h"                 // Load the mbed libraries
#include "ChainableLED.h"         // Driver for the LED

InterruptIn pir(D5);              // Declare the PIR sensor
ChainableLED rgbLed(D6, D7, 1);   // Declare the LED (it's chainable!)

Timeout pirTimeout;               // Timer we're using to disable the light after X seconds

// YOUR CODE HERE


int main() {

  printf("Hello world\r\n");

  // set the color to RED at startup, so we can verify that wiring is correct
  rgbLed.setColorRGB(0, 0xff, 0x0, 0x0);

  pir.rise(&pir_rise);

  while (1) {
    wait_ms(500);
  }

}
