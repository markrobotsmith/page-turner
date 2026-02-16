#include <bluefruit.h>

// --- Config ---
#define PIN_BTN_NEXT    0  // D0
#define PIN_BTN_PREV    1  // D1

// XIAO nRF52840 specific internal pins for battery
#define PIN_VBAT        32 // P0.31 (AIN7) - Battery voltage sense
#define PIN_VBAT_ENABLE 14 // P0.14 - LOW to enable battery reading
#define PIN_HICHG       22 // P0.17 - Charge current setting (LOW=100mA, HIGH=50mA)
#define PIN_CHG         23 // P0.13 - Charge indicator (LOW=charging, HIGH=not charging)

// Auto-sleep timeout (1/2 hour = 3600000 ms)
#define INACTIVITY_TIMEOUT_MS 1800000UL  // 30min
//#define INACTIVITY_TIMEOUT_MS 60000UL  // 1 minute for testing

// Battery update interval (every 60 seconds)
#define BATTERY_UPDATE_INTERVAL 60000UL

// Warning time before auto-sleep (5 minutes before timeout)
#define WARNING_TIME_BEFORE_SLEEP 300000UL  // 5 minutes

BLEHidAdafruit blehid;
BLEBas blebas; // Battery Service
BLEDis bledis;

// Track last activity time
unsigned long lastActivityTime = 0;
unsigned long lastBatteryUpdate = 0;
bool warningShown = false;

