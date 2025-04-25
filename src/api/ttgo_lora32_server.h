#pragma once

#include "api.h"
#include "functions.h"

#include <SPI.h>
#include <LoRa.h>

#include <Wire.h>
// #include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// define the pins used by the LoRa transceiver module
#define TTGO_SCK 5
#define TTGO_MISO 19
#define TTGO_MOSI 27
#define TTGO_SS 18
#define TTGO_RST 14
#define TTGO_DIO0 26
#define TTGO_BAND 866E6

// OLED pins
#define TTGO_OLED_SDA 4
#define TTGO_OLED_SCL 15
#define TTGO_OLED_RST 16
#define TTGO_SCREEN_WIDTH 128 // OLED display width, in pixels
#define TTGO_SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(TTGO_SCREEN_WIDTH, TTGO_SCREEN_HEIGHT, &Wire, TTGO_OLED_RST);

class TTGODisplay
{
public:
    void setup()
    {
    }
};

TTGODisplay *ttgoDisplay;

class TTGOLora32Server : public ApiDriver
{
protected:
private:
public:
    bool active = false;
    static void registerApi()
    {
        registerDriver("lora", create, true);
    }
    static ApiDriver *create()
    {
        return new TTGOLora32Server();
    }
    void connect()
    {
        if (!LoRa.begin(TTGO_BAND))
        {
            Serial.println("Starting LoRa failed!");
            active = false;
        }
        else
        {
            active = true;
        }
    }
    void setup() override
    {
        Serial.println("TTGO setup");
        // reset OLED display via software
        pinMode(TTGO_OLED_RST, OUTPUT);
        digitalWrite(TTGO_OLED_RST, LOW);
        delay(20);
        digitalWrite(TTGO_OLED_RST, HIGH);

        // initialize OLED
        Wire.begin(TTGO_OLED_SDA, TTGO_OLED_SCL);
        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false))
        { // Address 0x3C for 128x32
            Serial.println(F("SSD1306 allocation failed"));
            for (;;)
                ; // Don't proceed, loop forever
        }
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("LORA SENDER ");
        display.display();

        Serial.println("LoRa Sender Test");

        // SPI LoRa pins
        SPI.begin(TTGO_SCK, TTGO_MISO, TTGO_MOSI, TTGO_SS);
        LoRa.setPins(TTGO_SS, TTGO_RST, TTGO_DIO0);
        connect();
    }
};

TTGOLora32Server *lora;
