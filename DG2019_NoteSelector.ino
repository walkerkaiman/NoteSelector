/**
  Title:    Chimes

  Event:    Digital Graffiti 2019

  Purpose:  To classify a measurement of an accelerometer into
            one of serveral categories. The classification is then
            sent to a video server and an audio server via OSC UDP
            over WiFi.

            This firmware determine how high someone is swinging,
            and then sends the classification to the host computer
            once the swing enters the zone.

            The OSC message will have a unique address for each sensor.
            This OSC address will be used to the identify which sensor
            is sending the system values.

            There are three classifications: NOTE_A, NOTE_B, NOTE_C.
            They report char values of '1', '2', '3'.

            The zone is the physical, lowest point of the swing. Its
            value is determined by NOTE_A. Accelerometer values that
            are lessthan NOTE_A are considered to be inside the zone
            and will not report a note until the swing raises above
            NOTE_A.

  Hardware: Microcontroller - Adafruit HUZZAH with ESP8266
            Accelerometer   - Adafruit MMA8451
            Battery         - Lithium Ion 3.7v 2200 mAh

  Hook-Up:  The accelerometer communicates over the I2C bus on the HUZZAH.
            This makes for a simple wiring. The accelerometer grabs power
            from the 3V pin on the HUZZAH and ground from the HUZZAH's GND pin.

  Diagram:

                                   ________________________________
                                  |                                |
                                  |   3.7 2200 mAh Lithium Ion     |
                                  |__|_____________________________|
                           __________|__________________________
                          |   _______|______________________    |
   _________________      |  |     __|______________________|___|__
  |                 |     |  |    | BAT                    SCL SDA |
  |                 |     |  |    |                                |
  |     MMA8451     |     |  |    |                                |
  |                 |     |  |    |        Adafruit HUZZAH         |
  |                 |     |  |    |                                |
  | VIN GND SCL SDA |     |  |    | 3V  GND                        |
  |__|___|___|___|__|     |  |    |_|____|_________________________|
     |   |   |   |        |  |      |    |
     |   |   |   |________|  |      |    |
     |   |   |_______________|      |    |
     |   |__________________________|____|
     |______________________________|

  @author   Kaiman Walker
  @email    walkerkaiman@gmail.com
  @version  1.1 // April 11, 2019
*/

// Accelerometer Libraries
#include <Wire.h>
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>

// WiFi Libraries
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// WiFi Network Settings
const char         NETWORK   []  = "Chimes";
const char         PASSWORD  []  = "chimes123";
const IPAddress    SOUND_IP (192, 168, 0, 101);
const IPAddress    VIDEO_IP (192, 168, 0, 161);

// UDP Message Settings
WiFiUDP udp;
const unsigned int NOTE_DELAY    = 750;
const unsigned int MESSAGE_DELAY = 10;
const unsigned int LOCAL_PORT    = 9000;
const unsigned int REMOTE_PORT   = 8000;

// Board Settings
const unsigned int BAUD_RATE     = 115200;

// Classifier Settings
const byte         NAME          = 'A';
const byte         NOTE_A        = 50;
const byte         NOTE_B        = 100;
const byte         NOTE_C        = 200;

// The following is assigned at runtime. Do not assign here.
Adafruit_MMA8451 mma = Adafruit_MMA8451();
WiFiUDP Udp;

byte    highestSwing, currentSwing;
boolean inZone, prevInZone;

void setup() {
  // Microcontroller Initialization
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(BAUD_RATE);

  // Accelerometer Initialization
  if (mma.begin() == false) {
    Serial.println("Couldnt start tilt sensor");
    while (true); // This will cause the board to timeout and force a restart
  }

  mma.setRange(MMA8451_RANGE_2_G);

  // WiFi Initialization
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(NETWORK);
  WiFi.begin(NETWORK, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("UDP server on port %d\n", LOCAL_PORT);

  Udp.begin(LOCAL_PORT);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());

  // Initialize global flags and variables
  highestSwing = 0;
  currentSwing = 0;

  digitalWrite(LED_BUILTIN, HIGH);

  sendNote('0');
}

void loop() {
  updateStates();

  // If we endered the zone in this frame
  if (prevInZone == false && inZone == true) {
    char newNote = classifyNote(highestSwing);

    Serial.println("Sending Note: ");
    Serial.println(newNote);
    Serial.println(highestSwing);
    sendNote(newNote);
  }

  prevInZone = inZone;
}

/**
    updateStates

    This updates the global variables and global flags on each frame.

    @param none
    @return none
*/

void updateStates () {
  currentSwing = getSwingAmount();
  inZone = currentSwing < NOTE_A;

  // If the swing is out of the zone and the swing is still rising
  if (inZone == false && currentSwing > highestSwing) {
    highestSwing = currentSwing; // Set a new highest value.
  }
}


/**
     getSwingAmount

     This function is trying to determine how high someone
     is swinging. The direction or speed is not a concern.

     This reads and maps the sensor value so it can be sent
     to the host computer when it is in debug mode. UDP can
     only send bytes so this is why the mapping is necessary.

     @param none
     @return byte An always positive magnitude of the swing's tilt amount.
*/

byte getSwingAmount () {
  // Read values from the sensor
  mma.read();

  // Calculate and map the magnitude
  const int RAW = abs(mma.x) + abs(mma.y);
  const int SCALED = map(RAW, 0, 4000, 0, 255);
  const byte CONSTRAINED = (byte)constrain(SCALED, 0, 255);

  return CONSTRAINED;
}


/**
    classifyNote

    This function runs when the swing enters the zone. It classifies
    the note from the height of the swing.

    Any swing height that is less that the NOTE_A threshold isn't
    registered as a note. Any note greater than the NOTE_D threshold
    is classified as NOTE_D.

    @param byte The swing height to be used for classification.
    @return char The classified note
*/

char classifyNote (byte swing) {
  char classification = '1';

  if (swing >= NOTE_B && swing < NOTE_C) {
    classification = '2';
  } else if (swing >= NOTE_C) {
    classification = '3';
  }

  return classification;
}

/**
    sendNote

    This function creates a UDP package and sends the char note
    to the remote host computer.

    The first element in the UDP package is the name of the sensor
    so the host computer is able to tell the sensors apart without
    referencing the dynamic IP addresses.

    @param char The classified note to be sent
    @return none
*/

void sendNote (char note) {
  // Package the data to be sent as UDP
  char udpPacket [] = {NAME, note};

  // Send a message to the sound computer
  udp.beginPacket(SOUND_IP, REMOTE_PORT);
  udp.write(udpPacket);
  udp.endPacket();

  // Blink the LED
  digitalWrite(LED_BUILTIN, LOW);
  delay(MESSAGE_DELAY);
  digitalWrite(LED_BUILTIN, HIGH);

  // Send a message to the video computer
  udp.beginPacket(VIDEO_IP, REMOTE_PORT);
  udp.write(udpPacket);
  udp.endPacket();

  // Blink the LED
  digitalWrite(LED_BUILTIN, LOW);
  delay(MESSAGE_DELAY);
  digitalWrite(LED_BUILTIN, HIGH);

  // Report the process for debugging
  Serial.println("Note Sent");
  Serial.println();

  // Reset the highest swing when you send a note.
  highestSwing = 0;
  delay(NOTE_DELAY);
}
