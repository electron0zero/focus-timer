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

// Rotary Encoder pin definitions (no conflict with OLED PINS)
// #define ENC_SW   D4  // GPIO2 (needs to be HIGH at boot, connected to FLASH button, boot fails if pulled LOW) 
// D4 Pin is hard-wired to on board LED so each time the SW is pressed, the LED will on turned on.
// it can be a feature or a bug, I liked that it's indicating button press but you may not want that.
// if you don't want, use D0 ping with a pull up register to disable the onboard LED.
#define ENC_SW   D0  // GPIO16 (no INT, OK for polling), need a 10K register as pull up with 3.3V
#define ENC_DT   D2  // GPIO4 (INT-capable, safe)
#define ENC_CLK  D1  // GPIO5 (INT-capable)


volatile int rotation = 0;
volatile bool rotated = false;

unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 10;  // ms

unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 10;  // ms

volatile int lastClkState = HIGH;
volatile int lastDtState = HIGH;
bool lastButtonState = HIGH;


void ICACHE_RAM_ATTR handleRotation() {
  int clkState = digitalRead(ENC_CLK);
  int dtState = digitalRead(ENC_DT);  

  // Only act when CLK changes and don't act on each Interrupt trigger
  if (clkState != lastClkState) {
    if (dtState != clkState) {
      rotation++;
    } else {
      rotation--;
    }
    rotated = true;
  }

  Serial.print("Last ENC_DT: ");
  Serial.print(lastDtState);
  Serial.print(" ENC_DT: ");
  Serial.print(dtState);
  Serial.print(" Last ENC_CLK: ");
  Serial.print(lastClkState);
  Serial.print(" ENC_CLK: ");
  Serial.print(clkState);
  Serial.print(" rotation: ");
  Serial.println(rotation);

  lastClkState = clkState;
  lastDtState = dtState;
}

void setup() {
  Serial.begin(115200);

  // IMP. NOTE: make sure the use a 10k pull up resister between SW and VCC (+ pin) of the encoder
  // to pull SW pin high otherwise the button press will make SW LOW and it will be stuck LOW
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT_PULLUP);

  Serial.println("");
  Serial.println("Starting up...");
  Serial.print("Initial ENC_SW: ");
  Serial.println(digitalRead(ENC_SW)); // Should be HIGH with pullup
  Serial.print("Initial ENC_DT: ");
  Serial.println(digitalRead(ENC_DT)); // Should be HIGH with pullup
  Serial.print("\nInitial ENC_CLK: ");
  Serial.println(digitalRead(ENC_CLK)); // Should be HIGH with pullup


  attachInterrupt(digitalPinToInterrupt(ENC_CLK), handleRotation, CHANGE);


    // Init display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    for (;;); // Halt
  }

  display.clearDisplay();
  display.setTextSize(4);     // Large text
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("READY");
  display.display();
  delay(1000); // wait 500ms with ready text
  display.clearDisplay();
}

void loop() {

  // Handle button presses and states
  handleButtonPresses(millis());

  static int lastRotation = 0;

  if (rotated) {
    rotated = false;
    Serial.print("Rotation: ");
    Serial.println(rotation);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Rotation: ");
  display.println(rotation);
  display.display();

  // refresh every 100ms
  delay(100);
}

//===========================================
// Detect button presses with debounce

bool buttonPressed() {
  bool currentState = digitalRead(ENC_SW);
  // Serial.print("called buttonPressed, currentState: ");
  // Serial.println(currentState);

  if (lastButtonState == HIGH && currentState == LOW && (millis() - lastButtonDebounceTime > buttonDebounceDelay)) {
    lastButtonDebounceTime = millis(); 
    lastButtonState = currentState;
    return true;
  }

  lastButtonState = currentState;
  return false;
}

//=========================================================
// Handle button presses and manage state transitions
void handleButtonPresses(unsigned long currentMillis) {
  if (buttonPressed()) {
    Serial.println("button pressed, reset rotation");
    rotation = 0;
  };

  return;
}