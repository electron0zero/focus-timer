#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED pin mapping for Display with Driver IC: SSD1306
// Display Source: https://robu.in/product/0-96-oled-display-module/#tab-description
// In my testing it only supports SPI and I2C doesn't work even tho it says I2C is supported.
// 
// Pin connections for SPI
// | OLED Pin   | ESP8266 Pin | Notes                |
// | ---------- | ----------- | -------------------- |
// | GND        | GND         | Ground               |
// | VDD        | 3.3V        | Power (3.3V)         |
// | SCK (CLK)  | D5 (GPIO14) | SPI Clock            |
// | SDA (MOSI) | D7 (GPIO13) | SPI Data             |
// | RES        | D6 (GPIO12) | Reset                |
// | DC         | D3 (GPIO0)  | Data/Command select  |
// | CS         | D8 (GPIO15) | Chip select          |
// 
// Don’t use D7, D5 for anything other than SPI for OLED when using hardware SPI.
// we are using hardware SPI, so you must not interfere with D5 (SCK) and D7 (MOSI),
// which are automatically used by SPI.
// 
// see https://lastminuteengineers.com/wemos-d1-mini-pinout-reference/ for the details
// on which can be used for what, and why.

// Working pin setup for OLED without interfering with the encoder pins and onboard LED
// Don't use D4, it's connected to the onboard blue LED so leave D4 Pin unused
#define OLED_CLK    D5  // GPIO14
#define OLED_MOSI   D7  // GPIO13
#define OLED_RESET  D6  // GPIO12 — safe
#define OLED_DC     D3  // GPIO0 — safe if pulled HIGH at boot (default with pull-up)
#define OLED_CS     D8  // GPIO15 — must be LOW at boot (OLED CS is typically LOW)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         &SPI, OLED_DC, OLED_RESET, OLED_CS);

// store the count
volatile int counter = 0;

void setup() {
  Serial.begin(115200);
  delay(100)

  Serial.println("starting up...");

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello OLED");
  display.display();
}

void loop() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.print("Seconds: ");
  display.println(counter);
  display.display();

  // refresh every 1s
  delay(1000);
  counter++
}
