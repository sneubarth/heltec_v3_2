#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define CS_PIN 8
#define DIO1_PIN 14
#define RST_PIN 12
#define BUSY_PIN 13
#define VEXT_PIN 36
#define OLED_RESET_PIN 21

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

SPIClass customSPI(HSPI);
Module mod(CS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN, customSPI);
SX1262 radio(&mod);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

volatile bool receivedFlag = false;
uint32_t validPacketCount = 0;
uint32_t missedPacketCount = 0;
bool oledInitialized = false;

void setFlag() {
    receivedFlag = true;
}

void scanI2C() {
    Serial.println("Scanning I2C bus:");
    bool found = false;
    for (uint8_t addr = 8; addr < 128; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("addr: 0x");
            Serial.println(addr, HEX);
            found = true;
        }
    }
    Serial.println(found ? "Scan complete." : "No I2C devices found.");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Starting setup...");

    // Enable Vext
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);
    Serial.println("Vext (GPIO 36) enabled");
    delay(1000);

    // Configure OLED reset
    pinMode(OLED_RESET_PIN, OUTPUT);
    digitalWrite(OLED_RESET_PIN, LOW);
    delay(10);
    digitalWrite(OLED_RESET_PIN, HIGH);
    Serial.println("OLED reset done");

    // Initialize I2C
    Wire.begin(17, 18); // SDA=17, SCL=18
    Wire.setClock(100000);
    delay(100);
    Serial.println("I2C initialized (SDA=17, SCL=18)");

    // Scan I2C devices
    scanI2C();

    // Initialize OLED
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        oledInitialized = true;
        Serial.println("OLED initialized at 0x3C");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("LoRa RX");
        display.setCursor(0, 8);
        display.println("RSSI: Waiting...");
        display.setCursor(0, 16);
        display.println("SNR: Waiting...");
        display.setCursor(0, 24);
        display.println("packet: 0");
        display.display();
        Serial.println("OLED splash displayed");
    } else {
        Serial.println("OLED init failed at 0x3C");
        oledInitialized = false;
    }

    // Initialize SPI and LoRa
    customSPI.begin(9, 11, 10);
    radio.setTCXO(1.6);
    int state = radio.begin(915.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa init failed, code: ");
        Serial.println(state);
        while (true);
    }
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(8);
    radio.setCodingRate(5);
    radio.setSyncWord(0x12);
    radio.setPreambleLength(16);
    radio.explicitHeader();
    radio.setCRC(true);

    pinMode(DIO1_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(DIO1_PIN), setFlag, RISING);
    radio.startReceive();
    Serial.println("Receiver started");
}

void loop() {
    if (receivedFlag) {
        receivedFlag = false;

        static unsigned long lastPacketTime = 0;
        uint8_t buffer[64] = {0};
        int state = radio.readData(buffer, 32);

        if (state == RADIOLIB_ERR_NONE) {
            validPacketCount++;
            unsigned long currentTime = millis();
            unsigned long elapsed = currentTime - lastPacketTime;
            if (elapsed > 1500 && lastPacketTime > 0) {
                missedPacketCount += (elapsed / 1236);
            }

            // Convert buffer to string
            String packet = "";
            for (size_t i = 0; i < 32; i++) {
                char c = (char)buffer[i];
                packet += (c >= 32 && c <= 126) ? c : ' ';
            }

            // Debug packet
            Serial.print("Packet length: ");
            Serial.println(packet.length());
            Serial.print("Packet: [");
            Serial.print(packet);
            Serial.println("]");
            Serial.print("Buffer: ");
            for (size_t i = 0; i < 32; i++) {
                Serial.print((uint8_t)buffer[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            // Extract display string
            String displayStr = packet.substring(13, 30); // "T: 70.0 F, H:57 %"

            // Debug display string
            Serial.print("displayStr: [");
            Serial.print(displayStr);
            Serial.print("] len: ");
            Serial.println(displayStr.length());
            Serial.print("displayStr hex: ");
            for (size_t i = 0; i < displayStr.length(); i++) {
                Serial.print((uint8_t)displayStr[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            // Serial output
            Serial.print("Received packet at ");
            Serial.print(currentTime);
            Serial.print(" ms (elapsed: ");
            Serial.print(elapsed);
            Serial.print(" ms, len=");
            Serial.print(radio.getPacketLength());
            Serial.print("): ");
            Serial.print(packet);
            Serial.println();
            Serial.print("RSSI: ");
            Serial.print(radio.getRSSI());
            Serial.println(" dBm");
            Serial.print("SNR: ");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
            Serial.print("Display: [");
            Serial.print(displayStr);
            Serial.println("]");
            Serial.print("Valid packets received: ");
            Serial.println(validPacketCount);
            Serial.print("Estimated missed packets: ");
            Serial.println(missedPacketCount);

            // Update OLED
            if (oledInitialized) {
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(0, 0);
                display.println("LoRa RX");
                display.setCursor(0, 8);
                display.print("RSSI: ");
                display.print((int)radio.getRSSI());
                display.println(" dBm");
                display.setCursor(0, 16);
                display.print("SNR: ");
                display.print(radio.getSNR(), 1);
                display.println(" dB");
                display.setCursor(0, 24);
                display.print("packet: ");
                display.println(validPacketCount);
                display.setCursor(10, 40);
                display.println(displayStr); // "T: 70.0 F, H:57 %"
                display.display();
                Serial.println("OLED updated");
            } else {
                Serial.println("OLED not initialized");
            }

            lastPacketTime = currentTime;
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println("CRC mismatch");
            missedPacketCount++;
        } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
            Serial.print("Error, code: ");
            Serial.println(state);
            missedPacketCount++;
        }

        radio.startReceive();
    }
}