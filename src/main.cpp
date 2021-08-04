#include <Arduino.h>

#include <Wire.h>
#include <SerialTransfer.h>

SerialTransfer MasterMCU;

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "SECRETS.h"

const char* SSID = SECRET_SSID;
const char* PASS = SECRET_PASS;
char broker_ip[] = SECRET_BROKER_IP;
unsigned long tic = millis();

WiFiClient WiFiClient;
PubSubClient MosquittoClient(WiFiClient);

void displayMessage(String mess) {
  /*
  Flesh out later for displaying deserialized payload to OLED.
  */
}

struct PAYMASTER {
  /*
  The payload with control recipe for tx to Arduino Uno slave.
  It has on/off instructions for water pump ("PUMP"), speed setting
  for PC fan (0-255), brightness setting for Neopixel LED lights
  (0-255). Arduino Uno receives struct, unpacks information and
  executes instructions.
  */
  bool PUMP;
  uint8_t FAN;
  uint8_t LED;
} Controller;

struct PAYSLAVE {
  /*
  The payload returned by Arduino Uno slave with status report.
  Currently reports only fan speed which is read off the pin no. 3 
  (yellow wire) of a PC fan.
  */
  uint8_t fan;
} status;

void debugTx() {
  /*
  Pretty-print tx payload (struct).
  */
  Serial.print("MASTER TX: ");
  Serial.print(millis());
  Serial.print("   Water: ");
  Serial.print(Controller.PUMP);
  Serial.print(", Fan: ");
  Serial.print(Controller.FAN);
  Serial.print(", LED: ");
  Serial.println(Controller.LED);
}

void debugRx() {
  /*
  Pretty-print rx payload (struct).
  */
  Serial.print("MASTER RX: ");
  Serial.print(millis());
  Serial.print(", Fan: ");
  Serial.println(status.fan);
}

void transmitCommand() {
  /*
  Transmit and receive payloads over UART with Arduino Uno.
  */
  MasterMCU.txObj(Controller, sizeof(Controller));
  MasterMCU.sendDatum(Controller), sizeof(Controller);
  debugTx();

  if (MasterMCU.available()) {
    MasterMCU.rxObj(status);
    debugRx();
  } else if (MasterMCU.status < 0) {
    Serial.print("ERROR: ");

    if (MasterMCU.status == -1)
      Serial.println(F("CRC_ERROR"));
    else if (MasterMCU.status == -2)
      Serial.println(F("PAYLOAD_ERROR"));
    else if (MasterMCU.status == -3)
      Serial.println(F("STOP_BYTE_ERROR"));
  }
}

void debug_serialization() {
  Serial.print("PUMP: ");
  Serial.print(Controller.PUMP);
  Serial.print(" FAN: ");
  Serial.print(Controller.FAN);
  Serial.print(" LED: ");
  Serial.println(Controller.LED);
}

String KEY;
String VAL;
int ch;

void deserializeJSON(char *PAYLOAD, unsigned int length) {
  /*
  Deserialize the serialized JSON read from topic on MQTT broker.
  The array of char is read one character at a time to extract the 
  key-value pairs and populate the struct.
  Uses the fact that JSON payload has enclosing curly-braces, with
  comma-separated key-value pairs, a colon separating key and value,
  and text enclosed in quotes. 
  EXPECTS ONLY INTEGER VALUES!!! KEYS MUST BE CONSISTENT WITH PAYLOAD STRUCT!!!
  */
  for (int i = 0; i < length; i++) {
    ch = PAYLOAD[i];
    //Serial.print(char(ch));
    if ((ch == '{') || (ch == 32) || (ch == 34) || (ch == 39)) { // Discaed: curly brace, whitespace, quotation marks
      // Do nothing!
    } else if (ch == ':') {
      // Print the key now if you need to. Otherwise do nothing.
    } else if ((ch == 44) || (ch == '}')) { // comma
      if (KEY == "FAN") {
        Controller.FAN = VAL.toInt();
      } else if (KEY == "LED") {
        Controller.LED = VAL.toInt();
      }
      KEY = "";
      VAL = "";
    } else if ((ch >= '0') && (ch <= '9')) {
      VAL += char(ch);
    } else if (((ch >= 65) && (ch <= 90)) || ((ch > 97) && (ch < 122))) { // A-Z or a-z
      KEY += char(ch);
    }
  }  
  debug_serialization();
  transmitCommand();
}

void mosquittoDo(char* topic, byte* payload, unsigned int length) {
  /*
  Handle a new message published to the subscribed topic on the 
  MQTT broker and show in the OLED display.
  This is the heart of the subscriber, making it so the nodemcu
  can act upon information, say, to operate a solenoid valve when
  the mositure sensor indicates dry soil.
  */
  Serial.print("Got a message in topic ");
  Serial.println(topic);
  Serial.print("Received data: ");
  char message2display[length];
  for (unsigned int i = 0; i < length; i++) {
    Serial.print(char(payload[i]));
    message2display[i] = payload[i];
  }
  Serial.println();
  deserializeJSON(message2display, length);
  char message4OLED[length+6]; // 'Got: ' and null terminator.
  snprintf(message4OLED, length+6, "Got: %s", message2display); 
  displayMessage(message4OLED);
}

void setup() {
  // Connect to WiFi:
  Serial.begin(9600);
  WiFi.mode(WIFI_OFF);
  delay(1500);
  MasterMCU.begin(Serial);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(699);
    Serial.print(".");
  }
  Serial.print("Connected: ");
  Serial.println(SSID);

  /* 
  Configure the Mosquitto client with the particulars 
  of the MQTT broker. The client is dual-use as publisher
  and subscriber.
  In order to publish, use the 'publish()' method of the 
  client object. The message will be serialized JSON 
  containing sensor readings. 
  In order to listen, subscribe to the topic of interest
  and handle the message with the callback in event-driven 
  pattern. 
  */
  MosquittoClient.setServer(broker_ip, 1883);
  MosquittoClient.setCallback(mosquittoDo);
}

void reconnect() {
  /*
  Connect to the MQTT broker in order to publish a message
  or listen in on a topic of interest. 
  The 'connect()' methods wants client credentials. When the
  MQTT broker is not set up for authentication, we have successfully
  connected to the MQTT broker using dummy data, passing string literals 
  for args 'id' and 'user' and NULL for 'pass'.
  Having connected successully, proceed to publish or listen.
  */
  while (!MosquittoClient.connected()) {
    if (MosquittoClient.connect("VASISH", "SECOND SAPTARISHI VASISHTA", NULL)) {
      Serial.println("Uh-Kay!");
      MosquittoClient.subscribe("Act"); // SUBSCRIBE TO TOPIC
    } else {
      Serial.print("Retrying ");
      Serial.println(broker_ip);
      delay(699);
    }
  }
}

void loop() {
  unsigned long toc = millis();
  #define TIMER_INTERVAL 60000
  #define onboard_led 16

  /*
    Lights! Camera! Action!
    Here is where the action is for publisher and subscriber.
    Note the use of millis to scheduling the publication of
    sensor readings to the MQTT broker in a non-blocking way
    for the listener. The use of 'delay()' would block the 
    listener, causing events to be missed.  
  */
  digitalWrite(onboard_led, LOW);
  if (toc - tic > TIMER_INTERVAL) {
    tic = toc;
    if (!MosquittoClient.connected()) {
      Serial.println("Made no MQTT connection.");
      reconnect();
    } else {
      digitalWrite(onboard_led, HIGH);
      //publish_message(); // Publisher action
    }
  }
  MosquittoClient.loop(); // Subscriber action
}