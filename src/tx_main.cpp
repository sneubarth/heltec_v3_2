#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "sx1262.h"
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT31.h>

#define CS_PIN 8
#define DIO1_PIN 14
#define RST_PIN 12
#define BUSY_PIN 13
#define VEXT_PIN 36  // Vext control for OLED
#define OLED_RESET_PIN 21  // OLED reset line

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C  // Confirmed SSD1306 I2C address
#define SHT31_ADDRESS 0x44  // SHT31 I2C address

SPIClass customSPI(HSPI);
Module mod(CS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN, customSPI);
SX1262 radio(&mod);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);
Adafruit_SHT31 sht31 = Adafruit_SHT31();

volatile bool transmittedFlag = false;
uint32_t packetCount = 0;  // Counter for transmitted packets
bool oledInitialized = false;  // Flag for OLED initialization
bool sht31Initialized = false;  // Flag for SHT31 initialization

void setFlag() {
    transmittedFlag = true;
}

void scanI2C() {
    Serial.println("Scanning I2C bus...");
    bool found = false;
    for (uint8_t addr = 1; addr < 128; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("Device found at 0x");
            Serial.println(addr, HEX);
            found = true;
        }
    }
    if (!found) {
        Serial.println("No I2C devices found");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting setup...");

    // Enable Vext power (active low)
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);
    Serial.println("Vext (GPIO 36) set LOW");
    delay(1000);  // Wait 1 second

    // Configure OLED reset
    pinMode(OLED_RESET_PIN, OUTPUT);
    digitalWrite(OLED_RESET_PIN, LOW);  // Reset OLED
    delay(10);  // Hold low for 10 ms
    digitalWrite(OLED_RESET_PIN, HIGH);  // Enable OLED
    Serial.println("OLED reset (GPIO 21) completed");

    // Initialize I2C
    Wire.begin(17, 18);  // SDA=17, SCL=18
    Serial.println("I2C initialized (SDA=17, SCL=18)");

    // Scan I2C bus
    scanI2C();

    // Initialize OLED
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        oledInitialized = true;
        Serial.println("OLED initialized successfully at address 0x3C");
        display.clearDisplay();
        display.setTextSize(1);  // ~6px wide, 8px high
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("LoRa TX");
        display.setCursor(0, 10);
        display.println("packet: 0");
        display.display();
        Serial.println("OLED splash screen displayed");
    } else {
        Serial.println("OLED initialization failed at address 0x3C");
    }

    // Initialize SHT31
    if (sht31.begin(SHT31_ADDRESS)) {
        sht31Initialized = true;
        Serial.println("SHT31 initialized successfully at address 0x44");
    } else {
        Serial.println("SHT31 initialization failed at address 0x44");
    }

    // Initialize SPI and LoRa
    customSPI.begin(9, 11, 10);
    radio.setTCXO(1.6);
    radio.begin(RADIO_FREQUENCY);
    radio.setBandwidth(RADIO_BANDWIDTH);
    radio.setSpreadingFactor(RADIO_SPREADING_FACTOR);
    radio.setCodingRate(RADIO_CODING_RATE);
    radio.setSyncWord(RADIO_SYNC_WORD);
    radio.setPreambleLength(RADIO_PREAMBLE_LENGTH);
    radio.explicitHeader();
    radio.setCRC(true);

    pinMode(DIO1_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(DIO1_PIN), setFlag, RISING);
    Serial.println("Transmitter started successfully");
}

void loop() {
    static unsigned long lastTransmitTime = 0;
    const unsigned long transmitInterval = 5000;  // Transmit every 5 second

    if (millis() - lastTransmitTime >= transmitInterval) {
        // Prepare message
        char packet[33];  // 32 chars + null terminator
        float temperatureF = 0.0;
        float humidity = 0.0;
        if (sht31Initialized) {
            float cTemp = sht31.readTemperature();
            if (!isnan(cTemp)) {
                temperatureF = cTemp * 9.0 / 5.0 + 32.0;  // Convert to °F
                humidity = sht31.readHumidity();
                snprintf(packet, sizeof(packet), "packet %03lu, T:%5.1f F, H:%02.0f %%", packetCount + 1, temperatureF, humidity);
                Serial.print("Temperature: ");
                Serial.print(temperatureF, 1);
                Serial.println(" F");
                Serial.print("Humidity: ");
                Serial.print(humidity, 0);
                Serial.println(" %");
            } else {
                Serial.println("Failed to read SHT31 temperature");
                snprintf(packet, sizeof(packet), "packet %03lu, T:---.- F, H:-- %%", packetCount + 1);
            }
        } else {
            snprintf(packet, sizeof(packet), "packet %03lu, T:---.- F, H:-- %%", packetCount + 1);
        }

        // Transmit packet
        transmittedFlag = false;
        int state = radio.transmit((uint8_t*)packet, 32);  // Fixed length 32

        if (state == RADIOLIB_ERR_NONE && transmittedFlag) {
            packetCount++;  // Increment counter
            unsigned long currentTime = millis();
            Serial.print("Transmitted packet at ");
            Serial.print(currentTime);
            Serial.print(" ms, len=32: ");
            Serial.print(packet);
            Serial.print(" [Hex: ");
            for (size_t i = 0; i < 32; i++) {
                if (packet[i] < 16) Serial.print("0");
                Serial.print(packet[i], HEX);
                Serial.print(" ");
            }
            Serial.println("]");
            Serial.print("Packets transmitted: ");
            Serial.println(packetCount);

            // Update OLED if initialized
            if (oledInitialized) {
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(0, 0);
                display.println("LoRa TX");
                display.setCursor(0, 10);
                display.print("packet: ");
                display.println(packetCount);
                if (sht31Initialized && !isnan(temperatureF)) {
                    display.setCursor(10, 30);  // Indent for readability
                    display.print("T: ");
                    display.print(temperatureF, 1);
                    display.println(" F");
                    display.setCursor(10, 40);
                    display.print("H: ");
                    display.print(humidity, 0);
                    display.println(" %");
                }
                display.display();
            }
        } else if (state != RADIOLIB_ERR_NONE) {
            Serial.print("Transmission failed, code: ");
            Serial.println(state);
        }

        lastTransmitTime = millis();
    }
}
