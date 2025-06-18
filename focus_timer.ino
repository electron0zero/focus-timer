#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//=========================================
// Hardware PIN Definitions / Mappings
// NOTE: make sure harware wiring matches this pin mapping

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

// Init the disaply lib
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         &SPI, OLED_DC, OLED_RESET, OLED_CS);

// Rotary Encoder pin definitions (no conflict with OLED PINS)
// #define ENC_SW   D4  // GPIO2 (needs to be HIGH at boot, connected to FLASH button, boot fails if pulled LOW)
//
// D4 Pin is hard-wired to on board LED so each time the SW is pressed, the LED will on turned on.
// it can be a feature or a bug, I liked that it's indicating button press but you may not want that.
// if you don't want, use D0 ping with a pull up register to disable the onboard LED.
//
// IMP. NOTE for ENC_SW when using PIN D0: make sure the use a 10k pull up resister between SW and VCC (+ pin)
// of the encoder to pull SW pin high otherwise the button press will make SW LOW and it will be stuck LOW
#define ENC_SW   D0  // GPIO16 (no INT, OK for polling), needs a 10K register as pull up with 3.3V
#define ENC_DT   D2  // GPIO4 (INT-capable, safe)
#define ENC_CLK  D1  // GPIO5 (INT-capable)


//=========================================
int flowMinutes = 0;   // Total flow minutes
int menuIndex = 0;     // 0 for UP, 1 for DOWN, 2 for Reset
String menuOptions[3] = {"UP", "DOWN", "Reset"};  // Label reset option as "Reset"
// unsigned long lastActivityTime = 0;  // For inactivity detection
// const unsigned long inactivityLimit = 3 * 60000;  // 3 minutes in milliseconds

// enum State { MENU, COUNTING_UP, COUNTING_DOWN, SELECTING_DOWN_DURATION, IDLE }; // not using the idle mode
enum State { MENU, COUNTING_UP, COUNTING_DOWN, SELECTING_DOWN_DURATION };
State currentState = MENU;

int countdownValue = 50;  // Default value for countdown
int initialCountdownValue = 50;  // Store the countdown value when selected
unsigned long previousMillis = 0;  // For counting logic
int elapsedMinutes = 0;
bool isCounting = false;

// IDLE mode extended behavior
// const unsigned long displayOffTimeLimit = 30 * 60000;  // 30 minutes in milliseconds

// unsigned long idleStartTime = 0;  // Track when IDLE mode starts
// bool displayOff = false;  // Track if the display is off

// counter to know if the encoder was rotated or not, with direction
// We use following values:
// Clockwise = 1
// Counter Clockwise = -1
// No Rotation = 0
volatile int rotation = 0;
volatile bool rotated = false;
// volatile int counter = 0;

// Rotary encoder debounce variables
unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 10;  // ms

// unsigned long lastRotaryTime = 0;
// const unsigned long rotaryDebounceDelay = 150;  // Faster debounce for rotary encoder

// Push button debounce and last state
unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 10;  // ms

// unsigned long buttonDebounceTime = 0;
// const unsigned long buttonDebounceDelay = 800;  // Debounce delay

volatile int lastClkState = HIGH;
volatile int lastDtState = HIGH;
bool lastButtonState = HIGH;

//=========================================
// Detect encoder rotation with Interrupts and debounce

void ICACHE_RAM_ATTR handleRotationState() {
  int clkState = digitalRead(ENC_CLK);
  int dtState = digitalRead(ENC_DT);

  // Only act when CLK changes and don't act on each Interrupt trigger
  if (clkState != lastClkState) {
    if (dtState != clkState) {
      // clockwise rotation happened
      rotation = 1;
      // counter++;
    } else {
      // counter clockwise rotation happened
      rotation = -1;
      // counter--;
    }
    rotated = true;
  }

  // Serial.print("Last ENC_DT: ");
  // Serial.print(lastDtState);
  // Serial.print(" ENC_DT: ");
  // Serial.print(dtState);
  // Serial.print(" Last ENC_CLK: ");
  // Serial.print(lastClkState);
  // Serial.print(" ENC_CLK: ");
  // Serial.print(clkState);
  // Serial.print(" counter: ");
  // Serial.println(counter);
  // Serial.print(" rotation: ");
  // Serial.println(rotation);
  // Serial.print(" rotated: ");
  // Serial.println(rotated);

  lastClkState = clkState;
  lastDtState = dtState;
}

