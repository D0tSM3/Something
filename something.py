import serial
import csv
import time
import os

CSV_FILE = r'C:\Users\Carl Rohan\Downloads\Something\Something.csv'
SERIAL_PORT = 'COM5'  # Change to your Arduino COM port
BAUD_RATE = 9600

MENU = {
    '1': ("Fried Rice", 50, "FR"),
    '2': ("Chicken Adobo", 100, "CA"),
    '3': ("Pork Sisig", 150, "S"),
    '4': ("Beef Steak", 200, "BS"),
    '5': ("Lechon Kawali", 250, "LK"),
}

def print_menu():
    print("\nMenu:")
    for k, (name, price, code) in MENU.items():
        print(f"{k}: {name} ({code}) - {price} PHP")
    print("6: Finish ordering")

def collect_order():
    order_items = {}
    while True:
        print_menu()
        inp = input("Enter item number and quantity (e.g. 1 3) or '6' to finish: ").strip().lower()
        if inp == '6':
            break
        parts = inp.split()
        if len(parts) != 2:
            print("Invalid input, format: item_number quantity")
            continue

        item_num, qty_str = parts
        if item_num not in MENU:
            print("Invalid item number")
            continue
        try:
            qty = int(qty_str)
            if qty < 1:
                print("Quantity must be positive")
                continue
        except ValueError:
            print("Invalid quantity")
            continue

        if item_num in order_items:
            order_items[item_num] += qty
        else:
            order_items[item_num] = qty

        summary = ", ".join(f"{v}x({MENU[k][2]})" for k, v in order_items.items())
        ser.write(f"ORDER:{summary}\n".encode())
        print(f"Current order: {summary}")
    return order_items

def update_account(uid, ser, purchased_summary, total_price):
    rows = []
    found = False
    current_balance = 0
    user_name = ""
    response_code = "OK"

    if os.path.exists(CSV_FILE):
        with open(CSV_FILE, 'r', newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if row['UID'].lower() == uid.lower():
                    found = True
                    user_name = row['Name']
                    balance = int(row['Balance'])
                    if balance < total_price:
                        response_code = "REJECTED"
                        current_balance = balance
                    else:
                        new_balance = balance - total_price
                        row['Balance'] = str(new_balance)
                        current_balance = new_balance
                    rows.append(row)
                else:
                    rows.append(row)
    else:
        print(f"{CSV_FILE} not found. Creating new file.")

    if not found:
        response_code = "UNKNOWN"
        user_name = ""
        current_balance = 0

    with open(CSV_FILE, 'w', newline='') as csvfile:
        fieldnames = ['UID', 'Name', 'Balance']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"\nUID: {uid}\nUser: {user_name}\nTotal price: {total_price} PHP\nBalance after: {current_balance} PHP\nStatus: {response_code}")
    print(f"Purchased: {purchased_summary}")

    ser.write(f"PAY:{response_code},{user_name},{current_balance}\n".encode())

def main():
    global ser
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException:
        print(f"Cannot open serial port {SERIAL_PORT}. Check connection.")
        return

    time.sleep(2)  # Allow connection to stabilize
    print("Welcome! Waiting for RFID scan...")

    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line:
            print(f"\nScanned UID: {line}")

            order_items = collect_order()
            if not order_items:
                print("No items selected. Cancelling transaction.")
                continue

            purchased_desc = ", ".join(f"{qty}x({MENU[item][2]})" for item, qty in order_items.items())
            print(f"\nFinal Order: {purchased_desc}")

            ser.write(f"ORDER:{purchased_desc}\n".encode())

            print("Please tap your RFID card to pay...")

            total_price = sum(MENU[item][1] * qty for item, qty in order_items.items())
            update_account(line, ser, purchased_desc, total_price)

            print("Ready for next order.\n")

if __name__ == '__main__':
    main()