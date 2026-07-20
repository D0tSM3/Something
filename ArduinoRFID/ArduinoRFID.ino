#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define MAX_RESPONSE_LEN 100

enum SystemState {
  WELCOME,
  ORDERING,
  WAITING_FOR_PAYMENT,
  PROCESSING_PAYMENT
};

SystemState currentState = WELCOME;
String currentUserUID = "";

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  // Welcome message
  lcd.setCursor(0, 0);
  lcd.print("Welcome!");
  lcd.setCursor(0, 1);
  lcd.print("Tap card to order");
  
  currentState = WELCOME;
}

void loop() {
  // Check for serial commands from Python
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("SHOW_TOTAL:")) {
      String total = command.substring(11);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Total: " + total);
      lcd.setCursor(0, 1);
      lcd.print("Tap card to pay");
      currentState = WAITING_FOR_PAYMENT;
    }
    else if (command.startsWith("START_ORDERING:")) {
      String userName = command.substring(15);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hello " + userName);
      lcd.setCursor(0, 1);
      lcd.print("Ordering...");
      currentState = ORDERING;
    }
    else if (command == "RESET_SYSTEM") {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Welcome!");
      lcd.setCursor(0, 1);
      lcd.print("Tap card to order");
      currentState = WELCOME;
      currentUserUID = "";
    }
  }

  // Handle RFID scanning based on current state
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    
    // Get UID as hex string
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    if (currentState == WELCOME) {
      // First tap - start ordering
      currentUserUID = uid;
      Serial.print("CARD_FOR_ORDERING:");
      Serial.println(uid);
      
    } else if (currentState == WAITING_FOR_PAYMENT) {
      // Second tap - process payment
      if (uid == currentUserUID) {
        currentState = PROCESSING_PAYMENT;
        Serial.print("RFID_SCANNED:");
        Serial.println(uid);

        // Read Python response: CODE,Name,Balance,Summary\n
        char response[MAX_RESPONSE_LEN];
        byte idx = 0;
        unsigned long startTime = millis();
        while (millis() - startTime < 4000 && idx < MAX_RESPONSE_LEN - 1) {
          if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') break;
            response[idx++] = c;
          }
        }
        response[idx] = '\0';

        String respStr = String(response);
        int firstComma = respStr.indexOf(',');
        int secondComma = respStr.indexOf(',', firstComma + 1);
        int thirdComma = respStr.indexOf(',', secondComma + 1);

        String code = "";
        String name = "";
        String balance = "";

        if (firstComma >= 0 && secondComma >= 0) {
          code = respStr.substring(0, firstComma);
          name = respStr.substring(firstComma + 1, secondComma);
          balance = respStr.substring(secondComma + 1, thirdComma >= 0 ? thirdComma : respStr.length());
        } else {
          code = respStr;
        }

        lcd.clear();

        if (code == "OK") {
          lcd.setCursor(0, 0);
          lcd.print("Payment Success!");
          delay(2000);

          // Show name and balance
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(name);
          lcd.setCursor(0, 1);
          lcd.print("Bal: ");
          lcd.print(balance);
          lcd.print(" PHP");
          delay(3000);
        } else if (code == "REJECTED") {
          lcd.print("Insufficient");
          lcd.setCursor(0, 1);
          lcd.print("Funds!");
          delay(3000);
        } else if (code == "UNKNOWN") {
          lcd.print("Unknown Card!");
          delay(3000);
        } else {
          lcd.print("Error!");
          delay(3000);
        }

        // Signal Python to reset system
        Serial.println("TRANSACTION_COMPLETE");
      } else {
        // Different card tapped
        lcd.clear();
        lcd.print("Please use same");
        lcd.setCursor(0, 1);
        lcd.print("card for payment");
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tap card to pay");
      }
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  delay(200);
}