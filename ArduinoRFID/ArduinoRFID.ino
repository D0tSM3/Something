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

// Helper to scroll text on LCD line
void scrollText(const String &text, int row, int width = 16, int delayMs = 700) {
  int len = text.length();
  if (len <= width) {
    lcd.setCursor(0, row);
    lcd.print(text);
    delay(delayMs);
  } else {
    for (int i = 0; i <= len - width; i++) {
      lcd.setCursor(0, row);
      lcd.print(text.substring(i, i + width));
      delay(delayMs);
    }
    // Replace print(" ".repeat(width)) with a loop
    lcd.setCursor(0, row);
    for (int i = 0; i < width; i++) {
      lcd.print(' ');
    }
    delay(delayMs);
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  // Welcome message
  lcd.setCursor(0, 0);
  lcd.print("Welcome!");
  delay(3000);

  // Signal Python to start ordering
  Serial.println("START_ORDERING");
  
  lcd.clear();
  lcd.print("Ordering...");
  currentState = ORDERING;
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
      lcd.print("Please tap to pay");
      currentState = WAITING_FOR_PAYMENT;
    }
    else if (command == "ORDERING_COMPLETE") {
      currentState = WAITING_FOR_PAYMENT;
    }
    else if (command == "RESET_SYSTEM") {
      lcd.clear();
      lcd.print("Welcome!");
      delay(3000);
      Serial.println("START_ORDERING");
      lcd.clear();
      lcd.print("Ordering...");
      currentState = ORDERING;
    }
  }

  // Handle RFID scanning only when waiting for payment
  if (currentState == WAITING_FOR_PAYMENT) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      currentState = PROCESSING_PAYMENT;
      
      // Send UID as hex string via Serial
      Serial.print("RFID_SCANNED:");
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(rfid.uid.uidByte[i], HEX);
      }
      Serial.println();

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
      String purchaseDesc = "";

      if (firstComma >= 0 && secondComma >= 0 && thirdComma >= 0) {
        code = respStr.substring(0, firstComma);
        name = respStr.substring(firstComma + 1, secondComma);
        balance = respStr.substring(secondComma + 1, thirdComma);
        purchaseDesc = respStr.substring(thirdComma + 1);
      } else {
        code = respStr;
      }

      lcd.clear();

      if (code == "OK") {
        lcd.setCursor(0, 0);
        lcd.print("Purchase Success");
        delay(2000);

        // Scroll purchase description line by line (split by commas)
        int start = 0;
        int end = purchaseDesc.indexOf(',');
        while (end != -1) {
          scrollText(purchaseDesc.substring(start, end), 0);
          start = end + 1;
          end = purchaseDesc.indexOf(',', start);
        }
        // Last portion after last comma
        scrollText(purchaseDesc.substring(start), 0);

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

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  delay(200);
}