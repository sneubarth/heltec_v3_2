#include <Arduino.h>
#include <RadioLib.h>

#define CS_PIN 8
#define DIO1_PIN 14
#define RST_PIN 12
#define BUSY_PIN 13

SPIClass customSPI(HSPI);
Module mod(CS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN, customSPI);
SX1262 radio(&mod);

void setup() {
    Serial.begin(115200);
    delay(1000);

    customSPI.begin(9, 11, 10);

    Serial.println("Debug: CS_PIN = " + String(CS_PIN));
    Serial.println("Debug: DIO1_PIN = " + String(DIO1_PIN));
    Serial.println("Debug: RST_PIN = " + String(RST_PIN));
    Serial.println("Debug: BUSY_PIN = " + String(BUSY_PIN));
    Serial.println("Debug: Custom SPI pins: SCK=9, MISO=11, MOSI=10");

    int state = radio.setTCXO(1.6);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("TCXO failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }

    state = radio.begin(915.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Radio initialization failed, code: ");
        Serial.println(state);
        while (true) { delay(10); }
    }

    state = radio.setBandwidth(125.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Bandwidth failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.setSpreadingFactor(8);  // Match RX
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Spreading factor failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.setCodingRate(5);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Coding rate failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.setSyncWord(0x12);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Sync word failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.setPreambleLength(16);  // Match RX
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Preamble length failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.explicitHeader();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Explicit header failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    state = radio.setCRC(true);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("CRC failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }

    Serial.println("Transmitter started successfully.");
}

void loop() {
    const char message[] = "hello";
    unsigned long start = micros();
    int state = radio.transmit(message, strlen(message));
    unsigned long duration = micros() - start;
    if (state == RADIOLIB_ERR_NONE) {
        Serial.print("Transmitted: ");
        Serial.print(message);
        Serial.print(" [");
        Serial.print(duration / 1000.0);
        Serial.println(" ms]");
    } else {
        Serial.print("Transmission failed, code: ");
        Serial.println(state);
    }
    delay(1000);
}