void setup() {
  Serial.begin(115200);
  initEncoder();
  // attach Interrupt for encoder pins and handleRotationState
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), handleRotationState, CHANGE);
  initDisplay();
  updateDisplay();

  Serial.println("Setup complete, starting loop...");
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle rotary encoder input
  // rotation state is handled by handleRotation, called by Interrupt
  handleRotaryInput();

  // Handle button presses and states
  handleButtonPresses(currentMillis);

  // Handle counting logic
  handleCounting(currentMillis);

  // disable the idle mode feature
  // Handle inactivity
  // handleInactivity(currentMillis);

  // reset rotation variables
  if (rotated) {
    rotated = false;
    // reset the rotation state if we rotated
    rotation = 0;
  }
}

//=========================================================
// Initialize hardware pins, and initialize the OLED display
void initEncoder() {
  // init encoder pins
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT_PULLUP);

  // log init state of encoder pins
  Serial.println("");
  Serial.println("Starting up...");
  Serial.print("Initial ENC_SW: ");
  Serial.println(digitalRead(ENC_SW)); // Should be HIGH with pullup
  Serial.print("Initial ENC_DT: ");
  Serial.println(digitalRead(ENC_DT)); // Should be HIGH with pullup
  Serial.print("Initial ENC_CLK: ");
  Serial.println(digitalRead(ENC_CLK)); // Should be HIGH with pullup

}

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    for (;;); // Halt
  }
  Serial.println("Display initialized.");

  // print READY for 1s to show that it's ready
  display.clearDisplay();
  display.setTextSize(4); // Large text
  display.setTextColor(WHITE);
  display.setCursor(0, 20); // print the text center of display
  display.println("READY");
  display.display();

  // wait 1 second with ready before we clear the display
  delay(1000);
  display.clearDisplay();
}

//=========================================================
// Update the OLED display with the current state
void updateDisplay() {
  display.setTextColor(WHITE);
  display.clearDisplay();

  // Display top row
  String topRowText;

  if (currentState == COUNTING_UP) {
    topRowText = "Focus! \x18";  // Focus with upward triangle for counting UP
  } else if (currentState == COUNTING_DOWN) {
    topRowText = "Focus! \x19";  // Focus with downward triangle for counting DOWN
  } else {
    topRowText = "Flow: " + String(flowMinutes);  // Display total flow minutes when not counting
  }

  int topRowTextWidth = topRowText.length() * 12;  // TextSize 2, so 12 pixels per char
  int topRowX = (128 - topRowTextWidth) / 2;  // Center the text on the top row

  display.setTextSize(2);  // Larger size for top row
  display.setCursor(topRowX, 0);  // Centered on top row
  display.print(topRowText);

  // Display main row (menu or counting values)
  String mainRowText;

  if (currentState == MENU) {
    mainRowText = menuOptions[menuIndex];  // Display UP, DOWN, or Reset in the menu
  } else if (currentState == COUNTING_UP) {
    mainRowText = String(elapsedMinutes);  // Display counting up minutes
  } else if (currentState == COUNTING_DOWN || currentState == SELECTING_DOWN_DURATION) {
    mainRowText = String(countdownValue);  // Display countdown minutes
  }
  // disable the IDLE mode
  // else if (currentState == IDLE) {
  //   mainRowText = "IDLE?";
  // }

  int mainRowTextWidth = mainRowText.length() * 24;  // TextSize 4, so 24 pixels per char
  int mainRowX = (128 - mainRowTextWidth) / 2;  // Calculate centered X position

  display.setTextSize(4);  // Larger size for main row
  display.setCursor(mainRowX, 30);  // Centered on main row
  display.print(mainRowText);

  display.display();  // Show the updated display
}

