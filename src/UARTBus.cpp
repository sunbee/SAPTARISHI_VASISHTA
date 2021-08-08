/*
Copyright of Sanjay R. Bhatikar
*/
#include "UARTBus.h"

UARTBus::UARTBus() {

}

void UARTBus::start_UARTBus() {
    this->_UARTBus.begin(Serial);
}

void UARTBus::debugTx() {
    /*
    Pretty-print tx payload (struct).
    */
    Serial.print("Instructions: ");
    Serial.print(millis());
    Serial.print("Water: ");
    Serial.print(this->_control.PUMP);
    Serial.print(", Fan: ");
    Serial.print(this->_control.FAN);
    Serial.print(", LED: ");
    Serial.println(this->_control.LED);
}

void UARTBus::debugRx() {
    /*
    Pretty-print rx payload (struct).
    */
    Serial.print("Status: ");
    Serial.print(millis());
    Serial.print(", Fan: ");
    Serial.println(this->_status.FAN);
}

void UARTBus::txControl() {
    /*
    Transmit and receive payloads over UART with Arduino Uno.
    */
    this->_UARTBus.txObj(this->_control, sizeof(this->_control));
    this->_UARTBus.sendDatum(this->_control);
}

void UARTBus::rxStatus() {
    if (this->_UARTBus.available()) {
        this->_UARTBus.rxObj(this->_status);
    } else if (this->_UARTBus.status < 0) {
        Serial.print("ERROR: ");

        if (this->_UARTBus.status == -1)
            Serial.println(F("CRC_ERROR"));
        else if (this->_UARTBus.status == -2)
            Serial.println(F("PAYLOAD_ERROR"));
        else if (this->_UARTBus.status == -3)
            Serial.println(F("STOP_BYTE_ERROR"));
    }
};

TX UARTBus::get_control(bool liveReading) {
    if (liveReading) {
        this->txControl();
    }
    return this->_control;
};

RX UARTBus::get_status(bool liveReading) {
    if (liveReading) {
        this->rxStatus();
    }
    return this->_status;
}

void UARTBus::deserializeJSON(char *PAYLOAD, unsigned int length) {
    /*
    Deserialize the serialized JSON read from topic on MQTT broker.
    The array of char is read one character at a time to extract the 
    key-value pairs and populate the struct.
    Uses the fact that JSON payload has enclosing curly-braces, with
    comma-separated key-value pairs, a colon separating key and value,
    and text enclosed in quotes. 
    EXPECTS ONLY INTEGER VALUES!!! KEYS MUST BE CONSISTENT WITH PAYLOAD STRUCT!!!
    */
    String KEY;
    String VAL;
    int ch;

    for (int i = 0; i < length; i++) {
        ch = PAYLOAD[i];
        if ((ch == '{') || (ch == 32) || (ch == 34) || (ch == 39)) { // Discard: curly brace, whitespace, quotation marks
            // Do nothing!
        } else if (ch == ':') {
            // Print the key now if you need to. Otherwise do nothing.
        } else if ((ch == 44) || (ch == '}')) { // comma
            if (KEY == "PUMP") {
                this->_control.PUMP = VAL.toInt();
            } else if (KEY == "FAN") {
                this->_control.FAN = VAL.toInt();
            } else if (KEY == "LED") {
                this->_control.LED = VAL.toInt();
            }
            KEY = "";
            VAL = "";
        } else if ((ch >= '0') && (ch <= '9')) {
            VAL += char(ch);
        } else if (((ch >= 65) && (ch <= 90)) || ((ch > 97) && (ch < 122))) { // A-Z or a-z
            KEY += char(ch);
        }
    }  
}