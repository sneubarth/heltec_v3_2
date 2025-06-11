#include <Arduino.h>
#include <RadioLib.h>

#define CS_PIN 8
#define DIO1_PIN 14
#define RST_PIN 12
#define BUSY_PIN 13

SPIClass customSPI(HSPI);
Module mod(CS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN, customSPI);
SX1262 radio(&mod);

// Flag for DIO1 interrupt
volatile bool receivedFlag = false;

void setFlag() {
    receivedFlag = true;
}

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
    state = radio.setSpreadingFactor(8);
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
    state = radio.setPreambleLength(16);
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

    // Set DIO1 interrupt handler
    pinMode(DIO1_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(DIO1_PIN), setFlag, RISING);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Start receive failed, code ");
        Serial.println(state);
        while (true) { delay(10); }
    }
    Serial.println("Receiver started successfully.");
}

void loop() {
    if (receivedFlag) {
        receivedFlag = false;  // Clear flag

        static unsigned long lastPacketTime = 0;
        uint8_t buffer[64] = {0};
        int state = radio.readData(buffer, 64);

        if (state == RADIOLIB_ERR_NONE) {
            float rssi = radio.getRSSI();

            size_t length = radio.getPacketLength();
            unsigned long currentTime = millis();
            Serial.print("Received packet at ");
            Serial.print(currentTime);
            Serial.print(" ms (interval: ");
            Serial.print(currentTime - lastPacketTime);
            Serial.print(" ms, len=");
            Serial.print(length);
            Serial.print("): ");
            String packet = "";
            for (size_t i = 0; i < length; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) {
                    packet += (char)buffer[i];
                }
            }
            Serial.print(packet);
            Serial.print(" [Hex: ");
            for (size_t i = 0; i < length; i++) {
                if (buffer[i] < 16) Serial.print("0");
                Serial.print(buffer[i], HEX);
                Serial.print(" ");
            }
            Serial.println("]");
            Serial.print("RSSI: ");
            Serial.print(rssi);
            Serial.println(" dBm");
            Serial.print("SNR: ");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
            lastPacketTime = currentTime;
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println("CRC mismatch detected");
        } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
            Serial.print("Receiving failed, code: ");
            Serial.println(state);
        }

        // Clear interrupts and restart RX
        radio.startReceive();
    }
}