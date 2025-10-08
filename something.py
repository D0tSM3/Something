import serial
import csv
import time
import os

CSV_FILE = r'C:\Users\Carl Rohan\Downloads\Something\Something.csv'
SERIAL_PORT = 'COM5'  # Set your Arduino port here
BAUD_RATE = 9600

def update_account(uid, ser):
    rows = []
    found = False
    current_balance = 0
    user_name = ""
    response_code = "OK"  # OK, REJECTED, UNKNOWN

    if os.path.exists(CSV_FILE):
        with open(CSV_FILE, 'r', newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if row['UID'].lower() == uid.lower():
                    found = True
                    user_name = row['Name']
                    balance = int(row['Balance'])
                    if balance < 100:
                        # Reject transaction due to insufficient funds
                        response_code = "REJECTED"
                        current_balance = balance
                    else:
                        # Deduct 100
                        new_balance = balance - 100
                        row['Balance'] = str(new_balance)
                        current_balance = new_balance
                    rows.append(row)
                else:
                    rows.append(row)
    else:
        print(f"{CSV_FILE} not found. Creating new file.")

    if not found:
        # Reject unknown UID - do not add account
        response_code = "UNKNOWN"
        user_name = ""
        current_balance = 0

    with open(CSV_FILE, 'w', newline='') as csvfile:
        fieldnames = ['UID', 'Name', 'Balance']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"UID: {uid} user: {user_name} balance: {current_balance} status: {response_code}")

    # Send to Arduino: format "CODE,Name,Balance\n"
    # Name and Balance empty if unknown or rejected accordingly
    response_str = f"{response_code},{user_name},{current_balance}\n"
    ser.write(response_str.encode())

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException:
        print(f"Cannot open serial port {SERIAL_PORT}.")
        return

    time.sleep(2)  # Allow connection to settle

    print("Listening for RFID scans...")
    while True:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            if line:
                print(f"Received UID: {line}")
                update_account(line, ser)
        except Exception as e:
            print(f"Error: {e}")

if __name__ == '__main__':
    main()