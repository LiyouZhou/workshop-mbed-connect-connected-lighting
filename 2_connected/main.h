#include "mbed.h"
#include "rtos.h"
#include "security.h"                         // Load the security certificate
#include "easy-connect.h"                     // Connectivity driver
#include "simple-mbed-client.h"               // Load mbed Client
#include "ChainableLED.h"                     // Driver for the LED
#include "accelerometer.h"                    // Driver for the accelerometer

SimpleMbedClient client;          // Get a reference to Client

// Our peripherals
Accelerometer accelerometer(10, true); // Declare the accelerometer
ChainableLED rgbLed(D6, D7, 1);   // Declare the LED (it's chainable!)

// We need a way to signal from an interrupt context -> main thread, use a Semaphore for it...
Semaphore updates(0);

// Fwd declaration
void putLightsOn();
void colorChanged(int newColor);

// Variable that holds whether the light is on because the PIR sensor triggered (and timeout didnt happen yet)
bool ledOnBecauseOfPir = false;

// Timeout based on led/0/timeout, disables the light after a set interval
Timeout pirTimeout;

// Permanent statuses (set by led/0/permanent_status)
#define STATUS_NONE     0
#define STATUS_ON       1
#define STATUS_OFF      2

// clear the lights
void putLightsOff() {
  rgbLed.setColorRGB(0, 0x0, 0x0, 0x0);
}

// Status changes
void statusChanged(int newStatus) {
  switch (newStatus) {
    case STATUS_ON: // Permanently on? OK.
      putLightsOn();
      break;
    case STATUS_NONE: // Otherwise listen to PIR sensor
    case STATUS_OFF:  // Or be off forever
      putLightsOff();
      break;
  }
}

// Here are our resources:

// YOUR CODE HERE (remove the next 5 lines first!!!)
// We encode color in 3 bytes [R, G, B] and put it in an Int (default color: blue)
int ledColor = 0x0000ff;
int ledTimeout = 5;
int ledStatus = STATUS_NONE;
int pirCount = 0;
// END OF YOUR CODE HERE


// As said above, color is encoded in three bytes
void putLightsOn() {
  // parse the individual channels
  int redCh   = ledColor >> 16 & 0xff;
  int greenCh = ledColor >> 8 & 0xff;
  int blueCh  = ledColor & 0xff;

  rgbLed.setColorRGB(0, redCh, greenCh, blueCh);
}

// Color updated from the cloud,
// if the LED is on because of the PIR, or if the LED is on permanently -> Set the color.
void colorChanged(int newColor) {
  if (ledOnBecauseOfPir || ledStatus == STATUS_ON) {
    putLightsOn();
  }
}

// Timeout (from led/0/timeout) happened after PIR sensor was triggered...
void onPirTimeout() {
  // if we're not permanent on
  if (ledStatus != STATUS_ON) {
    // clear the lights
    putLightsOff();

    ledOnBecauseOfPir = false;
  }
}

// When the motion sensor fires...
void movement_detected() {
  printf("Motion Sensor fired\r\n");

  // go back to main thread to update the resource (we can't do this in interrupt context)
  updates.release();

  // Permanent off? Don't put the lights on...
  if (ledStatus == STATUS_OFF) return;

  // Otherwise do it!
  ledOnBecauseOfPir = true;
  putLightsOn();

  // And attach the timeout
  pirTimeout.attach(&onPirTimeout, static_cast<float>(ledTimeout));
}

// Registered callback for mbed Client
DigitalOut statusLed(LED1, 1);
bool isRegistered = false;
void registered() {
  isRegistered = true;
  statusLed = 0;
}

int main(int, char**) {
  printf("Hello world\r\n");

  putLightsOff();

  // Motion sensor runs in a separate RTOS thread

  // Connect to the internet (using connectivity method from mbed_app.json)
  NetworkInterface* network = easy_connect(true);
  if (!network) {
    printf("Connect to internet failed... See serial output.\r\n");
    return 1;
  }

  // Set the DeviceType to light-system and connect to mbed Device Connector
  struct MbedClientOptions options = client.get_default_options();
  options.DeviceType = "light-system";
  if (!client.setup(options, network)) {
    printf("Setting up mbed_client failed...\r\n");
    return 1;
  }

  client.on_registered(&registered);
  
  accelerometer.start();
  accelerometer.change(&movement_detected);

  // Main loop. We don't want to do Network operations from ISR
  while (1) {
    int v = updates.wait(25000);
    if (v == 1) {
      pirCount = pirCount + 1;
    }
    if (isRegistered) {
      client.keep_alive();
    }
  }
}