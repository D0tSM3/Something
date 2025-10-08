#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define MAX_LINE_LEN 100

String currentOrder = "";

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome");
  delay(3000);

  lcd.clear();
  lcd.print("Enter order...");
}

// Helper function to scroll text in LCD line 1
void scrollText(const String &text, int width = 16, int delayMs = 700) {
  int len = text.length();
  if (len <= width) {
    lcd.setCursor(0, 1);
    lcd.print(text + String("                ").substring(0, width - len)); // pad spaces
    delay(delayMs);
  } else {
    for (int i = 0; i <= len - width; i++) {
      lcd.setCursor(0, 1);
      lcd.print(text.substring(i, i + width));
      delay(delayMs);
    }
    lcd.setCursor(0, 1);
    lcd.print("                "); // clear line
    delay(delayMs);
  }
}

// Show current order
void showOrder() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Order:");
  scrollText(currentOrder);
}

// Show a 2-line message
void showMessage(const String &line1, const String &line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// Process received serial line
void processLine(const String &line) {
  if (line.startsWith("ORDER:")) {
    currentOrder = line.substring(6); // remove "ORDER:"
    showOrder();
  } else if (line.startsWith("PAY:")) {
    // Expect format PAY:CODE,Name,Balance
    int comma1 = line.indexOf(',', 4);
    int comma2 = line.indexOf(',', comma1 + 1);
    if (comma1 > 4 && comma2 > comma1) {
      String code = line.substring(4, comma1);
      String name = line.substring(comma1 + 1, comma2);
      String balance = line.substring(comma2 + 1);
      if (code == "OK") {
        showMessage("Payment Success", name + " Bal:" + balance + "PHP");
      } else if (code == "REJECTED") {
        showMessage("Payment", "Rejected: Low Funds");
      } else if (code == "UNKNOWN") {
        showMessage("Unknown Card", "");
      } else {
        showMessage("Payment", "Error");
      }
    } else {
      showMessage("Malformed", "Payment Data");
    }
  }
}

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Send UID to Python
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
      Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();
    
    // Reset order display waiting for next order
    currentOrder = "";
  }

  // Read and process complete lines from Serial
  static String inputLine = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      inputLine.trim();
      if (inputLine.length() > 0) {
        processLine(inputLine);
      }
      inputLine = "";
    } else {
      inputLine += c;
    }
  }
  
  delay(50);
}