//===========================================
// Detect encoder button presses with debounce
bool buttonPressed() {
  bool currentState = digitalRead(ENC_SW);
  // Serial.print("called buttonPressed, currentState: ");
  // Serial.println(currentState);

  if (lastButtonState == HIGH && currentState == LOW && (millis() - lastButtonDebounceTime > buttonDebounceDelay)) {
    Serial.println("push button pressed...");
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
  if (!buttonPressed()) return;

  switch (currentState) {
    case MENU:
      if (menuIndex == 0) {  // UP selected
        startCountingUp();
      } else if (menuIndex == 1) {  // DOWN selected
        startSelectingDownDuration();
      } else if (menuIndex == 2) {  // Reset selected
        resetFlowMinutes();  // Reset the total focus time to 0
      }
      break;

    case SELECTING_DOWN_DURATION:
      confirmCountdownSelection();
      break;

    case COUNTING_UP:
      stopCountingUp();
      break;

    case COUNTING_DOWN:
      stopCountingDown();
      break;
  }
  updateDisplay();
}

//=========================================================
// Start counting up
void startCountingUp() {
  currentState = COUNTING_UP;
  elapsedMinutes = 0;
  isCounting = true;
  // lastActivityTime = millis();  // Reset inactivity timer
  Serial.println("Counting UP started.");
}

//=========================================================
// Start selecting the countdown duration
void startSelectingDownDuration() {
  currentState = SELECTING_DOWN_DURATION;
  countdownValue = 50;
  // lastActivityTime = millis();  // Reset inactivity timer
  Serial.println("Selecting DOWN duration.");
}

//=========================================================
// Confirm countdown selection and start counting down
void confirmCountdownSelection() {
  initialCountdownValue = countdownValue;
  currentState = COUNTING_DOWN;
  isCounting = true;
  // lastActivityTime = millis();  // Reset inactivity timer
  Serial.print("Counting DOWN started with "); Serial.print(countdownValue); Serial.println(" minutes.");
}

//=========================================================
// Stop counting up and return to menu
void stopCountingUp() {
  flowMinutes += elapsedMinutes;
  successAnimation();
  currentState = MENU;
  isCounting = false;
  Serial.println("Counting UP stopped. Returning to MENU.");
}

//=========================================================
// Stop counting down and return to menu
void stopCountingDown() {
  flowMinutes += (initialCountdownValue - countdownValue);
  successAnimation();
  currentState = MENU;
  isCounting = false;
  Serial.println("Counting DOWN stopped. Returning to MENU.");
}

//=========================================================
// Reset the total flow minutes counter to 0
void resetFlowMinutes() {
  flowMinutes = 0;
  Serial.println("Flow minutes reset to 0.");
  successAnimation();
  updateDisplay();  // Update the display to show the reset value
}

//=========================================================
// Handle counting up or down logic
void handleCounting(unsigned long currentMillis) {
  // no-op if we are not counting or 1 minute has not passed since we counted
  if (!isCounting || (currentMillis - previousMillis < 60000)) return;

  previousMillis = currentMillis;

  if (currentState == COUNTING_UP) {
    elapsedMinutes++;
    updateDisplay();
    Serial.print("Counting UP: "); Serial.println(elapsedMinutes);
  } else if (currentState == COUNTING_DOWN) {
    countdownValue--;
    if (countdownValue <= 0) {
      flowMinutes += initialCountdownValue;
      successAnimation();
      currentState = MENU;
      isCounting = false;
      Serial.println("Countdown finished, returning to MENU.");
    }
    updateDisplay();
    Serial.print("Counting DOWN: "); Serial.println(countdownValue);
  }
}

//=========================================================
// Success animation when a session ends
void successAnimation() {
  display.clearDisplay();
  int centerX = 64, centerY = 32;

  for (int radius = 2; radius <= 30; radius += 2) {
    display.drawCircle(centerX, centerY, radius, WHITE);
    display.display();
    delay(100);

    if (radius % 4 == 0) {
      display.clearDisplay();
      display.display();
      delay(2);
    }
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print("SUCCESS!");
  display.display();
  delay(500); // 0.5 seconds
  display.clearDisplay();
  display.display();
}

//=========================================================
// Handle rotary input for menu and countdown selection
// this is not my code, I need to modify my rotation code to adopt this.
void handleRotaryInput() {
  if (rotation == 0) return;  // No rotation detected

  // IDLE mode feature disabled
  // lastActivityTime = millis();  // Reset inactivity timer on any valid rotation
  Serial.print(millis());  // Print the current time in milliseconds
  Serial.print(" - Rotation detected, activity timer reset. Rotation: ");
  Serial.println(rotation);

  if (currentState == MENU) {
    menuIndex = (menuIndex + rotation + 3) % 3;  // Update for 3 menu options: UP, DOWN, Reset
    updateDisplay();
    Serial.print(millis());  // Print the current time in milliseconds
    Serial.print(" - Menu option: "); Serial.println(menuOptions[menuIndex]);
  } else if (currentState == SELECTING_DOWN_DURATION) {
    countdownValue = max(1, countdownValue + rotation);
    updateDisplay();
    Serial.print(millis());  // Print the current time in milliseconds
    Serial.print(" - Countdown value: "); Serial.println(countdownValue);
  }
}

// //=========================================================
// // Handle inactivity and switch to IDLE if necessary
// // NOTE(suraj): we don't want to have ideal feature and stop the display after a while
// // so I am going to comment this out.
// void handleInactivity(unsigned long currentMillis) {
//   // Comment out frequent serial prints to improve performance
//   // Serial.print(millis());
//   // Serial.print(" - Current time (millis): ");
//   // Serial.println(currentMillis);

//   // Serial.print(millis());
//   // Serial.print(" - Last activity time (millis): ");
//   // Serial.println(lastActivityTime);

//   // Make sure the subtraction does not cause an overflow/underflow
//   if (currentMillis >= lastActivityTime) {
//     unsigned long timeSinceLastActivity = currentMillis - lastActivityTime;

//     // Serial.print(millis());
//     // Serial.print(" - Time since last activity (ms): ");
//     // Serial.println(timeSinceLastActivity);

//     // Check if the user is in the MENU or selecting countdown duration mode
//     if ((currentState == MENU || currentState == SELECTING_DOWN_DURATION) && (timeSinceLastActivity > inactivityLimit)) {
//       if (currentState != IDLE) {
//         currentState = IDLE;
//         idleStartTime = millis();  // Record when IDLE mode starts
//         updateDisplay();
//         Serial.print(millis());  // Print the current time in milliseconds
//         Serial.println(" - IDLE state entered due to inactivity.");
//       }
//     }
//   } else {
//     // Comment out warning to reduce unnecessary serial prints
//     // Serial.println(" - Warning: currentMillis is less than lastActivityTime!");
//   }

//   // Check if the user has been in IDLE for more than 30 minutes and turn off the display
//   if (currentState == IDLE && !displayOff && (currentMillis - idleStartTime > displayOffTimeLimit)) {
//     displayOff = true;
//     display.ssd1306_command(SSD1306_DISPLAYOFF);  // Turn off the display
//     Serial.print(millis());  // Print the current time in milliseconds
//     Serial.println(" - Display turned off after 30 minutes of IDLE.");
//   }

//   // Exit IDLE if any rotary or button action happens
//   if (currentState == IDLE && (rotation != 0 || buttonPressed())) {
//     currentState = MENU;
//     lastActivityTime = millis();  // Reset inactivity timer upon exiting IDLE

//     // Turn the display back on if it was off
//     if (displayOff) {
//       display.ssd1306_command(SSD1306_DISPLAYON);
//       displayOff = false;
//       Serial.print(millis());  // Print the current time in milliseconds
//       Serial.println(" - Display turned back on.");
//     }

//     updateDisplay();
//     Serial.print(millis());  // Print the current time in milliseconds
//     Serial.println(" - Exiting IDLE mode. Back to MENU.");
//   }
// }
