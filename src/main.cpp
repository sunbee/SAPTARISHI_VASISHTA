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
  bool PUMP;
  uint8_t FAN;
  uint8_t LED;
} Controller;

struct PAYSLAVE {
  /*
  fan: the fan speed read off the pin no. 3 (yellow wire) of a PC fan.
  */
  uint8_t fan;
} status;

void debugTx() {
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
  Serial.print("MASTER RX: ");
  Serial.print(millis());
  Serial.print(", Fan: ");
  Serial.println(status.fan);
}

void transmitCommand() {
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
  for (int i = 0; i < length; i++) {
    ch = PAYLOAD[i];
    //Serial.print(char(ch));
    delay(99);
    if ((ch == '{') || (ch == 32) || (ch == 34) || (ch == 39)) { // Discaed: curly brace, whitespace, quotation marks

    } else if (ch == ':') {

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
  // put your setup code here, to run once:
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
  // put your main code here, to run repeatedly:
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