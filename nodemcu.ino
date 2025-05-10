#include <SPI.h>
#include <MFRC522.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define RST_PIN D0
#define SS_PIN  D8
#define ADC_PIN A0

SoftwareSerial gpsSerial(D3, D4);
TinyGPSPlus gps;

MFRC522 mfrc522(SS_PIN, RST_PIN);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const float voltageDividerFactor = 12.0;
const float MIN_VOLTAGE = 11.8;
const float MAX_VOLTAGE = 12.6;

const byte validUID[4] = {0x6A, 0x07, 0x18, 0xB1};

String rfidStatus = "Waiting for RFID...";
String gpsStatus  = "GPS: N/A";

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();
  mfrc522.PCD_Init();

  gpsSerial.begin(9600);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 15, "System Starting...");
  u8g2.sendBuffer();
  delay(2000);
}

void loop() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }

  if (gps.location.isUpdated()) {
    char gpsStr[50];
    snprintf(gpsStr, sizeof(gpsStr), "Lat: %.6f", gps.location.lat());
    gpsStatus = String(gpsStr);
    gpsStatus += "\nLng: " + String(gps.location.lng(), 6);
    gpsStatus += "\nSpd: " + String(gps.speed.kmph()) + " km/h";
  }

  int adcValue = analogRead(ADC_PIN);
  float measuredVoltage = (adcValue / 1023.0) * 1.0;
  float batteryVoltage = measuredVoltage * voltageDividerFactor;
  int batteryPercentage = map(batteryVoltage * 100, (int)(MIN_VOLTAGE * 100), (int)(MAX_VOLTAGE * 100), 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);

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

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  drawBatteryIcon(batteryPercentage);

  char voltageStr[20];
  snprintf(voltageStr, sizeof(voltageStr), "V: %.2f V (%d%%)", batteryVoltage, batteryPercentage);
  u8g2.drawStr(0, 25, voltageStr);
  u8g2.drawStr(0, 40, rfidStatus.c_str());
  u8g2.drawStr(0, 55, gpsStatus.c_str());

  u8g2.sendBuffer();
  delay(1000);
}

void drawBatteryIcon(int percentage) {
  const int x = 0;
  const int y = 0;
  const int iconWidth = 20;
  const int iconHeight = 10;
  const int terminalWidth = 2;
  const int terminalHeight = 4;

  u8g2.drawFrame(x, y, iconWidth, iconHeight);
  u8g2.drawBox(x + iconWidth, y + (iconHeight - terminalHeight) / 2, terminalWidth, terminalHeight);
  int fillWidth = (percentage * (iconWidth - 2)) / 100;
  u8g2.drawBox(x + 1, y + 1, fillWidth, iconHeight - 2);
}
