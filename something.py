import serial
import csv
import time
import os

CSV_FILE = r'C:\Users\Pocoyo\Desktop\coding\Arduino_Rfid\Something\Something.csv'
SERIAL_PORT = 'COM6'  # Change to actual port
BAUD_RATE = 9600

MENU = {
    '1': ("Fried Rice", 50),
    '2': ("Chicken Adobo", 100),
    '3': ("Pork Sisig", 150),
    '4': ("Beef Steak", 200),
    '5': ("Lechon Kawali", 250),
}

current_order = []
total_price = 0

def display_menu():
    print("\n" + "="*40)
    print("           FOOD MENU")
    print("="*40)
    for num, (name, price) in MENU.items():
        print(f"{num}: {name} - {price} PHP")
    print("="*40)
    print("Instructions:")
    print("- Enter item number (1-5)")
    print("- For multiple quantities: '2x3' (3 Chicken Adobo)")
    print("- Type 'done' when finished ordering")
    print("="*40)

def add_to_order(selection):
    global current_order, total_price
    
    if 'x' in selection:
        try:
            item_num, qty_str = selection.split('x')
            item_num = item_num.strip()
            qty = int(qty_str.strip())
        except ValueError:
            print(f"Invalid format: {selection}")
            return False
    else:
        item_num = selection.strip()
        qty = 1

    if item_num in MENU and qty > 0:
        name, price = MENU[item_num]
        item_total = price * qty
        current_order.append({
            'name': name,
            'qty': qty,
            'price': price,
            'total': item_total
        })
        total_price += item_total
        print(f"Added: {qty}x {name} - {item_total} PHP")
        print(f"Current total: {total_price} PHP")
        return True
    else:
        print(f"Invalid item or quantity: {selection}")
        return False

def get_order_summary():
    global current_order
    detailed_items = []
    for item in current_order:
        detailed_items.append(f"{item['qty']}x {item['name']} ({item['total']}PHP)")
    return ', '.join(detailed_items)

def take_order(ser):
    global current_order, total_price
    current_order = []
    total_price = 0
    
    display_menu()
    
    while True:
        selection = input("Enter your selection (or 'done' to finish): ").strip().lower()
        
        if selection == 'done':
            if current_order:
                print(f"\nOrder Summary:")
                for item in current_order:
                    print(f"  {item['qty']}x {item['name']} - {item['total']} PHP")
                print(f"Total: {total_price} PHP")
                
                # Send total to Arduino
                ser.write(f"SHOW_TOTAL:{total_price} PHP\n".encode())
                return get_order_summary(), total_price
            else:
                print("No items ordered. Please add items or type 'done' to cancel.")
        else:
            add_to_order(selection)

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

    if response_code == "OK":
        with open(CSV_FILE, 'w', newline='') as csvfile:
            fieldnames = ['UID', 'Name', 'Balance']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

    print(f"\nTransaction Details:")
    print(f"UID: {uid}")
    print(f"User: {user_name}")
    print(f"Balance after purchase: {current_balance} PHP")
    print(f"Status: {response_code}")
    print(f"Purchased items: {purchased_summary}")
    print(f"Total price: {total_price} PHP")

    # Limit summary length
    send_summary = purchased_summary
    if len(send_summary) > 100:
        send_summary = send_summary[:97] + "..."

    response_str = f"{response_code},{user_name},{current_balance},{send_summary}\n"
    ser.write(response_str.encode())

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException:
        print(f"Cannot open serial port {SERIAL_PORT} - check it.")
        return

    time.sleep(2)
    print("System ready. Waiting for Arduino...")

    current_order_summary = ""
    current_total = 0

    while True:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            if line:
                print(f"Received: {line}")

                if line == "START_ORDERING":
                    print("\nStarting new order...")
                    current_order_summary, current_total = take_order(ser)

                elif line.startswith("RFID_SCANNED:"):
                    uid = line.replace("RFID_SCANNED:", "")
                    print(f"\nRFID Card Detected: {uid}")
                    print(f"Processing payment for: {current_order_summary}")
                    print(f"Total amount: {current_total} PHP")
                    
                    update_account(uid, ser, current_order_summary, current_total)

                elif line == "TRANSACTION_COMPLETE":
                    print("\nTransaction completed. Resetting system...")
                    time.sleep(2)
                    ser.write("RESET_SYSTEM\n".encode())

        except Exception as e:
            print(f"Error: {e}")

if __name__ == '__main__':
    main()