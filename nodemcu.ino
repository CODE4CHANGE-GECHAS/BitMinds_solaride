#include <SPI.h>
#include <MFRC522.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// --------------------- Pin Definitions ---------------------
// RFID Module Pins using NodeMCU D-pin nomenclature:
#define RST_PIN D0       // RFID reset pin (D0 corresponds to GPIO16)
#define SS_PIN  D8       // RFID Slave Select (D8 corresponds to GPIO15)

// Battery voltage sensor on ADC pin (voltage divider output)
#define ADC_PIN A0

// GPS Module: using SoftwareSerial
// Connect GPS TX to NodeMCU D3 (RX) and GPS RX to NodeMCU D4 (TX, if needed)
SoftwareSerial gpsSerial(D3, D4); // RX = D3, TX = D4
TinyGPSPlus gps;                  // Create TinyGPS++ object

// Create RFID instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Initialize the SH1106 OLED display (128x64) via hardware I2C
// NodeMCU default I2C pins are typically used.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --------------------- Battery Voltage Settings ---------------------
const float voltageDividerFactor = 12.0;  // Example: 12V battery becomes ~1V at ADC
const float MIN_VOLTAGE = 11.8;  // Voltage corresponding to 0% battery
const float MAX_VOLTAGE = 12.6;  // Voltage corresponding to 100% battery

// --------------------- RFID Settings ---------------------
const byte validUID[4] = {0x6A, 0x07, 0x18, 0xB1};  // Predefined valid UID

// Global status strings
String rfidStatus = "Waiting for RFID...";
String gpsStatus  = "GPS: N/A";

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial to initialize
  
  // Initialize RFID module
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RFID system ready.");

  // Initialize GPS module
  gpsSerial.begin(9600);
  Serial.println("GPS system ready.");

  // Initialize OLED display
  u8g2.begin();
  
  // Optionally display a startup message
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 15, "System Starting...");
  u8g2.sendBuffer();
  delay(2000);
}

void loop() {
  // --------------------- Process GPS Data ---------------------
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }
  
  // Update GPS status if new data is available
  if (gps.location.isUpdated()) {
    char gpsStr[50];
    snprintf(gpsStr, sizeof(gpsStr), "Lat: %.6f", gps.location.lat());
    gpsStatus = String(gpsStr);
    gpsStatus += "\nLng: " + String(gps.location.lng(), 6);
    gpsStatus += "\nSpd: " + String(gps.speed.kmph()) + " km/h";
  }

  // --------------------- Process Battery Voltage ---------------------
  int adcValue = analogRead(ADC_PIN);
  float measuredVoltage = (adcValue / 1023.0) * 1.0; // ADC assumes 0–1V range
  float batteryVoltage = measuredVoltage * voltageDividerFactor;
  int batteryPercentage = map(batteryVoltage * 100, (int)(MIN_VOLTAGE * 100), (int)(MAX_VOLTAGE * 100), 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  
  // --------------------- Process RFID Data ---------------------
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    bool isValid = true;
    if (mfrc522.uid.size != 4) {
      isValid = false;
    } else {
      for (byte i = 0; i < 4; i++) {
        if (mfrc522.uid.uidByte[i] != validUID[i]) {
          isValid = false;
          break;
        }
      }
    }
    
    if (isValid) {
      rfidStatus = "RFID: UNLOCK";
      Serial.println("RFID: UNLOCK");
      // Send unlock command via Serial for the connected Arduino (which drives the servo)
      Serial.println("UNLOCK");
    } else {
      rfidStatus = "RFID: INVALID";
      Serial.println("RFID: INVALID");
    }
    
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(500);
  } else {
    rfidStatus = "Waiting for RFID...";
  }
  
  // --------------------- Update OLED Display ---------------------
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  
  // Draw a small battery icon in the top-left corner
  drawBatteryIcon(batteryPercentage);
  
  // Display battery voltage and percentage near the icon
  char voltageStr[20];
  snprintf(voltageStr, sizeof(voltageStr), "V: %.2f V (%d%%)", batteryVoltage, batteryPercentage);
  u8g2.drawStr(0, 25, voltageStr);
  
  // Display RFID status
  u8g2.drawStr(0, 40, rfidStatus.c_str());
  
  // Display GPS status (we break the string by line if needed)
  u8g2.drawStr(0, 55, gpsStatus.c_str());
  
  u8g2.sendBuffer();
  
  delay(1000);  // Update every second
}

// Function to draw a compact battery icon at the top-left corner
void drawBatteryIcon(int percentage) {
  const int x = 0;            // X-coordinate of the icon
  const int y = 0;            // Y-coordinate of the icon
  const int iconWidth = 20;   // Battery body width
  const int iconHeight = 10;  // Battery body height
  const int terminalWidth = 2;// Battery terminal width
  const int terminalHeight = 4;// Battery terminal height

  // Draw battery outline
  u8g2.drawFrame(x, y, iconWidth, iconHeight);
  // Draw battery terminal (on the right side)
  u8g2.drawBox(x + iconWidth, y + (iconHeight - terminalHeight) / 2, terminalWidth, terminalHeight);
  // Calculate fill width (leave a 1-pixel margin)
  int fillWidth = (percentage * (iconWidth - 2)) / 100;
  // Draw filled part of battery
  u8g2.drawBox(x + 1, y + 1, fillWidth, iconHeight - 2);
}