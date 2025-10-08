#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>

// RFID pins
#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buffer size for response from Python
#define MAX_RESPONSE_LEN 64

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Access Control");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(2000);

  lcd.clear();
  lcd.print("Scan RFID Card");
  lcd.setCursor(0, 1);
  lcd.print("to Deduct");
}

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Send UID as hex string over Serial
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
      Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    // Read response from Python: "CODE,Name,Balance\n"
    char response[MAX_RESPONSE_LEN];
    byte idx = 0;
    unsigned long startTime = millis();
    while (millis() - startTime < 3000 && idx < MAX_RESPONSE_LEN - 1) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') break;
        response[idx++] = c;
      }
    }
    response[idx] = '\0';  // Null-terminate

    String respStr = String(response);
    int firstComma = respStr.indexOf(',');
    int secondComma = respStr.indexOf(',', firstComma + 1);

    String code = "";
    String name = "";
    String balance = "";

    if (firstComma >= 0 && secondComma >= 0) {
      code = respStr.substring(0, firstComma);
      name = respStr.substring(firstComma + 1, secondComma);
      balance = respStr.substring(secondComma + 1);
    } else {
      code = respStr;  // fallback if format invalid
    }

    lcd.clear();

    if (code == "OK") {
      lcd.setCursor(0, 0);
      lcd.print("Purchase Success");
      delay(3000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(name);
      lcd.setCursor(0, 1);
      lcd.print("Bal: ");
      lcd.print(balance);
      lcd.print(" PHP");
      delay(3000);
    } 
    else if (code == "REJECTED") {
      lcd.print("Insufficient");
      lcd.setCursor(0, 1);
      lcd.print("Funds!");
      delay(3000);
    } 
    else if (code == "UNKNOWN") {
      lcd.print("Unknown Card!");
      delay(3000);
    } 
    else {
      lcd.print("Error!");
      delay(3000);
    }

    lcd.clear();
    lcd.print("Scan RFID Card");
    lcd.setCursor(0, 1);
    lcd.print("to Deduct");

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  delay(200);
}