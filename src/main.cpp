#include <Arduino.h>

/*
Control Automated Hydroponic Growout with ESP8266-12E nodemcu.
The node listens to topic Act on MQTT broker for control instructions.
The instructions, in serialized JSON, are deserialized and repackaged
as struct. The struct is sent by serial transfer to partnering
Arduino which executes instructions and sends back status.
*/

#include "UARTBus.h"
UARTBus SerialBus;

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "SECRETS.h"

#define TIMER_MQTT 60000   // Check connection with MQTT broker
#define TIMER_UART 30000   // Tx instructions 
#define onboard_led 16

const char* SSID = SECRET_SSID;
const char* PASS = SECRET_PASS;
char broker_ip[] = SECRET_BROKER_IP;
unsigned long tic_MQTT = millis();
unsigned long tic_UART = millis();

/*
Once the first message is read from topic Act on MQTT broker,
this node can trasmit instructions payload to the partner 
on a timer.
*/
bool fuse = false;   // Flag receipt of at least one message on MQTT broker's topic labeled Act 


WiFiClient WiFiClient;
PubSubClient MosquittoClient(WiFiClient);

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
  SerialBus.deserializeJSON(message2display, length);
  SerialBus.TxRx(true);   // Transmit instructions and receive status.
  fuse = true;            // Trigger timer for transmitting instructions at regular intervals
}

void setup() {
  // Connect to WiFi:
  Serial.begin(9600);
  WiFi.mode(WIFI_OFF);
  delay(1500);
  SerialBus.start_UARTBus();
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

  /*
    Lights! Camera! Action!
    Here is where the action is for publisher and subscriber.
    Note the use of millis to scheduling the publication of
    sensor readings to the MQTT broker in a non-blocking way
    for the listener. The use of 'delay()' would block the 
    listener, causing events to be missed.  
  */
  digitalWrite(onboard_led, LOW);
  if (toc - tic_MQTT > TIMER_MQTT) {
    tic_MQTT = toc;
    if (!MosquittoClient.connected()) {
      Serial.println("Made no MQTT connection.");
      reconnect();
    } else {
      digitalWrite(onboard_led, HIGH);
    }
  }
  if (toc - tic_UART > TIMER_UART) {
    tic_UART = toc;
    if (fuse) {
      SerialBus.TxRx(true); // Transmit instructions and receive status.
    } else {
      Serial.println("Received no message from broker.");
    }
  }
  MosquittoClient.loop(); // Subscriber action
}