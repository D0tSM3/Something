#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Tap RFID Card");
}

void loop() {
  // Step 1: Wait for card
  static bool awaiting_menu = false;
  static bool awaiting_payment = false;
  if (!awaiting_menu && !awaiting_payment) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      uid.toUpperCase();
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Checking card...");
      Serial.print("RFID_SCANNED:"); Serial.println(uid);
      // Wait for result
      unsigned long t0 = millis();
      while (!Serial.available() && millis()-t0<3000) delay(10);
      if (Serial.available()) {
        String resp = Serial.readStringUntil('\n');
        resp.trim();
        if (resp == "UNKNOWN") {
          lcd.clear(); lcd.setCursor(0,0); lcd.print("Unknown Card!");
          delay(2000);
          lcd.clear(); lcd.setCursor(0,0); lcd.print("Tap RFID Card");
        } else if (resp.startsWith("OK,")) {
          // Greet user and show menu
          int s1 = resp.indexOf(','); int s2 = resp.indexOf(',',s1+1);
          String user = resp.substring(s1+1, s2);
          lcd.clear(); lcd.print("Hi,"); lcd.setCursor(0,1); lcd.print(user);
          delay(2000);
          lcd.clear(); lcd.setCursor(0,0); lcd.print("1: Check Balance"); lcd.setCursor(0,1); lcd.print("2: Order");
          awaiting_menu = true;
          // Send menu ready signal to Python
          Serial.println("MENU_READY");
        }
      }
      rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
    }
  }

  // Menu selection via Serial input (from Python)
  if (awaiting_menu) {
    if (Serial.available()) {
      String choice = Serial.readStringUntil('\n');
      choice.trim();
      
      if (choice == "1") {
        Serial.println("CHECK_BALANCE");
        // Wait for reply and show balance
        unsigned long t0 = millis();
        while (!Serial.available() && millis()-t0<2000) delay(10);
        if (Serial.available()) {
          String resp = Serial.readStringUntil('\n');
          resp.trim();
          if (resp.startsWith("BALANCE:")) {
            lcd.clear(); lcd.print("Balance:");
            lcd.setCursor(0,1); lcd.print(resp.substring(8) + " PHP");
          } else {
            lcd.clear(); lcd.print("Error!");
          }
          delay(2500);
          lcd.clear(); lcd.print("Tap RFID Card");
          awaiting_menu = false;
        }
      }
      else if (choice == "2") {
        Serial.println("ORDER");
        // Show ordering status
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Ordering...");
        lcd.setCursor(0,1); lcd.print("Please wait");
        
        // Show order prompt from Python
        unsigned long t0 = millis();
        while (!Serial.available() && millis()-t0<10000) delay(10); // Increased timeout for ordering
        if (Serial.available()) {
          String resp = Serial.readStringUntil('\n');
          if (resp.startsWith("SHOW_TOTAL:")) {
            lcd.clear();
            lcd.print("Total: ");
            lcd.print(resp.substring(11));
            lcd.setCursor(0, 1); lcd.print("Tap to pay!");
            awaiting_menu = false;
            awaiting_payment = true;
          }
        }
      }
    }
  }
  
  // After order, payment required by tapping card again
  if (awaiting_payment) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      uid.toUpperCase();
      Serial.print("RFID_SCANNED:"); Serial.println(uid);  // This triggers payment on Python
      // Wait for payment response
      unsigned long t0 = millis();
      while (!Serial.available() && millis()-t0<3000) delay(10);
      if (Serial.available()) {
        String resp = Serial.readStringUntil('\n');
        int s1 = resp.indexOf(','); int s2 = resp.indexOf(',',s1+1); int s3 = resp.indexOf(',',s2+1);
        String code = resp.substring(0,s1);
        String user = resp.substring(s1+1, s2);
        String balance = resp.substring(s2+1, s3);
        String purchase = resp.substring(s3+1);
        lcd.clear();
        if (code == "OK") {
          lcd.print("Order Success!");
          lcd.setCursor(0,1); lcd.print("Bal:"+balance+" PHP");
        } else if (code == "REJECTED") {
          lcd.print("Insufficient"); lcd.setCursor(0,1); lcd.print("Funds!");
        } else if (code == "UNKNOWN") {
          lcd.print("Unknown Card!");
        } else {
          lcd.print("Payment Error!");
        }
        delay(3000);
        Serial.println("TRANSACTION_COMPLETE");
        // Wait for RESET_SYSTEM then return to RFID waiting
        t0 = millis();
        while (!Serial.available() && millis()-t0<2000) delay(10);
        if (Serial.available()) {
          String x = Serial.readStringUntil('\n');
        }
        lcd.clear(); lcd.setCursor(0,0); lcd.print("Tap RFID Card");
        awaiting_payment = false;
      }
      rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
    }
  }
  delay(50);
}