#ifndef UART_BUS_H
#define UART_BUS_H

#include <Wire.h>
#include <SerialTransfer.h>

struct TX {
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
};

struct RX {
  /*
  The payload returned by Arduino Uno slave with status report.
  Currently reports only fan speed which is read off the pin no. 3 
  (yellow wire) of a PC fan.
  */
  uint8_t FAN;
};

/* 
    Exchange payloads, sending instructions and receiving status.
    Store the payload using STRUCT, _control for Tx, _status for Rx.
*/
class UARTBus
{
    public:
        UARTBus();  

        void start_UARTBus();
        void debugRx();             // Print status payload (STRUCT)
        void debugTx();             // Print instructions payload (STRUCT)
        void txControl();           // Transmit instructions payload (STRUCT)
        void rxStatus();            // Receive status paylood (STRUCT), if available
        TX get_control(bool=false); // Show instructions payload
        RX get_status(bool=false);  // Show status payload
        void deserializeJSON(char *PAYLOAD, unsigned int length); // Helper function converta serialized JSON from MQTT broker
        void TxRx(bool=false);       // Transmit instructions payload, receive status payload, optionally print to console
    private:
        SerialTransfer _UARTBus;
        TX _control;
        RX _status;
};

#endif