void setup() {
  // 1. Setup Hardware
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  
  // Turn off all LEDs (active LOW on XIAO)
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  
  // Battery monitoring pins
  pinMode(PIN_VBAT, INPUT);
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  pinMode(PIN_HICHG, OUTPUT);
  pinMode(PIN_CHG, INPUT);
  
  digitalWrite(PIN_VBAT_ENABLE, LOW);  // Enable battery reading
  digitalWrite(PIN_HICHG, LOW);        // Set charge current to 100mA
  
  // Initialize ADC for battery reading
  analogReference(AR_DEFAULT);    // 0.6V * 6 = 3.6V reference
  analogReadResolution(12);       // 12-bit resolution (0-4095)

  // 2. Setup Bluetooth
  Bluefruit.begin();
  Bluefruit.setTxPower(4); // +4dBm for good range
  Bluefruit.setName("PageTurner");
  
  // CRITICAL: Disable connection LED to prevent advertising issues
  Bluefruit.setConnLedInterval(0);
  
  bledis.setManufacturer("markrobotsmith");
  bledis.setModel("ptv1");
  bledis.begin();

  // Security setup for pairing
  Bluefruit.Security.setMITM(false);
  Bluefruit.Security.setIOCaps(false, false, false);
  
  // 3. Setup Services
  blebas.begin(); // Battery service
  blehid.begin();

  // 4. Advertising Packet (Keep it minimal to avoid overflow)
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  
  // 5. Scan Response (Put additional services here to avoid packet overflow)
  Bluefruit.ScanResponse.addName();
  Bluefruit.ScanResponse.addService(blebas); // Battery service in scan response
  
  // 6. Start Advertising
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // Fast/Slow adv intervals in 0.625ms units
  Bluefruit.Advertising.setFastTimeout(30);   // Fast adv for 30s
  Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising
  
  // === STARTUP FEEDBACK ===
  // Double blue blink: Device is powered on and advertising
  blinkLED(LED_BLUE, 2, 200);
  
  // 7. Setup Deep Sleep Wakeup Pins
  nrf_gpio_cfg_sense_input(g_ADigitalPinMap[PIN_BTN_NEXT], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  nrf_gpio_cfg_sense_input(g_ADigitalPinMap[PIN_BTN_PREV], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  
  // Initialize activity timer
  lastActivityTime = millis();
  lastBatteryUpdate = millis();
  
  // Initial battery reading
  updateBattery();
}

void loop() {
  // === PERIODIC BATTERY UPDATE ===
  // Update battery level every 60 seconds when connected
  if (Bluefruit.connected() && (millis() - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL)) {
    updateBattery();
    lastBatteryUpdate = millis();
  }
  
  // === CONNECTION STATUS INDICATOR ===
  // Slow breathing green when connected (every 3 seconds)
  static unsigned long lastBreathTime = 0;
  if (Bluefruit.connected() && (millis() - lastBreathTime > 3000)) {
    breathLED(LED_GREEN, 100);
    lastBreathTime = millis();
  }
  
  // === PAIRING RESET LOGIC ===
  if (digitalRead(PIN_BTN_NEXT) == LOW) {
    unsigned long startPress = millis();
    while (digitalRead(PIN_BTN_NEXT) == LOW) {
      // Hold indicator - blink blue faster as you hold longer
      if (millis() - startPress > 5000) {
        // 5 Seconds passed: PAIRING RESET
        // Rapid blue flashes to confirm reset
        blinkLED(LED_BLUE, 10, 50);
        Bluefruit.Central.clearBonds(); // Forget all phones
        Bluefruit.Advertising.restartOnDisconnect(true); 
        return;
      } else if (millis() - startPress > 3000) {
        // Warning: getting close to reset
        pulseLED(LED_BLUE, 1);
      }
    }
    
    // === BUTTON PRESS FEEDBACK ===
    if (Bluefruit.connected()) {
      // Send key command
      uint8_t keycodesLeft[6] = { HID_KEY_ARROW_RIGHT, HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE };
      blehid.keyboardReport(0, keycodesLeft);
      blehid.keyRelease();
      
      // Quick green flash: Key sent successfully
      flashQuick(LED_GREEN);
      
      // Reset inactivity timer and warning flag
      lastActivityTime = millis();
      warningShown = false;
    } else {
      // Not connected - quick red flash
      flashQuick(LED_RED);
    }
    
    delay(200); // Debounce
  }

  if (digitalRead(PIN_BTN_PREV) == LOW) {
    if (Bluefruit.connected()) {
      // Send key command
      uint8_t keycodesRight[6] = { HID_KEY_ARROW_LEFT, HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE , HID_KEY_NONE };
      blehid.keyboardReport(0, keycodesRight);
      blehid.keyRelease();
      
      // Quick green flash: Key sent successfully
      flashQuick(LED_GREEN);
      
      // Reset inactivity timer and warning flag
      lastActivityTime = millis();
      warningShown = false;
    } else {
      // Not connected - quick red flash
      flashQuick(LED_RED);
    }
    
    delay(200); // Debounce
  }

  // === INACTIVITY WARNING ===
  // Warn 5 minutes before auto-sleep
  if (Bluefruit.connected() && !warningShown) {
    unsigned long inactiveTime = millis() - lastActivityTime;
    
    if (inactiveTime >= (INACTIVITY_TIMEOUT_MS - WARNING_TIME_BEFORE_SLEEP)) {
      // Yellow (red + green) warning blink
      blinkYellow(3, 300);
      warningShown = true;
    }
  }

  // === AUTO-SLEEP LOGIC (INACTIVITY) ===
  if (Bluefruit.connected()) {
    unsigned long inactiveTime = millis() - lastActivityTime;
    
    if (inactiveTime >= INACTIVITY_TIMEOUT_MS) {
      // Going to sleep - triple red blink
      blinkLED(LED_RED, 3, 300);
      
      // Disconnect gracefully
      Bluefruit.disconnect(Bluefruit.connHandle());
      delay(500);
      
      // Disable battery reading to save power
      digitalWrite(PIN_VBAT_ENABLE, HIGH);
      
      // Turn off all LEDs
      allLedsOff();
      
      // Enter System OFF
      sd_power_system_off(); 
    }
  }
  
  // === SLEEP LOGIC (NOT CONNECTED) ===
  // If not connected for 60s, go to DEEP SLEEP
  if (!Bluefruit.connected() && millis() > 60000) { 
     // Triple red blink before sleep
     blinkLED(LED_RED, 3, 300);
     
     // Disable battery reading to save power
     digitalWrite(PIN_VBAT_ENABLE, HIGH);
     
     allLedsOff();
     sd_power_system_off(); 
  }
}

// === BATTERY MONITORING ===
void updateBattery() {
  // Read ADC value (12-bit = 0-4095)
  int vbat_raw = analogRead(PIN_VBAT);
  
  // Convert to voltage using XIAO's resistor divider (2.961 ratio) and 3.6V reference
  // Voltage = ADC * (Vref / ADC_max) * Resistor_ratio
  float voltage = vbat_raw * (3.6 / 4095.0) * 2.961;
  
  // Convert voltage to percentage (LiPo: 4.2V = 100%, 3.3V = 0%)
  // Using a more accurate discharge curve
  int percentage;
  
  if (voltage >= 4.2) {
    percentage = 100;
  } else if (voltage >= 4.0) {
    // 4.2V - 4.0V = 100% - 80% (fast drop at top)
    percentage = 80 + (int)((voltage - 4.0) * 100);
  } else if (voltage >= 3.7) {
    // 4.0V - 3.7V = 80% - 40% (linear middle section)
    percentage = 40 + (int)((voltage - 3.7) * 133);
  } else if (voltage >= 3.4) {
    // 3.7V - 3.4V = 40% - 10% (linear)
    percentage = 10 + (int)((voltage - 3.4) * 100);
  } else if (voltage >= 3.3) {
    // 3.4V - 3.3V = 10% - 0% (rapid drop at bottom)
    percentage = (int)((voltage - 3.3) * 100);
  } else {
    percentage = 0;
  }
  
  // Clamp to valid range
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  
  // Update BLE Battery Service
  blebas.write(percentage);
  
  // Optional: Check if charging
  // bool isCharging = (digitalRead(PIN_CHG) == LOW);
}

// === LED HELPER FUNCTIONS ===

// Quick flash (50ms) - for button feedback
void flashQuick(uint8_t pin) {
  digitalWrite(pin, LOW);
  delay(50);
  digitalWrite(pin, HIGH);
}

// Standard blink
void blinkLED(uint8_t pin, int count, int delayMs) {
  for(int i=0; i<count; i++) {
    digitalWrite(pin, LOW);
    delay(delayMs);
    digitalWrite(pin, HIGH);
    delay(delayMs);
  }
}

// Breathing effect (slow fade in/out)
void breathLED(uint8_t pin, int duration) {
  digitalWrite(pin, LOW);
  delay(duration);
  digitalWrite(pin, HIGH);
}

// Pulse effect (quick on/off)
void pulseLED(uint8_t pin, int count) {
  for(int i=0; i<count; i++) {
    digitalWrite(pin, LOW);
    delay(100);
    digitalWrite(pin, HIGH);
    delay(100);
  }
}

// Yellow warning (both red and green)
void blinkYellow(int count, int delayMs) {
  for(int i=0; i<count; i++) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    delay(delayMs);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    delay(delayMs);
  }
}

// Turn off all LEDs
void allLedsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
}
